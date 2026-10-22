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
#include "gob/pregob/emmanuelle/emmanuelle.h"
#include "gob/pregob/emmanuelle/fight.h"
#include "gob/pregob/emmanuelle/saveload.h"
#include "gob/pregob/emmanuelle/text.h"

namespace Gob {
namespace Emmanuelle {

OldTotRendererPreparedState::OldTotRendererPreparedState() {
	atariPaletteValid = false;
	atariBlack = false;
	amigaPaletteValid = false;
	amigaBlack = false;

	amigaModuleGeneration = 0;
	amigaPresentedGeneration = 0;

	atariPresentationMode = 0;
	amigaPresentationMode = 0;

	for (uint i = 0; i < ARRAYSIZE(atariPalette); ++i)
		atariPalette[i] = 0;
	for (uint i = 0; i < ARRAYSIZE(amigaPalette); ++i)
		amigaPalette[i] = 0;
}

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

bool OldTotRenderer::exportState(EmmanuelleRendererState &out) const {
	if (!_vm || !_vm->_draw || !_vm->_draw->_backSurface)
		return false;
	exportOldTotSurface(*_vm->_draw->_backSurface, out.framebuffer);
	out.savedRegions.clear();
	for (uint i = 0; i < _savedRegions.size(); ++i) {
		const OldTotPreparedSavedRegion &saved = _savedRegions[i];
		if (!saved.surface)
			return false;
		EmmanuelleSavedRegionState state;
		state.x = saved.x;
		state.y = saved.y;
		exportOldTotSurface(*saved.surface, state.surface);
		out.savedRegions.push_back(state);
	}
	for (uint i = 0; i < ARRAYSIZE(out.atariPalette); ++i)
		out.atariPalette[i] = _atariLogicalPalette[i];
	for (uint i = 0; i < ARRAYSIZE(out.amigaPalette); ++i)
		out.amigaPalette[i] = _amigaLogicalPalette[i];
	out.atariPaletteValid = _atariPaletteValid;
	out.atariBlack = _atariBlack;
	out.amigaPaletteValid = _amigaPaletteValid;
	out.amigaBlack = _amigaBlack;
	out.amigaModuleGeneration = _amigaModuleGeneration;
	out.amigaPresentedGeneration = _amigaPresentedGeneration;

	return true;
}

bool OldTotRenderer::prepareState(const EmmanuelleRendererState &in,
		byte atariPresentationMode,
		OldTotRendererPreparedState &out) const {
	if (!_vm || !_vm->_draw || !_vm->_draw->_backSurface)
		return false;
	return prepareOldTotRendererState(in,
		_vm->_draw->_backSurface->getWidth(),
		_vm->_draw->_backSurface->getHeight(),
		_vm->_draw->_backSurface->getBPP(), atariPresentationMode, out);
}

bool prepareOldTotRendererState(const EmmanuelleRendererState &in,
		uint16 width, uint16 height, byte bytesPerPixel,
		byte atariPresentationMode, OldTotRendererPreparedState &out) {
	if (!validateEmmanuelleRendererState(in, width, height, bytesPerPixel,
			atariPresentationMode))
		return false;

	OldTotRendererPreparedState prepared;
	if (!importOldTotSurface(in.framebuffer, prepared.framebuffer))
		return false;
	for (uint i = 0; i < in.savedRegions.size(); ++i) {
		const EmmanuelleSavedRegionState &state = in.savedRegions[i];
		SurfacePtr surface;
		if (!importOldTotSurface(state.surface, surface) ||
			state.x < 0 || state.y < 0 ||
			state.x + state.surface.width > in.framebuffer.width ||
			state.y + state.surface.height > in.framebuffer.height)
			return false;
		prepared.savedRegions.push_back(OldTotPreparedSavedRegion(
			surface, state.x, state.y));
	}
	if (!in.amigaPresentedFrame.pixels.empty() &&
		(!importOldTotSurface(in.amigaPresentedFrame,
			 prepared.amigaPresentedFrame) ||
		 in.amigaPresentedFrame.width != in.framebuffer.width ||
		 in.amigaPresentedFrame.height != in.framebuffer.height ||
		 in.amigaPresentedFrame.bytesPerPixel !=
			in.framebuffer.bytesPerPixel))
		return false;
	for (uint i = 0; i < ARRAYSIZE(prepared.atariPalette); ++i)
		prepared.atariPalette[i] = in.atariPalette[i];
	for (uint i = 0; i < ARRAYSIZE(prepared.amigaPalette); ++i)
		prepared.amigaPalette[i] = in.amigaPalette[i];
	prepared.atariPaletteValid = in.atariPaletteValid;
	prepared.atariBlack = in.atariBlack;
	prepared.amigaPaletteValid = in.amigaPaletteValid;
	prepared.amigaBlack = in.amigaBlack;
	prepared.amigaModuleGeneration = in.amigaModuleGeneration;
	prepared.amigaPresentedGeneration = in.amigaPresentedGeneration;
	prepared.atariPresentationMode = atariPresentationMode;
	prepared.amigaPresentationMode = in.amigaPresentationMode;

	out.framebuffer = prepared.framebuffer;
	out.savedRegions.swap(prepared.savedRegions);
	out.amigaPresentedFrame = prepared.amigaPresentedFrame;
	for (uint i = 0; i < ARRAYSIZE(out.atariPalette); ++i)
		out.atariPalette[i] = prepared.atariPalette[i];
	for (uint i = 0; i < ARRAYSIZE(out.amigaPalette); ++i)
		out.amigaPalette[i] = prepared.amigaPalette[i];
	out.atariPaletteValid = prepared.atariPaletteValid;
	out.atariBlack = prepared.atariBlack;
	out.amigaPaletteValid = prepared.amigaPaletteValid;
	out.amigaBlack = prepared.amigaBlack;
	out.amigaModuleGeneration = prepared.amigaModuleGeneration;
	out.amigaPresentedGeneration = prepared.amigaPresentedGeneration;
	out.atariPresentationMode = prepared.atariPresentationMode;
	out.amigaPresentationMode = prepared.amigaPresentationMode;
	return true;
}

void OldTotRenderer::commitState(OldTotRendererPreparedState &in) {
	_vm->_draw->_backSurface->blit(*in.framebuffer);
	_savedRegions.swap(in.savedRegions);
	for (uint i = 0; i < ARRAYSIZE(_atariLogicalPalette); ++i)
		_atariLogicalPalette[i] = in.atariPalette[i];
	for (uint i = 0; i < ARRAYSIZE(_amigaLogicalPalette); ++i)
		_amigaLogicalPalette[i] = in.amigaPalette[i];
	_atariPaletteValid = in.atariPaletteValid;
	_atariBlack = in.atariBlack;
	_presentation = PresentationState(_platform);

	_atariPresentedSurface = _platform == kOldTotPlatformAtariST
		? in.framebuffer : SurfacePtr();
	_atariTransitionTarget.reset();
	clearDirtyRect();
	_amigaPaletteValid = in.amigaPaletteValid;
	_amigaBlack = in.amigaBlack;
	_amigaPresentedSurface = in.amigaPresentedFrame;
	_amigaTransitionTarget.reset();
	_amigaModuleGeneration = in.amigaModuleGeneration;
	_amigaPresentedGeneration = in.amigaPresentedGeneration;

	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 0, 0,
		in.framebuffer->getWidth() - 1, in.framebuffer->getHeight() - 1);
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

void PreGob_Emmanuelle::presentFrame(bool waitForFrame, bool force) {
	if (_introSkip.fastForwarding() && !force)
		return;
	_vm->_draw->blitInvalidated();
	const byte videoMode = _scriptVM
		? _scriptVM->runtimeState().videoMode : _runtimeState.videoMode;
	_renderer.presentDosFrame(_moduleFileStem, videoMode);

	if (waitForFrame)
		_vm->_util->waitEndFrame(false);
	else
		_vm->_video->retrace();
}

void PreGob_Emmanuelle::defineCursor(uint16 descriptorOffset) {
	if (!_tot)
		return;

	OldTotDrawDescriptor descriptor;
	if (!_tot->decodeDrawDescriptor(descriptorOffset, descriptor)) {
		debugC(1, kDebugGraphics,
			   "Emmanuelle: invalid cursor descriptor 0x%04X",
			   descriptorOffset);
		return;
	}

	SurfacePtr cursor = _renderer.decodeSpriteSurface(*_tot, descriptor);
	if (!cursor || cursor->getWidth() != 16 || cursor->getHeight() != 16) {
		debugC(1, kDebugGraphics,
			   "Emmanuelle: invalid cursor image at 0x%04X",
			   descriptorOffset);
		return;
	}

	// Native input coordinates are the sprite origin; hit testing adds the
	// cursor's 8,8 center itself. Keep the backend hotspot at the origin so
	// mouse event coordinates retain that convention.
	setCursor(*cursor, 0, 0);
	_cursorDefined = true;
	_cursorDescriptor = descriptorOffset;
}

void PreGob_Emmanuelle::drawNode(uint16 nodeOffset) {
	if (!_tot || !_scriptVM ||
		uint32(nodeOffset) + 12U > _tot->size())
		return;

	OldTotDrawDescriptor descriptor;
	if (!_tot->decodeDrawDescriptor(nodeOffset, descriptor))
		return;
	const int16 x = descriptor.xIndirect()
						? (int16)_scriptVM->readVariable(descriptor.x)
						: (int16)descriptor.x;
	const int16 y = descriptor.yIndirect()
						? (int16)_scriptVM->readVariable(descriptor.y)
						: (int16)descriptor.y;
	_renderer.drawSprite(*_tot, descriptor, x, y);

}

bool PreGob_Emmanuelle::handleProtectionTextNode(uint16 nodeOffset) {
	if (!_tot || !_scriptVM || uint32(nodeOffset) + 15U > _tot->size() ||
		(_tot->uint16BEAt(nodeOffset + 13) & 0x00FF) != 0x00FF)
		return false;

	uint32 cursor = nodeOffset + 15;
	uint32 recordUnits = 0;
	while (cursor + 4 <= _tot->size() && _tot->byteAt(cursor) != 0) {
		++recordUnits;
		cursor += 4;
	}
	if (cursor >= _tot->size() || (recordUnits & 1) != 0 ||
		recordUnits == 0)
		return false;

	const uint16 choices = (uint16)(recordUnits / 2);
	const uint32 recordOffset = nodeOffset + 15 +
								8U * randomBounded(choices);
	byte raw[8];
	for (uint i = 0; i < ARRAYSIZE(raw); ++i)
		raw[i] = _tot->byteAt(recordOffset + i);

	ProtectionChallenge challenge;
	if (!decodeProtectionRecord(raw, challenge))
		return false;
	uint16 variables[7];
	for (uint i = 0; i < ARRAYSIZE(variables); ++i)
		variables[i] = _scriptVM->readVariable(i);
	installProtectionChallenge(challenge, variables);
	for (uint i = 1; i < ARRAYSIZE(variables); ++i)
		_scriptVM->writeVariable(i, variables[i]);
	return true;
}

void PreGob_Emmanuelle::renderTextPlan(uint16 nodeOffset) {
	if (!_tot || !_scriptVM)
		return;

	OldTotTextPlan plan;
	if (!planOldTotText(*_tot, *_scriptVM, nodeOffset, plan))
		return;
	if (plan.background != 0x10) {
		_vm->_draw->_backSurface->fillRect(
			plan.x, plan.y, plan.x + plan.width - 1,
			plan.y + plan.height - 1, plan.background);
	}

	Font *font = _fonts[plan.fontIndex & 1];
	if (!font)
		return;
	for (uint i = 0; i < plan.glyphs.size(); ++i) {
		const OldTotTextGlyph &glyph = plan.glyphs[i];
		font->drawLetter(*_vm->_draw->_backSurface, glyph.character,
						 glyph.x, glyph.y, glyph.color, plan.background,
						 plan.background == 0x10);
	}
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, plan.x, plan.y,
							plan.x + plan.width - 1,
							plan.y + plan.height - 1);
	_renderer.notePresentationDirtyRect(plan.x, plan.y,
								  plan.x + plan.width - 1,
								  plan.y + plan.height - 1);
}

void PreGob_Emmanuelle::textNode(uint16 nodeOffset) {
	if (!handleProtectionTextNode(nodeOffset))
		renderTextPlan(nodeOffset);
}

void PreGob_Emmanuelle::saveScreenRect(uint16 x, uint16 y, uint16 width,
									   uint16 height) {
	_renderer.saveScreenRect(x, y, width, height);
}

void PreGob_Emmanuelle::restoreScreenRect() {
	_renderer.restoreScreenRect();
}

void PreGob_Emmanuelle::fillScreen(byte color) {
	_renderer.fillScreen(color);
}

void PreGob_Emmanuelle::reinitializeVideoBackend(byte videoMode) {
	_renderer.applyDosPalette(_moduleFileStem, videoMode);
}

} // End of namespace Emmanuelle
} // End of namespace Gob
