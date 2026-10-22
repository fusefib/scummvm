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

#ifndef GOB_PREGOB_EMMANUELLE_RUNTIME_H
#define GOB_PREGOB_EMMANUELLE_RUNTIME_H

#include "common/scummsys.h"
#include "gob/pregob/emmanuelle/platform.h"

namespace Gob {
namespace Emmanuelle {

/** One-entry input latch used while native waits continue pumping events. */
class InputLatch {
public:
	InputLatch();

	void noteKey(int16 key);
	void noteLatestKey(int16 key);
	void noteMouse(int16 x, int16 y, byte buttons);
	void clear();
	// Inspect pending input without consuming it.
	int16 peek(int16 &x, int16 &y, byte &buttons) const;
	int16 take(int16 &x, int16 &y, byte &buttons);

private:
	int16 _key;
	int16 _mouseX;
	int16 _mouseY;
	byte _mouseButtons;
};

struct InputBoundary {
	InputBoundary() : nativeDelayUnits(0), discardStale(false),
							showCursor(false) {}

	uint16 nativeDelayUnits;
	bool discardStale;
	bool showCursor;
};

/** Delay before a fresh opcode-1B wait discards stale input. */
uint16 inputEntryDelay(OldTotPlatformTag platform);
InputBoundary choiceInputBoundary(OldTotPlatformTag platform,
										bool initialEntry, bool reentering);
/** Keyboard cursor motion performed by the native input service. */
byte keyboardCursorStep(OldTotPlatformTag platform);
/** Lowest cursor origin accepted by the native input service. */
int16 minimumCursorOrigin(OldTotPlatformTag platform);
bool recentersMouseOnModuleLoad(OldTotPlatformTag platform);

} // namespace Emmanuelle
} // namespace Gob

#endif // GOB_PREGOB_EMMANUELLE_RUNTIME_H
