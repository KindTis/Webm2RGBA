#pragma once
#include <stdint.h>

enum YCbCrType
{
	YCBCR_JPEG,
	YCBCR_601,
	YCBCR_709
};

void yuv420_rgb24_sse(uint32_t width, uint32_t height, const uint8_t *Y, const uint8_t *U, const uint8_t *V, const uint8_t *A, uint32_t Y_stride, uint32_t U_stride, uint32_t V_stride, uint32_t A_stride, uint8_t *RGBA, uint32_t RGBA_stride, YCbCrType yuv_type);

void yuv420_rgb24_std(uint32_t width, uint32_t height, const uint8_t *Y, const uint8_t *U, const uint8_t *V, const uint8_t *A, uint32_t Y_stride, uint32_t U_stride, uint32_t V_stride, uint32_t A_stride, uint8_t *RGBA, uint32_t RGBA_stride, YCbCrType yuv_type);