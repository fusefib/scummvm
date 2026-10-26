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
#include "gob/pregob/emmanuelle/amiga.h"
#include "gob/pregob/emmanuelle/atari.h"
#include "gob/pregob/emmanuelle/dos.h"

namespace Gob {
namespace Emmanuelle {

void PreGob_Emmanuelle::setInputContext(EmmanuelleInputContext context) {
	Common::Keymapper *keymapper = _vm->getEventManager()->getKeymapper();
	if (!keymapper)
		return;
	const EmmanuelleInputContext effectiveContext =
		_introSkip.available() ? kEmmanuelleInputIntro : context;
	for (int value = kEmmanuelleInputIntro;
		 value <= kEmmanuelleInputFight; ++value) {
		const EmmanuelleInputContext candidate =
			(EmmanuelleInputContext)value;
		Common::Keymap *keymap = keymapper->getKeymap(
			emmanuelleInputKeymapId(candidate));
		if (keymap)
			keymap->setEnabled(candidate == effectiveContext);
	}
	_inputContext = effectiveContext;
}

void PreGob_Emmanuelle::handleCustomAction(
		Common::CustomEventType action) {
	if (action == (Common::CustomEventType)kEmmanuelleActionOpenMainMenu) {
		const EmmanuelleSaveBoundaryState state = saveBoundaryState();
		_mainMenuActionThisPoll = state.canHandoffMenu();
		if (_diagnosticTraceEnabled && !_mainMenuActionThisPoll)
			debugC(2, kDebugSaveLoad,
				   "EMMA_TRACE event=menu_request result=ignored module=%s "
				   "active_input=%u node=0x%04X suspended=%u blocker=%s",
				   _moduleFileStem.c_str(), state.activeInput ? 1U : 0U,
				   state.activeInputNode, state.suspendedWait ? 1U : 0U,
				   emmanuelleSaveBoundaryBlockerName(
					   state.blocker()));
		return;
	}
	if (action == (Common::CustomEventType)kEmmanuelleActionSkipIntro)
		_introSkip.request();
}

void PreGob_Emmanuelle::processGameEvents() {
	_vm->_util->processInput();
	if (_platformTag == kOldTotPlatformAmiga &&
		_audioKind == kOldTotAudioAmigaSample && _audioActive &&
		!_vm->_sound->blasterPlayingSound())
		setAudioState(kOldTotAudioAmigaSample, _audioSlot, 0,
			_audioRepeatCount, false, false);

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
	if (shouldAbortExecution())
		return kKeyNone;
	serviceAtariAudio();
	const bool paced = _platformTag == kOldTotPlatformAtariST &&
		servicePresentation();
	if (!paced)
		presentFrame(true);
	if (_hostFailed)
		return kKeyNone;
	processGameEvents();
	return takePendingInput(mouseX, mouseY, mouseButtons);
}

int16 PreGob_Emmanuelle::pollInterpreterInput(
	int16 &mouseX, int16 &mouseY, MouseButtons &mouseButtons, bool inFight) {
	const bool atari = _platformTag == kOldTotPlatformAtariST;
	if (shouldAbortExecution())
		return kKeyNone;
	processGameEvents();
	if (shouldAbortExecution()) {
		_schedulerDebt = 0;
		return kKeyNone;
	}

	serviceAtariAudio();
	serviceColorCycle();
	serviceInterpreterTimer();

	const bool paced = servicePresentation();
	// Amiga fights perform their own frame pacing.
	if ((_platformTag != kOldTotPlatformAmiga || !inFight) &&
		!paced && !shouldAbortExecution() && !_introSkip.fastForwarding())
		presentFrame(true);
	if (atari && !shouldAbortExecution())
		fadeAtariFromBlack();
	if (shouldAbortExecution()) {
		_schedulerDebt = 0;
		return kKeyNone;
	}

	return takePendingInput(mouseX, mouseY, mouseButtons);
}

void PreGob_Emmanuelle::serviceInterpreterTimer() {
	if (!_scriptVM || shouldAbortExecution()) {
		_schedulerDebt = 0;
		return;
	}

	_schedulerDebt += accumulateTimerQuanta(
		_platformTag, _schedulerAccumulator);
	_schedulerDebt = _scriptVM->advanceTimer(_schedulerDebt, 8);
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
			_platformTag == kOldTotPlatformAmiga
				? amigaNativeDelayMilliseconds(boundary.nativeDelayUnits)
				: boundary.nativeDelayUnits;
		delay(delayMilliseconds);
	}
	if (boundary.discardStale)
		discardPendingInput();
	if (_platformTag == kOldTotPlatformAtariST && boundary.discardStale) {
		int16 initialX = 0;
		int16 initialY = 0;
		_vm->_util->getMouseState(&initialX, &initialY, nullptr);
		initialX = CLIP<int16>(initialX, 0, 304);
		initialY = CLIP<int16>(initialY, 0, 184);
		_vm->_util->setMousePos(initialX, initialY);
	}
	if (boundary.showCursor)
		showCursor();
}

int16 PreGob_Emmanuelle::chooseInputRegion(uint16 nodeOffset, byte count,
											bool initialEntry, bool reentering) {
	if (!_tot || uint32(nodeOffset) + 6U + 4U * uint32(count) >
							 _tot->size()) {
		failHostOperation("input regions", nodeOffset);
		return -1;
	}
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
	if (_platformTag == kOldTotPlatformAtariST &&
		_vm->_sound && _vm->_sound->blasterPlayingSound())
		stopSampleAudio();
	_mainMenuActionThisPoll = false;
	const int16 key = pollInterpreterInput(mouseX, mouseY, mouseButtons);
	const bool menuAction = _mainMenuActionThisPoll;
	_mainMenuActionThisPoll = false;
	if (shouldAbortExecution()) {
		if (menuAction && _diagnosticTraceEnabled)
			debugC(2, kDebugSaveLoad,
				   "EMMA_TRACE event=menu_request result=ignored reason=quit "
				   "module=%s",
				   _moduleFileStem.c_str());
		_openMainMenuAfterSuspend = false;
		setInputContext(kEmmanuelleInputNone);
		return -3;
	}
	const EmmanuelleSaveBoundaryState boundary = saveBoundaryState();
	if (menuAction && boundary.canHandoffMenu() &&
		boundary.activeInputNode == nodeOffset) {
		_openMainMenuAfterSuspend = true;
		if (_diagnosticTraceEnabled)
			debugC(2, kDebugSaveLoad,
				   "EMMA_TRACE event=menu_request result=accepted module=%s "
				   "node=0x%04X",
				   _moduleFileStem.c_str(), nodeOffset);
		setInputContext(kEmmanuelleInputNone);
		return -3;
	}
	if (menuAction && _diagnosticTraceEnabled)
		debugC(2, kDebugSaveLoad,
			   "EMMA_TRACE event=menu_request result=ignored module=%s "
			   "node=0x%04X blocker=%s",
			   _moduleFileStem.c_str(), nodeOffset,
			   emmanuelleSaveBoundaryBlockerName(boundary.blocker()));
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
		_alternateInputGeometry ? 0 : 1,
		_alternateInputGeometry ? 6 : 8);
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
	} while (!shouldAbortExecution() && !_introSkip.fastForwarding() &&
			 !inputIdle(key, mouseButtons));
	do {
		key = pollInterpreterInput(mouseX, mouseY, mouseButtons);
	} while (!shouldAbortExecution() && !_introSkip.fastForwarding() &&
			 key != kKeyReturn);
	do {
		key = pollInterpreterInput(mouseX, mouseY, mouseButtons);
	} while (!shouldAbortExecution() && !_introSkip.fastForwarding() &&
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
	} while (!shouldAbortExecution() && !_introSkip.fastForwarding() &&
			 !inputIdle(key, mouseButtons));
}

void PreGob_Emmanuelle::waitOpcode03(byte arg, uint16) {
	if (_hostFailed)
		return;
	if (_introSkip.fastForwarding()) {
		servicePresentation();
		processGameEvents();
		return;
	}
	if (arg == 0) {
		if (_platformTag == kOldTotPlatformAmiga ||
			_platformTag == kOldTotPlatformDOS)
			waitReturn();
		else
			waitAtariContinuation();
		return;
	}
	if (arg == 0xFF) {
		if (_platformTag == kOldTotPlatformAmiga)
			waitAmigaInputRelease();
		else if (_platformTag == kOldTotPlatformDOS)
			waitInputRelease();
		else
			waitAtariInputRelease();
		return;
	}
	// Gameplay waits count presentation toward an elapsed target.
	// INIT retains its native requested sleep.
	const uint32 presentationStart = _currentModule != kModuleInit ? g_system->getMillis() : 0;
	if (!servicePresentation())
		presentFrame(true);
	if (shouldAbortExecution())
		return;
	if (_platformTag == kOldTotPlatformAtariST)
		fadeAtariFromBlack();
	noteInitialTomahawkVisible();
	serviceWaitColorCycle();
	if (_currentModule != kModuleInit) {
		waitForMinimumDuration(presentationStart, 100 + (arg / 5) * 20);
		return;
	}
	uint16 milliseconds = opcode03DelayUnits(arg);
	if (_platformTag == kOldTotPlatformDOS)
		milliseconds = dosOpcode03DelayMilliseconds(arg);
	else if (_platformTag == kOldTotPlatformAmiga)
		milliseconds = amigaNativeDelayMilliseconds(milliseconds);
	delay(milliseconds);
}

void PreGob_Emmanuelle::delay(uint16 milliseconds) {
	while (milliseconds && !shouldAbortExecution()) {
		if (_introSkip.fastForwarding()) {
			processGameEvents();
			return;
		}
		const uint16 slice = milliseconds < 20 ? milliseconds : 20;
		_vm->_util->delay(slice);
		processGameEvents();
		serviceAtariAudio();
		milliseconds -= slice;
	}
}

void PreGob_Emmanuelle::waitPresentationFrames(byte count) {
	for (byte i = 0; i < count && !shouldAbortExecution(); ++i) {
		if (!_introSkip.fastForwarding())
			_vm->_util->waitEndFrame(false);
		processGameEvents();
	}
}

} // End of namespace Emmanuelle
} // End of namespace Gob
