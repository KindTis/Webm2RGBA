#include "YUVtoRGB.h"
#include <immintrin.h>
#include <vector>
#include <memory>

#define LOAD_SI256 _mm256_load_si256
#define SAVE_SI256 _mm256_stream_si256

#define UV2RGB_32_AVX(U, V, R1, G1, B1, R2, G2, B2) \
    r_tmp = _mm256_srai_epi16(_mm256_mullo_epi16(V, _mm256_set1_epi16(param->cr_factor)), 6); \
    g_tmp = _mm256_srai_epi16(_mm256_add_epi16( \
        _mm256_mullo_epi16(U, _mm256_set1_epi16(param->g_cb_factor)), \
        _mm256_mullo_epi16(V, _mm256_set1_epi16(param->g_cr_factor))), 7); \
    b_tmp = _mm256_srai_epi16(_mm256_mullo_epi16(U, _mm256_set1_epi16(param->cb_factor)), 6); \
    R1 = _mm256_unpacklo_epi16(r_tmp, r_tmp); /* 00-33, 88-11 */ \
    G1 = _mm256_unpacklo_epi16(g_tmp, g_tmp); \
    B1 = _mm256_unpacklo_epi16(b_tmp, b_tmp); \
    R2 = _mm256_unpackhi_epi16(r_tmp, r_tmp); /* 44-77, 1212-1515 */ \
    G2 = _mm256_unpackhi_epi16(g_tmp, g_tmp); \
    B2 = _mm256_unpackhi_epi16(b_tmp, b_tmp);

#define ADD_Y2RGB_32_AVX(Y1, Y2, R1, G1, B1, R2, G2, B2) \
    Y1 = _mm256_srai_epi16(_mm256_mullo_epi16(Y1, _mm256_set1_epi16(param->y_factor)), 7); \
    Y2 = _mm256_srai_epi16(_mm256_mullo_epi16(Y2, _mm256_set1_epi16(param->y_factor)), 7); \
    R1 = _mm256_add_epi16(Y1, R1); /* 0-7, 16-23 */ \
    G1 = _mm256_sub_epi16(Y1, G1); \
    B1 = _mm256_add_epi16(Y1, B1); \
    R2 = _mm256_add_epi16(Y2, R2); /* 8-15, 24-31 */ \
    G2 = _mm256_sub_epi16(Y2, G2); \
    B2 = _mm256_add_epi16(Y2, B2);

#define PACK_RGBA32_32_AVX(R, G, B, A, RGBA) {\
    __m256i rg_lo = _mm256_unpacklo_epi8(R, G); \
	__m256i rg_hi = _mm256_unpackhi_epi8(R, G); \
	__m256i ba_lo = _mm256_unpacklo_epi8(B, A); \
	__m256i ba_hi = _mm256_unpackhi_epi8(B, A); \
	__m256i t0 = _mm256_unpacklo_epi16(rg_lo, ba_lo); \
	__m256i t1 = _mm256_unpackhi_epi16(rg_lo, ba_lo); \
	__m256i t2 = _mm256_unpacklo_epi16(rg_hi, ba_hi); \
	__m256i t3 = _mm256_unpackhi_epi16(rg_hi, ba_hi); \
	RGBA[0] = _mm256_permute2x128_si256(t0, t1, 0x20); \
	RGBA[2] = _mm256_permute2x128_si256(t0, t1, 0x31); \
	RGBA[1] = _mm256_permute2x128_si256(t2, t3, 0x20); \
	RGBA[3] = _mm256_permute2x128_si256(t2, t3, 0x31);}

void yuv420_rgb24_avx(uint32_t width, uint32_t height,
	const uint8_t* Y, const uint8_t* U, const uint8_t* V, const uint8_t* A,
	uint32_t Y_stride, uint32_t U_stride, uint32_t V_stride, uint32_t A_stride,
	uint8_t* RGBA, uint32_t RGBA_stride, YCbCrType yuv_type)
{
	const YUV2RGBParam* const param = &(YUV2RGB[yuv_type]);
	for (uint32_t h = 0; h < (height - 1); h += 2)
	{
		const uint8_t* y_ptr1 = Y + h * Y_stride;
		const uint8_t* y_ptr2 = Y + (h + 1) * Y_stride;
		const uint8_t* u_ptr = U + (h / 2) * U_stride;
		const uint8_t* v_ptr = V + (h / 2) * V_stride;
		const uint8_t* a_ptr1 = (A) ? A + h * A_stride : nullptr;
		const uint8_t* a_ptr2 = (A) ? A + (h + 1) * A_stride : nullptr;
		uint8_t* rgba_ptr1 = RGBA + h * RGBA_stride;
		uint8_t* rgba_ptr2 = RGBA + (h + 1) * RGBA_stride;

		uint32_t w = 0;
		for (; w < (width - 31); w += 32)
		{
			__m256i u = LOAD_SI256((const __m256i*)u_ptr);
			__m256i v = LOAD_SI256((const __m256i*)v_ptr);

			u = _mm256_sub_epi8(u, _mm256_set1_epi8(128));
			v = _mm256_sub_epi8(v, _mm256_set1_epi8(128));

			__m256i u_16_1 = _mm256_cvtepi8_epi16(_mm256_castsi256_si128(u));
			__m256i v_16_1 = _mm256_cvtepi8_epi16(_mm256_castsi256_si128(v));

			__m256i r_tmp, g_tmp, b_tmp;
			__m256i r_uv_16_1, g_uv_16_1, b_uv_16_1;
			__m256i r_uv_16_2, g_uv_16_2, b_uv_16_2;

			UV2RGB_32_AVX(u_16_1, v_16_1,
				r_uv_16_1, g_uv_16_1, b_uv_16_1,
				r_uv_16_2, g_uv_16_2, b_uv_16_2);

			// 첫번째 라인
			__m256i y1 = LOAD_SI256((const __m256i*)y_ptr1);
			y1 = _mm256_sub_epi8(y1, _mm256_set1_epi8(param->y_offset));

			__m256i y_16_1 = _mm256_unpacklo_epi8(y1, _mm256_setzero_si256());
			__m256i y_16_2 = _mm256_unpackhi_epi8(y1, _mm256_setzero_si256());

			__m256i r_16_1 = r_uv_16_1;
			__m256i g_16_1 = g_uv_16_1;
			__m256i b_16_1 = b_uv_16_1;
			__m256i r_16_2 = r_uv_16_2;
			__m256i g_16_2 = g_uv_16_2;
			__m256i b_16_2 = b_uv_16_2;

			ADD_Y2RGB_32_AVX(y_16_1, y_16_2,
				r_16_1, g_16_1, b_16_1,
				r_16_2, g_16_2, b_16_2);

			__m256i r_8_1 = _mm256_packus_epi16(r_16_1, r_16_2);
			__m256i g_8_1 = _mm256_packus_epi16(g_16_1, g_16_2);
			__m256i b_8_1 = _mm256_packus_epi16(b_16_1, b_16_2);
			__m256i a_8_1 = (a_ptr1) ? LOAD_SI256((const __m256i*)a_ptr1) :
				_mm256_set1_epi8((char)255);

			// 두번째 라인
			__m256i y2 = LOAD_SI256((const __m256i*)y_ptr2);
			y2 = _mm256_sub_epi8(y2, _mm256_set1_epi8(param->y_offset));

			y_16_1 = _mm256_unpacklo_epi8(y2, _mm256_setzero_si256());
			y_16_2 = _mm256_unpackhi_epi8(y2, _mm256_setzero_si256());

			r_16_1 = r_uv_16_1;
			g_16_1 = g_uv_16_1;
			b_16_1 = b_uv_16_1;
			r_16_2 = r_uv_16_2;
			g_16_2 = g_uv_16_2;
			b_16_2 = b_uv_16_2;

			ADD_Y2RGB_32_AVX(y_16_1, y_16_2,
				r_16_1, g_16_1, b_16_1,
				r_16_2, g_16_2, b_16_2);

			__m256i r_8_2 = _mm256_packus_epi16(r_16_1, r_16_2);
			__m256i g_8_2 = _mm256_packus_epi16(g_16_1, g_16_2);
			__m256i b_8_2 = _mm256_packus_epi16(b_16_1, b_16_2);
			__m256i a_8_2 = (a_ptr2) ? LOAD_SI256((const __m256i*)a_ptr2) :
				_mm256_set1_epi8((char)255);

			__m256i rgba1[4], rgba2[4];
			PACK_RGBA32_32_AVX(r_8_1, g_8_1, b_8_1, a_8_1, rgba1);
			PACK_RGBA32_32_AVX(r_8_2, g_8_2, b_8_2, a_8_2, rgba2);

			memcpy(rgba_ptr1, &rgba1[0], sizeof(__m256i) * 4);
			memcpy(rgba_ptr2, &rgba2[0], sizeof(__m256i) * 4);

			y_ptr1 += 32;
			y_ptr2 += 32;
			u_ptr += 16;
			v_ptr += 16;
			a_ptr1 = (a_ptr1) ? a_ptr1 + 32 : nullptr;
			a_ptr2 = (a_ptr2) ? a_ptr2 + 32 : nullptr;
			rgba_ptr1 += 128;
			rgba_ptr2 += 128;
		}

		// 남은 픽셀 처리
		yuv420_rgb24_extra(w, width, y_ptr1, y_ptr2, u_ptr, v_ptr,
			a_ptr1, a_ptr2, rgba_ptr1, rgba_ptr2, yuv_type);
	}

	_mm256_zeroupper();
}