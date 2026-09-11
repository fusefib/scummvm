/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include "video/bwdif.h"
#include <emmintrin.h>

#if !defined(__x86_64__)
#if defined(__clang__)
#pragma clang attribute push (__attribute__((target("sse2"))), apply_to=function)
#elif defined(__GNUC__)
#pragma GCC push_options
#pragma GCC target("sse2")
#endif
#endif

namespace Video {

static FORCEINLINE __m128i loadSamples(const byte *p) {
	uint32 value;
	memcpy(&value, p, 4);
	const __m128i zero = _mm_setzero_si128();
	return _mm_unpacklo_epi16(_mm_unpacklo_epi8(_mm_cvtsi32_si128(value), zero), zero);
}

static FORCEINLINE __m128i absSamples(__m128i v) {
	const __m128i sign = _mm_srai_epi32(v, 31);
	return _mm_sub_epi32(_mm_xor_si128(v, sign), sign);
}

static FORCEINLINE __m128i choose(__m128i mask, __m128i a, __m128i b) {
	return _mm_or_si128(_mm_and_si128(mask, a), _mm_andnot_si128(mask, b));
}

static FORCEINLINE __m128i maximum(__m128i a, __m128i b) {
	return choose(_mm_cmpgt_epi32(a, b), a, b);
}

static FORCEINLINE __m128i minimum(__m128i a, __m128i b) {
	return choose(_mm_cmpgt_epi32(a, b), b, a);
}

static FORCEINLINE __m128i weighted(__m128i sum, int coefficient) {
	// Each 32-bit lane contains a nonnegative sum of at most four bytes.
	// Its high word is zero; PMADDWD therefore computes four exact 32-bit
	// products without requiring SSE4.1. All coefficients fit signed 16-bit.
	return _mm_madd_epi16(sum, _mm_set1_epi32(coefficient));
}

static FORCEINLINE void storeSamples(byte *dst, __m128i value) {
	const __m128i zero = _mm_setzero_si128();
	value = _mm_packus_epi16(_mm_packs_epi32(value, zero), zero);
	const uint32 packed = _mm_cvtsi128_si32(value);
	memcpy(dst, &packed, 4);
}

void bwdifFilterRowSSE2(byte *dst, const BWDIFRow &row, int width) {
	const int p = row.pitch;
	const __m128i zero = _mm_setzero_si128();
	int x = 0;
	for (; x + 4 <= width; x += 4) {
		const byte *a = row.prev + x;
		const byte *b = row.cur + x;
		const byte *c = row.next + x;
		const __m128i upper = loadSamples(b + row.above);
		const __m128i lower = loadSamples(b + row.below);
		const __m128i nearSum = _mm_add_epi32(upper, lower);
		if (row.mode == BWDIFRow::kIntra) {
			const __m128i farSum = _mm_add_epi32(loadSamples(b - 3 * p), loadSamples(b + 3 * p));
			storeSamples(dst + x, _mm_srai_epi32(_mm_sub_epi32(weighted(nearSum, 5077), weighted(farSum, 981)), 13));
			continue;
		}

		const __m128i centerSum = _mm_add_epi32(loadSamples(a), loadSamples(b));
		const __m128i center = _mm_srli_epi32(centerSum, 1);
		const __m128i motion = absSamples(_mm_sub_epi32(loadSamples(a), loadSamples(b)));
		const __m128i prevDiff = _mm_srli_epi32(_mm_add_epi32(
				absSamples(_mm_sub_epi32(loadSamples(a + row.above), upper)),
				absSamples(_mm_sub_epi32(loadSamples(a + row.below), lower))), 1);
		const __m128i nextDiff = _mm_srli_epi32(_mm_add_epi32(
				absSamples(_mm_sub_epi32(loadSamples(c + row.above), upper)),
				absSamples(_mm_sub_epi32(loadSamples(c + row.below), lower))), 1);
		__m128i limit = maximum(_mm_srli_epi32(motion, 1), maximum(prevDiff, nextDiff));
		const __m128i still = _mm_cmpeq_epi32(limit, zero);
		if (_mm_movemask_epi8(still) == 0xFFFF) {
			storeSamples(dst + x, center);
			continue;
		}

		if (row.spatial) {
			const __m128i upperDiff = _mm_sub_epi32(_mm_srli_epi32(_mm_add_epi32(
					loadSamples(a - 2 * p), loadSamples(b - 2 * p)), 1), upper);
			const __m128i lowerDiff = _mm_sub_epi32(_mm_srli_epi32(_mm_add_epi32(
					loadSamples(a + 2 * p), loadSamples(b + 2 * p)), 1), lower);
			const __m128i upperCenter = _mm_sub_epi32(center, upper);
			const __m128i lowerCenter = _mm_sub_epi32(center, lower);
			const __m128i low = minimum(minimum(upperCenter, lowerCenter), maximum(upperDiff, lowerDiff));
			const __m128i high = maximum(maximum(upperCenter, lowerCenter), minimum(upperDiff, lowerDiff));
			limit = maximum(limit, maximum(low, _mm_sub_epi32(zero, high)));
		}

		__m128i predicted = _mm_srli_epi32(nearSum, 1);
		if (row.mode == BWDIFRow::kTemporal) {
			const __m128i farSum = _mm_add_epi32(loadSamples(b - 3 * p), loadSamples(b + 3 * p));
			const __m128i two = _mm_add_epi32(
					_mm_add_epi32(loadSamples(a - 2 * p), loadSamples(b - 2 * p)),
					_mm_add_epi32(loadSamples(a + 2 * p), loadSamples(b + 2 * p)));
			const __m128i four = _mm_add_epi32(
					_mm_add_epi32(loadSamples(a - 4 * p), loadSamples(b - 4 * p)),
					_mm_add_epi32(loadSamples(a + 4 * p), loadSamples(b + 4 * p)));
			__m128i temporal = _mm_add_epi32(_mm_sub_epi32(weighted(centerSum, 5570), weighted(two, 3801)), weighted(four, 1016));
			temporal = _mm_add_epi32(_mm_srai_epi32(temporal, 2), weighted(nearSum, 4309));
			temporal = _mm_srai_epi32(_mm_sub_epi32(temporal, weighted(farSum, 213)), 13);
			const __m128i spatial = _mm_srai_epi32(_mm_sub_epi32(weighted(nearSum, 5077), weighted(farSum, 981)), 13);
			predicted = choose(_mm_cmpgt_epi32(absSamples(_mm_sub_epi32(upper, lower)), motion), temporal, spatial);
		}
		predicted = minimum(maximum(predicted, _mm_sub_epi32(center, limit)), _mm_add_epi32(center, limit));
		storeSamples(dst + x, choose(still, center, predicted));
	}
	if (x < width) {
		BWDIFRow tail = row;
		tail.prev += x;
		tail.cur += x;
		tail.next += x;
		bwdifFilterRowScalar(dst + x, tail, width - x);
	}
}

} // End of namespace Video

#if !defined(__x86_64__)
#if defined(__clang__)
#pragma clang attribute pop
#elif defined(__GNUC__)
#pragma GCC pop_options
#endif
#endif
