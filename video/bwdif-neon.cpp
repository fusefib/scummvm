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

#include "common/scummsys.h"

#if defined(USE_MPEG2) && defined(SCUMMVM_NEON)

#include "video/bwdif.h"

#include <arm_neon.h>

#if !defined(__aarch64__) && !defined(__ARM_NEON)
#if defined(__clang__)
#pragma clang attribute push (__attribute__((target("neon"))), apply_to=function)
#elif defined(__GNUC__)
#pragma GCC push_options
#pragma GCC target("fpu=neon")
#endif
#endif

namespace Video {

static FORCEINLINE int32x4_t loadSamples(const byte *src) {
	// Only four source bytes are available at the end of a vectorized row.
	// Byte staging also preserves sample order independently of endianness.
	byte samples[8] = { 0 };
	memcpy(samples, src, 4);
	return vreinterpretq_s32_u32(vmovl_u16(vget_low_u16(vmovl_u8(vld1_u8(samples)))));
}

static FORCEINLINE void storeSamples(byte *dst, int32x4_t value) {
	const uint16x4_t clipped = vqmovun_s32(value);
	byte samples[8];
	vst1_u8(samples, vqmovn_u16(vcombine_u16(clipped, vdup_n_u16(0))));
	memcpy(dst, samples, 4);
}

void bwdifFilterRowNEON(byte *dst, const BWDIFRow &row, int width) {
	const int p = row.pitch;
	const int32x4_t zero = vdupq_n_s32(0);
	int x = 0;
	for (; x + 4 <= width; x += 4) {
		const byte *a = row.prev + x;
		const byte *b = row.cur + x;
		const byte *c = row.next + x;
		const int32x4_t upper = loadSamples(b + row.above);
		const int32x4_t lower = loadSamples(b + row.below);
		const int32x4_t nearSum = vaddq_s32(upper, lower);
		if (row.mode == BWDIFRow::kIntra) {
			const int32x4_t farSum = vaddq_s32(loadSamples(b - 3 * p), loadSamples(b + 3 * p));
			storeSamples(dst + x, vshrq_n_s32(vsubq_s32(vmulq_n_s32(nearSum, 5077), vmulq_n_s32(farSum, 981)), 13));
			continue;
		}

		const int32x4_t previous = loadSamples(a);
		const int32x4_t current = loadSamples(b);
		const int32x4_t centerSum = vaddq_s32(previous, current);
		const int32x4_t center = vshrq_n_s32(centerSum, 1);
		const int32x4_t motion = vabdq_s32(previous, current);
		const int32x4_t prevDiff = vshrq_n_s32(vaddq_s32(
				vabdq_s32(loadSamples(a + row.above), upper),
				vabdq_s32(loadSamples(a + row.below), lower)), 1);
		const int32x4_t nextDiff = vshrq_n_s32(vaddq_s32(
				vabdq_s32(loadSamples(c + row.above), upper),
				vabdq_s32(loadSamples(c + row.below), lower)), 1);
		int32x4_t limit = vmaxq_s32(vshrq_n_s32(motion, 1), vmaxq_s32(prevDiff, nextDiff));
		const uint32x4_t still = vceqq_s32(limit, zero);
		const int32x2_t combined = vorr_s32(vget_low_s32(limit), vget_high_s32(limit));
		if ((vget_lane_s32(combined, 0) | vget_lane_s32(combined, 1)) == 0) {
			storeSamples(dst + x, center);
			continue;
		}

		if (row.spatial) {
			const int32x4_t upperDiff = vsubq_s32(vshrq_n_s32(vaddq_s32(
					loadSamples(a - 2 * p), loadSamples(b - 2 * p)), 1), upper);
			const int32x4_t lowerDiff = vsubq_s32(vshrq_n_s32(vaddq_s32(
					loadSamples(a + 2 * p), loadSamples(b + 2 * p)), 1), lower);
			const int32x4_t upperCenter = vsubq_s32(center, upper);
			const int32x4_t lowerCenter = vsubq_s32(center, lower);
			const int32x4_t low = vminq_s32(vminq_s32(upperCenter, lowerCenter), vmaxq_s32(upperDiff, lowerDiff));
			const int32x4_t high = vmaxq_s32(vmaxq_s32(upperCenter, lowerCenter), vminq_s32(upperDiff, lowerDiff));
			limit = vmaxq_s32(limit, vmaxq_s32(low, vnegq_s32(high)));
		}

		int32x4_t predicted = vshrq_n_s32(nearSum, 1);
		if (row.mode == BWDIFRow::kTemporal) {
			const int32x4_t farSum = vaddq_s32(loadSamples(b - 3 * p), loadSamples(b + 3 * p));
			const int32x4_t two = vaddq_s32(
					vaddq_s32(loadSamples(a - 2 * p), loadSamples(b - 2 * p)),
					vaddq_s32(loadSamples(a + 2 * p), loadSamples(b + 2 * p)));
			const int32x4_t four = vaddq_s32(
					vaddq_s32(loadSamples(a - 4 * p), loadSamples(b - 4 * p)),
					vaddq_s32(loadSamples(a + 4 * p), loadSamples(b + 4 * p)));
			int32x4_t temporal = vaddq_s32(vsubq_s32(vmulq_n_s32(centerSum, 5570), vmulq_n_s32(two, 3801)), vmulq_n_s32(four, 1016));
			temporal = vaddq_s32(vshrq_n_s32(temporal, 2), vmulq_n_s32(nearSum, 4309));
			temporal = vshrq_n_s32(vsubq_s32(temporal, vmulq_n_s32(farSum, 213)), 13);
			const int32x4_t spatial = vshrq_n_s32(vsubq_s32(vmulq_n_s32(nearSum, 5077), vmulq_n_s32(farSum, 981)), 13);
			predicted = vbslq_s32(vcgtq_s32(vabdq_s32(upper, lower), motion), temporal, spatial);
		}
		predicted = vminq_s32(vmaxq_s32(predicted, vsubq_s32(center, limit)), vaddq_s32(center, limit));
		storeSamples(dst + x, vbslq_s32(still, center, predicted));
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

#if !defined(__aarch64__) && !defined(__ARM_NEON)
#if defined(__clang__)
#pragma clang attribute pop
#elif defined(__GNUC__)
#pragma GCC pop_options
#endif
#endif

#endif // USE_MPEG2 && SCUMMVM_NEON
