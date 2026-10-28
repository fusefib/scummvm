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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "common/endian.h"
#include "common/md5.h"
#include "common/memstream.h"
#include "gob/surface.h"
#include "gob/pregob/mewilo/font.h"

namespace Gob {
namespace Mewilo {

MewiloFont::MewiloFont() {
}

bool MewiloFont::load(Common::SeekableReadStream &stream, bool thomsonWrapper) {
	_data.clear();
	if (stream.size() != (thomsonWrapper ? 1034 : 1024) || !stream.seek(0))
		return false;
	if (thomsonWrapper) {
		// Native BIN load record: 00, size 0400, destination 9C00, payload.
		byte header[5];
		if (stream.read(header, 5) != 5 || header[0] != 0 ||
			READ_BE_UINT16(header + 1) != 1024 || READ_BE_UINT16(header + 3) != 0x9C00)
			return false;
	}
	_data.resize(1024);
	if (stream.read(_data.data(), _data.size()) != _data.size()) {
		_data.clear();
		return false;
	}
	if (thomsonWrapper) {
		byte end[5];
		if (stream.read(end, 5) != 5 || end[0] != 0xFF || READ_BE_UINT16(end + 1) != 0) {
			_data.clear();
			return false;
		}
	}
	return true;
}

void MewiloFont::drawChar(Surface &dest, byte c, int16 x, int16 y, byte color) const {
	if (!valid() || c >= 128)
		return;

	const uint32 glyphBase = (uint32)c * 8;
	for (int row = 0; row < 8; ++row) {
		// Native code reads glyph bytes from (c+1)*8-1 down to c*8.
		const byte bits = _data[glyphBase + (7 - row)];
		for (int col = 0; col < 8; ++col) {
			if (!(bits & (0x80 >> col)))
				continue;
			const int16 px = x + col;
			const int16 py = y + row;
			if (px >= 0 && py >= 0 && px < dest.getWidth() && py < dest.getHeight())
				dest.putPixel(px, py, color);
		}
	}
}

bool MewiloFont::computeMD5(byte digest[16]) const {
	if (!valid()) return false;
	Common::MemoryReadStream owned(_data.data(), _data.size());
	return Common::computeStreamMD5(owned, digest);
}

} // End of namespace Mewilo
} // End of namespace Gob
