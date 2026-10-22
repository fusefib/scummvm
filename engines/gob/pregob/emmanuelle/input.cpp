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

#include "gob/pregob/emmanuelle/emmanuelle.h"

#include "backends/keymapper/keymapper.h"
#include "common/events.h"
#include "graphics/cursorman.h"
#include "gob/gob.h"
#include "gob/sound/sound.h"
#include "gob/util.h"
#include "gob/pregob/emmanuelle/dos.h"

namespace Gob {
namespace Emmanuelle {

void PreGob_Emmanuelle::processGameEvents() {
	_vm->_util->processInput();

	int16 key = kKeyNone;
	while (_vm->_util->checkKey(key)) {
		if (_inputContext == kEmmanuelleInputFight &&
			_platformTag == kOldTotPlatformDOS)
			_pendingInput.noteLatestKey(key);
		else
			_pendingInput.noteKey(key);
	}

	int16 mouseX = 0;
	int16 mouseY = 0;
	MouseButtons mouseButtons = kMouseButtonsNone;
	_vm->_util->getMouseState(&mouseX, &mouseY, &mouseButtons);
	_pendingInput.noteMouse(mouseX, mouseY, (byte)mouseButtons);
	if (mouseButtons != kMouseButtonsNone)
		_vm->_util->forceMouseUp();
	if (_introSkip.activate(_vm->shouldQuit()))
		beginIntroFastForward();
}

int16 PreGob_Emmanuelle::takePendingInput(
	int16 &mouseX, int16 &mouseY, MouseButtons &mouseButtons) {
	byte buttons = kMouseButtonsNone;
	const int16 key = _pendingInput.take(mouseX, mouseY, buttons);
	mouseButtons = (MouseButtons)buttons;
	return key;
}

int16 PreGob_Emmanuelle::pollGameInput(int16 &mouseX, int16 &mouseY,
										 MouseButtons &mouseButtons) {
	presentFrame(true);
	processGameEvents();
	return takePendingInput(mouseX, mouseY, mouseButtons);
}

int16 PreGob_Emmanuelle::pollInterpreterInput(
	int16 &mouseX, int16 &mouseY, MouseButtons &mouseButtons) {

	const int16 key = pollGameInput(mouseX, mouseY, mouseButtons);
	serviceInterpreterTimer();
	return key;
}

bool PreGob_Emmanuelle::inputIdle(int16 key,
										MouseButtons mouseButtons) const {
	return key == kKeyNone && mouseButtons == kMouseButtonsNone;
}

void PreGob_Emmanuelle::discardPendingInput() {
	_pendingInput.clear();
	_vm->_util->clearKeyBuf();
	_vm->_util->forceMouseUp();
}

void PreGob_Emmanuelle::applyInputBoundary(
	const InputBoundary &boundary) {
	if (boundary.nativeDelayUnits) {
		const uint16 delayMilliseconds =
			boundary.nativeDelayUnits;
		delay(delayMilliseconds);
	}
	if (boundary.discardStale)
		discardPendingInput();

	if (boundary.showCursor)
		showCursor();
}

int16 PreGob_Emmanuelle::chooseInputRegion(uint16 nodeOffset, byte count,
											bool initialEntry, bool reentering) {
	if (!_tot || uint32(nodeOffset) + 6U + 4U * uint32(count) >
							 _tot->size())
		return -1;
	// INIT's first choice is copy protection. End intro skipping before
	// accepting input so the skip action cannot select an answer.
	if (_introSkip.available())
		finishIntro();

	setInputContext(kEmmanuelleInputChoice);
	applyInputBoundary(choiceInputBoundary(
		_platformTag, initialEntry, reentering));
	int16 mouseX = 0;
	int16 mouseY = 0;
	MouseButtons mouseButtons = kMouseButtonsNone;
	// Atari traite_souris_ stops an active sample on every input pass.

	_mainMenuActionThisPoll = false;
	const int16 key = pollInterpreterInput(mouseX, mouseY, mouseButtons);
	_mainMenuActionThisPoll = false;
	if (_vm->shouldQuit()) {

		_openMainMenuAfterSuspend = false;
		setInputContext(kEmmanuelleInputNone);
		return -3;
	}
	const int16 cursorMinimum = minimumCursorOrigin(_platformTag);
	int16 cursorX = CLIP<int16>(mouseX, cursorMinimum, 316);
	int16 cursorY = CLIP<int16>(mouseY, cursorMinimum, 196);
	if (cursorX != mouseX || cursorY != mouseY)
		_vm->_util->setMousePos(cursorX, cursorY);

	if (key == kKeyEscape || mouseButtons == kMouseButtonsRight) {
		if (_diagnosticTraceEnabled)
			debugC(3, kDebugGameFlow,
				"EMMA_TRACE event=input_cancel module=%s node=0x%04X",
				_moduleFileStem.c_str(), nodeOffset);
		hideCursor();
		setInputContext(kEmmanuelleInputNone);
		return -2;
	}
	const byte cursorStep = keyboardCursorStep(_platformTag);
	if (key == kKeyLeft) {
		cursorX = cursorX >= cursorMinimum + cursorStep
			? cursorX - cursorStep : cursorMinimum;
		_vm->_util->setMousePos(cursorX, cursorY);
		setInputContext(kEmmanuelleInputNone);
		return -3;
	}
	if (key == kKeyRight) {
		cursorX = cursorX <= 316 - cursorStep
			? cursorX + cursorStep : 316;
		_vm->_util->setMousePos(cursorX, cursorY);
		setInputContext(kEmmanuelleInputNone);
		return -3;
	}
	if (key == kKeyUp) {
		cursorY = cursorY >= cursorMinimum + cursorStep
			? cursorY - cursorStep : cursorMinimum;
		_vm->_util->setMousePos(cursorX, cursorY);
		setInputContext(kEmmanuelleInputNone);
		return -3;
	}
	if (key == kKeyDown) {
		cursorY = cursorY <= 196 - cursorStep
			? cursorY + cursorStep : 196;
		_vm->_util->setMousePos(cursorX, cursorY);
		setInputContext(kEmmanuelleInputNone);
		return -3;
	}

	const bool selected = key == kKeyReturn ||
						  mouseButtons == kMouseButtonsLeft;
	if (!selected) {
		setInputContext(kEmmanuelleInputNone);
		return -3;
	}
	hideCursor();

	const int16 region = hitTestOldTotInputRegions(
		*_tot, nodeOffset, count, cursorX, cursorY,
		1,
		8);
	if (_diagnosticTraceEnabled)
		debugC(3, kDebugGameFlow,
			   "EMMA_TRACE event=input_region module=%s node=0x%04X count=%u x=%d y=%d region=%d",
			   _moduleFileStem.c_str(), nodeOffset, (uint)count, (int)cursorX,
			   (int)cursorY, (int)region);
	setInputContext(kEmmanuelleInputNone);
	return region;
}

void PreGob_Emmanuelle::waitReturn() {
	setInputContext(kEmmanuelleInputContinue);
	int16 mouseX = 0;
	int16 mouseY = 0;
	MouseButtons mouseButtons = kMouseButtonsNone;
	int16 key = kKeyNone;
	do {
		key = pollInterpreterInput(mouseX, mouseY, mouseButtons);
	} while (!_vm->shouldQuit() && !_introSkip.fastForwarding() &&
			 !inputIdle(key, mouseButtons));
	do {
		key = pollInterpreterInput(mouseX, mouseY, mouseButtons);
	} while (!_vm->shouldQuit() && !_introSkip.fastForwarding() &&
			 key != kKeyReturn);
	do {
		key = pollInterpreterInput(mouseX, mouseY, mouseButtons);
	} while (!_vm->shouldQuit() && !_introSkip.fastForwarding() &&
			 !inputIdle(key, mouseButtons));
	setInputContext(kEmmanuelleInputNone);
}

void PreGob_Emmanuelle::waitInputRelease() {
	int16 mouseX = 0;
	int16 mouseY = 0;
	MouseButtons mouseButtons = kMouseButtonsNone;
	int16 key = kKeyNone;
	delay(200);
	do {
		key = pollInterpreterInput(mouseX, mouseY, mouseButtons);
		if (!inputIdle(key, mouseButtons))
			delay(40);
	} while (!_vm->shouldQuit() && !_introSkip.fastForwarding() &&
			 !inputIdle(key, mouseButtons));
}

void PreGob_Emmanuelle::waitOpcode03(byte arg) {
	if (_introSkip.fastForwarding()) {

		processGameEvents();
		return;
	}

	if (arg == 0) {
		waitReturn();
		return;
	}
	if (arg == 0xFF) {
		waitInputRelease();
		return;
	}

	presentFrame(true);
	processGameEvents();
	if (!_vm->shouldQuit())
		delay(dosOpcode03DelayMilliseconds(arg));
	return;
}

void PreGob_Emmanuelle::delay(uint16 milliseconds) {
	while (milliseconds && !_vm->shouldQuit()) {
		if (_introSkip.fastForwarding()) {
			processGameEvents();
			return;
		}
		const uint16 slice = milliseconds < 20 ? milliseconds : 20;
		_vm->_util->delay(slice);
		processGameEvents();

		milliseconds -= slice;
	}
}

} // End of namespace Emmanuelle
} // End of namespace Gob
