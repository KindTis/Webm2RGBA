#pragma once
#include <cstdint>

enum YCbCrType
{
	YCBCR_JPEG,
	YCBCR_601,
	YCBCR_709
};

struct YUV2RGBParam
{
	uint8_t cb_factor;   // [(255*CbNorm)/CbRange]
	uint8_t cr_factor;   // [(255*CrNorm)/CrRange]
	uint8_t g_cb_factor; // [Bf/Gf*(255*CbNorm)/CbRange]
	uint8_t g_cr_factor; // [Rf/Gf*(255*CrNorm)/CrRange]
	uint8_t y_factor;    // [(YMax-YMin)/255]
	uint8_t y_offset;    // YMin
};

#define FIXED_POINT_VALUE(value, precision) ((uint8_t)(((value)*(1<<precision))+0.5))

#define YUV2RGB_PARAM(Rf, Bf, YMin, YMax, CbCrRange) { \
	FIXED_POINT_VALUE(255.0 * (2.0 * (1 - Bf)) / CbCrRange, 6), \
	FIXED_POINT_VALUE(255.0 * (2.0 * (1 - Rf)) / CbCrRange, 6), \
	FIXED_POINT_VALUE(Bf / (1.0 - Bf - Rf) * 255.0 * (2.0 * (1 - Bf)) / CbCrRange, 7), \
	FIXED_POINT_VALUE(Rf / (1.0 - Bf - Rf) * 255.0 * (2.0 * (1 - Rf)) / CbCrRange, 7), \
	FIXED_POINT_VALUE(255.0 / (YMax - YMin), 7), \
	(uint8_t)YMin\
}

static const YUV2RGBParam YUV2RGB[3] = {
	// ITU-T T.871 (JPEG)
	YUV2RGB_PARAM(0.299, 0.114, 0.0, 255.0, 255.0),
	// ITU-R BT.601-7
	YUV2RGB_PARAM(0.299, 0.114, 16.0, 235.0, 224.0),
	// ITU-R BT.709-6
	YUV2RGB_PARAM(0.2126, 0.0722, 16.0, 235.0, 224.0)
};

void yuv420_rgb24_avx(uint32_t width, uint32_t height, const uint8_t* Y, const uint8_t* U, const uint8_t* V, const uint8_t* A, uint32_t Y_stride, uint32_t U_stride, uint32_t V_stride, uint32_t A_stride, uint8_t* RGBA, uint32_t RGBA_stride, YCbCrType yuv_type);
void yuv420_rgb24_sse(uint32_t width, uint32_t height, const uint8_t* Y, const uint8_t* U, const uint8_t* V, const uint8_t* A, uint32_t Y_stride, uint32_t U_stride, uint32_t V_stride, uint32_t A_stride, uint8_t* RGBA, uint32_t RGBA_stride, YCbCrType yuv_type);
void yuv420_rgb24_std(uint32_t width, uint32_t height, const uint8_t* Y, const uint8_t* U, const uint8_t* V, const uint8_t* A, uint32_t Y_stride, uint32_t U_stride, uint32_t V_stride, uint32_t A_stride, uint8_t* RGBA, uint32_t RGBA_stride, YCbCrType yuv_type);
void yuv420_rgb24_extra(int w, int width, const uint8_t* y_ptr1, const uint8_t* y_ptr2, const uint8_t* u_ptr, const uint8_t* v_ptr, const uint8_t* a_ptr1, const uint8_t* a_ptr2, uint8_t* rgb_ptr1, uint8_t* rgb_ptr2, YCbCrType yuv_type);