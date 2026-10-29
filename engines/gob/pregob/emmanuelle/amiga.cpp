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

#include "gob/draw.h"
#include "gob/gob.h"
#include "gob/sound/sound.h"
#include "gob/util.h"
#include "gob/video.h"
#include "gob/pregob/emmanuelle/amiga.h"

namespace Gob {
namespace Emmanuelle {

const char *const *PreGob_Emmanuelle::amigaIntroSampleNames(byte &count) const {
	static const char *const names[] = {"P1.SPL", "intro.spl"};
	count = ARRAYSIZE(names);
	return names;
}

void PreGob_Emmanuelle::serviceAmigaFightFrame(
	const FightState &state) {
	if (_platformTag != kOldTotPlatformAmiga)
		return;

	int16 slot = fightSampleSlot(
		false, state.left.action, state.left.frame);
	const int16 rightSlot = fightSampleSlot(
		true, state.right.action, state.right.frame);
	if (rightSlot >= 0)
		slot = rightSlot;
	if (slot >= 0 && slot < _sampleCount) {
		stopSampleAudio();
		_vm->_sound->blasterPlay(&_samples[slot], 1, 0);
		setAudioState(kOldTotAudioAmigaSample, slot, 0, 1,
							 false, true);
	}
	delay(amigaNativeDelayMilliseconds(200));
}

bool PreGob_Emmanuelle::handleAmigaFF0B(byte command) {
	switch (command) {
	case 40:
		_colorCycleEnabled = false;
		if (_scriptVM && _scriptVM->readVariable(0) == 10)
			_renderer.applyAmigaPalette(3, true);
		return true;
	case 41:
		_colorCycleEnabled = true;
		return true;
	case 42:
		_renderer.disarmAmigaTransition();
		return true;
	case 43:
		servicePresentation();
		_renderer.armAmigaTransition(43);
		return true;
	case 44:
		_renderer.armAmigaTransition(44);
		return true;
	case 45: {
		uint16 words[5];
		const byte variant = (byte)randomBounded(3);
		if (getAmigaEffect45(variant, words))
			_renderer.applyAmigaEffect45(words);
		return true;
	}
	case 46:
		for (byte i = 0; i < 4 && !_vm->shouldQuit(); ++i) {
			_renderer.rotateAmigaEffect46();
			delay(amigaNativeDelayMilliseconds(250));
		}
		return true;
	case 47:
		_renderer.noteAmigaPaletteTransition();
		fadeAmigaToBlack();
		_renderer.applyAmigaPalette(4, false);
		return true;
	case 80:
		stopSampleAudio();
		return true;
	case 81:
		if (_scriptVM && _scriptVM->readVariable(0) == 0)
			playAmigaIntro();
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
		return playAmigaSampleCommand(command);
	case 201:
		if (_scriptVM && command201SkipsIntermediateNodes(
						  _platformTag, _scriptVM->readVariable(0)))
			_skipIntermediateSequenceNodes = true;
		return true;
	default:
		return false;
	}
}

void PreGob_Emmanuelle::fadeAmigaFromBlack() {
	for (byte phase = 0; phase < 16 && !shouldAbortExecution(); ++phase) {
		if (!_renderer.presentAmigaFadeFromBlackStep(phase))
			break;
		waitPresentationFrames();
	}
}

void PreGob_Emmanuelle::fadeAmigaToBlack() {
	for (byte phase = 0; phase < 16 && !shouldAbortExecution(); ++phase) {
		if (!_renderer.presentAmigaFadeToBlackStep(phase))
			break;
		waitPresentationFrames();
	}
}

bool PreGob_Emmanuelle::playAmigaIntro() {
	if (_platformTag != kOldTotPlatformAmiga || _sampleBank != 1 ||
		_sampleCount < 2)
		return false;
	if (_introSkip.fastForwarding()) {
		stopSampleAudio();
		return true;
	}

	stopSampleAudio();
	_vm->_sound->blasterPlay(&_samples[1], -1, 0);
	setAudioState(kOldTotAudioAmigaSample, 1, 0, -1, false, true);
	while (!_vm->shouldQuit() && !_introSkip.fastForwarding()) {
		_vm->_util->delay(1);
		processGameEvents();
		(void)randomBounded(100);
		int16 mouseX = 0;
		int16 mouseY = 0;
		MouseButtons mouseButtons = kMouseButtonsNone;
		const int16 key = takePendingInput(mouseX, mouseY, mouseButtons);
		if (key != kKeyNone || mouseButtons != kMouseButtonsNone)
			break;
	}
	stopSampleAudio();
	return true;
}

bool PreGob_Emmanuelle::playAmigaSampleCommand(byte command) {
	AmigaSampleCommand sampleCommand;
	if (!getAmigaSampleCommand(command, sampleCommand))
		return false;
	if (_introSkip.fastForwarding()) {
		stopSampleAudio();
		return true;
	}
	if (sampleCommand.slot >= _sampleCount)
		return true;

	if (sampleCommand.stopAllFirst)
		stopSampleAudio();
	if (sampleCommand.preDelay)
		delay(amigaNativeDelayMilliseconds(sampleCommand.preDelay));
	if (_vm->shouldQuit())
		return true;
	if (_introSkip.fastForwarding()) {
		stopSampleAudio();
		return true;
	}
	stopSampleAudio();
	_vm->_sound->blasterPlay(&_samples[sampleCommand.slot],
							 sampleCommand.mixerRepeatCount, 0);
	setAudioState(kOldTotAudioAmigaSample, sampleCommand.slot, 0,
						 sampleCommand.mixerRepeatCount, false, true);
	if (sampleCommand.postDelay)
		delay(amigaNativeDelayMilliseconds(sampleCommand.postDelay));
	return true;
}

void PreGob_Emmanuelle::waitAmigaInputRelease() {
	int16 mouseX = 0;
	int16 mouseY = 0;
	MouseButtons mouseButtons = kMouseButtonsNone;
	int16 key = kKeyNone;
	do {
		delay(amigaNativeDelayMilliseconds(100));
		key = pollInterpreterInput(mouseX, mouseY, mouseButtons);
	} while (!shouldAbortExecution() && !_introSkip.fastForwarding() &&
			 key != kKeyNone);
}

bool OldTotRenderer::decodeAmigaPackedPayload(
	const Common::Array<byte> &payload, uint16 width, uint16 height,
	Surface &out) const {
	if (out.getBPP() != 1 || out.getWidth() != width || out.getHeight() != height)
		return false;
	// The caller owns an unpublished sprite; malformed data cannot alter the screen.
	return decodeAmigaPackedPixels(payload, out.getData(), (uint32)width * height);
}

bool OldTotRenderer::presentAmigaMode44Step(byte variant, byte phase) {
	if (_platform != kOldTotPlatformAmiga || variant >= 5 ||
		!_amigaTransitionTarget || !_vm || !_vm->_draw ||
		!_vm->_draw->_backSurface)
		return false;

	AmigaMode44Rect rect;
	if (!getAmigaMode44Rect(variant, phase, rect))
		return false;
	copyPresentationRect(*_amigaTransitionTarget, rect.x, rect.y,
							rect.x + rect.width - 1,
							rect.y + rect.height - 1);
	if (phase == amigaMode44WaitCount(variant))
		finishAmigaTransitionFrame();
	return true;
}

bool OldTotRenderer::presentAmigaMode43Step(byte phase) {
	if (_platform != kOldTotPlatformAmiga || phase >= 16 ||
		!_amigaTransitionTarget || !_vm || !_vm->_draw ||
		!_vm->_draw->_backSurface)
		return false;
	Surface &back = *_vm->_draw->_backSurface;
	if (_presentationDirtyRect.isEmpty()) {
		finishAmigaTransitionFrame();
		return true;
	}
	const int16 left = (_presentationDirtyRect.left / 16) * 16;
	int16 right = (((_presentationDirtyRect.right - 1) / 16) + 1) * 16 - 1;
	right = MIN<int16>(right, back.getWidth() - 1);
	uint32 ordinal = 0;
	for (int16 y = _presentationDirtyRect.top; y < _presentationDirtyRect.bottom; ++y) {
		for (int16 wordX = left; wordX <= right;
			 wordX += 16, ++ordinal) {
			const int16 x = wordX +
				amigaMode43PixelOffset(phase, ordinal);
			if (x <= right && x < (int16)back.getWidth())
				back.putPixelRaw(
					x, y, _amigaTransitionTarget->get(x, y).get());
		}
	}
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, left,
							_presentationDirtyRect.top, right, _presentationDirtyRect.bottom - 1);
	if (phase == 15)
		finishAmigaTransitionFrame();
	return true;
}

void OldTotRenderer::finishAmigaTransitionFrame() {
	if (!_amigaTransitionTarget || !_vm || !_vm->_draw ||
		!_vm->_draw->_backSurface)
		return;
	_vm->_draw->_backSurface->blit(*_amigaTransitionTarget, 0, 0);
	_amigaPresentedSurface = _amigaTransitionTarget;
	_amigaTransitionTarget.reset();
	clearDirtyRect();
}

bool OldTotRenderer::beginAmigaTransitionFrame() {
	if (_platform != kOldTotPlatformAmiga || !_vm || !_vm->_draw ||
		!_vm->_draw->_backSurface)
		return false;
	Surface &back = *_vm->_draw->_backSurface;
	_amigaTransitionTarget = clonePresentationFrame(back);
	if (!amigaSnapshotGenerationUsable(
			_amigaModuleGeneration, _amigaPresentedGeneration,
			(bool)_amigaPresentedSurface) ||
		_amigaPresentedSurface->getWidth() != back.getWidth() ||
		_amigaPresentedSurface->getHeight() != back.getHeight())
		_amigaPresentedSurface = clonePresentationFrame(back);
	back.blit(*_amigaPresentedSurface, 0, 0);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 0, 0,
							back.getWidth() - 1, back.getHeight() - 1);
	return true;
}

void OldTotRenderer::resetAmigaModuleState() {
	_savedRegions.clear();
	_presentation.resetModule();
	_amigaTransitionTarget.reset();
	clearDirtyRect();
}

void OldTotRenderer::beginAmigaModuleState() {
	if (_platform == kOldTotPlatformAmiga)
		++_amigaModuleGeneration;
}

void OldTotRenderer::noteAmigaPaletteTransition() {
	if (_platform == kOldTotPlatformAmiga)
		_presentation.notePaletteTransition();
}

void OldTotRenderer::disarmAmigaTransition() {
	_presentation.disarm();
	clearDirtyRect();
	_amigaTransitionTarget.reset();
}

void OldTotRenderer::armAmigaTransition(byte mode) {
	if (_platform != kOldTotPlatformAmiga || (mode != 43 && mode != 44))
		return;
	if (!amigaSnapshotGenerationUsable(
			_amigaModuleGeneration, _amigaPresentedGeneration,
			(bool)_amigaPresentedSurface))
		captureAmigaPresentedFrame();
	_presentation.arm(mode);
	_amigaTransitionTarget.reset();
	clearDirtyRect();
}

void OldTotRenderer::commitAmigaDirectFrame() {
	if (_platform != kOldTotPlatformAmiga)
		return;
	_presentation.commitDirectFrame();
	_amigaTransitionTarget.reset();
	clearDirtyRect();
	captureAmigaPresentedFrame();
}

void OldTotRenderer::captureAmigaPresentedFrame() {
	if (_platform != kOldTotPlatformAmiga || !_vm || !_vm->_draw ||
		!_vm->_draw->_backSurface)
		return;
	_amigaPresentedSurface = clonePresentationFrame(*_vm->_draw->_backSurface);
	_amigaPresentedGeneration = _amigaModuleGeneration;
}

bool OldTotRenderer::restoreAmigaPaletteImmediately() {
	if (!_amigaPaletteValid)
		return false;
	showAmigaWords(_amigaLogicalPalette);
	_amigaBlack = false;
	return true;
}

bool OldTotRenderer::presentAmigaFadeFromBlackStep(byte phase) {
	if (!_amigaPaletteValid || !_amigaBlack || phase >= 16)
		return false;
	uint16 work[32];
	const byte dim = 16 - phase;
	for (uint i = 0; i < 32; ++i)
		work[i] = dimAmigaPaletteWord(_amigaLogicalPalette[i], dim);
	showAmigaWords(work);
	if (phase == 15) {
		showAmigaWords(_amigaLogicalPalette);
		_amigaBlack = false;
	}
	return true;
}

bool OldTotRenderer::presentAmigaFadeToBlackStep(byte phase) {
	if (!_amigaPaletteValid || _amigaBlack || phase >= 16)
		return false;
	uint16 work[32];
	for (uint i = 0; i < 32; ++i)
		work[i] = dimAmigaPaletteWord(_amigaLogicalPalette[i], phase + 1);
	showAmigaWords(work);
	if (phase == 15)
		_amigaBlack = true;
	return true;
}

bool OldTotRenderer::rotateAmigaColorCycle() {
	if (!_amigaPaletteValid)
		return false;
	const uint16 old7 = _amigaLogicalPalette[7];
	_amigaLogicalPalette[7] = _amigaLogicalPalette[6];
	_amigaLogicalPalette[6] = _amigaLogicalPalette[5];
	_amigaLogicalPalette[5] = _amigaLogicalPalette[4];
	_amigaLogicalPalette[4] = old7;
	showAmigaWords(_amigaLogicalPalette);
	return true;
}

bool OldTotRenderer::rotateAmigaEffect46() {
	if (!_amigaPaletteValid)
		return false;
	// Native command 46 moves 15 -> 3 -> 9 -> 10 -> 11 -> 15. Reversing it
	// leaves Cupid with the wrong colours on the following protection screen.
	const uint16 old15 = _amigaLogicalPalette[15];
	_amigaLogicalPalette[15] = _amigaLogicalPalette[11];
	_amigaLogicalPalette[11] = _amigaLogicalPalette[10];
	_amigaLogicalPalette[10] = _amigaLogicalPalette[9];
	_amigaLogicalPalette[9] = _amigaLogicalPalette[3];
	_amigaLogicalPalette[3] = old15;
	showAmigaWords(_amigaLogicalPalette);
	return true;
}

bool OldTotRenderer::applyAmigaEffect45(const uint16 words[5]) {
	if (!_amigaPaletteValid || !words)
		return false;
	_amigaLogicalPalette[3] = words[0];
	_amigaLogicalPalette[9] = words[1];
	_amigaLogicalPalette[10] = words[2];
	_amigaLogicalPalette[11] = words[3];
	_amigaLogicalPalette[15] = words[4];
	showAmigaWords(_amigaLogicalPalette);
	return true;
}

bool OldTotRenderer::applyAmigaPalette(byte paletteNumber,
									   bool makeVisible) {
	uint16 words[32];
	if (!getAmigaNumberedPalette(paletteNumber, words))
		return false;
	return setAmigaWords(words, makeVisible);
}

bool OldTotRenderer::applyAmigaModulePalette(
	const Common::String &moduleName) {
	uint16 words[32];
	if (!getAmigaModulePalette(moduleName, words))
		return false;
	if (_amigaPaletteValid) {
		for (uint i = 0; i < ARRAYSIZE(_amigaLogicalPalette); ++i) {
			if (_amigaLogicalPalette[i] != words[i]) {
				_presentation.notePaletteTransition();
				break;
			}
		}
	}
	return setAmigaWords(words, !_amigaBlack);
}

bool OldTotRenderer::amigaModulePaletteDiffers(
	const Common::String &moduleName) const {
	uint16 words[32];
	if (!_amigaPaletteValid || !getAmigaModulePalette(moduleName, words))
		return false;
	for (uint i = 0; i < ARRAYSIZE(_amigaLogicalPalette); ++i) {
		if (_amigaLogicalPalette[i] != words[i])
			return true;
	}
	return false;
}

bool OldTotRenderer::setAmigaWords(const uint16 words[32],
								   bool makeVisible) {
	if (!words)
		return false;
	for (uint i = 0; i < 32; ++i)
		_amigaLogicalPalette[i] = words[i];
	_amigaPaletteValid = true;
	if (makeVisible && !_amigaBlack)
		showAmigaWords(_amigaLogicalPalette);
	return true;
}

void OldTotRenderer::showAmigaWords(const uint16 words[32]) {
	if (!_vm || !_vm->_draw || !_vm->_video || !words)
		return;
	for (uint i = 0; i < 32; ++i) {
		_vm->_draw->_vgaPalette[i].red =
			amigaPaletteComponent(words[i], 8);
		_vm->_draw->_vgaPalette[i].green =
			amigaPaletteComponent(words[i], 4);
		_vm->_draw->_vgaPalette[i].blue =
			amigaPaletteComponent(words[i], 0);
	}
	_vm->_video->setPalette(_vm->_draw->_vgaPalette);
}

static const uint16 kAmigaPalettes[5][32] = {
	{0x000, 0xFEE, 0xF00, 0x4F0, 0x211, 0x311, 0x511, 0x700,
	 0xD00, 0xF0C, 0xB08, 0x705, 0x041, 0x061, 0x191, 0x3B2,
	 0x420, 0x900, 0xC40, 0xE61, 0xE83, 0xFB4, 0xFD6, 0xFE8,
	 0x113, 0x309, 0x45F, 0x0CF, 0x334, 0x557, 0x889, 0xBBC},
	{0x000, 0xFFF, 0xF00, 0x4F0, 0x211, 0x9EF, 0xBFF, 0xDFF,
	 0xD00, 0xF0C, 0xB08, 0x705, 0x041, 0x061, 0x191, 0x3B2,
	 0x420, 0x900, 0xC40, 0xE71, 0xE83, 0xFB4, 0xFD6, 0xFE8,
	 0x113, 0x309, 0x45F, 0x0CF, 0x334, 0x557, 0x889, 0xBBC},
	{0x000, 0xFEE, 0x000, 0x000, 0x000, 0x446, 0x99B, 0xDDF,
	 0xD00, 0xF0C, 0x000, 0x223, 0x041, 0x061, 0x191, 0x3B2,
	 0x420, 0x900, 0xC40, 0xE61, 0xE83, 0xFB4, 0xFD6, 0xFE8,
	 0x113, 0x309, 0x45F, 0x0CF, 0x334, 0x557, 0x889, 0xBBC},
	{0x021, 0xFFF, 0x003, 0x004, 0x006, 0x009, 0x02B, 0x05D,
	 0x08F, 0x2AF, 0x7BF, 0x9CF, 0xADF, 0xDEF, 0x400, 0x600,
	 0x800, 0xB21, 0xE62, 0xE83, 0xFA4, 0xFC8, 0xF00, 0xF6A,
	 0xF9C, 0xF99, 0x072, 0x0A2, 0x0E4, 0x0F6, 0x777, 0x888},
	{0x000, 0x88A, 0xFF7, 0xFFF, 0x001, 0x004, 0x036, 0x306,
	 0xF00, 0xFF0, 0xFBF, 0xF0B, 0x031, 0x051, 0x181, 0x5F3,
	 0x310, 0x600, 0x200, 0x210, 0x421, 0x622, 0x652, 0xFF7,
	 0x004, 0x024, 0x306, 0x0BE, 0x003, 0x226, 0x447, 0x558}};

static const uint16 kAmigaEffect45Sets[3][5] = {
	{0x8EF, 0x0AF, 0x03F, 0x008, 0x003},
	{0xFF9, 0xFB0, 0xF30, 0x800, 0x300},
	{0xFF7, 0x9F0, 0x0F0, 0x080, 0x030}};

static const byte kAmigaMode43Order[16] = {
	0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15};

bool getAmigaSampleCommand(byte command, AmigaSampleCommand &out) {
	out = AmigaSampleCommand();
	switch (command) {
	case 82:
		out.slot = 0;
		out.mixerRepeatCount = 2;
		out.preDelay = 50;
		out.postDelay = 100;
		out.stopAllFirst = true;
		return true;
	case 83:
		out.slot = 1;
		out.mixerRepeatCount = -1;
		return true;
	case 85:
		out.slot = 16;
		out.mixerRepeatCount = 2;
		return true;
	case 86:
		out.slot = 2;
		out.mixerRepeatCount = -1;
		return true;
	case 89:
		out.slot = 3;
		out.mixerRepeatCount = 2;
		out.postDelay = 100;
		return true;
	case 90:
		out.slot = 4;
		out.mixerRepeatCount = -1;
		return true;
	case 91:
		out.slot = 5;
		out.mixerRepeatCount = -1;
		return true;
	case 92:
		out.slot = 6;
		out.mixerRepeatCount = 2;
		out.postDelay = 100;
		return true;
	case 94:
		out.slot = 7;
		out.mixerRepeatCount = 2;
		out.postDelay = 100;
		return true;
	default:
		return false;
	}
}

uint16 amigaInitialModuleSelector(uint16 scriptSelector,
								 bool forceLoadHot) {
	return forceLoadHot ? 1 : scriptSelector;
}

uint16 amigaNativeRand(uint32 &state) {
	state = state * 0x41C64E6DU + 0x3039U;
	return (uint16)((state >> 16) & 0x7FFFU);
}

uint16 amigaRandomBounded(uint32 &state, uint16 upperExclusive) {
	const uint16 value = amigaNativeRand(state);
	return upperExclusive
			   ? (uint16)((value & 0x0FFFU) % upperExclusive)
			   : 0;
}

uint16 amigaNativeDelayMilliseconds(uint16 units) {
	return (uint16)(units / 30U) * 20U;
}

void getAmigaSchedulerTiming(byte period, uint16 &reload,
							 uint16 &initialCountdown) {
	reload = period == 200 ? 60 : 25;
	// Amiga timer quanta represent 50 ms; include the zero-test quantum.
	if (period == 50 || period == 100 || period == 200)
		reload = (period == 50 ? 1500 : 2000) / 50 - 1;
	initialCountdown = 0;
}

bool decodeAmigaPackedPixels(const Common::Array<byte> &payload,
							 byte *pixels, uint32 pixelCount) {
	if (!pixels || !pixelCount)
		return false;
	uint32 pixel = 0;
	uint32 position = 0;
	while (pixel < pixelCount) {
		if (position >= payload.size())
			return false;
		const byte value = payload[position++];
		const byte color = value >> 3;
		uint32 run = 0;
		if (value & 0x04) {
			run = (value & 0x03) + 1;
		} else {
			if (position >= payload.size())
				return false;
			run = ((((uint32)value & 0x03) << 8) |
				   payload[position++]) +
				  1;
		}
		// The last run may extend past the image.
		const uint32 remaining = pixelCount - pixel;
		const uint32 count = run < remaining ? run : remaining;
		memset(pixels + pixel, color, count);
		pixel += count;
	}
	return true;
}

static byte amigaModulePaletteNumber(const Common::String &moduleName) {
	byte paletteNumber = 0;
	if (moduleName.equalsIgnoreCase("INIT") ||
		moduleName.equalsIgnoreCase("INIT.TOA")) {
		paletteNumber = 5;
	} else if (moduleName.equalsIgnoreCase("LOADCHUT") ||
			   moduleName.equalsIgnoreCase("LOADCHUT.TOA")) {
		paletteNumber = 3;
	} else if (moduleName.equalsIgnoreCase("LOADPLA") ||
			   moduleName.equalsIgnoreCase("LOADPLA.TOA") ||
			   moduleName.equalsIgnoreCase("LOADAERO") ||
			   moduleName.equalsIgnoreCase("LOADAERO.TOA") ||
			   moduleName.equalsIgnoreCase("LOADGAR") ||
			   moduleName.equalsIgnoreCase("LOADGAR.TOA")) {
		paletteNumber = 2;
	} else if (moduleName.equalsIgnoreCase("LOADHOT") ||
			   moduleName.equalsIgnoreCase("LOADHOT.TOA") ||
			   moduleName.equalsIgnoreCase("LOADCAS") ||
			   moduleName.equalsIgnoreCase("LOADCAS.TOA") ||
			   moduleName.equalsIgnoreCase("LOADFAV") ||
			   moduleName.equalsIgnoreCase("LOADFAV.TOA")) {
		paletteNumber = 1;
	}
	return paletteNumber;
}

bool getAmigaModulePalette(const Common::String &moduleName,
						   uint16 words[32]) {
	return getAmigaNumberedPalette(amigaModulePaletteNumber(moduleName), words);
}

bool getAmigaTextColorTable(const Common::String &moduleName, byte colors[16]) {
	// The Amiga loader selects logical text colors with each module. These
	// remain selected when a script temporarily changes the visible palette.
	static const byte tables[4][16] = {
		{0, 1, 25, 26, 26, 12, 13, 31, 1, 2, 0, 9, 17, 21, 22, 23},
		{0, 10, 25, 26, 26, 12, 13, 31, 1, 2, 0, 9, 17, 21, 22, 23},
		{0, 5, 25, 26, 26, 12, 13, 31, 1, 2, 0, 31, 17, 21, 22, 23},
		{0, 30, 25, 29, 31, 12, 13, 14, 2, 8, 0, 11, 26, 2, 22, 3}
	};
	const byte palette = amigaModulePaletteNumber(moduleName);
	if (palette == 0 || palette == 4)
		return false;
	memcpy(colors, tables[palette == 5 ? 3 : palette - 1], 16);
	return true;
}

bool getAmigaNumberedPalette(byte paletteNumber, uint16 words[32]) {
	if (!words || paletteNumber < 1 || paletteNumber > 5)
		return false;
	memcpy(words, kAmigaPalettes[paletteNumber - 1], 32 * sizeof(*words));
	return true;
}

bool getAmigaEffect45(byte variant, uint16 words[5]) {
	if (!words || variant >= ARRAYSIZE(kAmigaEffect45Sets))
		return false;
	memcpy(words, kAmigaEffect45Sets[variant], 5 * sizeof(*words));
	return true;
}

byte amigaPaletteComponent(uint16 word, byte shift) {
	const byte nibble = (byte)((word >> shift) & 0x0F);
	return (byte)((nibble << 2) | (nibble >> 2));
}

uint16 dimAmigaPaletteWord(uint16 word, byte steps) {
	byte red = (word >> 8) & 0x0F;
	byte green = (word >> 4) & 0x0F;
	byte blue = word & 0x0F;
	red = red > steps ? red - steps : 0;
	green = green > steps ? green - steps : 0;
	blue = blue > steps ? blue - steps : 0;
	return (uint16)((red << 8) | (green << 4) | blue);
}

byte prepareAmigaWaitColorCounter(byte counter) {
	counter %= 6;
	return counter < 4 ? 4 : counter;
}

byte amigaMode43PixelOffset(byte phase, uint32 wordOrdinal) {
	return kAmigaMode43Order[(phase + wordOrdinal) & 15];
}

byte amigaMode44WaitCount(byte variant) {
	return variant == 0 ? 9 : (variant < 5 ? 18 : 0);
}

bool getAmigaMode44Rect(byte variant, byte stage, AmigaMode44Rect &out) {
	const byte maxStage = amigaMode44WaitCount(variant);
	if (variant >= 5 || stage > maxStage)
		return false;
	if (variant == 0) {
		out.x = 144 - (int16)stage * 16;
		out.y = 90 - (int16)stage * 10;
		out.width = (uint16)(32 + stage * 32);
		out.height = (uint16)(20 + stage * 20);
	} else {
		out.width = (uint16)(32 + stage * 16);
		out.height = (uint16)(20 + stage * 10);
		out.x = variant == 1 || variant == 2
					? 288 - (int16)stage * 16
					: 0;
		out.y = variant == 2 || variant == 4
					? 180 - (int16)stage * 10
					: 0;
	}
	return true;
}

bool amigaSnapshotGenerationUsable(
	uint32 moduleGeneration, uint32 presentedGeneration, bool hasSnapshot) {
	if (!hasSnapshot || moduleGeneration == 0 || presentedGeneration == 0)
		return false;
	return presentedGeneration == moduleGeneration ||
		(moduleGeneration > 1 &&
		 presentedGeneration == moduleGeneration - 1);
}

} // End of namespace Emmanuelle
} // End of namespace Gob
