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

const char *const *PreGob_Emmanuelle::atariIntroSampleNames(byte &count) const {
	static const char *const names[] = {
		"P1.SPL", "S4.SPL", "S5.SPL", "S6.SPL", "S7.SPL", "S8.SPL",
		"S9.SPL", "S10.SPL", "S11.SPL", "S12.SPL", "S13.SPL",
		"S14.SPL", "S15.SPL", "S16.SPL", "S17.SPL", "S18.SPL",
		"S19.SPL"};
	count = ARRAYSIZE(names);
	return names;
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

bool PreGob_Emmanuelle::playAtariIntro() {
	if (_platformTag != kOldTotPlatformAtariST)
		return false;
	if (_introSkip.fastForwarding()) {
		stopSampleAudio();
		return true;
	}
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
