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


#ifndef VIDEO_BWDIF_H
#define VIDEO_BWDIF_H

#include "common/scummsys.h"

namespace Video {

// The row kernel reconstructs the missing field of a single-rate output.
struct BWDIFRow {
	enum Mode { kEdge, kIntra, kTemporal };
	const byte *prev, *cur, *next;
	int pitch, above, below;
	Mode mode;
	bool spatial;
};

typedef void (*BWDIFRowFilter)(byte *dst, const BWDIFRow &row, int width);
void bwdifFilterRowScalar(byte *dst, const BWDIFRow &row, int width);
#ifdef SCUMMVM_SSE2
void bwdifFilterRowSSE2(byte *dst, const BWDIFRow &row, int width);
#endif

class BWDIF {
public:
	// The caller supplies the backend's runtime CPU capability.
	explicit BWDIF(bool useSSE2 = false);

	// All four planes have the same pitch; no padding is read or written.
	// Small planes are copied unchanged. Source and destination must not alias.
	void filterPlane(byte *dst, const byte *prev, const byte *cur, const byte *next,
			int width, int height, int pitch, bool topFieldFirst, bool firstPicture) const;

private:
	BWDIFRowFilter _filterRow;
};

} // End of namespace Video

#endif
