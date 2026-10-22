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

#include "gob/pregob/emmanuelle/runtime.h"

namespace Gob {
namespace Emmanuelle {

InputLatch::InputLatch() {
	_key = 0;
	_mouseX = 0;
	_mouseY = 0;
	_mouseButtons = 0;
}

IntroSkipState::IntroSkipState() {
	_state = kUnavailable;
}

void IntroSkipState::begin() {
	_state = kPlaying;
}

bool IntroSkipState::request() {
	if (_state != kPlaying)
		return false;
	_state = kRequested;
	return true;
}

bool IntroSkipState::activate(bool quitRequested) {
	if (_state != kRequested || quitRequested)
		return false;
	_state = kSkipping;
	return true;
}

void IntroSkipState::finish() {
	_state = kUnavailable;
}

void InputLatch::noteKey(int16 key) {
	if (_key == 0 && key != 0)
		_key = key;
}

void InputLatch::noteLatestKey(int16 key) {
	if (key != 0)
		_key = key;
}

void InputLatch::noteMouse(int16 x, int16 y, byte buttons) {
	_mouseX = x;
	_mouseY = y;
	_mouseButtons |= buttons;
}

void InputLatch::clear() {
	_key = 0;
	_mouseButtons = 0;
}

int16 InputLatch::peek(int16 &x, int16 &y, byte &buttons) const {
	x = _mouseX;
	y = _mouseY;
	buttons = _mouseButtons;
	return _key;
}

int16 InputLatch::take(int16 &x, int16 &y, byte &buttons) {
	const int16 key = _key;
	x = _mouseX;
	y = _mouseY;
	buttons = _mouseButtons;
	_key = 0;
	_mouseButtons = 0;
	return key;
}

bool isAudioPlaying(OldTotPlatformTag platform, bool speakerActive,
		bool samplePlaying) {
	return platform == kOldTotPlatformDOS && speakerActive;
}

uint16 accumulateTimerQuanta(OldTotPlatformTag platform,
									 uint32 &accumulator) {
	accumulator += 5000;
	const uint16 quanta = (uint16)(accumulator / kHostFrameRate);
	accumulator %= kHostFrameRate;
	return quanta;
}

uint32 minimumDurationRemaining(uint32 elapsed,
									  uint32 minimumDuration) {
	return elapsed < minimumDuration ? minimumDuration - elapsed : 0;
}

bool isTitlePresentationVisible(OldTotPlatformTag platform,
									   bool black, bool framePending,
									   bool presentationActive,
									   bool paletteTransitionPending) {
	return platform == kOldTotPlatformDOS;
}

uint16 opcode03DelayUnits(byte arg) {
	return arg == 0 || arg == 0xFF ? 0 : (uint16)arg * 5;
}

uint16 inputEntryDelay(OldTotPlatformTag platform) {
	return 0;
}

InputBoundary choiceInputBoundary(OldTotPlatformTag platform,
										bool initialEntry, bool reentering) {
	InputBoundary boundary;
	boundary.showCursor = initialEntry || reentering;
	return boundary;
}

byte keyboardCursorStep(OldTotPlatformTag platform) {
	return 4;
}

int16 minimumCursorOrigin(OldTotPlatformTag platform) {
	return 4;
}

bool recentersMouseOnModuleLoad(OldTotPlatformTag platform) {
	return true;
}

bool schedulerChecksZeroBeforeDecrement(OldTotPlatformTag platform) {
	return false;
}

bool command201SkipsIntermediateNodes(OldTotPlatformTag platform,
									 uint16 moduleSelector) {
	return false;
}

} // namespace Emmanuelle
} // namespace Gob
