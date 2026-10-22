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

#include "gob/pregob/emmanuelle/renderer.h"

#include "common/system.h"

#include "gob/draw.h"
#include "gob/global.h"
#include "gob/gob.h"
#include "gob/util.h"
#include "gob/video.h"
#include "gob/pregob/emmanuelle/animation.h"
#include "gob/pregob/emmanuelle/dos.h"
#include "gob/pregob/emmanuelle/fight.h"
#include "gob/pregob/emmanuelle/saveload.h"
#include "gob/pregob/emmanuelle/text.h"

namespace Gob {
namespace Emmanuelle {

OldTotRenderer::OldTotRenderer(GobEngine *vm) :
	_vm(vm), _presentation(kOldTotPlatformDOS),
	_atariPresentedSurface(), _atariTransitionTarget(),
	_amigaPresentedSurface(), _amigaTransitionTarget() {
	_platform = kOldTotPlatformDOS;

	// The module host owns both resource banks.
	_localBank = nullptr;
	_sharedBank = nullptr;

	// A valid logical palette can be retained while the display is black.
	_atariPaletteValid = false;
	_atariBlack = false;
	_amigaPaletteValid = false;
	_amigaBlack = false;

	_dirtyValid = false;
	_dirtyLeft = 0;
	_dirtyTop = 0;
	_dirtyRight = 0;
	_dirtyBottom = 0;

	// Module changes invalidate the saved Amiga presentation frame.
	_amigaModuleGeneration = 0;
	_amigaPresentedGeneration = 0;

	for (uint i = 0; i < ARRAYSIZE(_atariLogicalPalette); ++i)
		_atariLogicalPalette[i] = 0;
	for (uint i = 0; i < ARRAYSIZE(_amigaLogicalPalette); ++i)
		_amigaLogicalPalette[i] = 0;
}

void OldTotRenderer::setResourceBanks(const OldIxxBank *localBank,
									  const OldIxxBank *sharedBank) {
	_localBank = localBank;
	_sharedBank = sharedBank;
}

bool OldTotRenderer::nextPresentationStep(bool black, byte randomVariant,
		PresentationStep &step) {
	return _platform != kOldTotPlatformDOS &&
		_presentation.nextStep(black, randomVariant, step);
}

SurfacePtr OldTotRenderer::clonePresentationFrame(
	const Surface &source) const {
	SurfacePtr copy(new Surface(source.getWidth(), source.getHeight(),
								source.getBPP()));
	copy->blit(source, 0, 0);
	return copy;
}

void OldTotRenderer::clearDirtyRect() {
	_dirtyValid = false;
	_dirtyLeft = 0;
	_dirtyTop = 0;
	_dirtyRight = 0;
	_dirtyBottom = 0;
}

void OldTotRenderer::notePresentationDirtyRect(int16 left, int16 top,
	int16 right, int16 bottom) {
	return;
}

bool OldTotRenderer::applyDosPalette(const Common::String &moduleName,
									 byte videoMode) {
	if (!_vm || !_vm->_draw || !_vm->_global || !_vm->_video)
		return false;

	if (videoMode <= 1) {
		byte evenTable[16];
		byte oddTable[16];
		if (!getDosCgaColorTables(moduleName, evenTable, oddTable))
			return false;
		const byte monochromeIndexes[4] = {0, 8, 7, 15};
		const byte colorIndexes[4] = {1, 11, 13, 15};
		const byte *indexes = videoMode == 0
			? monochromeIndexes : colorIndexes;
		for (uint i = 0; i < 4; ++i)
			_vm->_draw->_vgaPalette[i] =
				_vm->_global->_vgaPalette[indexes[i]];
		_vm->_video->setPalette(_vm->_draw->_vgaPalette);
		return true;
	}

	byte registers[16];
	if (!getDosPaletteRegisters(moduleName, registers))
		return false;

	for (uint i = 0; i < 16; ++i) {
		const byte colorIndex = dosEgaPaletteIndex(registers[i]);
		_vm->_draw->_vgaPalette[i] = _vm->_global->_vgaPalette[colorIndex];
	}
	_vm->_video->setPalette(_vm->_draw->_vgaPalette);
	return true;
}

bool OldTotRenderer::presentDosFrame(const Common::String &moduleName,
									 byte videoMode) {
	if (videoMode > 1)
		return true;
	if (!_vm || !_vm->_draw || !_vm->_draw->_backSurface ||
		!_vm->_draw->_frontSurface || !_vm->_video)
		return false;

	byte evenTable[16];
	byte oddTable[16];
	if (!getDosCgaColorTables(moduleName, evenTable, oddTable))
		return false;

	const Surface &back = *_vm->_draw->_backSurface;
	Surface &front = *_vm->_draw->_frontSurface;
	if (back.getWidth() != front.getWidth() ||
		back.getHeight() != front.getHeight())
		return false;
	for (uint16 y = 0; y < back.getHeight(); ++y) {
		for (uint16 x = 0; x < back.getWidth(); ++x) {
			front.putPixelRaw(x, y, dosCgaDitheredColor(
				(byte)back.get(x, y).get(), x, y, evenTable, oddTable));
		}
	}
	_vm->_video->dirtyRectsAll();
	return true;
}

void OldTotRenderer::copyPresentationRect(const Surface &target,
	int16 left, int16 top, int16 right, int16 bottom) {
	if (!_vm || !_vm->_draw || !_vm->_draw->_backSurface)
		return;
	Surface &back = *_vm->_draw->_backSurface;
	left = MAX<int16>(left, 0);
	top = MAX<int16>(top, 0);
	right = MIN<int16>(right, back.getWidth() - 1);
	bottom = MIN<int16>(bottom, back.getHeight() - 1);
	if (left > right || top > bottom)
		return;
	back.blit(target, left, top, right, bottom, left, top);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, left, top,
							right, bottom);
}

bool OldTotRenderer::saveScreenRect(uint16 x, uint16 y, uint16 width,
									uint16 height) {
	if (!_vm || !_vm->_draw || !_vm->_draw->_backSurface || width == 0 ||
		height == 0 || _savedRegions.size() >= 10)
		return false;

	SurfacePtr saved(new Surface(width, height, 1));
	saved->blit(*_vm->_draw->_backSurface, x, y, x + width - 1,
				y + height - 1, 0, 0);
	_savedRegions.push_back(OldTotPreparedSavedRegion(saved, x, y));
	return true;
}

bool OldTotRenderer::restoreScreenRect() {
	if (!_vm || !_vm->_draw || !_vm->_draw->_backSurface ||
		_savedRegions.empty())
		return false;

	const OldTotPreparedSavedRegion saved = _savedRegions.back();
	_savedRegions.pop_back();
	_vm->_draw->_backSurface->blit(*saved.surface, saved.x, saved.y);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, saved.x, saved.y,
							saved.x + saved.surface->getWidth() - 1,
							saved.y + saved.surface->getHeight() - 1);
	uint16 dirtyLeft = saved.x;
	uint16 dirtyRight = saved.x + saved.surface->getWidth() - 1;

	notePresentationDirtyRect(dirtyLeft, saved.y, dirtyRight,
					   saved.y + saved.surface->getHeight() - 1);
	return true;
}

void OldTotRenderer::fillScreen(byte color) {
	if (!_vm || !_vm->_draw || !_vm->_draw->_backSurface)
		return;

	Surface &back = *_vm->_draw->_backSurface;
	back.fillRect(0, 0, back.getWidth() - 1, back.getHeight() - 1, color);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 0, 0, back.getWidth() - 1,
							back.getHeight() - 1);
	notePresentationDirtyRect(0, 0, back.getWidth() - 1,
		back.getHeight() - 1);
}

bool OldTotRenderer::drawPackedPayload(const Common::Array<byte> &payload,
									   uint16 width, uint16 height, int16 x, int16 y, bool transparentZero,
									   bool horizontalFlip, bool verticalFlip) {
	if (payload.empty() || !_vm || !_vm->_draw || !_vm->_video ||
		!_vm->_draw->_backSurface)
		return false;

	Surface &dest = *_vm->_draw->_backSurface;

	Common::Array<byte> packedPayload = payload;
	if (!horizontalFlip && !verticalFlip) {
		_vm->_video->drawPackedSprite(&packedPayload[0], width, height, x, y,
									  transparentZero ? 1 : 0, dest);
	} else {
		Surface temp(width, height, dest.getBPP());
		temp.clear();
		_vm->_video->drawPackedSprite(&packedPayload[0], width, height, 0, 0,
									  0, temp);
		const uint32 transparent = temp.getColorFromIndex(0);
		for (int16 dy = 0; dy < (int16)height; ++dy) {
			const int16 targetY = y + dy;
			if (targetY < 0 || targetY >= (int16)dest.getHeight())
				continue;
			const int16 sourceY = verticalFlip ? height - 1 - dy : dy;
			for (int16 dx = 0; dx < (int16)width; ++dx) {
				const int16 targetX = x + dx;
				if (targetX < 0 || targetX >= (int16)dest.getWidth())
					continue;
				const int16 sourceX = horizontalFlip ? width - 1 - dx : dx;
				const uint32 pixel = temp.get(sourceX, sourceY).get();
				if (!transparentZero || pixel != transparent)
					dest.putPixelRaw(targetX, targetY, pixel);
			}
		}
	}

	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, x, y,
							x + width - 1, y + height - 1);
	notePresentationDirtyRect(x, y, x + width - 1, y + height - 1);
	return true;
}

bool OldTotRenderer::drawExternalSprite(const OldTotDrawDescriptor &desc,
										int16 x, int16 y) {
	if (!desc.external() || desc.linkedOrInline())
		return false;

	const OldIxxBank *bank = desc.bankSelector() == 0 ? _localBank : _sharedBank;
	if (!bank)
		return false;

	Common::Array<byte> payload;
	if (!bank->getPayload(desc.resourceIndex, payload))
		return false;

	return drawPackedPayload(payload, desc.width, desc.height, x, y,
							 desc.transparentZero(), desc.horizontalFlip(), desc.verticalFlip());
}

bool OldTotRenderer::drawSprite(const OldTotData &tot,
								const OldTotDrawDescriptor &desc, int16 x, int16 y) {
	if (desc.external())
		return drawExternalSprite(desc, x, y);

	Common::Array<byte> payload;
	if (!tot.getPackedPrefix(desc.payloadOffset,
							 (uint32)desc.width * desc.height, payload,
							 _platform == kOldTotPlatformAmiga))
		return false;

	return drawPackedPayload(payload, desc.width, desc.height, x, y,
							 desc.transparentZero(), desc.horizontalFlip(), desc.verticalFlip());
}

SurfacePtr OldTotRenderer::decodeSpriteSurface(const OldTotData &tot,
											   const OldTotDrawDescriptor &desc) {
	Common::Array<byte> payload;
	if (desc.external()) {
		const OldIxxBank *bank = desc.bankSelector() == 0 ? _localBank : _sharedBank;
		if (!bank || !bank->getPayload(desc.resourceIndex, payload))
			return SurfacePtr();
	} else if (!tot.getPackedPrefix(desc.payloadOffset,
									(uint32)desc.width * desc.height, payload,
									_platform == kOldTotPlatformAmiga)) {
		return SurfacePtr();
	}

	if (payload.empty() || !_vm || !_vm->_video)
		return SurfacePtr();

	SurfacePtr raw(new Surface(desc.width, desc.height, 1));
	raw->clear();
	_vm->_video->drawPackedSprite(&payload[0], desc.width, desc.height,
								  0, 0, 0, *raw);
	if (!desc.horizontalFlip() && !desc.verticalFlip())
		return raw;

	SurfacePtr flipped(new Surface(desc.width, desc.height, 1));
	flipped->clear();
	for (int16 y = 0; y < (int16)desc.height; ++y) {
		const int16 sourceY = desc.verticalFlip() ? desc.height - 1 - y : y;
		for (int16 x = 0; x < (int16)desc.width; ++x) {
			const int16 sourceX = desc.horizontalFlip() ? desc.width - 1 - x : x;
			flipped->putPixelRaw(x, y, raw->get(sourceX, sourceY).get());
		}
	}
	return flipped;
}

SurfacePtr OldTotRenderer::buildMaskedCell(const Surface &sprite,
										   uint16 cellWidth, uint16 cellHeight, int16 offsetX, int16 offsetY,
										   bool transparentZero) const {
	if (!cellWidth || !cellHeight || offsetX < 0 || offsetY < 0 ||
		offsetX + (int16)sprite.getWidth() > (int16)cellWidth ||
		offsetY + (int16)sprite.getHeight() > (int16)cellHeight)
		return SurfacePtr();

	SurfacePtr cell(new Surface(cellWidth, cellHeight, 1));
	cell->fillRect(0, 0, cellWidth - 1, cellHeight - 1, 0xFF);
	for (int16 y = 0; y < (int16)sprite.getHeight(); ++y) {
		for (int16 x = 0; x < (int16)sprite.getWidth(); ++x) {
			const uint32 pixel = sprite.get(x, y).get();
			if (transparentZero && pixel == sprite.getColorFromIndex(0))
				continue;
			cell->putPixelRaw(offsetX + x, offsetY + y, pixel);
		}
	}
	return cell;
}

SurfacePtr OldTotRenderer::buildFightCell(const Surface &sprite,
									  uint16 cellWidth, uint16 cellHeight,
									  int16 offsetX, int16 offsetY) const {
	// RECUP actors use the native fight compositor, which masks source color
	// zero independently of the ordinary old-TOT descriptor flag.
	return buildMaskedCell(sprite, cellWidth, cellHeight, offsetX, offsetY,
						   true);
}

Font *OldTotRenderer::loadLetFont(Common::SeekableReadStream &stream) const {
	const int64 size = stream.size();
	if (size < 4 || size > 0x7FFFFFFF)
		return nullptr;

	byte *data = new byte[(uint32)size];
	stream.seek(0);
	if (stream.read(data, (uint32)size) != (uint32)size) {
		delete[] data;
		return nullptr;
	}
	return new Font(data);
}

void OldTotRenderer::drawLetText(const Font &font, const Common::String &text,
								 int16 x, int16 y, byte foreground, byte background,
								 bool transparentBackground) {
	if (!_vm || !_vm->_draw || !_vm->_draw->_backSurface)
		return;

	const int16 startX = x;
	const int16 startY = y;
	int16 maxX = x;
	int16 maxY = y + font.getCharHeight();
	const char *textPosition = text.c_str();
	while (*textPosition) {
		const byte character = (byte)*textPosition++;
		if (character == '@') {
			if (x > maxX)
				maxX = x;
			x = startX;
			y += font.getCharHeight();
			if (y + font.getCharHeight() > maxY)
				maxY = y + font.getCharHeight();
			continue;
		}
		font.drawLetter(*_vm->_draw->_backSurface, character, x, y,
						foreground, background, transparentBackground);
		x += font.getCharWidth(character);
	}
	if (x > maxX)
		maxX = x;
	if (maxX > startX && maxY > startY) {
		_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, startX, startY,
								maxX - 1, maxY - 1);
		notePresentationDirtyRect(startX, startY, maxX - 1, maxY - 1);
	}
}

} // End of namespace Emmanuelle
} // End of namespace Gob
