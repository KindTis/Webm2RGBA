#include "WebmDecoder.h"

#include <glew/glew.h>
#include <glfw/glfw3.h>
#include <glm/glm.hpp>

#include "tdogl/Program.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <memory>
#include <tchar.h>

#define ScreenWidth 800
#define ScreenHeight 600

void OnError(int errorCode, const char* msg) {
	throw std::runtime_error(msg);
}

class OpenglApp
{
public:
	OpenglApp() : SCREEN_SIZE(ScreenWidth, ScreenHeight)
	{

	}

public:
	bool InitApp(const std::string &vertex, const std::string &fragment)
	{
		if (!_CreateWindow())
			return false;

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		std::vector<tdogl::Shader> shaders;
		shaders.push_back(tdogl::Shader::shaderFromFile(vertex.c_str(), GL_VERTEX_SHADER));
		shaders.push_back(tdogl::Shader::shaderFromFile(fragment.c_str(), GL_FRAGMENT_SHADER));
		mProgram = std::make_unique<tdogl::Program>(shaders);

		assert(mProgram && "there is no program shader");
		if (!mProgram)
			return false;

		glGenVertexArrays(1, &mVAO);
		glBindVertexArray(mVAO);

		glGenBuffers(1, &mVBO);
		glBindBuffer(GL_ARRAY_BUFFER, mVBO);

		GLfloat vertexData[] = {
			//  X     Y     Z       U     V
			-1.0f, -1.0f, 0.0f,   0.0f, 1.0f,
			-1.0f, 1.0f, 0.0f,    0.0f, 0.0f,
			1.0f, 1.0f, 0.0f,	  1.0f, 0.0f,

			1.0f, 1.0f, 0.0f,	  1.0f, 0.0f,
			-1.0f, -1.0f, 0.0f,   0.0f, 1.0f,
			1.0f, -1.0f, 0.0f,    1.0f, 1.0f,
		};
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertexData), vertexData, GL_STATIC_DRAW);

		glEnableVertexAttribArray(mProgram->attrib("vert"));
		glVertexAttribPointer(mProgram->attrib("vert"), 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), nullptr);

		glEnableVertexAttribArray(mProgram->attrib("vertTexCoord"));
		glVertexAttribPointer(mProgram->attrib("vertTexCoord"), 2, GL_FLOAT, GL_TRUE, 5 * sizeof(GLfloat), (const GLvoid*)(3 * sizeof(GLfloat)));

		glBindVertexArray(0);

		glGenTextures(1, &textureID);
		rgba = new unsigned char[ScreenWidth * ScreenHeight * 4];
		for (int i = 0; i < 800 * 600 * 4; i += 4)
		{
			rgba[i + 0] = 255;
			rgba[i + 1] = 255;
			rgba[i + 2] = 0;
			rgba[i + 3] = 255;
		}
		return true;
	}

	bool LoadWebm(const std::string &webmPath, bool loop)
	{
		mWebmDecoder = std::make_unique<WebmDecoder>();
		return mWebmDecoder->Load(webmPath, loop);
	}

	void Run()
	{
		if (!mWebmDecoder)
			return;

		while (!glfwWindowShouldClose(mWindow))
		{
			glfwPollEvents();
			mWebmDecoder->Update();
			_Render();
		}

		glfwTerminate();
		delete[] rgba;
		rgba = nullptr;
		mWebmDecoder = nullptr;
	}

private:
	bool _CreateWindow()
	{
		glfwSetErrorCallback(OnError);
		if (!glfwInit())
			return false;

		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
		glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
		mWindow = glfwCreateWindow((int)SCREEN_SIZE.x, (int)SCREEN_SIZE.y, "OpenGL Tutorial", nullptr, nullptr);
		if (!mWindow)
			return false;

		glfwMakeContextCurrent(mWindow);

		glewExperimental = GL_TRUE;
		if (glewInit() != GLEW_OK)
			return false;

		std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
		std::cout << "GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
		std::cout << "Vendor: " << glGetString(GL_VENDOR) << std::endl;
		std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;

		if (!GLEW_VERSION_3_2)
			return false;

		return true;
	}

	void _Render()
	{
		assert(mWebmDecoder);
		int width = 0, height = 0;
		uint8_t *pixels = nullptr;

		std::tie(width, height, pixels) = mWebmDecoder->GetRGBA();

		glClearColor(0, 0, 1, 1);
		glClear(GL_COLOR_BUFFER_BIT);

		mProgram->use();

		glActiveTexture(GL_TEXTURE0);

		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

		mProgram->setUniform("tex", 0);
		glBindVertexArray(mVAO);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 6);
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
		mProgram->stopUsing();

		glfwSwapBuffers(mWindow);
	}

private:
	const glm::vec2 SCREEN_SIZE;
	GLFWwindow* mWindow = nullptr;
	std::unique_ptr<tdogl::Program> mProgram;
	std::unique_ptr<WebmDecoder> mWebmDecoder;
	GLuint mVAO = 0;
	GLuint mVBO = 0;
	GLuint textureID = 0;
	unsigned char *rgba = nullptr;
};


int _tmain(int argc, _TCHAR* argv[])
{
	OpenglApp app;
	if (!app.InitApp("shader-vertex.txt", "shader-fragment.txt"))
		return 0;

	if (!app.LoadWebm("dancer1.webm", true))
		return 0;

	app.Run();
	system("pause");
	return 0;
}