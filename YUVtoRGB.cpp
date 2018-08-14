#include "YUVtoRGB.h"
#include <emmintrin.h>
#include <memory>

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

uint8_t clamp(int16_t value)
{
	return value < 0 ? 0 : (value > 255 ? 255 : value);
}

void yuv420_rgb24_std(uint32_t width, uint32_t height, const uint8_t *Y, const uint8_t *U, const uint8_t *V, const uint8_t *A, uint32_t Y_stride, uint32_t U_stride, uint32_t V_stride, uint32_t A_stride, uint8_t *RGBA, uint32_t RGBA_stride, YCbCrType yuv_type)
{
	const YUV2RGBParam *const param = &(YUV2RGB[yuv_type]);
	uint32_t x, y;
	for (y = 0; y < (height - 1); y += 2)
	{
		const uint8_t *y_ptr1 = Y + y * Y_stride,
			*y_ptr2 = Y + (y + 1) * Y_stride,
			*u_ptr = U + (y / 2) * U_stride,
			*v_ptr = V + (y / 2) * V_stride,
			*a_ptr1 = (A) ? A + y * A_stride : nullptr,
			*a_ptr2 = (A) ? A + (y + 1) * A_stride : nullptr;

		uint8_t *rgb_ptr1 = RGBA + y * RGBA_stride,
			*rgb_ptr2 = RGBA + (y + 1) * RGBA_stride;

		for (x = 0; x < (width - 1); x += 2)
		{
			int8_t u_tmp, v_tmp;
			u_tmp = u_ptr[0] - 128;
			v_tmp = v_ptr[0] - 128;

			//compute Cb Cr color offsets, common to four pixels
			int16_t b_cb_offset, r_cr_offset, g_cbcr_offset;
			b_cb_offset = (param->cb_factor*u_tmp) >> 6;
			r_cr_offset = (param->cr_factor*v_tmp) >> 6;
			g_cbcr_offset = (param->g_cb_factor*u_tmp + param->g_cr_factor*v_tmp) >> 7;

			int16_t y_tmp;
			y_tmp = (param->y_factor*(y_ptr1[0] - param->y_offset)) >> 7;
			rgb_ptr1[0] = clamp(y_tmp + r_cr_offset);
			rgb_ptr1[1] = clamp(y_tmp - g_cbcr_offset);
			rgb_ptr1[2] = clamp(y_tmp + b_cb_offset);
			rgb_ptr1[3] = (a_ptr1) ? *(a_ptr1++) : 255;

			y_tmp = (param->y_factor*(y_ptr1[1] - param->y_offset)) >> 7;
			rgb_ptr1[4] = clamp(y_tmp + r_cr_offset);
			rgb_ptr1[5] = clamp(y_tmp - g_cbcr_offset);
			rgb_ptr1[6] = clamp(y_tmp + b_cb_offset);
			rgb_ptr1[7] = (a_ptr1) ? *(a_ptr1++) : 255;

			y_tmp = (param->y_factor*(y_ptr2[0] - param->y_offset)) >> 7;
			rgb_ptr2[0] = clamp(y_tmp + r_cr_offset);
			rgb_ptr2[1] = clamp(y_tmp - g_cbcr_offset);
			rgb_ptr2[2] = clamp(y_tmp + b_cb_offset);
			rgb_ptr2[3] = (a_ptr2) ? *(a_ptr2++) : 255;

			y_tmp = (param->y_factor*(y_ptr2[1] - param->y_offset)) >> 7;
			rgb_ptr2[4] = clamp(y_tmp + r_cr_offset);
			rgb_ptr2[5] = clamp(y_tmp - g_cbcr_offset);
			rgb_ptr2[6] = clamp(y_tmp + b_cb_offset);
			rgb_ptr2[7] = (a_ptr2) ? *(a_ptr2++) : 255;

			y_ptr1 += 2;
			y_ptr2 += 2;
			u_ptr += 1;
			v_ptr += 1;
			rgb_ptr1 += 8;
			rgb_ptr2 += 8;
		}
	}
}

void yuv420_rgb24_extra(int w, int width, const uint8_t *y_ptr1, const uint8_t *y_ptr2, const uint8_t *u_ptr, const uint8_t *v_ptr, const uint8_t *a_ptr1, const uint8_t *a_ptr2, uint8_t* rgb_ptr1, uint8_t* rgb_ptr2, YCbCrType yuv_type)
{
	const YUV2RGBParam *const param = &(YUV2RGB[yuv_type]);
	for (; w < (width - 1); w += 2)
	{
		int8_t u_tmp, v_tmp;
		u_tmp = u_ptr[0] - 128;
		v_tmp = v_ptr[0] - 128;

		int16_t b_cb_offset, r_cr_offset, g_cbcr_offset;
		b_cb_offset = (param->cb_factor*u_tmp) >> 6;
		r_cr_offset = (param->cr_factor*v_tmp) >> 6;
		g_cbcr_offset = (param->g_cb_factor*u_tmp + param->g_cr_factor*v_tmp) >> 7;

		int16_t y_tmp;
		y_tmp = (param->y_factor*(y_ptr1[0] - param->y_offset)) >> 7;
		rgb_ptr1[0] = clamp(y_tmp + r_cr_offset);
		rgb_ptr1[1] = clamp(y_tmp - g_cbcr_offset);
		rgb_ptr1[2] = clamp(y_tmp + b_cb_offset);
		rgb_ptr1[3] = (a_ptr1) ? *(a_ptr1++) : 255;

		y_tmp = (param->y_factor*(y_ptr1[1] - param->y_offset)) >> 7;
		rgb_ptr1[4] = clamp(y_tmp + r_cr_offset);
		rgb_ptr1[5] = clamp(y_tmp - g_cbcr_offset);
		rgb_ptr1[6] = clamp(y_tmp + b_cb_offset);
		rgb_ptr1[7] = (a_ptr1) ? *(a_ptr1++) : 255;

		y_tmp = (param->y_factor*(y_ptr2[0] - param->y_offset)) >> 7;
		rgb_ptr2[0] = clamp(y_tmp + r_cr_offset);
		rgb_ptr2[1] = clamp(y_tmp - g_cbcr_offset);
		rgb_ptr2[2] = clamp(y_tmp + b_cb_offset);
		rgb_ptr2[3] = (a_ptr2) ? *(a_ptr2++) : 255;

		y_tmp = (param->y_factor*(y_ptr2[1] - param->y_offset)) >> 7;
		rgb_ptr2[4] = clamp(y_tmp + r_cr_offset);
		rgb_ptr2[5] = clamp(y_tmp - g_cbcr_offset);
		rgb_ptr2[6] = clamp(y_tmp + b_cb_offset);
		rgb_ptr2[7] = (a_ptr2) ? *(a_ptr2++) : 255;

		y_ptr1 += 2;
		y_ptr2 += 2;
		u_ptr += 1;
		v_ptr += 1;
		rgb_ptr1 += 8;
		rgb_ptr2 += 8;
	}
}

#define LOAD_SI128 _mm_load_si128
#define SAVE_SI128 _mm_stream_si128

#define UV2RGB_16(U, V, R1, G1, B1, R2, G2, B2) \
	r_tmp = _mm_srai_epi16(_mm_mullo_epi16(V, _mm_set1_epi16(param->cr_factor)), 6); \
	g_tmp = _mm_srai_epi16(_mm_add_epi16( \
		_mm_mullo_epi16(U, _mm_set1_epi16(param->g_cb_factor)), \
		_mm_mullo_epi16(V, _mm_set1_epi16(param->g_cr_factor))), 7); \
	b_tmp = _mm_srai_epi16(_mm_mullo_epi16(U, _mm_set1_epi16(param->cb_factor)), 6); \
	R1 = _mm_unpacklo_epi16(r_tmp, r_tmp); \
	G1 = _mm_unpacklo_epi16(g_tmp, g_tmp); \
	B1 = _mm_unpacklo_epi16(b_tmp, b_tmp); \
	R2 = _mm_unpackhi_epi16(r_tmp, r_tmp); \
	G2 = _mm_unpackhi_epi16(g_tmp, g_tmp); \
	B2 = _mm_unpackhi_epi16(b_tmp, b_tmp); \

#define ADD_Y2RGB_16(Y1, Y2, R1, G1, B1, R2, G2, B2) \
	Y1 = _mm_srai_epi16(_mm_mullo_epi16(Y1, _mm_set1_epi16(param->y_factor)), 7); \
	Y2 = _mm_srai_epi16(_mm_mullo_epi16(Y2, _mm_set1_epi16(param->y_factor)), 7); \
	\
	R1 = _mm_add_epi16(Y1, R1); \
	G1 = _mm_sub_epi16(Y1, G1); \
	B1 = _mm_add_epi16(Y1, B1); \
	R2 = _mm_add_epi16(Y2, R2); \
	G2 = _mm_sub_epi16(Y2, G2); \
	B2 = _mm_add_epi16(Y2, B2); \

#define PACK_RGB24_32_STEP(RS1, RS2, RS3, RS4, RS5, RS6, RD1, RD2, RD3, RD4, RD5, RD6) \
RD1 = _mm_packus_epi16(_mm_and_si128(RS1,_mm_set1_epi16(0xFF)), _mm_and_si128(RS2,_mm_set1_epi16(0xFF))); \
RD2 = _mm_packus_epi16(_mm_and_si128(RS3,_mm_set1_epi16(0xFF)), _mm_and_si128(RS4,_mm_set1_epi16(0xFF))); \
RD3 = _mm_packus_epi16(_mm_and_si128(RS5,_mm_set1_epi16(0xFF)), _mm_and_si128(RS6,_mm_set1_epi16(0xFF))); \
RD4 = _mm_packus_epi16(_mm_srli_epi16(RS1,8), _mm_srli_epi16(RS2,8)); \
RD5 = _mm_packus_epi16(_mm_srli_epi16(RS3,8), _mm_srli_epi16(RS4,8)); \
RD6 = _mm_packus_epi16(_mm_srli_epi16(RS5,8), _mm_srli_epi16(RS6,8)); \

#define PACK_RGB24_32(R1, R2, G1, G2, B1, B2, RGB1, RGB2, RGB3, RGB4, RGB5, RGB6) \
PACK_RGB24_32_STEP(R1, R2, G1, G2, B1, B2, RGB1, RGB2, RGB3, RGB4, RGB5, RGB6) \
PACK_RGB24_32_STEP(RGB1, RGB2, RGB3, RGB4, RGB5, RGB6, R1, R2, G1, G2, B1, B2) \
PACK_RGB24_32_STEP(R1, R2, G1, G2, B1, B2, RGB1, RGB2, RGB3, RGB4, RGB5, RGB6) \
PACK_RGB24_32_STEP(RGB1, RGB2, RGB3, RGB4, RGB5, RGB6, R1, R2, G1, G2, B1, B2) \
PACK_RGB24_32_STEP(R1, R2, G1, G2, B1, B2, RGB1, RGB2, RGB3, RGB4, RGB5, RGB6) \

void yuv420_rgb24_sse(uint32_t width, uint32_t height, const uint8_t *Y, const uint8_t *U, const uint8_t *V, const uint8_t *A, uint32_t Y_stride, uint32_t U_stride, uint32_t V_stride, uint32_t A_stride, uint8_t *RGBA, uint32_t RGBA_stride, YCbCrType yuv_type)
{
	const YUV2RGBParam *const param = &(YUV2RGB[yuv_type]);
	for (uint32_t h = 0; h < (height - 1); h += 2)
	{
		const uint8_t *y_ptr1 = Y + h * Y_stride;
		const uint8_t *y_ptr2 = Y + (h + 1) * Y_stride;
		const uint8_t *u_ptr = U + (h / 2) * U_stride;
		const uint8_t *v_ptr = V + (h / 2) * V_stride;
		const uint8_t *a_ptr1 = (A) ? A + h * A_stride : nullptr;
		const uint8_t *a_ptr2 = (A) ? A + (h + 1) * A_stride : nullptr;

		uint8_t *rgba_ptr1 = RGBA + (h * RGBA_stride);
		uint8_t *rgba_ptr2 = RGBA + ((h + 1) * RGBA_stride);

		uint32_t w = 0;
		for (; w < (width - 31); w += 32)
		{
			__m128i u = LOAD_SI128((const __m128i*)(u_ptr));
			__m128i v = LOAD_SI128((const __m128i*)(v_ptr));

			__m128i r_tmp, g_tmp, b_tmp;
			__m128i r_16_1, g_16_1, b_16_1, r_16_2, g_16_2, b_16_2;
			__m128i r_uv_16_1, g_uv_16_1, b_uv_16_1, r_uv_16_2, g_uv_16_2, b_uv_16_2;
			__m128i y_16_1, y_16_2;

			u = _mm_add_epi8(u, _mm_set1_epi8(-128));
			v = _mm_add_epi8(v, _mm_set1_epi8(-128));

			/* process first 16 pixels of first line */
			__m128i u_16 = _mm_srai_epi16(_mm_unpacklo_epi8(u, u), 8);
			__m128i v_16 = _mm_srai_epi16(_mm_unpacklo_epi8(v, v), 8);

			UV2RGB_16(u_16, v_16, r_uv_16_1, g_uv_16_1, b_uv_16_1, r_uv_16_2, g_uv_16_2, b_uv_16_2)
				r_16_1 = r_uv_16_1; g_16_1 = g_uv_16_1; b_16_1 = b_uv_16_1;
			r_16_2 = r_uv_16_2; g_16_2 = g_uv_16_2; b_16_2 = b_uv_16_2;

			__m128i y = LOAD_SI128((const __m128i*)(y_ptr1));
			y = _mm_sub_epi8(y, _mm_set1_epi8(param->y_offset));
			y_16_1 = _mm_unpacklo_epi8(y, _mm_setzero_si128());
			y_16_2 = _mm_unpackhi_epi8(y, _mm_setzero_si128());

			ADD_Y2RGB_16(y_16_1, y_16_2, r_16_1, g_16_1, b_16_1, r_16_2, g_16_2, b_16_2);

			__m128i r_8_11 = _mm_packus_epi16(r_16_1, r_16_2);
			__m128i g_8_11 = _mm_packus_epi16(g_16_1, g_16_2);
			__m128i b_8_11 = _mm_packus_epi16(b_16_1, b_16_2);

			/* process first 16 pixels of second line */
			r_16_1 = r_uv_16_1; g_16_1 = g_uv_16_1; b_16_1 = b_uv_16_1;
			r_16_2 = r_uv_16_2; g_16_2 = g_uv_16_2; b_16_2 = b_uv_16_2;

			y = LOAD_SI128((const __m128i*)(y_ptr2));
			y = _mm_sub_epi8(y, _mm_set1_epi8(param->y_offset));
			y_16_1 = _mm_unpacklo_epi8(y, _mm_setzero_si128());
			y_16_2 = _mm_unpackhi_epi8(y, _mm_setzero_si128());

			ADD_Y2RGB_16(y_16_1, y_16_2, r_16_1, g_16_1, b_16_1, r_16_2, g_16_2, b_16_2);

			__m128i r_8_21 = _mm_packus_epi16(r_16_1, r_16_2);
			__m128i g_8_21 = _mm_packus_epi16(g_16_1, g_16_2);
			__m128i b_8_21 = _mm_packus_epi16(b_16_1, b_16_2);

			/* process last 16 pixels of first line */
			u_16 = _mm_srai_epi16(_mm_unpackhi_epi8(u, u), 8);
			v_16 = _mm_srai_epi16(_mm_unpackhi_epi8(v, v), 8);

			UV2RGB_16(u_16, v_16, r_uv_16_1, g_uv_16_1, b_uv_16_1, r_uv_16_2, g_uv_16_2, b_uv_16_2);
			r_16_1 = r_uv_16_1; g_16_1 = g_uv_16_1; b_16_1 = b_uv_16_1;
			r_16_2 = r_uv_16_2; g_16_2 = g_uv_16_2; b_16_2 = b_uv_16_2;

			y = LOAD_SI128((const __m128i*)(y_ptr1 + 16));
			y = _mm_sub_epi8(y, _mm_set1_epi8(param->y_offset));
			y_16_1 = _mm_unpacklo_epi8(y, _mm_setzero_si128());
			y_16_2 = _mm_unpackhi_epi8(y, _mm_setzero_si128());

			ADD_Y2RGB_16(y_16_1, y_16_2, r_16_1, g_16_1, b_16_1, r_16_2, g_16_2, b_16_2);

			__m128i r_8_12 = _mm_packus_epi16(r_16_1, r_16_2);
			__m128i g_8_12 = _mm_packus_epi16(g_16_1, g_16_2);
			__m128i b_8_12 = _mm_packus_epi16(b_16_1, b_16_2);

			/* process last 16 pixels of second line */
			r_16_1 = r_uv_16_1; g_16_1 = g_uv_16_1; b_16_1 = b_uv_16_1;
			r_16_2 = r_uv_16_2; g_16_2 = g_uv_16_2; b_16_2 = b_uv_16_2;

			y = LOAD_SI128((const __m128i*)(y_ptr2 + 16));
			y = _mm_sub_epi8(y, _mm_set1_epi8(param->y_offset));
			y_16_1 = _mm_unpacklo_epi8(y, _mm_setzero_si128());
			y_16_2 = _mm_unpackhi_epi8(y, _mm_setzero_si128());

			ADD_Y2RGB_16(y_16_1, y_16_2, r_16_1, g_16_1, b_16_1, r_16_2, g_16_2, b_16_2);

			__m128i r_8_22 = _mm_packus_epi16(r_16_1, r_16_2);
			__m128i g_8_22 = _mm_packus_epi16(g_16_1, g_16_2);
			__m128i b_8_22 = _mm_packus_epi16(b_16_1, b_16_2);

			__m128i rgb[6];

			PACK_RGB24_32(r_8_11, r_8_12, g_8_11, g_8_12, b_8_11, b_8_12, rgb[0], rgb[1], rgb[2], rgb[3], rgb[4], rgb[5]);

			//SAVE_SI128((__m128i*)(rgba_ptr1), rgb_1);
			//SAVE_SI128((__m128i*)(rgba_ptr1 + 16), rgb_2);
			//SAVE_SI128((__m128i*)(rgba_ptr1 + 32), rgb_3);
			//SAVE_SI128((__m128i*)(rgba_ptr1 + 48), rgb_4);
			//SAVE_SI128((__m128i*)(rgba_ptr1 + 64), rgb_5);
			//SAVE_SI128((__m128i*)(rgba_ptr1 + 80), rgb_6);

			uint8_t *rgb_src = rgb[0].m128i_u8;
			for (int i = 0; i < 32; ++i)
			{
				memcpy(rgba_ptr1, rgb_src, sizeof(uint8_t) * 3);
				rgba_ptr1 += 3; rgb_src += 3;
				*(rgba_ptr1++) = (a_ptr1) ? *(a_ptr1++) : 255;
			}

			PACK_RGB24_32(r_8_21, r_8_22, g_8_21, g_8_22, b_8_21, b_8_22, rgb[0], rgb[1], rgb[2], rgb[3], rgb[4], rgb[5]);

			//SAVE_SI128((__m128i*)(rgba_ptr2), rgb_1);
			//SAVE_SI128((__m128i*)(rgba_ptr2 + 16), rgb_2);
			//SAVE_SI128((__m128i*)(rgba_ptr2 + 32), rgb_3);
			//SAVE_SI128((__m128i*)(rgba_ptr2 + 48), rgb_4);
			//SAVE_SI128((__m128i*)(rgba_ptr2 + 64), rgb_5);
			//SAVE_SI128((__m128i*)(rgba_ptr2 + 80), rgb_6);

			rgb_src = rgb[0].m128i_u8;
			for (int i = 0; i < 32; ++i)
			{
				memcpy(rgba_ptr2, rgb_src, sizeof(uint8_t) * 3);
				rgba_ptr2 += 3; rgb_src += 3;
				*(rgba_ptr2++) = (a_ptr2) ? *(a_ptr2++) : 255;
			}

			y_ptr1 += 32;
			y_ptr2 += 32;
			u_ptr += 16;
			v_ptr += 16;
		}

		// width의 남은 픽셀은 그냥 계산
		yuv420_rgb24_extra(w, width, y_ptr1, y_ptr2, u_ptr, v_ptr, a_ptr1, a_ptr2, rgba_ptr1, rgba_ptr2, yuv_type);
	}
}