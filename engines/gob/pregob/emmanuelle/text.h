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

#ifndef GOB_PREGOB_EMMANUELLE_TEXT_H
#define GOB_PREGOB_EMMANUELLE_TEXT_H

#include "common/array.h"
#include "common/scummsys.h"
#include "common/ustr.h"
#include "gob/pregob/emmanuelle/interpreter.h"
#include "gob/pregob/emmanuelle/oldtot.h"

namespace Gob {

class Font;

namespace Emmanuelle {

struct OldTotTextGlyph {
	byte character, color;
	int16 x, y;
	// Ink may move; the script's opaque character cell must not move with it.
	int16 cellY;
};

struct OldTotTextPlan {
	OldTotTextPlan() {
		x = 0;
		y = 0;
		width = 0;
		height = 0;
		lineAdvance = 0;

		background = 0x10;
		fontIndex = 0;
	}
	int16 x, y;
	uint16 width, height;
	byte lineAdvance, background, fontIndex;
	Common::Array<OldTotTextGlyph> glyphs;
};

/** Decode ordinary opcode 0x1D text using 8x8 LET glyphs and native wrapping.
 * Returns false for the randomized copy-protection form.
 */
bool planOldTotText(const OldTotData &data, const OldTotVM &vm, uint16 offset,
	OldTotTextPlan &out, Common::U32String *speechText = nullptr);

/** Align the original LET bitmaps without changing wrapping or cell positions.
 * Returns the highest ink row, for restoring pixels above the text rectangle.
 * keepSingleLineBaseline allows descenders below a single line when they fit.
 */
int16 alignOldTotTextBaselines(OldTotTextPlan &plan, const Font &font,
	bool keepSingleLineBaseline = false);

} // End of namespace Emmanuelle
} // End of namespace Gob

#endif // GOB_PREGOB_EMMANUELLE_TEXT_H
