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

#include "gob/surface.h"
#include "gob/pregob/mewilo/font.h"

namespace Gob {
namespace Mewilo {

MewiloFont::MewiloFont() {
}

bool MewiloFont::load(Common::SeekableReadStream &stream) {
	_data.clear();
	if (stream.size() != 1024)
		return false;
	_data.resize(1024);
	stream.seek(0);
	if (stream.read(_data.data(), _data.size()) != _data.size()) {
		_data.clear();
		return false;
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

} // End of namespace Mewilo
} // End of namespace Gob
