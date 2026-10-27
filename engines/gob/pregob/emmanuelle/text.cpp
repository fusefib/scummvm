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
 *
 * This file is dual-licensed.
 * In addition to the GPLv3 license mentioned above, this code is also
 * licensed under LGPL 2.1. See LICENSES/COPYING.LGPL file for the
 * full text of the license.
 *
 */

#include "gob/pregob/emmanuelle/text.h"

namespace Gob {
namespace Emmanuelle {

static bool isControl(byte c) { return c == '[' || c == ']' || c == '{' || c == '}'; }

static void appendDecimal(Common::Array<byte> &out, uint16 value, byte width) {
	byte tmp[6];
	uint n = 0;
	do {
		tmp[n++] = (byte)('0' + (value % 10));
		value /= 10;
	} while (value && n < 5);
	while (n < width) {
		out.push_back(' ');
		--width;
	}
	while (n)
		out.push_back(tmp[--n]);
}

bool planOldTotText(const OldTotData &d, const OldTotVM &vm, uint16 o, OldTotTextPlan &out) {
	if (uint32(o) + 15u > d.size() || d.byteAt(o) != 0x1D)
		return false;
	const uint16 flags = d.uint16BEAt(o + 13);
	if ((flags & 0x00FF) == 0x00FF)
		return false;
	out.x = (int16)((flags & 0x8000) ? vm.readVariable(d.uint16BEAt(o + 1)) : d.uint16BEAt(o + 1));
	out.y = (int16)((flags & 0x4000) ? vm.readVariable(d.uint16BEAt(o + 3)) : d.uint16BEAt(o + 3));
	out.width = d.uint16BEAt(o + 5);
	out.height = d.uint16BEAt(o + 7);
	out.lineAdvance = d.byteAt(o + 9);
	if (!out.lineAdvance)
		out.lineAdvance = 1;
	out.background = d.byteAt(o + 10);
	const byte primary = d.byteAt(o + 11), alternate = d.byteAt(o + 12);
	out.fontIndex = (flags & 1) ? 1 : 0;
	out.glyphs.clear();
	Common::Array<byte> expanded;
	for (uint32 p = o + 15; p < d.size(); ++p) {
		byte c = d.byteAt(p);
		if (!c)
			break;
		if (c == 0xBA && p + 2 < d.size()) {
			byte v = d.byteAt(++p), w = d.byteAt(++p);
			if (v >= 0x20 && w >= 0x20) {
				appendDecimal(expanded, vm.readVariable(v - 0x20), w - 0x20);
				continue;
			}
		}
		// Preserve JEUCAR glyph indices, including localized characters.
		expanded.push_back(c);
	}
	const uint maxChars = out.width >= 8 ? out.width / 8 : 1;
	uint pos = 0;
	int16 cy = out.y;
	byte color = primary;
	while (pos < expanded.size() && cy < out.y + (int16)out.height) {
		uint start = pos, end = pos, visible = 0;
		while (end < expanded.size()) {
			byte c = expanded[end];
			if (isControl(c)) {
				++end;
				continue;
			}
			if (c == '@')
				break;
			++visible;
			++end;
			if (visible >= maxChars)
				break;
		}
		if (end < expanded.size() && expanded[end] != '@' && expanded[end] != ' ' && !isControl(expanded[end]) && visible >= maxChars) {
			uint q = end;
			while (q > start && expanded[q - 1] != ' ')
				--q;
			if (q > start)
				end = q - 1;
		}
		int16 cx = out.x;
		for (uint q = start; q < end; ++q) {
			byte c = expanded[q];
			if (c == '[') {
				color = alternate;
				continue;
			}
			if (c == ']') {
				color = primary;
				continue;
			}
			if (c == '{' || c == '}')
				continue;
			if (c == ' ' || c >= 0x20) {
				const int16 cellY = cy + (out.lineAdvance > 8 ? (out.lineAdvance - 8) / 2 : 0);
				OldTotTextGlyph g = {c, color, cx, cellY, cellY};
				out.glyphs.push_back(g);
				cx += 8;
			}
		}
		pos = end;
		if (pos < expanded.size() && expanded[pos] == '@') {
			// Explicit line breaks preserve following spaces. The scripts use
			// them to position labels over their input regions.
			++pos;
		} else {
			// Spaces consumed by automatic word wrapping do not begin the next
			// visual line.
			while (pos < expanded.size() && expanded[pos] == ' ')
				++pos;
		}
		cy += out.lineAdvance;
	}
	return true;
}

} // End of namespace Emmanuelle
} // End of namespace Gob
