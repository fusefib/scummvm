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
#include "graphics/surface.h"

#include "common/system.h"

#include "gob/draw.h"
#include "gob/global.h"
#include "gob/gob.h"
#include "gob/util.h"
#include "gob/video.h"
#include "gob/pregob/emmanuelle/amiga.h"
#include "gob/pregob/emmanuelle/animation.h"
#include "gob/pregob/emmanuelle/atari.h"
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
	if (_platform == kOldTotPlatformAmiga) {
		out.amigaPresentationMode = _presentation.mode();
		out.amigaPresentationVariant = _presentation.variant();
		out.amigaPresentationPhase = _presentation.phase();
		out.amigaPresentationPending = _presentation.pending();
		out.amigaPresentationActive = _presentation.active();
		out.amigaPaletteTransitionPending =
			_presentation.paletteTransitionPending();
		if (_amigaPresentedSurface)
			exportOldTotSurface(*_amigaPresentedSurface,
				out.amigaPresentedFrame);
	}
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
		if (!importOldTotSurface(state.surface, surface))
			return false;
		prepared.savedRegions.push_back(OldTotPreparedSavedRegion(
			surface, state.x, state.y));
	}
	if (!in.amigaPresentedFrame.pixels.empty() &&
		!importOldTotSurface(in.amigaPresentedFrame,
			prepared.amigaPresentedFrame))
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

	SWAP(out, prepared);
	return true;
}

void OldTotRenderer::commitState(OldTotRendererPreparedState &in) {
	_vm->_draw->_backSurface->blit(*in.framebuffer);
	// Dialogue scripts restore their windows before the next saveable choice.
	// Loading recreates screen captures without temporary caption backgrounds.
	_savedRegions.swap(in.savedRegions);
	for (uint i = 0; i < ARRAYSIZE(_atariLogicalPalette); ++i)
		_atariLogicalPalette[i] = in.atariPalette[i];
	for (uint i = 0; i < ARRAYSIZE(_amigaLogicalPalette); ++i)
		_amigaLogicalPalette[i] = in.amigaPalette[i];
	_atariPaletteValid = in.atariPaletteValid;
	_atariBlack = in.atariBlack;
	_presentation = PresentationState(_platform);
	// An armed transition is idle. Choose its variant on the next draw,
	// preserving the RNG state saved at the input boundary.
	const byte mode = _platform == kOldTotPlatformAtariST
		? in.atariPresentationMode : (_platform == kOldTotPlatformAmiga
			? in.amigaPresentationMode : 0);
	if (mode == 43 || mode == 44)
		_presentation.arm(mode);
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

	if (_platform == kOldTotPlatformAtariST && _atariPaletteValid) {
		if (_atariBlack) {
			uint16 black[16] = {0};
			showAtariWords(black);
		} else {
			showAtariWords(_atariLogicalPalette);
		}
	} else if (_platform == kOldTotPlatformAmiga && _amigaPaletteValid) {
		if (_amigaBlack) {
			uint16 black[32] = {0};
			showAmigaWords(black);
		} else {
			showAmigaWords(_amigaLogicalPalette);
		}
	}
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 0, 0,
		in.framebuffer->getWidth() - 1, in.framebuffer->getHeight() - 1);
}

void OldTotRenderer::setResourceBanks(const OldIxxBank *localBank,
									  const OldIxxBank *sharedBank) {
	_localBank = localBank;
	_sharedBank = sharedBank;
}

bool OldTotRenderer::nextPresentationStep(bool black, byte randomVariant,
		PresentationStep &step, bool sharedCadence) {
	return _presentation.nextStep(black, randomVariant, step, sharedCadence);
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
	if (!_vm || !_vm->_draw ||
		!_vm->_draw->_backSurface)
		return;
	const int16 width = (int16)_vm->_draw->_backSurface->getWidth();
	const int16 height = (int16)_vm->_draw->_backSurface->getHeight();
	if (right < 0 || bottom < 0 || left >= width || top >= height)
		return;
	left = MAX<int16>(left, 0);
	top = MAX<int16>(top, 0);
	right = MIN<int16>(right, width - 1);
	bottom = MIN<int16>(bottom, height - 1);
	if (left > right || top > bottom)
		return;

	_presentation.noteChange();
	if (_presentation.mode() != 43)
		return;
	if (!_dirtyValid) {
		_dirtyLeft = left;
		_dirtyTop = top;
		_dirtyRight = right;
		_dirtyBottom = bottom;
		_dirtyValid = true;
		return;
	}
	_dirtyLeft = MIN(_dirtyLeft, left);
	_dirtyTop = MIN(_dirtyTop, top);
	_dirtyRight = MAX(_dirtyRight, right);
	_dirtyBottom = MAX(_dirtyBottom, bottom);
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

	const Surface &back = *_vm->_draw->_backSurface;
	if (x >= back.getWidth() || y >= back.getHeight() ||
		width > back.getWidth() || height > back.getHeight())
		return false;
	// Waterfall captures extend below the screen; retain only visible pixels.
	width = MIN<uint16>(width, back.getWidth() - x);
	height = MIN<uint16>(height, back.getHeight() - y);
	SurfacePtr saved(new Surface(width, height, back.getBPP()));
	saved->blit(back, x, y, x + width - 1,
				y + height - 1, 0, 0);
	_savedRegions.push_back(OldTotPreparedSavedRegion(saved, x, y));
	return true;
}

bool OldTotRenderer::restoreScreenRect() {
	if (!_vm || !_vm->_draw || !_vm->_draw->_backSurface ||
		_savedRegions.empty())
		return false;

	const OldTotPreparedSavedRegion saved = _savedRegions.back();
	// Restoring the captured window also removes raised letters. Its saved
	// caption background is no longer needed and is released with the window.
	_savedRegions.pop_back();
	_vm->_draw->_backSurface->blit(*saved.surface, saved.x, saved.y);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, saved.x, saved.y,
							saved.x + saved.surface->getWidth() - 1,
							saved.y + saved.surface->getHeight() - 1);
	uint16 dirtyLeft = saved.x;
	uint16 dirtyRight = saved.x + saved.surface->getWidth() - 1;
	if (_platform == kOldTotPlatformAtariST)
		getAtariStRestoredDirtyBounds(
			saved.x, saved.surface->getWidth(),
			_vm->_draw->_backSurface->getWidth(), dirtyLeft, dirtyRight);
	notePresentationDirtyRect(dirtyLeft, saved.y, dirtyRight,
					   saved.y + saved.surface->getHeight() - 1);
	return true;
}

Common::Rect OldTotRenderer::restoreAndCaptureTextOverhang(const Common::Rect &textRect,
		int16 inkTop) {
	Common::Rect restored;
	if (!_vm || !_vm->_draw || !_vm->_draw->_backSurface)
		return restored;
	Surface &back = *_vm->_draw->_backSurface;
	for (uint i = _savedRegions.size(); i > 0; --i) {
		OldTotPreparedSavedRegion &saved = _savedRegions[i - 1];
		const Common::Rect window(saved.x, saved.y,
			saved.x + saved.surface->getWidth(), saved.y + saved.surface->getHeight());
		if (!window.contains(textRect))
			continue;

		// Only replacing the same text box clears its raised letters. Other
		// text must leave this caption's saved background alone.
		if (saved.textOverhangBackground && !saved.textRect.equals(textRect))
			return restored;
		if (saved.textOverhangBackground) {
			restored = saved.textOverhangRect();
			back.blit(*saved.textOverhangBackground, restored.left, restored.top);
			saved.textOverhangBackground.reset();
		}

		// French LOADFAV:10B8 -> HUD -> 1147 reuses the upper panel. Raising
		// the first caption leaves a pixel at (162,4) above the area cleared by
		// the next caption. Restore the panel pixels under those old letters;
		// enlarging a dirty rectangle would only publish the leftover pixel.
		Common::Rect band(textRect.left, MIN<int16>(inkTop, textRect.top),
			textRect.right, textRect.top);
		band.clip(Common::Rect(0, 0, back.getWidth(), back.getHeight()));
		if (!band.isEmpty() && window.contains(band)) {
			saved.textOverhangBackground = SurfacePtr(new Surface(band.width(), band.height(), back.getBPP()));
			saved.textOverhangBackground->blit(back, band.left, band.top,
				band.right - 1, band.bottom - 1, 0, 0);
			saved.textRect = textRect;
		}
		return restored;
	}
	// INIT status text has no dialogue capture. Its native background pieces
	// are redrawn before each replacement; do not create a persistent fallback.
	return restored;
}

void OldTotRenderer::discardCoveredTextOverhangBacking(const Common::Rect &redraw) {
	for (uint i = _savedRegions.size(); i > 0; --i) {
		OldTotPreparedSavedRegion &saved = _savedRegions[i - 1];
		// A covering redraw replaces the panel pixels. Drop their saved copy
		// so a later caption cannot paint the old panel onto the new artwork.
		if (saved.textOverhangBackground && redraw.contains(saved.textOverhangRect()))
			saved.textOverhangBackground.reset();
		const Common::Rect window(saved.x, saved.y,
			saved.x + saved.surface->getWidth(), saved.y + saved.surface->getHeight());
		// A nested window captures the parent's displayed caption. Its restore
		// will bring that caption back, so retain the parent's saved background.
		if (window.contains(redraw))
			return;
	}
}

void OldTotRenderer::fillScreen(byte color) {
	if (!_vm || !_vm->_draw || !_vm->_draw->_backSurface)
		return;

	Surface &back = *_vm->_draw->_backSurface;
	back.fillRect(0, 0, back.getWidth() - 1, back.getHeight() - 1, color);
	for (uint i = 0; i < _savedRegions.size(); ++i)
		_savedRegions[i].textOverhangBackground.reset();
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 0, 0, back.getWidth() - 1,
							back.getHeight() - 1);
	notePresentationDirtyRect(0, 0, back.getWidth() - 1,
		back.getHeight() - 1);
}

bool OldTotRenderer::drawPackedPayload(Common::Array<byte> &payload,
									   uint16 width, uint16 height, int16 x, int16 y, bool transparentZero,
									   bool horizontalFlip, bool verticalFlip) {
	if (payload.empty() || !_vm || !_vm->_draw || !_vm->_video ||
		!_vm->_draw->_backSurface)
		return false;

	Surface &dest = *_vm->_draw->_backSurface;
	if (_platform == kOldTotPlatformAmiga) {
		Surface decoded(width, height, 1);
		if (!decodeAmigaPackedPayload(payload, width, height, decoded))
			return false;
		for (int16 dy = 0; dy < (int16)height; ++dy) {
			const int16 targetY = y + dy;
			if (targetY < 0 || targetY >= (int16)dest.getHeight())
				continue;
			const int16 sourceY =
				verticalFlip ? height - 1 - dy : dy;
			for (int16 dx = 0; dx < (int16)width; ++dx) {
				const int16 targetX = x + dx;
				if (targetX < 0 || targetX >= (int16)dest.getWidth())
					continue;
				const int16 sourceX =
					horizontalFlip ? width - 1 - dx : dx;
				const uint32 pixel = decoded.get(sourceX, sourceY).get();
				if (!transparentZero || pixel != 0)
					dest.putPixelRaw(targetX, targetY, pixel);
			}
		}
		if (!transparentZero)
			discardCoveredTextOverhangBacking(Common::Rect(x, y, x + width, y + height));
		_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, x, y,
								x + width - 1, y + height - 1);
		notePresentationDirtyRect(x, y, x + width - 1, y + height - 1);
		return true;
	}

	if (!horizontalFlip && !verticalFlip) {
		_vm->_video->drawPackedSprite(&payload[0], width, height, x, y,
									  transparentZero ? 1 : 0, dest);
	} else {
		Surface temp(width, height, dest.getBPP());
		temp.clear();
		_vm->_video->drawPackedSprite(&payload[0], width, height, 0, 0,
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

	if (!transparentZero)
		discardCoveredTextOverhangBacking(Common::Rect(x, y, x + width, y + height));
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
	if (_platform == kOldTotPlatformAmiga) {
		if (!decodeAmigaPackedPayload(payload, desc.width, desc.height, *raw))
			return SurfacePtr();
	} else {
		_vm->_video->drawPackedSprite(&payload[0], desc.width, desc.height,
									  0, 0, 0, *raw);
	}
	if (!desc.horizontalFlip() && !desc.verticalFlip())
		return raw;

	// The decoded image is exclusively owned. The view never frees its pixels.
	Graphics::Surface image;
	image.init(desc.width, desc.height, desc.width, raw->getData(),
		Graphics::PixelFormat::createFormatCLUT8());
	const Common::Rect bounds(desc.width, desc.height);
	if (desc.horizontalFlip())
		image.flipHorizontal(bounds);
	if (desc.verticalFlip())
		image.flipVertical(bounds);
	return raw;
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

void PreGob_Emmanuelle::presentFrame(bool waitForFrame, bool force) {
	if (shouldAbortExecution() || (_introSkip.fastForwarding() && !force))
		return;
	_vm->_draw->blitInvalidated();
	if (_platformTag == kOldTotPlatformDOS) {
		const byte videoMode = _scriptVM
			? _scriptVM->runtimeState().videoMode : _runtimeState.videoMode;
		if (!_renderer.presentDosFrame(_moduleFileStem, videoMode)) {
			failHostOperation("DOS presentation");
			return;
		}
	}
	if (_platformTag == kOldTotPlatformAtariST)
		_renderer.captureAtariPresentedFrame();
	if (waitForFrame)
		_vm->_util->waitEndFrame(false);
	else
		_vm->_video->retrace();
	if (_platformTag == kOldTotPlatformDOS && !shouldAbortExecution())
		_renderer.commitDirectFrame();
}

bool PreGob_Emmanuelle::servicePresentation() {
	const bool amiga = _platformTag == kOldTotPlatformAmiga;
	const bool sharedCadence = _currentModule != kModuleInit &&
		!_fightActive && !_audioBlocking;
	if (_platformTag == kOldTotPlatformDOS && !sharedCadence)
		return false;

	bool paced = false;
	PresentationStep step;
	while (!shouldAbortExecution() && !_renderer.presentationQuiescent()) {
		byte randomVariant = _renderer.presentationVariant();
		// Mode 44 advances RNG here even when black selects a fade instead.
		if (!_renderer.presentationActive() &&
			_renderer.presentationMode() == 44)
			randomVariant = (byte)randomBounded(5);
		if (!_renderer.nextPresentationStep(amiga ? _renderer.amigaBlack() :
				_renderer.atariBlack(), randomVariant, step, sharedCadence))
			break;

		bool rendered = true;
		if (step.operation == kPresentationMode43 ||
			step.operation == kPresentationMode44) {
			if (step.phase == 0)
				rendered = amiga ? _renderer.beginAmigaTransitionFrame() :
					_renderer.beginAtariTransitionFrame();
			if (rendered && step.operation == kPresentationMode43)
				rendered = amiga ? _renderer.presentAmigaMode43Step(step.phase) :
					_renderer.presentAtariMode43Step(step.phase);
			else if (rendered && step.operation == kPresentationMode44)
				rendered = amiga
					? _renderer.presentAmigaMode44Step(step.variant, step.phase) :
					_renderer.presentAtariMode44Step(step.variant, step.phase);
		}
		if (!rendered) {
			failHostOperation("presentation step");
			break;
		}
		// Ordinary gameplay follows Amiga's native publication order:
		// wait, publish, wait, capture. Idle input service still waits once.
		if (!_introSkip.fastForwarding() &&
			(!(amiga || sharedCadence) || step.operation != kPresentationFrame || step.phase != 0)) {
			_vm->_draw->blitInvalidated();
			if (_platformTag == kOldTotPlatformDOS &&
				!_renderer.presentDosFrame(_moduleFileStem,
					_scriptVM ? _scriptVM->runtimeState().videoMode : _runtimeState.videoMode)) {
				failHostOperation("DOS presentation");
				break;
			}
		}
		waitPresentationFrames();
		paced = true;
		if (step.operation == kPresentationFadeFromBlack && !shouldAbortExecution()) {
			if (amiga)
				fadeAmigaFromBlack();
			else
				fadeAtariFromBlack();
		}
		if (step.complete && !shouldAbortExecution()) {
			if (amiga)
				_renderer.captureAmigaPresentedFrame();
			else if (_platformTag == kOldTotPlatformAtariST)
				_renderer.captureAtariPresentedFrame();
		}
		if (_diagnosticTraceEnabled && step.operation != kPresentationFrame &&
			(step.phase == 0 || step.complete))
			debugC(3, kDebugGraphics,
				"EMMA_TRACE event=%s_present module=%s operation=%u "
				"variant=%u phase=%u complete=%u mode=%u "
				"frame_pending=%u active=%u palette_transition=%u black=%u",
				amiga ? "amiga" : "atari", _moduleFileStem.c_str(),
				(uint)step.operation, (uint)step.variant, (uint)step.phase,
				step.complete ? 1U : 0U,
				(uint)_renderer.presentationMode(),
				_renderer.presentationPending() ? 1U : 0U,
				_renderer.presentationActive() ? 1U : 0U,
				amiga && _renderer.amigaPaletteTransitionPending() ? 1U : 0U,
				(amiga ? _renderer.amigaBlack() : _renderer.atariBlack()) ? 1U : 0U);
		if (step.complete)
			break;
	}
	return paced;
}

void PreGob_Emmanuelle::waitForMinimumDuration(
	uint32 startTime, uint32 minimumMilliseconds) {
	while (!shouldAbortExecution() && !_introSkip.fastForwarding()) {
		const uint32 elapsed = g_system->getMillis() - startTime;
		const uint32 remaining = minimumDurationRemaining(
			elapsed, minimumMilliseconds);
		if (remaining == 0)
			break;

		delay(remaining < 20 ? (uint16)remaining : 20);
	}
}

bool PreGob_Emmanuelle::initialTomahawkPresentationVisible() const {
	const bool atari = _platformTag == kOldTotPlatformAtariST;
	const bool amiga = _platformTag == kOldTotPlatformAmiga;
	return isTitlePresentationVisible(
		_platformTag,
		atari ? _renderer.atariBlack() :
			(amiga && _renderer.amigaBlack()),
		_renderer.presentationPending(),
		_renderer.presentationActive(),
		amiga && _renderer.amigaPaletteTransitionPending());
}

void PreGob_Emmanuelle::noteInitialTomahawkVisible() {
	if (_initialTitleState != kInitialTitleDrawn ||
		!_initialInitPresentation || _introSkip.fastForwarding() ||
		!initialTomahawkPresentationVisible())
		return;

	_initialTitleState = kInitialTitleVisible;
	_initialTomahawkVisibleStart = g_system->getMillis();
	if (_diagnosticTraceEnabled)
		debugC(2, kDebugGraphics,
			"EMMA_TRACE event=tomahawk_visible platform=%u time_ms=%u",
			(uint)_platformTag, (uint)_initialTomahawkVisibleStart);
}

void PreGob_Emmanuelle::ensureInitialTomahawkHandoff() {
	if (_initialTitleState == kInitialTitleDrawn && !_vm->shouldQuit()) {
		if (!initialTomahawkPresentationVisible())
			resolveIntroPresentation();
		noteInitialTomahawkVisible();
	}
}

void PreGob_Emmanuelle::completeInitialTomahawkHandoff(
	ModuleId moduleId) {
	if (_initialTitleState != kInitialTitleVisible)
		return;
	_initialTitleState = kInitialTitleNone;
	if (moduleId != kModuleLoadHot)
		return;
	const uint16 minimum = kInitialTomahawkMinimumMilliseconds;
	if (!_vm->shouldQuit())
		waitForMinimumDuration(_initialTomahawkVisibleStart, minimum);
	discardPendingInput();
	if (_diagnosticTraceEnabled)
		debugC(2, kDebugFileIO,
			"EMMA_TRACE event=tomahawk_handoff_end platform=%u "
			"visible_elapsed_ms=%u minimum_ms=%u",
			(uint)_platformTag,
			(uint)(g_system->getMillis() - _initialTomahawkVisibleStart),
			(uint)minimum);
}

bool PreGob_Emmanuelle::resolveIntroPresentation() {
	if (_vm->shouldQuit())
		return false;
	if (_platformTag == kOldTotPlatformAtariST) {
		servicePresentation();
		if (_renderer.atariBlack())
			_renderer.restoreAtariPaletteImmediately();
		if (!_renderer.presentationQuiescent() ||
			_renderer.atariBlack())
			return false;
	} else if (_platformTag == kOldTotPlatformAmiga) {
		servicePresentation();
		if (_renderer.amigaBlack())
			_renderer.restoreAmigaPaletteImmediately();
		if (!_renderer.presentationQuiescent() ||
			_renderer.amigaPaletteTransitionPending() ||
			_renderer.amigaBlack())
			return false;
	}
	presentFrame(false, true);
	if (!_vm->shouldQuit() &&
		_platformTag == kOldTotPlatformDOS && _initialInitPresentation)
		_dosInitialFramePresented = true;
	return !_vm->shouldQuit();
}

void PreGob_Emmanuelle::defineCursor(uint16 descriptorOffset) {
	OldTotDrawDescriptor descriptor;
	if (!_tot || !_tot->decodeDrawDescriptor(descriptorOffset, descriptor)) {
		failHostOperation("cursor descriptor", descriptorOffset);
		return;
	}

	SurfacePtr cursor = _renderer.decodeSpriteSurface(*_tot, descriptor);
	if (!cursor || cursor->getWidth() != 16 || cursor->getHeight() != 16) {
		failHostOperation("cursor image", descriptorOffset);
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
	OldTotDrawDescriptor descriptor;
	if (!_tot || !_scriptVM ||
		!_tot->decodeDrawDescriptor(nodeOffset, descriptor)) {
		failHostOperation("draw descriptor", nodeOffset);
		return;
	}
	const int16 x = descriptor.xIndirect()
						? (int16)_scriptVM->readVariable(descriptor.x)
						: (int16)descriptor.x;
	const int16 y = descriptor.yIndirect()
						? (int16)_scriptVM->readVariable(descriptor.y)
						: (int16)descriptor.y;
	if (!_renderer.drawSprite(*_tot, descriptor, x, y)) {
		failHostOperation("sprite draw", nodeOffset);
		return;
	}
	const bool initialTomahawkDraw = _initialInitPresentation &&
		!_introSkip.available() && descriptor.width == 320 &&
		descriptor.height == 200;
	if (initialTomahawkDraw && _initialTitleState == kInitialTitleNone)
		_initialTitleState = kInitialTitleDrawn;
	if (_platformTag == kOldTotPlatformDOS &&
		_initialInitPresentation &&
		!_introSkip.fastForwarding()) {
		// DOS draws directly to video memory. Reproduce that visibility and
		// decoding time throughout fresh INIT, including the Tomahawk title.
		const byte videoMode = _scriptVM->runtimeState().videoMode;
		const uint16 drawDelay = dosIntroDrawDelayMilliseconds(
			videoMode, descriptor.width, descriptor.height);
		const bool assemblingInitialFrame = !_dosInitialFramePresented;
		const bool initialFrameComplete = assemblingInitialFrame &&
			extendDosInitialFrameCoverage(_dosInitialFrameCoverage,
				_dosInitialFrameHasCoverage, x, y, descriptor.width,
				descriptor.height);
		if (assemblingInitialFrame) {
			delay(drawDelay);
			if (initialFrameComplete && !_vm->shouldQuit()) {
				presentFrame(false);
				_dosInitialFramePresented = true;
			}
		} else {
			presentFrame(false);
			if (initialTomahawkDraw)
				noteInitialTomahawkVisible();
			delay(drawDelay);
		}
	}
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

// Align sharp-s and cedillas using the original font bitmaps. Text wrapping,
// background cells and script-defined text boxes keep their original positions;
// only the letters move. Earlier lines move up when needed to leave room.
//
// A raised letter can extend above the area cleared by the next caption.
// Dialogue windows therefore save that narrow strip of panel background before
// drawing text. restoreAndCaptureTextOverhang() puts it back on replacement;
// the native window restore releases it. This preserves the panel's artwork.
int16 alignOldTotTextBaselines(OldTotTextPlan &plan, const Font &font,
		bool keepSingleLineBaseline) {
	bool needsAlignment = false;
	for (uint i = 0; i < plan.glyphs.size(); ++i) {
		OldTotTextGlyph &glyph = plan.glyphs[i];
		glyph.y = glyph.cellY;
		needsAlignment |= glyph.character == 0xE1 || glyph.character == 0x80 || glyph.character == 0x87;
	}
	if (!needsAlignment)
		return plan.y;
	const bool keepBaseline = keepSingleLineBaseline &&
		plan.glyphs[0].cellY == plan.glyphs.back().cellY &&
		plan.glyphs[0].cellY + font.getCharHeight() + 2 <= plan.y + plan.height;

	// Place native E1 (sharp-s) one row below ordinary letters, 87/80 (c/C
	// with cedilla) two rows below. Lift the line to fit the lowest character.
	// Mixed German LOADHOT:23C4 uses offsets -2/-1/0 respectively.
	byte pixels[64];
	Surface mask(8, 8, 1, pixels);
	int lift = 0;
	int nextLineY = 0;
	int nextInkTop = 32767;
	int inkTop = plan.y;
	uint end = plan.glyphs.size();
	while (end) {
		const int cellY = plan.glyphs[end - 1].cellY;
		uint start = end - 1;
		while (start && plan.glyphs[start - 1].cellY == cellY)
			--start;
		bool visibleLine = false;
		for (uint i = start; i < end; ++i)
			visibleLine |= plan.glyphs[i].character != ' ';
		if (!visibleLine) {
			end = start;
			continue;
		}

		// Work backward so a raised line also raises its contiguous predecessors.
		// Extra blank-line spacing absorbs that carry: the German protection
		// screen's later colour label must not push its top instructions offscreen.
		if (nextInkTop != 32767)
			lift = MAX(0, lift - MAX(0, nextLineY - cellY - plan.lineAdvance));
		for (uint i = start; i < end; ++i) {
			OldTotTextGlyph &glyph = plan.glyphs[i];
			const int offset = glyph.character == 0xE1 ? 1 :
				(glyph.character == 0x80 || glyph.character == 0x87 ? 2 : 0);
			glyph.y = glyph.cellY + offset;
			if (!keepBaseline)
				lift = MAX(lift, offset);
		}
		for (uint i = start; i < end; ++i)
			plan.glyphs[i].y -= lift;

		// Measure ink rather than cells. The two native fonts have different
		// vertical extents; a full-cell collision test would lift too much.
		int top = 32767;
		int bottom = -32768;
		for (uint i = start; i < end; ++i) {
			const OldTotTextGlyph &glyph = plan.glyphs[i];
			mask.clear();
			font.drawLetterRaw(mask, glyph.character, 0, 0, 1, 0, false);
			for (uint row = 0; row < 8; ++row) {
				for (uint col = 0; col < 8; ++col) {
					if (pixels[row * 8 + col]) {
						top = MIN(top, glyph.y + (int)row);
						bottom = MAX(bottom, glyph.y + (int)row);
					}
				}
			}
		}
		if (bottom >= nextInkTop) {
			const int extra = bottom - nextInkTop + 1;
			lift += extra;
			top -= extra;
			for (uint i = start; i < end; ++i)
				plan.glyphs[i].y -= extra;
		}
		if (top != 32767)
			nextInkTop = top;
		inkTop = MIN(inkTop, top);
		nextLineY = cellY;
		end = start;
	}
	return inkTop;
}

void PreGob_Emmanuelle::renderTextPlan(uint16 nodeOffset) {
	OldTotTextPlan plan;
	if (!_tot || !_scriptVM ||
		!planOldTotText(*_tot, *_scriptVM, nodeOffset, plan)) {
		failHostOperation("text plan", nodeOffset);
		return;
	}
	Font *font = _fonts[plan.fontIndex & 1];
	if (!font) {
		failHostOperation("text font", nodeOffset);
		return;
	}
	// Keep airport names level; the box leaves room for the cedilla.
	const int16 inkTop = alignOldTotTextBaselines(plan, *font,
		_currentModule == kModuleLoadAero);
	if (_platformTag == kOldTotPlatformAmiga) {
		byte colors[16];
		if (!getAmigaTextColorTable(_moduleFileStem, colors) || plan.background > 16) {
			failHostOperation("text colors", nodeOffset);
			return;
		}
		// Script color 15 is pale yellow (23) in LOADCAS, not green (15).
		if (plan.background != 0x10)
			plan.background = colors[plan.background];
		for (uint i = 0; i < plan.glyphs.size(); ++i) {
			if (plan.glyphs[i].color >= 16) {
				failHostOperation("text colors", nodeOffset);
				return;
			}
			plan.glyphs[i].color = colors[plan.glyphs[i].color];
		}
	}
	const bool amiga = _platformTag == kOldTotPlatformAmiga;
	Common::Rect dirty(plan.x, plan.y, plan.x + plan.width, plan.y + plan.height);
	Common::Rect background = dirty;
	// Transparent text overlays the current caption rather than replacing it.
	const Common::Rect restored = plan.background == 0x10 ? Common::Rect() :
		_renderer.restoreAndCaptureTextOverhang(dirty, inkTop);
	if (!restored.isEmpty())
		dirty.extend(restored);
	if (amiga) {
		// Amiga fills whole eight-pixel units, then paints only glyph strokes.
		// In LOADCAS this keeps the x=83 text fill inside its x=80 bubble.
		background.left = plan.x & ~7;
		background.right = background.left + (plan.width & ~7);
	}
	const Common::Rect bounds(0, 0, _vm->_draw->_backSurface->getWidth(),
		_vm->_draw->_backSurface->getHeight());
	background.clip(bounds);
	if (plan.background != 0x10 && !background.isEmpty()) {
		_vm->_draw->_backSurface->fillRect(background.left, background.top,
			background.right - 1, background.bottom - 1, plan.background);
		dirty.extend(background);
	}
	for (uint i = 0; i < plan.glyphs.size(); ++i) {
		const OldTotTextGlyph &glyph = plan.glyphs[i];
		// DOS/ST's opaque cells remain at their nominal Y. Paint shifted ink
		// transparently so a raised letter cannot expand the original fill.
		if (!amiga && plan.background != 0x10) {
			_vm->_draw->_backSurface->fillRect(glyph.x, glyph.cellY,
				glyph.x + font->getCharWidth(glyph.character) - 1,
				glyph.cellY + font->getCharHeight() - 1, plan.background);
		}
		if (glyph.y < 0) {
			// Font accepts unsigned Y. Clip the unchanged 8x8 bitmap locally
			// rather than losing its visible strokes when a line crosses Y=0.
			byte pixels[64];
			Surface mask(8, 8, 1, pixels);
			mask.clear();
			font->drawLetterRaw(mask, glyph.character, 0, 0, 1, 0, false);
			for (int y = MAX(0, -glyph.y); y < 8; ++y) {
				for (int x = 0; x < 8; ++x) {
					if (pixels[y * 8 + x] && bounds.contains(glyph.x + x, glyph.y + y))
						_vm->_draw->_backSurface->putPixel(glyph.x + x, glyph.y + y, glyph.color);
				}
			}
		} else {
			font->drawLetter(*_vm->_draw->_backSurface, glyph.character, glyph.x, glyph.y,
				glyph.color, plan.background, true);
		}
		dirty.extend(Common::Rect(glyph.x, glyph.y,
			glyph.x + font->getCharWidth(glyph.character), glyph.y + font->getCharHeight()));
	}
	dirty.clip(bounds);
	if (!dirty.isEmpty()) {
		_vm->_draw->dirtiedRect(_vm->_draw->_backSurface,
			dirty.left, dirty.top, dirty.right - 1, dirty.bottom - 1);
		_renderer.notePresentationDirtyRect(
			dirty.left, dirty.top, dirty.right - 1, dirty.bottom - 1);
	}
}

void PreGob_Emmanuelle::textNode(uint16 nodeOffset) {
	if (!handleProtectionTextNode(nodeOffset))
		renderTextPlan(nodeOffset);
}

bool PreGob_Emmanuelle::renderAnimationFrame(
	const OldTotAnimationSetup &setup, uint16 step, byte frame,
	int16 baseX, int16 baseY) {
	if (!_tot || !_scriptVM ||
		setup.frameDescriptors.empty() || setup.slot >= 20)
		return false;

	AnimationFramePlan framePlan;
	if (!planAnimationFrame(setup, baseX, baseY, step, frame, framePlan))
		return false;
	OldTotDrawDescriptor descriptor;
	if (!_tot->decodeDrawDescriptor(framePlan.descriptorOffset, descriptor))
		return false;
	SurfacePtr sprite = _renderer.decodeSpriteSurface(*_tot, descriptor);
	if (!sprite)
		return false;
	SurfacePtr cell = _renderer.buildMaskedCell(
		*sprite, framePlan.cellWidth, framePlan.cellHeight, 0, 0,
		descriptor.transparentZero());
	if (!cell)
		return false;
	SpriteBounds previousBounds;
	const bool hadPrevious = _active.bounds(setup.slot, previousBounds);
	const bool composited = _active.composite(
		setup.slot, *cell, framePlan.x, framePlan.y, 0xFF);
	if (composited) {
		if (hadPrevious)
			_renderer.notePresentationDirtyRect(
				previousBounds.x, previousBounds.y,
				previousBounds.x + previousBounds.w - 1,
				previousBounds.y + previousBounds.h - 1);
		_renderer.notePresentationDirtyRect(
			framePlan.x, framePlan.y, framePlan.x + framePlan.cellWidth - 1,
			framePlan.y + framePlan.cellHeight - 1);
	}
	return composited;
}

void PreGob_Emmanuelle::animationNode(uint16 nodeOffset) {
	if (!_servicingTimer)
		_schedulerClockRunning = false;
	OldTotAnimationSetup setup;
	if (!_tot || !_scriptVM ||
		!_tot->decodeAnimationSetup(nodeOffset, setup)) {
		failHostOperation("animation setup", nodeOffset);
		return;
	}

	int16 baseX = 0;
	int16 baseY = 0;
	uint16 firstDescriptor = 0;
	if (setup.originX) {
		baseX = (int16)_scriptVM->readVariable(setup.originX);
		baseY = (int16)_scriptVM->readVariable(setup.originY);
	} else if (setup.descriptorForFrame(1, firstDescriptor)) {
		OldTotDrawDescriptor descriptor;
		if (_tot->decodeDrawDescriptor(firstDescriptor, descriptor)) {
			baseX = descriptor.xIndirect()
						? (int16)_scriptVM->readVariable(descriptor.x)
						: (int16)descriptor.x;
			baseY = descriptor.yIndirect()
						? (int16)_scriptVM->readVariable(descriptor.y)
						: (int16)descriptor.y;
		}
	}

	if (setup.flags & kAnimStepped) {
		for (uint16 step = 0; step < setup.stepCount && !shouldAbortExecution();
			 ++step) {
			// Known animations use up to 8 frames; a wider counter also
			// handles the inclusive byte limit without wrapping.
			for (uint16 frame = 1;
				 frame <= setup.frameMax && !shouldAbortExecution(); ++frame) {
				const bool rendered = renderAnimationFrame(
					setup, step, (byte)frame, baseX, baseY);
				if (!rendered && !setup.frameDescriptors.empty() && setup.slot < 20) {
					failHostOperation("animation frame", nodeOffset);
					return;
				}
				if (_currentModule != kModuleInit && rendered) {
					servicePresentation();
				} else if (_platformTag == kOldTotPlatformDOS && rendered &&
					!_introSkip.fastForwarding()) {
					presentFrame(true);
					processGameEvents();
				} else if (_platformTag == kOldTotPlatformAmiga && rendered) {
					servicePresentation();
					delay(amigaNativeDelayMilliseconds(150));
				} else if (_platformTag == kOldTotPlatformAtariST && rendered) {
					servicePresentation();
					waitPresentationFrames(atariStAnimationVblCount());
				}
				if (shouldAbortExecution())
					break;
				// Gameplay frames sleep after presentation; INIT keeps its native holds.
				if (_currentModule != kModuleInit) {
					if (rendered) {
						const uint16 milliseconds = 100 + (uint16)setup.frameDelay * _animationSpeedFactor * 10;
						if (_platformTag == kOldTotPlatformAtariST)
							delayAtariAnimation(milliseconds);
						else
							delay(milliseconds);
					}
				} else if (setup.frameDelay) {
					const uint16 units = (uint16)setup.frameDelay * 10;
					if (_platformTag == kOldTotPlatformAtariST)
						delayAtariAnimation(units);
					else if (_platformTag == kOldTotPlatformAmiga)
						delay(amigaNativeDelayMilliseconds(units));
					else
						delay(units);
				}
				if (_introSkip.fastForwarding())
					processGameEvents();
			}
		}
	}
	if (setup.flags & kAnimClear) {
		SpriteBounds restoredBounds;
		const bool hadActive = _active.bounds(setup.slot, restoredBounds);
		if (hadActive && !_active.restore(setup.slot)) {
			failHostOperation("animation restore", nodeOffset);
			return;
		}
		if (hadActive)
			_renderer.notePresentationDirtyRect(
				restoredBounds.x, restoredBounds.y,
				restoredBounds.x + restoredBounds.w - 1,
				restoredBounds.y + restoredBounds.h - 1);
	}
}

bool PreGob_Emmanuelle::saveScreenRect(uint16 x, uint16 y, uint16 width,
									   uint16 height) {
	if (_renderer.saveScreenRect(x, y, width, height))
		return true;
	failHostOperation("save screen region");
	return false;
}

bool PreGob_Emmanuelle::restoreScreenRect() {
	if (_renderer.restoreScreenRect())
		return true;
	failHostOperation("restore screen region");
	return false;
}

void PreGob_Emmanuelle::fillScreen(byte color) {
	_renderer.fillScreen(color);
}

bool PreGob_Emmanuelle::buildFightActor(
	bool rightActor, const FightActor &actor, SurfacePtr &cell,
	FightActorPlacement &placement) {
	if (!_recupTot || _recupRoots.size() != 40)
		return false;
	const byte actionIndex = fightRecupIndex(
		rightActor, actor.action, actor.frame);
	if (actionIndex == 0xFF || actionIndex >= _recupRoots.size())
		return false;

	OldTotDrawDescriptor descriptor;
	if (!_recupTot->decodeDrawDescriptor(_recupRoots[actionIndex], descriptor))
		return false;
	SurfacePtr sprite = _renderer.decodeSpriteSurface(
		*_recupTot, descriptor);
	if (!sprite)
		return false;

	if (!fightPlacement(rightActor, actor.action, actor.frame,
							  actor.anchorX, sprite->getWidth(), sprite->getHeight(), placement))
		return false;
	cell = _renderer.buildFightCell(
		*sprite, placement.cellWidth, placement.cellHeight,
		placement.x - placement.cellX, placement.y - placement.cellY);
	return cell;
}

bool PreGob_Emmanuelle::renderFightActors(const FightState &state) {
	SurfacePtr leftCell;
	SurfacePtr rightCell;
	FightActorPlacement leftPlacement;
	FightActorPlacement rightPlacement;
	if (!buildFightActor(false, state.left, leftCell, leftPlacement) ||
		!buildFightActor(true, state.right, rightCell, rightPlacement))
		return false;
	SpriteBounds previousLeft;
	SpriteBounds previousRight;
	const bool hadPreviousLeft = _active.bounds(0, previousLeft);
	const bool hadPreviousRight = _active.bounds(1, previousRight);

	const bool composited = _active.compositePair(
		0, *leftCell, leftPlacement.cellX, leftPlacement.cellY, 0xFF,
		1, *rightCell, rightPlacement.cellX, rightPlacement.cellY, 0xFF);
	if (composited) {
		if (hadPreviousLeft)
			_renderer.notePresentationDirtyRect(
				previousLeft.x, previousLeft.y,
				previousLeft.x + previousLeft.w - 1,
				previousLeft.y + previousLeft.h - 1);
		if (hadPreviousRight)
			_renderer.notePresentationDirtyRect(
				previousRight.x, previousRight.y,
				previousRight.x + previousRight.w - 1,
				previousRight.y + previousRight.h - 1);
		_renderer.notePresentationDirtyRect(leftPlacement.cellX,
			leftPlacement.cellY,
			leftPlacement.cellX + leftPlacement.cellWidth - 1,
			leftPlacement.cellY + leftPlacement.cellHeight - 1);
		_renderer.notePresentationDirtyRect(rightPlacement.cellX,
			rightPlacement.cellY,
			rightPlacement.cellX + rightPlacement.cellWidth - 1,
			rightPlacement.cellY + rightPlacement.cellHeight - 1);
	}
	return composited;
}

void PreGob_Emmanuelle::reinitializeVideoBackend(byte videoMode) {
	bool applied;
	if (_platformTag == kOldTotPlatformAmiga)
		applied = _renderer.applyAmigaModulePalette(_moduleFileStem);
	else if (_platformTag == kOldTotPlatformAtariST)
		applied = _renderer.applyAtariModulePalette(_moduleFileStem);
	else
		applied = _renderer.applyDosPalette(_moduleFileStem, videoMode);
	if (!applied)
		failHostOperation("video palette");
}

} // End of namespace Emmanuelle
} // End of namespace Gob
