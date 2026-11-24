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

#ifndef GOB_PREGOB_ONCEUPON_PALETTE_H
#define GOB_PREGOB_ONCEUPON_PALETTE_H

#include "common/scummsys.h"

namespace Gob {
class GobEngine;
namespace OnceUpon {

static const  int kPaletteSize  = 16;
static const uint kPaletteCount = 20;

extern const byte kCopyProtectionPalette[3 * kPaletteSize];
extern const byte kGamePalettes[kPaletteCount][3 * kPaletteSize];

/** The selected native palette and the visible backend colors have distinct roles. */
class Palette {
public:
	enum { kMenuFirstColor = 240, kMenuColorCount = 6 };
	struct State {
		bool holdingBlack;
		byte target[768]; // Six-bit native components, projected into Draw.
		byte visible[768]; // Exact eight-bit backend colors, including load/fade origins.
		State();
	};
	explicit Palette(GobEngine *vm);
	void reset(bool holdingBlack);
	bool holdingBlack() const { return _state.holdingBlack; }
	void selectTarget(const byte *colors, uint size);
	void showTarget();
	void fadeToTarget(bool pollBeforeReveal = false);
	void fadeToBlack();
	void showTemporary(const byte *colors, uint size);
	State snapshot() const;
	// Adoption updates colors and fade caches only; it never publishes pixels.
	void adopt(const State &state);
	void showMenuColors();
	void restoreMenuColors(const State &state);

private:
	GobEngine *_vm;
	State _state;
	void projectTarget();
	void captureVisible();
	void projectVisible();
	void fadeDOS(bool toBlack, bool pollBeforeReveal = false);
};

} // namespace OnceUpon
} // namespace Gob
#endif
