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

bool PreGob_Emmanuelle::handleAmigaFF0B(byte command) {
	if (!amigaHandlesNativeCommand(command))
		return false;
	switch (command) {
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
	for (byte phase = 0; phase < 16 && !_vm->shouldQuit(); ++phase) {
		if (!_renderer.presentAmigaFadeFromBlackStep(phase))
			break;
		waitPresentationFrames();
	}
}

void PreGob_Emmanuelle::fadeAmigaToBlack() {
	for (byte phase = 0; phase < 16 && !_vm->shouldQuit(); ++phase) {
		if (!_renderer.presentAmigaFadeToBlackStep(phase))
			break;
		waitPresentationFrames();
	}
}

void PreGob_Emmanuelle::waitAmigaInputRelease() {
	int16 mouseX = 0;
	int16 mouseY = 0;
	MouseButtons mouseButtons = kMouseButtonsNone;
	int16 key = kKeyNone;
	do {
		delay(amigaNativeDelayMilliseconds(100));
		key = pollInterpreterInput(mouseX, mouseY, mouseButtons);
	} while (!_vm->shouldQuit() && !_introSkip.fastForwarding() &&
			 key != kKeyNone);
}

bool OldTotRenderer::decodeAmigaPackedPayload(
	const Common::Array<byte> &payload, uint16 width, uint16 height,
	Surface &out) const {
	Common::Array<byte> pixels;
	if (!decodeAmigaPackedPixels(payload, width, height, pixels))
		return false;
	for (uint32 i = 0; i < pixels.size(); ++i)
		out.putPixelRaw(i % width, i / width, pixels[i]);
	return true;
}

void OldTotRenderer::resetAmigaModuleState() {
	_savedRegions.clear();
	_presentation.resetModule();
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

void OldTotRenderer::commitAmigaDirectFrame() {
	if (_platform != kOldTotPlatformAmiga)
		return;
	_presentation.commitDirectFrame();
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
		work[i] = _amigaLogicalPalette[i];
	for (byte step = 0; step <= phase; ++step) {
		for (uint i = 0; i < 32; ++i)
			work[i] = dimAmigaPaletteWord(work[i], 1);
	}
	showAmigaWords(work);
	if (phase == 15)
		_amigaBlack = true;
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

} // End of namespace Emmanuelle
} // End of namespace Gob
