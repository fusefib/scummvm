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
