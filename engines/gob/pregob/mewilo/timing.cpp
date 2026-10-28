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

#include "common/system.h"
#include "common/util.h"
#include "gob/gob.h"
#include "gob/global.h"
#include "gob/util.h"
#include "gob/pregob/mewilo/mewilo.h"

namespace Gob {
namespace Mewilo {

void PreGob_Mewilo::pauseChanged(bool paused) {
	if (paused) {
		if (_pauseDepth++ == 0)
			_pauseStart = g_system->getMillis();
	} else if (_pauseDepth && --_pauseDepth == 0) {
		_pausedMillis += g_system->getMillis() - _pauseStart;
		// A click used to close a modal must not become a game choice.
		_mousePresses.clear();
		_pendingKey = 0;
	}
}

uint32 PreGob_Mewilo::activeMillis() const {
	const uint32 now = _pauseDepth ? _pauseStart : g_system->getMillis();
	return now - _pausedMillis;
}

void PreGob_Mewilo::serviceEvents() {
	// No waitEndFrame here: a synchronous NOTE or short FF03 wait must not
	// acquire an additional 15-fps delay. Leave buffered keys for waitInput().
	_vm->_util->processInput();
	presentFrame();
	if (!shouldQuit() && !hasError())
		g_system->delayMillis(1);
}

void PreGob_Mewilo::waitMilliseconds(uint32 durationMs) {
	// Active time excludes modal pauses, just as mixer playback does. Rendering
	// and event processing still count, and subtraction is wrap-safe.
	const uint32 start = activeMillis();
	while (!shouldQuit() && !hasError()) {
		serviceEvents();
		const uint32 elapsed = activeMillis() - start;
		if (shouldQuit() || hasError() || elapsed >= durationMs)
			break;
		g_system->delayMillis(MIN<uint32>(10, durationMs - elapsed));
	}
}

bool PreGob_Mewilo::waitContinueInput() {
	MewiloInputEvent event;
	return waitInput(event);
}

bool PreGob_Mewilo::waitInputRelease() {
	int16 x, y; MouseButtons buttons;
	while (!shouldQuit() && !hasError()) {
		_vm->_util->getMouseState(&x, &y, &buttons);
		if (buttons == kMouseButtonsNone) return true;
		serviceEvents();
	}
	return false;
}

void PreGob_Mewilo::waitOrDelay(byte argument) {
	if (!argument) {
		if (waitContinueInput())
			waitInputRelease();
		return;
	}
	// Retain the explicit Atari compatibility estimate until its CPU loop is
	// calibrated. Other platforms must supply their own native conversion.
	const uint speed = MAX<int16>(1, _vm->_global->_speedFactor);
	bool calibrated;
	const uint32 nativeMs = mewiloNativeDelayMillis(_profile.platform, argument, calibrated);
	waitMilliseconds((nativeMs + speed - 1) / speed);
}

} // End of namespace Mewilo
} // End of namespace Gob
