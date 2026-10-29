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

#include "gob/pregob/mewilo/text.h"

namespace Gob {
namespace Mewilo {

static bool isZeroWidthMarkup(byte c) {
	return c == '[' || c == ']' || c == '{' || c == '}';
}

uint16 countMewiloVisibleTextCells(const Common::Array<byte> &line) {
	uint16 count = 0;
	for (uint i = 0; i < line.size(); ++i) {
		if (!isZeroWidthMarkup(line[i]))
			++count;
	}
	return count;
}

void normalizeMewiloAnswer(Common::Array<byte> &text) {
	for (uint i = 0; i < text.size(); ++i) {
		byte &c = text[i];
		if (c >= 'A' && c <= 'Z')
			c += 'a' - 'A';
		else if (c == 0x16 || c == 0x17 || c == 0x1B || c == 0x82 || c == 0x8A)
			c = 'e';
		else if (c == 0x18 || c == 0x1C || c == 0x85)
			c = 'a';
		else if (c == 0x19 || c == 0x97)
			c = 'u';
		else if (c == 0x1A || c == 0x87)
			c = 'c';
		else if (c == 0x1E || c == 0x1F)
			c = 'i';
		else if (c == 0x1D)
			c = 'o';
		else if ((c >= 0x21 && c <= 0x26) || (c >= 0x28 && c <= 0x2A) ||
			c == 0x2C || c == 0x2E || c == 0x2F ||
			(c >= 0x3A && c <= 0x40) || (c >= 0x5B && c <= 0x60) ||
			(c >= 0x7B && c <= 0x7F))
			c = ' ';
	}

	while (!text.empty() && text.front() == ' ')
		text.remove_at(0);
	while (!text.empty() && text.back() == ' ')
		text.remove_at(text.size() - 1);

	for (uint i = 1; i < text.size();) {
		if (text[i - 1] == ' ' && text[i] == ' ')
			text.remove_at(i);
		else
			++i;
	}
}

bool extractMewiloTextLine(const byte *source, uint32 sourceSize,
	uint16 visibleWidth, MewiloTextLine &line) {
	line.bytes.clear();
	line.consumed = 0;
	line.endOfText = false;

	if (!source || !sourceSize || !visibleWidth)
		return false;

	uint32 p = 0;
	uint16 visible = 0;

	while (p < sourceSize) {
		const byte c = source[p++];

		// The native helper first appends these delimiters to its temporary
		// Pascal string and then removes them.  The observable result is that
		// they terminate the extracted line without being rendered.
		if (c == 0) {
			line.consumed = p;
			line.endOfText = true;
			return true;
		}
		if (c == '@') {
			line.consumed = p;
			return true;
		}

		line.bytes.push_back(c);
		if (!isZeroWidthMarkup(c))
			++visible;

		if (visible < visibleWidth)
			continue;

		// Width boundary.  Native code examines the next source byte before
		// deciding whether to backtrack to a word boundary.  NUL, space and
		// '@' are consumed immediately.
		if (p >= sourceSize) {
			line.consumed = p;
			return true;
		}

		const byte next = source[p];
		if (next == 0 || next == ' ' || next == '@') {
			++p;
			line.consumed = p;
			line.endOfText = (next == 0);
			return true;
		}

		// The native helper searches its temporary raw-byte string backwards
		// for a literal space.  It rewinds the source pointer once for each
		// byte after that space (including zero-width markup), and truncates
		// the line before the space.  This makes the next line begin with the
		// first byte of the word rather than with whitespace.
		int32 lastSpace = -1;
		for (int32 i = (int32)line.bytes.size() - 1; i >= 0; --i) {
			if (line.bytes[(uint)i] == ' ') {
				lastSpace = i;
				break;
			}
		}

		if (lastSpace >= 0) {
			const uint32 trailing = line.bytes.size() - (uint32)lastSpace - 1;
			if (trailing <= p)
				p -= trailing;
			line.bytes.resize((uint)lastSpace);
		}

		line.consumed = p;
		return true;
	}

	// Reaching the supplied buffer limit without NUL is a malformed source.
	return false;
}

} // End of namespace Mewilo
} // End of namespace Gob
