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

#ifndef GOB_PREGOB_MEWILO_TEXT_H
#define GOB_PREGOB_MEWILO_TEXT_H

#include "common/array.h"
#include "common/scummsys.h"

namespace Gob {
namespace Mewilo {

/** One native 0x1D text line after width/delimiter extraction.
 *
 * The bytes retain Mewilo's inline [ ] { } markup.  '@' and the terminating
 * NUL are delimiters consumed by the extractor and are not returned.
 */
struct MewiloTextLine {
	Common::Array<byte> bytes;
	uint32 consumed;
	bool endOfText;

	MewiloTextLine() : consumed(0), endOfText(false) {}
};

/** Extract native 0x1D text: markup has zero width, '@' breaks a line,
 * and wrapping rewinds the source to the word after the last literal space.
 */
bool extractMewiloTextLine(const byte *source, uint32 sourceSize,
	uint16 visibleWidth, MewiloTextLine &line);

/** Count visible glyph cells in an extracted Mewilo line. */
uint16 countMewiloVisibleTextCells(const Common::Array<byte> &line);

/** Apply Mewilo's native case, accent, punctuation and space normalization. */
void normalizeMewiloAnswer(Common::Array<byte> &text);

} // End of namespace Mewilo
} // End of namespace Gob

#endif // GOB_PREGOB_MEWILO_TEXT_H
