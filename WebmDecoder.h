#pragma once

#include <string>
#include <vector>
#include <vp8.h>
#include <vp8dx.h>
#include <vpx_decoder.h>
#include <mkvparser.h>
#include <mkvreader.h>
#include <mkvmuxer.h>
#include "YUVtoRGB.h"

#define SAFE_DELETE(p)       { if(p) { delete (p);     (p)=NULL; } }
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p);   (p)=NULL; } }

const uint32_t VP8_FOURCC = 0x30385056;
const uint32_t VP9_FOURCC = 0x30395056;

class WebmDecoder
{
public:
	enum WEBM_STATE
	{
		NONE,
		LOAD_ERROR,
		PLAYING,
		END,
	};

private:
	struct webm_context
	{
		FILE *file;
		vpx_codec_ctx_t decoder;
		vpx_codec_ctx_t decoder_alpha;
		vpx_codec_iter_t iter;
		vpx_codec_iter_t iter_alpha;
		vpx_image_t *img;
		vpx_image_t *img_alpha;
		mkvparser::MkvReader *reader;
		mkvparser::Segment *segment;
		const mkvparser::Cluster *cluster;
		const mkvparser::Block *block;
		const mkvparser::BlockEntry *block_entry;
		uint8_t *pixels;
		std::vector<uint8_t> buffer;
		std::vector<uint8_t> buffer_alpha;
		uint32_t buffer_size;
		uint32_t buffer_alpha_size;
		WEBM_STATE state;
		int block_frame_index;
		int video_track_index;
		uint32_t fourcc;
		uint32_t video_width;
		uint32_t video_height;
		int framerate_numerator;
		int framerate_denominator;
		float frame_rate;
		uint64_t timestamp_ms;
		uint64_t begin_timestamp_ms;
		bool is_key_frame;
		bool is_play_loop;

		webm_context()
		{
			file = nullptr;
			decoder.iface = nullptr;
			decoder_alpha.iface = nullptr;
			img = nullptr;
			img_alpha = nullptr;
			iter = nullptr;
			iter_alpha = nullptr;
			reader = nullptr;
			segment = nullptr;
			cluster = nullptr;
			block = nullptr;
			block_entry = nullptr;
			pixels = nullptr;
			buffer.resize(1024 * 256);
			buffer_alpha.resize(1024 * 256);
			buffer_size = 0;
			buffer_alpha_size = 0;
			state = WEBM_STATE::NONE;
			block_frame_index = 0;
			video_track_index = 0;
			fourcc = 0;
			video_width = 0;
			video_height = 0;
			framerate_numerator = 0;
			framerate_denominator = 0;
			//frame_rate = 1.0f;
			timestamp_ms = 0;
			begin_timestamp_ms = 0;
			is_key_frame = false;
			is_play_loop = false;
		}

		void Reset()
		{
			if (file)
			{
				fclose(file);
				file = nullptr;
			}
			if (decoder.iface)
				vpx_codec_destroy(&decoder);
			if (decoder_alpha.iface)
				vpx_codec_destroy(&decoder_alpha);
			img = nullptr;
			img_alpha = nullptr;
			iter = nullptr;
			iter_alpha = nullptr;
			SAFE_DELETE(reader);
			SAFE_DELETE(segment);
			cluster = nullptr;
			block = nullptr;
			block_entry = nullptr;
			SAFE_DELETE_ARRAY(pixels);
			buffer_size = 0;
			buffer_alpha_size = 0;
			state = WEBM_STATE::NONE;
			block_frame_index = 0;
			video_track_index = 0;
			fourcc = 0;
			video_width = 0;
			video_height = 0;
			framerate_numerator = 0;
			framerate_denominator = 0;
			//frame_rate = 1.0f; LoadVPX 에서 초기화됨
			timestamp_ms = 0;
			begin_timestamp_ms = 0;
			is_key_frame = false;
			is_play_loop = false;
		}
	};

public:
	WebmDecoder();
	~WebmDecoder();

public:
	bool Load(const std::string &fileName, bool loop, float frameRate = 1.0f);
	bool IsInitialized();
	WEBM_STATE Update();
	void Stop();
	void Restart();
	std::tuple<int, int, uint8_t*> GetRGBA();

private:
	void _PrintError(vpx_codec_ctx_t *ctx, const char *error);
	bool _IsWebM();
	WEBM_STATE _ReadFrame();
	void _ConvertToRGBA();
	uint64_t _GetTime();

private:
	webm_context mCTX;
	long mAccumTime;
	bool mUsingSSE;
	bool mUsingAVX;

	using YUVtoRGBAFunc_t = void(*)(uint32_t, uint32_t,
		const uint8_t*, const uint8_t*, const uint8_t*, const uint8_t*,
		uint32_t, uint32_t, uint32_t, uint32_t,
		uint8_t*, uint32_t, YCbCrType);
	YUVtoRGBAFunc_t YUVtoRGBAFunc;
};