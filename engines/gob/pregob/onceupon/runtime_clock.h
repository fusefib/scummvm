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

#ifndef GOB_PREGOB_ONCEUPON_RUNTIME_CLOCK_H
#define GOB_PREGOB_ONCEUPON_RUNTIME_CLOCK_H

#include "common/scummsys.h"

namespace Gob {
class GobEngine;
namespace OnceUpon {

class Forest;

/** Once Upon's gameplay epoch; menus and nested engine pauses stop it once. */
class RuntimeClock {
public:
	explicit RuntimeClock(GobEngine *vm);
	uint32 now() const;
	void pause();
	void resume();
	void setFrameRate(int16 rate);
	bool waitFrame(Forest *forest = nullptr);
	void limitActionCursor(bool limited) { _limitActionCursor = limited; }
	void interruptWait() { _waitInterrupted = true; }
	void cancelWaitInterrupt() { _waitInterrupted = false; }
	/** Begin a fresh scene epoch without changing an outer modal pause. */
	void restart();

private:
	GobEngine *_vm;
	mutable uint32 _time, _lastWall;
	mutable uint16 _speed;
	uint _pauseDepth;
	bool _waitInterrupted;
	bool _limitActionCursor;
	uint32 _frameStart, _menuFrameStart;
};

} // namespace OnceUpon
} // namespace Gob
#endif
