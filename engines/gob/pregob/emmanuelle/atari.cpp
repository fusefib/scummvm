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

#include "gob/dataio.h"
#include "gob/draw.h"
#include "gob/gob.h"
#include "gob/sound/sound.h"
#include "gob/util.h"
#include "gob/video.h"
#include "gob/pregob/emmanuelle/atari.h"

namespace Gob {
namespace Emmanuelle {

namespace {
const char *const kAtariSoundBank1[17] = {
	"P1.SPL", "S4.SPL", "S5.SPL", "S6.SPL", "S7.SPL", "S8.SPL",
	"S9.SPL", "S10.SPL", "S11.SPL", "S12.SPL", "S13.SPL",
	"S14.SPL", "S15.SPL", "S16.SPL", "S17.SPL", "S18.SPL",
	"S19.SPL"};
} // End of anonymous namespace

const char *const *PreGob_Emmanuelle::atariIntroSampleNames(byte &count) const {
	count = ARRAYSIZE(kAtariSoundBank1);
	return kAtariSoundBank1;
}


void PreGob_Emmanuelle::startAtariFightConclusionAudio() {
	if (_platformTag != kOldTotPlatformAtariST || _sampleCount <= 13)
		return;
	stopSampleAudio();
	_vm->_sound->blasterPlay(&_samples[13], 1, 0);
	setAudioState(kOldTotAudioAtariSample, 13, 0, 1,
		false, true);
}

void PreGob_Emmanuelle::serviceAtariFightFrame(
	const FightState &state) {
	if (_platformTag != kOldTotPlatformAtariST)
		return;

	const int16 leftSlot = fightSampleSlot(
		false, state.left.action, state.left.frame);
	const int16 rightSlot = fightSampleSlot(
		true, state.right.action, state.right.frame);
	const int16 slots[2] = {leftSlot, rightSlot};
	bool played = false;
	stopSampleAudio();
	for (uint i = 0; i < ARRAYSIZE(slots) && !_vm->shouldQuit(); ++i) {
		if (slots[i] < 0 || slots[i] >= _sampleCount)
			continue;
		_vm->_sound->blasterPlay(&_samples[slots[i]], 1, 0);
		setAudioState(kOldTotAudioAtariSample, slots[i], 0, 1,
							 true, true);
		waitAtariAudio();
		played = true;
	}
	if (!played)
		delay(100);
}

bool PreGob_Emmanuelle::handleAtariFF0B(byte command) {
	if (!atariStHandlesNativeCommand(command))
		return false;
	switch (command) {
	case 40:
		if (_scriptVM && _scriptVM->readVariable(0) == 10)
			_renderer.applyAtariPalette(3);
		_colorCycleEnabled = false;
		return true;
	case 41:
		_colorCycleEnabled = true;
		return true;
	case 42:
		_renderer.disarmAtariTransition();
		return true;
	case 43:
		servicePresentation();
		_renderer.armAtariTransition(43);
		return true;
	case 44:
		_renderer.armAtariTransition(44);
		return true;
	case 45: {
		uint16 words[4];
		const byte variant = (byte)randomBounded(3);
		if (getAtariStEffect45(variant, words))
			_renderer.applyAtariEffectWords(words);
		return true;
	}
	case 46:
		for (byte i = 0; i < 3 && !_vm->shouldQuit(); ++i) {
			_renderer.rotateAtariEffectWords();
			delay(250);
		}
		return true;
	case 47:
		_renderer.selectAtariPalette(4, false);
		fadeAtariToBlack();
		return true;
	case 80:
		stopSampleAudio();
		return true;
	case 81:
		if (_scriptVM && _scriptVM->readVariable(0) == 0 &&
			_scriptVM->readVariable(1) == 0)
			playAtariIntro();
		return true;
	case 82:
	case 83:
	case 85:
	case 86:
	case 89:
	case 90:
	case 91:
	case 92:
	case 94:
		return playAtariSampleCommand(command);
	case 201:
		if (_scriptVM && command201SkipsIntermediateNodes(
						  _platformTag, _scriptVM->readVariable(0)))
			_skipIntermediateSequenceNodes = true;
		return true;
	default:
		return false;
	}
}

void PreGob_Emmanuelle::fadeAtariFromBlack() {
	for (byte phase = 0; phase < 8 && !shouldAbortExecution(); ++phase) {
		if (!_renderer.presentAtariFadeFromBlackStep(phase))
			break;
		waitPresentationFrames();
	}
}

void PreGob_Emmanuelle::fadeAtariToBlack() {
	for (byte phase = 0; phase < 8 && !shouldAbortExecution(); ++phase) {
		if (!_renderer.presentAtariFadeToBlackStep(phase))
			break;
		waitPresentationFrames();
	}
}

bool PreGob_Emmanuelle::playAtariIntro() {
	if (_platformTag != kOldTotPlatformAtariST)
		return false;
	if (_introSkip.fastForwarding()) {
		stopSampleAudio();
		return true;
	}
	if (_silentAtariAudio)
		return true;
	if (_sampleCount <= 16)
		return false;

	stopSampleAudio();
	_atariIntroPosition = 0;
	serviceAtariAudio();
	while (!_vm->shouldQuit() &&
		   !_introSkip.fastForwarding() &&
		   (_vm->_sound->blasterPlayingSound() ||
			_atariIntroPosition >= 0)) {
		_vm->_util->delay(1);
		processGameEvents();
		int16 mouseX = 0;
		int16 mouseY = 0;
		MouseButtons mouseButtons = kMouseButtonsNone;
		const int16 key = takePendingInput(mouseX, mouseY, mouseButtons);
		if (key != kKeyNone) {
			stopSampleAudio();
			return true;
		}
		serviceAtariAudio();
	}
	if (_vm->shouldQuit() || _introSkip.fastForwarding())
		stopSampleAudio();
	_atariIntroPosition = -1;
	return true;
}

bool PreGob_Emmanuelle::playAtariSampleCommand(byte command) {
	AtariStSampleCommand sampleCommand;
	if (!getAtariStSampleCommand(command, sampleCommand))
		return false;
	if (_introSkip.fastForwarding()) {
		stopSampleAudio();
		return true;
	}
	if (_silentAtariAudio) {
		if (sampleCommand.postDelay)
			delay(sampleCommand.postDelay);
		return true;
	}
	if (sampleCommand.slot >= _sampleCount)
		return true;

	if (sampleCommand.stopMode == kAtariStAudioStopAlways ||
		(sampleCommand.stopMode == kAtariStAudioStopIfPlaying &&
		 _vm->_sound->blasterPlayingSound()))
		stopSampleAudio();
	_vm->_sound->blasterPlay(&_samples[sampleCommand.slot],
							 sampleCommand.mixerRepeatCount, 0);
	setAudioState(kOldTotAudioAtariSample, sampleCommand.slot, 0,
						 sampleCommand.mixerRepeatCount,
						 sampleCommand.waitForCompletion, true);
	if (sampleCommand.waitForCompletion)
		waitAtariAudio();
	if (_vm->shouldQuit())
		return true;
	if (sampleCommand.postDelay)
		delay(sampleCommand.postDelay);
	return true;
}

void PreGob_Emmanuelle::waitAtariAudio() {
	while (!_vm->shouldQuit() && !_introSkip.fastForwarding() &&
		   _vm->_sound->blasterPlayingSound()) {
		_vm->_util->delay(1);
		processGameEvents();
	}
	if (_introSkip.fastForwarding())
		stopSampleAudio();
	if (_audioKind == kOldTotAudioAtariSample && _audioActive)
		setAudioState(kOldTotAudioAtariSample, _audioSlot, 0,
						   _audioRepeatCount, _audioBlocking, false);
}

void PreGob_Emmanuelle::serviceAtariAudio() {
	if (_platformTag != kOldTotPlatformAtariST ||
		_vm->_sound->blasterPlayingSound())
		return;
	if (_audioKind == kOldTotAudioAtariSample && _audioActive)
		setAudioState(kOldTotAudioAtariSample, _audioSlot, 0,
						   _audioRepeatCount, _audioBlocking, false);
	if (_atariIntroPosition < 0)
		return;
	uint count = 0;
	const byte *slots = getAtariStIntroComposition(count);
	if ((uint)_atariIntroPosition >= count) {
		_atariIntroPosition = -1;
		return;
	}

	const byte slot = slots[_atariIntroPosition++];
	if (slot < _sampleCount) {
		_vm->_sound->blasterPlay(&_samples[slot], 1, 0);
		setAudioState(kOldTotAudioAtariSample, slot, 0, 1,
						   false, true);
	}
}

void PreGob_Emmanuelle::delayAtariAnimation(uint16 units) {
	while (units && !shouldAbortExecution()) {
		const uint16 chunk = atariStAnimationDelayChunk(units);
		delay(chunk);
		if (shouldAbortExecution())
			break;
		_colorCycleCounter = 5;
		serviceColorCycle();
		units -= chunk;
	}
}

void PreGob_Emmanuelle::waitAtariInputRelease() {
	int16 mouseX = 0;
	int16 mouseY = 0;
	MouseButtons mouseButtons = kMouseButtonsNone;
	int16 key = kKeyNone;
	do {
		key = pollGameInput(mouseX, mouseY, mouseButtons);
		if (key != kKeyNone)
			delay(20);
	} while (!shouldAbortExecution() && !_introSkip.fastForwarding() &&
			 key != kKeyNone);
}

void PreGob_Emmanuelle::waitAtariContinuation() {
	if (!servicePresentation())
		presentFrame(true);
	if (shouldAbortExecution())
		return;
	fadeAtariFromBlack();
	if (_vm->_sound && _vm->_sound->blasterPlayingSound())
		stopSampleAudio();
	_pendingInput.clear();
	_vm->_util->clearKeyBuf();
	int16 mouseX = 0;
	int16 mouseY = 0;
	MouseButtons mouseButtons = kMouseButtonsNone;
	int16 key = kKeyNone;
	do {
		delay(20);
		key = pollGameInput(mouseX, mouseY, mouseButtons);
	} while (!shouldAbortExecution() && !_introSkip.fastForwarding() &&
			 !atariStContinuationAcceptsKey(key));
}

bool PreGob_Emmanuelle::hasAnyAtariSoundFile() const {
	for (byte bank = 1; bank <= 2; ++bank) {
		byte count;
		const char *const *names = sampleBankNames(bank, count);
		for (byte i = 0; i < count; ++i) {
			if (_vm->_dataIO->hasFile(names[i]))
				return true;
		}
	}
	return false;
}

bool OldTotRenderer::presentAtariMode44Step(byte variant, byte phase) {
	if (_platform != kOldTotPlatformAtariST || !_atariTransitionTarget ||
		!_vm || !_vm->_draw || !_vm->_draw->_backSurface)
		return false;
	AtariStMode44Rect rect;
	if (!getAtariStMode44Rect(variant, phase, rect))
		return false;
	copyPresentationRect(*_atariTransitionTarget, rect.x, rect.y,
		rect.x + rect.width - 1, rect.y + rect.height - 1);
	if (phase + 1 == atariStMode44PhaseCount(variant))
		finishAtariTransitionFrame();
	return true;
}

bool OldTotRenderer::presentAtariMode43Step(byte phase) {
	if (_platform != kOldTotPlatformAtariST || phase >= 16 ||
		!_atariTransitionTarget || !_vm || !_vm->_draw ||
		!_vm->_draw->_backSurface)
		return false;
	if (!_dirtyValid) {
		finishAtariTransitionFrame();
		return true;
	}

	Surface &back = *_vm->_draw->_backSurface;
	const int16 left = (_dirtyLeft / 16) * 16;
	int16 right = ((_dirtyRight / 16) + 1) * 16 - 1;
	right = MIN<int16>(right, back.getWidth() - 1);
	uint32 ordinal = 0;
	for (int16 y = _dirtyTop; y <= _dirtyBottom; ++y) {
		for (int16 wordX = left; wordX <= right;
			 wordX += 16, ++ordinal) {
			const uint16 mask = atariStMode43Mask(phase, ordinal);
			for (byte bit = 0; bit < 16; ++bit) {
				const int16 x = wordX + bit;
				if (x <= right && x < (int16)back.getWidth() &&
					(mask & (0x8000U >> bit)))
					back.putPixelRaw(x, y,
						_atariTransitionTarget->get(x, y).get());
			}
		}
	}
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, left,
		_dirtyTop, right, _dirtyBottom);
	if (phase == 15)
		finishAtariTransitionFrame();
	return true;
}

void OldTotRenderer::finishAtariTransitionFrame() {
	if (!_atariTransitionTarget || !_vm || !_vm->_draw ||
		!_vm->_draw->_backSurface)
		return;
	_vm->_draw->_backSurface->blit(*_atariTransitionTarget, 0, 0);
	_atariPresentedSurface = _atariTransitionTarget;
	_atariTransitionTarget.reset();
	clearDirtyRect();
}

bool OldTotRenderer::beginAtariTransitionFrame() {
	if (_platform != kOldTotPlatformAtariST || !_vm || !_vm->_draw ||
		!_vm->_draw->_backSurface)
		return false;
	Surface &back = *_vm->_draw->_backSurface;
	_atariTransitionTarget = clonePresentationFrame(back);
	if (!_atariPresentedSurface ||
		_atariPresentedSurface->getWidth() != back.getWidth() ||
		_atariPresentedSurface->getHeight() != back.getHeight())
		_atariPresentedSurface = clonePresentationFrame(back);
	back.blit(*_atariPresentedSurface, 0, 0);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 0, 0,
		back.getWidth() - 1, back.getHeight() - 1);
	return true;
}

void OldTotRenderer::resetAtariModuleState() {
	_presentation.resetModule();
	_atariTransitionTarget.reset();
	clearDirtyRect();
}

void OldTotRenderer::disarmAtariTransition() {
	_presentation.disarm();
	_atariTransitionTarget.reset();
	clearDirtyRect();
}

void OldTotRenderer::armAtariTransition(byte mode) {
	if (_platform != kOldTotPlatformAtariST ||
		(mode != 43 && mode != 44))
		return;
	if (!_atariPresentedSurface)
		captureAtariPresentedFrame();
	_presentation.arm(mode);
	_atariTransitionTarget.reset();
	clearDirtyRect();
}

void OldTotRenderer::captureAtariPresentedFrame() {
	if (_platform != kOldTotPlatformAtariST || !_vm || !_vm->_draw ||
		!_vm->_draw->_backSurface)
		return;
	_atariPresentedSurface = clonePresentationFrame(
		*_vm->_draw->_backSurface);
}

bool OldTotRenderer::restoreAtariPaletteImmediately() {
	if (!_atariPaletteValid)
		return false;
	showAtariWords(_atariLogicalPalette);
	_atariBlack = false;
	return true;
}

bool OldTotRenderer::presentAtariFadeFromBlackStep(byte phase) {
	if (!_atariPaletteValid || !_atariBlack || phase >= 8)
		return false;
	uint16 work[16];
	const byte dim = 8 - phase;
	for (uint i = 0; i < 16; ++i) {
		work[i] = dimAtariStPaletteWord(
			_atariLogicalPalette[i], dim);
	}
	showAtariWords(work);
	if (phase == 7) {
		showAtariWords(_atariLogicalPalette);
		_atariBlack = false;
	}
	return true;
}

bool OldTotRenderer::presentAtariFadeToBlackStep(byte phase) {
	if (!_atariPaletteValid || _atariBlack || phase >= 8)
		return false;
	uint16 work[16];
	for (uint i = 0; i < 16; ++i)
		work[i] = _atariLogicalPalette[i];
	for (byte step = 0; step <= phase; ++step) {
		for (uint i = 0; i < 16; ++i)
			work[i] = dimAtariStPaletteWord(work[i], 1);
	}
	showAtariWords(work);
	if (phase == 7)
		_atariBlack = true;
	return true;
}

bool OldTotRenderer::rotateAtariColorCycleWords() {
	if (!_atariPaletteValid)
		return false;
	const uint16 old1 = _atariLogicalPalette[1];
	const uint16 old6 = _atariLogicalPalette[6];
	const uint16 old11 = _atariLogicalPalette[11];
	_atariLogicalPalette[1] = old11;
	_atariLogicalPalette[6] = old1;
	_atariLogicalPalette[11] = old6;
	showAtariWords(_atariLogicalPalette);
	return true;
}

bool OldTotRenderer::rotateAtariEffectWords() {
	if (!_atariPaletteValid)
		return false;
	const uint16 old11 = _atariLogicalPalette[11];
	_atariLogicalPalette[11] = _atariLogicalPalette[12];
	_atariLogicalPalette[12] = _atariLogicalPalette[13];
	_atariLogicalPalette[13] = _atariLogicalPalette[15];
	_atariLogicalPalette[15] = old11;
	showAtariWords(_atariLogicalPalette);
	return true;
}

bool OldTotRenderer::applyAtariEffectWords(const uint16 words[4]) {
	if (!_atariPaletteValid || !words)
		return false;
	_atariLogicalPalette[11] = words[0];
	_atariLogicalPalette[12] = words[1];
	_atariLogicalPalette[13] = words[2];
	_atariLogicalPalette[15] = words[3];
	showAtariWords(_atariLogicalPalette);
	return true;
}

bool OldTotRenderer::applyAtariPalette(byte paletteNumber) {
	return selectAtariPalette(paletteNumber, true);
}

bool OldTotRenderer::selectAtariPalette(byte paletteNumber,
										bool makeVisible) {
	uint16 words[16];
	if (!getAtariStNumberedPalette(paletteNumber, words))
		return false;
	return setAtariWords(words, makeVisible);
}

bool OldTotRenderer::applyAtariModulePalette(
	const Common::String &moduleName) {
	uint16 words[16];
	if (!getAtariStModulePalette(moduleName, words))
		return false;
	return setAtariWords(words, !_atariBlack);
}

bool OldTotRenderer::atariModulePaletteDiffers(
	const Common::String &moduleName) const {
	uint16 words[16];
	if (!_atariPaletteValid || !getAtariStModulePalette(moduleName, words))
		return false;
	for (uint i = 0; i < ARRAYSIZE(_atariLogicalPalette); ++i) {
		if (_atariLogicalPalette[i] != words[i])
			return true;
	}
	return false;
}

bool OldTotRenderer::setAtariWords(const uint16 words[16],
								   bool makeVisible) {
	if (!words)
		return false;
	for (uint i = 0; i < 16; ++i)
		_atariLogicalPalette[i] = words[i];
	_atariPaletteValid = true;
	if (makeVisible)
		showAtariWords(_atariLogicalPalette);
	return true;
}

void OldTotRenderer::showAtariWords(const uint16 words[16]) {
	if (!_vm || !_vm->_draw || !_vm->_video || !words)
		return;

	for (uint i = 0; i < 16; ++i) {
		_vm->_draw->_vgaPalette[i].red =
			atariStPaletteComponent(words[i], 8);
		_vm->_draw->_vgaPalette[i].green =
			atariStPaletteComponent(words[i], 4);
		_vm->_draw->_vgaPalette[i].blue =
			atariStPaletteComponent(words[i], 0);
	}
	_vm->_video->setPalette(_vm->_draw->_vgaPalette);
}

} // End of namespace Emmanuelle
} // End of namespace Gob
