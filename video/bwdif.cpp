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


#include "common/util.h"
#include "video/bwdif.h"

namespace Video {

// BWDIF combines the BBC Weston three-field predictor with a YADIF motion
// envelope. Coefficients use 13 fractional bits; the high-frequency temporal
// term has two additional fractional bits. Retained scanlines are never changed.
// Algorithm credits: Martin Weston, Jim Easterbrook, Michael Niedermayer,
// James Darnley and Thomas Mundt. Reference behaviour: FFmpeg 7.1.5 send_frame.
void bwdifFilterRowScalar(byte *dst, const BWDIFRow &row, int width) {
	const int p = row.pitch;
	for (int x = 0; x < width; ++x) {
		const byte *a = row.prev + x;
		const byte *b = row.cur + x;
		const byte *c = row.next + x;
		const int upper = b[row.above];
		const int lower = b[row.below];

		if (row.mode == BWDIFRow::kIntra) {
			dst[x] = CLIP((5077 * (upper + lower) - 981 * (b[-3 * p] + b[3 * p])) >> 13, 0, 255);
			continue;
		}

		// For the first/only output, the temporal pair is previous/current.
		const int center = (a[0] + b[0]) >> 1;
		const int motion = ABS(a[0] - b[0]);
		int limit = MAX(motion >> 1,
				MAX((ABS(a[row.above] - upper) + ABS(a[row.below] - lower)) >> 1,
					(ABS(c[row.above] - upper) + ABS(c[row.below] - lower)) >> 1));
		if (!limit) {
			dst[x] = center;
			continue;
		}

		if (row.spatial) {
			const int upperDiff = ((a[-2 * p] + b[-2 * p]) >> 1) - upper;
			const int lowerDiff = ((a[2 * p] + b[2 * p]) >> 1) - lower;
			const int upperCenter = center - upper;
			const int lowerCenter = center - lower;
			limit = MAX(limit, MAX(MIN(MIN(upperCenter, lowerCenter), MAX(upperDiff, lowerDiff)),
					-MAX(MAX(upperCenter, lowerCenter), MIN(upperDiff, lowerDiff))));
		}

		int predicted = (upper + lower) >> 1;
		if (row.mode == BWDIFRow::kTemporal) {
			const int far = b[-3 * p] + b[3 * p];
			if (ABS(upper - lower) > motion) {
				const int temporal = 5570 * (a[0] + b[0])
						- 3801 * (a[-2 * p] + b[-2 * p] + a[2 * p] + b[2 * p])
						+ 1016 * (a[-4 * p] + b[-4 * p] + a[4 * p] + b[4 * p]);
				predicted = ((temporal >> 2) + 4309 * (upper + lower) - 213 * far) >> 13;
			} else {
				predicted = (5077 * (upper + lower) - 981 * far) >> 13;
			}
		}
		dst[x] = CLIP(CLIP(predicted, center - limit, center + limit), 0, 255);
	}
}

BWDIF::BWDIF() : _filterRow(bwdifFilterRowScalar) {
}

void BWDIF::filterPlane(byte *dst, const byte *prev, const byte *cur, const byte *next,
		int width, int height, int pitch, bool topFieldFirst, bool firstPicture) const {
	assert(width > 0 && height > 0 && pitch >= width);
	const int retained = topFieldFirst ? 0 : 1;
	for (int y = 0; y < height; ++y) {
		if (width < 3 || height < 4 || (y & 1) == retained) {
			memcpy(dst + y * pitch, cur + y * pitch, width);
			continue;
		}
		BWDIFRow row;
		row.prev = prev + y * pitch;
		row.cur = cur + y * pitch;
		row.next = next + y * pitch;
		row.pitch = pitch;
		row.above = y ? -pitch : pitch;
		row.below = y + 1 < height ? pitch : -pitch;
		row.spatial = y >= 2 && y + 2 < height;
		if (firstPicture)
			row.mode = (y < 3 || y + 3 >= height) ? BWDIFRow::kEdge : BWDIFRow::kIntra;
		else
			row.mode = (y < 4 || y + 4 >= height) ? BWDIFRow::kEdge : BWDIFRow::kTemporal;
		_filterRow(dst + y * pitch, row, width);
	}
}

} // End of namespace Video
