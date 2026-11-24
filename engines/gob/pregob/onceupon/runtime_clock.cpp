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

#include "common/system.h"
#include "common/util.h"
#include "gob/gob.h"
#include "gob/global.h"
#include "gob/util.h"
#include "gob/video.h"
#include "gob/pregob/onceupon/runtime_clock.h"
#include "gob/pregob/onceupon/forest.h"
#include "gob/pregob/onceupon/input.h"

namespace Gob {
namespace OnceUpon {

RuntimeClock::RuntimeClock(GobEngine *vm) : _vm(vm), _time(0),
	_lastWall(g_system->getMillis()), _speed(MAX<int16>(1, vm->_global->_speedFactor)),
	_pauseDepth(0), _waitInterrupted(false), _limitActionCursor(false), _frameStart(0), _menuFrameStart(_lastWall) {
}

uint32 RuntimeClock::now() const {
	const uint32 wall = g_system->getMillis();
	// Integrate the interval at its previous speed before observing a change.
	// Unsigned subtraction is deliberate: both epochs may wrap naturally.
	if (!_pauseDepth)
		_time += (wall - _lastWall) * _speed;
	_lastWall = wall;
	_speed = MAX<int16>(1, _vm->_global->_speedFactor);
	return _time;
}

void RuntimeClock::pause() {
	now();
	if (!_pauseDepth++)
		_menuFrameStart = g_system->getMillis();
}

void RuntimeClock::resume() {
	now();
	if (_pauseDepth)
		--_pauseDepth;
}

void RuntimeClock::setFrameRate(int16 rate) {
	_vm->_util->setFrameRate(rate);
	_frameStart = now();
	_menuFrameStart = g_system->getMillis();
}

void RuntimeClock::restart() {
	_waitInterrupted = false;
	_time = 0;
	_lastWall = g_system->getMillis();
	_speed = MAX<int16>(1, _vm->_global->_speedFactor);
	_vm->_util->setFrameRate(15);
	_frameStart = 0;
	_menuFrameStart = _lastWall;
}

bool RuntimeClock::waitFrame(Forest *forest) {
	if (_waitInterrupted)
		return false;
	// Menus share the pacing order but use wall milliseconds while gameplay is
	// frozen. A nested engine pause cannot shift either scene deadline twice.
	const bool menu = _pauseDepth != 0;
	uint32 &started = menu ? _menuFrameStart : _frameStart;
	uint32 current = menu ? g_system->getMillis() : now();
	const int32 elapsed = current - started;
	if (elapsed > 1000 || elapsed < 0) {
		_vm->_video->retrace();
		if (_waitInterrupted)
			return false;
		started = menu ? g_system->getMillis() : now();
		return true;
	}
	const int32 interval = 1000 / MAX<int16>(1, _vm->_util->getFrameRate());
	int32 remaining = 0;
	do {
		if (remaining > 0) {
			const uint speed = menu ? 1 : _speed;
			g_system->delayMillis(MAX<int32>(1, MIN<int32>(remaining, 10) / speed));
		}
		_vm->_util->processInput();
		if (_waitInterrupted)
			return false;
		if (_limitActionCursor && !menu) {
			int16 x = 0, y = 0;
			_vm->_util->getMouseState(&x, &y, nullptr);
			int16 boundedX = x, boundedY = y;
			KeyboardInput::clampActionPointer(boundedX, boundedY);
			if (x != boundedX || y != boundedY) {
				_vm->_util->setMousePos(boundedX, boundedY);
				_vm->_util->processInput();
				if (_waitInterrupted)
					return false;
			}
		}
		if (forest && !menu)
			forest->pollControls();
		_vm->_video->retrace();
		if (_waitInterrupted)
			return false;
		current = menu ? g_system->getMillis() : now();
		remaining = interval - (int32)(current - started);
	} while (remaining > 0);
	// Preserve the native no-catch-up policy after a late frame.
	started = current;
	return true;
}

} // namespace OnceUpon
} // namespace Gob
