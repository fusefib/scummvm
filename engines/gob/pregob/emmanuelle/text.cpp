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
#include "gob/video.h"

namespace Gob {
namespace Emmanuelle {

static bool isControl(byte c) { return c == '[' || c == ']' || c == '{' || c == '}'; }

static bool isSpeechLetter(uint32 c) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		(c >= 0xC0 && c <= 0xD6) || (c >= 0xD8 && c <= 0xF6) ||
		(c >= 0xF8 && c <= 0xFF);
}

static Common::U32String makeSpeechText(const Common::Array<byte> &expanded, uint length) {
	const Common::U32String text((const char *)expanded.data(), length, Common::kDos850);
	Common::U32String spoken;
	bool space = false;
	for (uint i = 0; i < text.size(); ++i) {
		const uint32 c = text[i];
		if (c == '[' || c == ']' || c == '{' || c == '}')
			continue;
		// Join printed word splits such as "COMPORTAMIEN-@ TO". Hyphens
		// elsewhere remain part of the text, including compound words.
		if (c == '-' && !space && !spoken.empty() && isSpeechLetter(spoken.lastChar()) &&
			i + 1 < text.size() && text[i + 1] == '@') {
			uint next = i + 2;
			while (next < text.size() && (text[next] == ' ' || text[next] == '[' ||
					text[next] == ']' || text[next] == '{' || text[next] == '}'))
				++next;
			if (next < text.size() && isSpeechLetter(text[next])) {
				i = next - 1;
				continue;
			}
		}
		if (c == '@' || c <= ' ') {
			space = !spoken.empty();
			continue;
		}
		if (space)
			spoken += ' ';
		spoken += c;
		space = false;
	}
	return spoken;
}

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

bool planOldTotText(const OldTotData &d, const OldTotVM &vm, uint16 o,
		OldTotTextPlan &out, Common::U32String *speechText) {
	if (speechText)
		speechText->clear();
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
	if (speechText && pos)
		*speechText = makeSpeechText(expanded, pos);
	return true;
}

int16 alignOldTotTextBaselines(OldTotTextPlan &plan, const Font &font,
		bool keepSingleLineBaseline) {
	bool needsAlignment = false;
	for (uint i = 0; i < plan.glyphs.size(); ++i) {
		OldTotTextGlyph &glyph = plan.glyphs[i];
		glyph.y = glyph.cellY;
		needsAlignment |= glyph.character == 0xE1 || glyph.character == 0x80 || glyph.character == 0x87;
	}
	if (!needsAlignment)
		return plan.y;
	const bool keepBaseline = keepSingleLineBaseline &&
		plan.glyphs[0].cellY == plan.glyphs.back().cellY &&
		plan.glyphs[0].cellY + font.getCharHeight() + 2 <= plan.y + plan.height;

	// Place native E1 (sharp-s) one row below ordinary letters, 87/80 (c/C
	// with cedilla) two rows below. Lift the line to fit the lowest character.
	// Mixed German LOADHOT:23C4 uses offsets -2/-1/0 respectively.
	byte pixels[64];
	Surface mask(8, 8, 1, pixels);
	int lift = 0;
	int nextLineY = 0;
	int nextInkTop = 32767;
	int inkTop = plan.y;
	uint end = plan.glyphs.size();
	while (end) {
		const int cellY = plan.glyphs[end - 1].cellY;
		uint start = end - 1;
		while (start && plan.glyphs[start - 1].cellY == cellY)
			--start;
		bool visibleLine = false;
		for (uint i = start; i < end; ++i)
			visibleLine |= plan.glyphs[i].character != ' ';
		if (!visibleLine) {
			end = start;
			continue;
		}

		// Work backward so a raised line also raises its contiguous predecessors.
		// Extra blank-line spacing absorbs that carry: the German protection
		// screen's later colour label must not push its top instructions offscreen.
		if (nextInkTop != 32767)
			lift = MAX(0, lift - MAX(0, nextLineY - cellY - plan.lineAdvance));
		for (uint i = start; i < end; ++i) {
			OldTotTextGlyph &glyph = plan.glyphs[i];
			const int offset = glyph.character == 0xE1 ? 1 :
				(glyph.character == 0x80 || glyph.character == 0x87 ? 2 : 0);
			glyph.y = glyph.cellY + offset;
			if (!keepBaseline)
				lift = MAX(lift, offset);
		}
		for (uint i = start; i < end; ++i)
			plan.glyphs[i].y -= lift;

		// Measure ink rather than cells. The two native fonts have different
		// vertical extents; a full-cell collision test would lift too much.
		int top = 32767;
		int bottom = -32768;
		for (uint i = start; i < end; ++i) {
			const OldTotTextGlyph &glyph = plan.glyphs[i];
			mask.clear();
			font.drawLetterRaw(mask, glyph.character, 0, 0, 1, 0, false);
			for (uint row = 0; row < 8; ++row) {
				for (uint col = 0; col < 8; ++col) {
					if (pixels[row * 8 + col]) {
						top = MIN(top, glyph.y + (int)row);
						bottom = MAX(bottom, glyph.y + (int)row);
					}
				}
			}
		}
		if (bottom >= nextInkTop) {
			const int extra = bottom - nextInkTop + 1;
			lift += extra;
			top -= extra;
			for (uint i = start; i < end; ++i)
				plan.glyphs[i].y -= extra;
		}
		if (top != 32767)
			nextInkTop = top;
		inkTop = MIN(inkTop, top);
		nextLineY = cellY;
		end = start;
	}
	return inkTop;
}

} // End of namespace Emmanuelle
} // End of namespace Gob
