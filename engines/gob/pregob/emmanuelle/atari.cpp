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
