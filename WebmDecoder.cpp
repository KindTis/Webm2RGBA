#include "WebmDecoder.h"

#include <assert.h>
#include <chrono>
#include <intrin.h>
#include <windows.h>
#include <stdio.h>
#include <chrono>
#include <stdarg.h>
#include <iostream>

#ifdef _DEBUG
#pragma comment(lib, "./lib/libvpxd.lib")
#pragma comment(lib, "./lib/libwebmd.lib")
#else
#pragma comment(lib, "./lib/libvpx.lib")
#pragma comment(lib, "./lib/libwebm.lib")
#endif

void OutputDebugTrace(char* lpszFormat, ...)
{
	va_list args;
	va_start(args, lpszFormat);

	int buf;
	char szBuffer[512];

	buf = _vsnprintf_s(szBuffer, sizeof(szBuffer) / sizeof(szBuffer[0]), lpszFormat, args);

	assert(buf >= 0);
	OutputDebugStringA(szBuffer);
	va_end(args);
}

WebmDecoder::WebmDecoder() : mAccumTime(0), mUsingSSE(false), mUsingAVX(false)
{
	int cpuInfo[4];
	__cpuid(cpuInfo, 1);
	mUsingSSE = ((cpuInfo[3] >> 26) & 1) != 0;

	__cpuid(cpuInfo, 7);
	mUsingAVX = (cpuInfo[1] & (1 << 5)) != 0;
}

WebmDecoder::~WebmDecoder()
{
	mCTX.Reset();
}

bool WebmDecoder::Load(const std::string &fileName, bool loop, float frameRate /*= 1.0f*/)
{
	mCTX.Reset();

	OutputDebugTrace("%s - load to %s.\n", __FUNCTION__, fileName.c_str());
	const errno_t error = fopen_s(&mCTX.file, fileName.c_str(), "rb");
	if (error)
	{
		OutputDebugTrace("%s - failed to open %s.\n", __FUNCTION__, fileName.c_str());
		return false;
	}

	if (!_IsWebM())
	{
		mCTX.Reset();
		return false;
	}

	if (vpx_codec_dec_init(&mCTX.decoder, vpx_codec_vp8_dx(), nullptr, 0))
	{
		mCTX.Reset();
		_PrintError(&mCTX.decoder, "failed to initialize decoder");
		return false;
	}

	// 알파용 디코더
	if (vpx_codec_dec_init(&mCTX.decoder_alpha, vpx_codec_vp8_dx(), nullptr, 0))
	{
		mCTX.Reset();
		_PrintError(&mCTX.decoder_alpha, "failed to initialize decoder");
		return false;
	}

	mCTX.frame_rate = frameRate;
	mCTX.is_play_loop = loop;
	mCTX.begin_timestamp_ms = _GetTime();

	YUVtoRGBAFunc = [this]() -> YUVtoRGBAFunc_t {
		if (mUsingAVX)
		{
			std::cout << "Decode Function: AVX2" << std::endl;
			return yuv420_rgb24_avx;
		}
		if (mUsingSSE)
		{
			std::cout << "Decode Function: SSE2" << std::endl;
			return yuv420_rgb24_sse;
		}
		std::cout << "Decode Function: Standard" << std::endl;
		return yuv420_rgb24_std;
		}();

	return true;
}

bool WebmDecoder::IsInitialized()
{
	return (mCTX.file) ? true : false;
}

WebmDecoder::WEBM_STATE WebmDecoder::Update()
{
	if (!mCTX.file)
		return WEBM_STATE::NONE;

	uint64_t systemTime = _GetTime() - mCTX.begin_timestamp_ms;
	if (mCTX.timestamp_ms > systemTime)
		return WEBM_STATE::PLAYING;

	mCTX.state = _ReadFrame();

	mCTX.img = nullptr;
	mCTX.img_alpha = nullptr;
	mCTX.iter = nullptr;
	mCTX.iter_alpha = nullptr;

	if (mCTX.state == WEBM_STATE::PLAYING)
	{
		if (vpx_codec_decode(&mCTX.decoder, &mCTX.buffer[0], mCTX.buffer_size, nullptr, 0))
		{
			_PrintError(&mCTX.decoder, "failed to decode frame");
			mCTX.state = WEBM_STATE::LOAD_ERROR;
			return mCTX.state;
		}
		mCTX.img = vpx_codec_get_frame(&mCTX.decoder, &mCTX.iter);

		if (mCTX.buffer_alpha_size > 0)
		{
			if (vpx_codec_decode(&mCTX.decoder_alpha, &mCTX.buffer_alpha[0], mCTX.buffer_alpha_size, nullptr, 0))
			{
				_PrintError(&mCTX.decoder_alpha, "failed to decode frame");
				mCTX.state = WEBM_STATE::LOAD_ERROR;
				return mCTX.state;
			}
			mCTX.img_alpha = vpx_codec_get_frame(&mCTX.decoder_alpha, &mCTX.iter_alpha);
		}
		_ConvertToRGBA();
	}
	return mCTX.state;
}

void WebmDecoder::Stop()
{
	mCTX.cluster = nullptr;
	mCTX.timestamp_ms = 0;
}

void WebmDecoder::Restart()
{
	if (!mCTX.file)
		return;

	mCTX.block_entry = nullptr;
	mCTX.cluster = mCTX.segment->GetFirst();
	mCTX.begin_timestamp_ms = _GetTime();
	mCTX.timestamp_ms = 0;
}


std::tuple<int, int, uint8_t*> WebmDecoder::GetRGBA()
{
	return std::make_tuple(
		(mCTX.img) ? mCTX.img->d_w : 0,
		(mCTX.img) ? mCTX.img->d_h : 0,
		mCTX.pixels);
}

void WebmDecoder::_PrintError(vpx_codec_ctx_t *ctx, const char *error)
{
	const char *detail = vpx_codec_error_detail(ctx);
	OutputDebugTrace("%s: %s\n", error, vpx_codec_error(ctx));
	if (detail)
		OutputDebugTrace("    %s\n", detail);
}

bool WebmDecoder::_IsWebM()
{
	mCTX.reader = new mkvparser::MkvReader(mCTX.file);
	mkvparser::EBMLHeader ebmlHeader;
	long long pos = 0;
	long long ret = ebmlHeader.Parse(mCTX.reader, pos);
	if (ret < 0) {
		OutputDebugTrace("%s - EBMLHeader::Parse() failed.\n", __FUNCTION__);
		return false;
	}

	// webm 형식만 지원하자
	if (_stricmp(ebmlHeader.m_docType, "webm"))
	{
		OutputDebugTrace("%s - this is not an WEBM file.\n", __FUNCTION__);
		return false;
	}

	// segment 불러오기
	mkvparser::Segment *segment;
	if (mkvparser::Segment::CreateInstance(mCTX.reader, pos, segment))
	{
		OutputDebugTrace("%s - failed to create segment instance.\n", __FUNCTION__);
		return false;
	}
	mCTX.segment = segment;
	if (mCTX.segment->Load() < 0)
	{
		OutputDebugTrace("%s - failed to load segment instance.\n", __FUNCTION__);
		return false;
	}

	// VideoTrack 확인
	const mkvparser::Tracks *const tracks = segment->GetTracks();
	const mkvparser::VideoTrack *video_track = nullptr;
	for (unsigned long i = 0; i < tracks->GetTracksCount(); ++i)
	{
		const mkvparser::Track *const track = tracks->GetTrackByIndex(i);
		if (track->GetType() == mkvparser::Track::kVideo)
		{
			video_track = static_cast<const mkvparser::VideoTrack*>(track);
			mCTX.video_track_index = static_cast<int>(track->GetNumber());
			break;
		}
	}

	if (video_track == nullptr || video_track->GetCodecId() == nullptr)
	{
		OutputDebugTrace("%s - unable to find video codec.\n", __FUNCTION__);
		return false;
	}

	// codec 확인
	if (!strncmp(video_track->GetCodecId(), "V_VP8", 5))
	{
		mCTX.fourcc = VP8_FOURCC;
	}
	else if (!strncmp(video_track->GetCodecId(), "V_VP9", 5))
	{
		mCTX.fourcc = VP9_FOURCC;
	}
	else
	{
		OutputDebugTrace("%s - it is not vp8 or vp9 codec.\n", __FUNCTION__);
		return false;
	}

	mCTX.framerate_numerator = 0;
	mCTX.framerate_denominator = 0;
	mCTX.video_width = static_cast<uint32_t>(video_track->GetWidth());
	mCTX.video_height = static_cast<uint32_t>(video_track->GetHeight());
	mCTX.cluster = mCTX.segment->GetFirst();

	return true;
}

WebmDecoder::WEBM_STATE WebmDecoder::_ReadFrame()
{
	if (!mCTX.cluster)
		return WEBM_STATE::NONE;

	bool block_entry_eos = false;
	do
	{
		long status = 0;
		bool get_new_block = false;
		if (mCTX.block_entry == nullptr && !block_entry_eos)
		{
			status = mCTX.cluster->GetFirst(mCTX.block_entry);
			get_new_block = true;
		}
		else if (block_entry_eos || mCTX.block_entry->EOS())
		{
			mCTX.cluster = mCTX.segment->GetNext(mCTX.cluster);
			if (mCTX.cluster == nullptr || mCTX.cluster->EOS())
			{
				if (!mCTX.is_play_loop)
				{
					mCTX.cluster = nullptr;
					return WEBM_STATE::END;
				}
				mCTX.cluster = mCTX.segment->GetFirst();
				mCTX.begin_timestamp_ms = _GetTime();
			}
			status = mCTX.cluster->GetFirst(mCTX.block_entry);
			block_entry_eos = false;
			get_new_block = true;
		}
		else if (mCTX.block == nullptr || mCTX.block_frame_index == mCTX.block->GetFrameCount() ||
			mCTX.block->GetTrackNumber() != mCTX.video_track_index)
		{
			status = mCTX.cluster->GetNext(mCTX.block_entry, mCTX.block_entry);
			if (mCTX.block_entry == nullptr || mCTX.block_entry->EOS())
			{
				block_entry_eos = true;
				continue;
			}
			get_new_block = true;
		}

		if (status || mCTX.block_entry == nullptr)
		{
			return WEBM_STATE::LOAD_ERROR;
		}

		if (get_new_block)
		{
			mCTX.block = mCTX.block_entry->GetBlock();
			int frameCount = mCTX.block->GetFrameCount();
			if (mCTX.block == nullptr)
				return WEBM_STATE::LOAD_ERROR;
			mCTX.block_frame_index = 0;
		}
	} while (block_entry_eos || mCTX.block->GetTrackNumber() != mCTX.video_track_index);

	const mkvparser::Block::Frame &frame = mCTX.block->GetFrame(mCTX.block_frame_index++);
	if (frame.len > static_cast<long>(mCTX.buffer.size()))
	{
		mCTX.buffer.resize(frame.len * 2);
	}
	mCTX.buffer_size = frame.len;
	mCTX.is_key_frame = mCTX.block->IsKey();
	std::chrono::nanoseconds timestamp_ns(mCTX.block->GetTime(mCTX.cluster));
	mCTX.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp_ns).count();
	mCTX.timestamp_ms = (long long)(mCTX.timestamp_ms / mCTX.frame_rate);

	long ret = frame.Read(mCTX.reader, &mCTX.buffer[0]);
	if (ret)
	{
		OutputDebugTrace("%s - failed to read frame\n", __FUNCTION__);
		return WEBM_STATE::LOAD_ERROR;
	}

	if (mCTX.block->GetFrameAdditionCount() > 0)
	{
		const mkvparser::Block::Frame &frame_addition = mCTX.block->GetFrameAddition(0);
		if (frame_addition.len > static_cast<long>(mCTX.buffer_alpha.size()))
		{
			mCTX.buffer_alpha.resize(frame_addition.len * 2);
		}
		mCTX.buffer_alpha_size = frame_addition.len;
		ret = frame_addition.Read(mCTX.reader, &mCTX.buffer_alpha[0]);
		if (ret)
		{
			OutputDebugTrace("%s - failed to read frame\n", __FUNCTION__);
			return WEBM_STATE::LOAD_ERROR;
		}
	}
	return WEBM_STATE::PLAYING;
}

void WebmDecoder::_ConvertToRGBA()
{
	if (!mCTX.img)
		return;

	const unsigned int width = mCTX.img->d_w;
	const unsigned int height = mCTX.img->d_h;

	int pixels = 0;
	if (!mCTX.pixels)
	{
		mCTX.pixels = new unsigned char[width * height * 4];
	}

	const unsigned char *y = mCTX.img->planes[VPX_PLANE_Y];
	const unsigned char *u = mCTX.img->planes[VPX_PLANE_U];
	const unsigned char *v = mCTX.img->planes[VPX_PLANE_V];
	const unsigned char *a = (mCTX.img_alpha) ? mCTX.img_alpha->planes[VPX_PLANE_Y] : nullptr;

	const int strideY = mCTX.img->stride[VPX_PLANE_Y];
	const int strideU = mCTX.img->stride[VPX_PLANE_U];
	const int strideV = mCTX.img->stride[VPX_PLANE_V];
	const int strideA = (mCTX.img_alpha) ? mCTX.img_alpha->stride[VPX_PLANE_Y] : 0;

	YUVtoRGBAFunc(width, height, y, u, v, a, strideY, strideU, strideV, strideA, mCTX.pixels, width * 4, YCBCR_JPEG);
}

uint64_t WebmDecoder::_GetTime()
{
	std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
	return now.count();
}
