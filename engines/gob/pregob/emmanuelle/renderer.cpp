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

OldTotRenderer::OldTotRenderer(GobEngine *vm) :
	_vm(vm) {

	// The module host owns both resource banks.
	_localBank = nullptr;
	_sharedBank = nullptr;
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

	return true;
}

bool OldTotRenderer::prepareState(const EmmanuelleRendererState &in,
		OldTotRendererPreparedState &out) const {
	if (!_vm || !_vm->_draw || !_vm->_draw->_backSurface)
		return false;
	return prepareOldTotRendererState(in,
		_vm->_draw->_backSurface->getWidth(),
		_vm->_draw->_backSurface->getHeight(),
		_vm->_draw->_backSurface->getBPP(), out);
}

bool prepareOldTotRendererState(const EmmanuelleRendererState &in,
		uint16 width, uint16 height, byte bytesPerPixel, OldTotRendererPreparedState &out) {
	if (!validateEmmanuelleRendererState(in, width, height, bytesPerPixel))
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

	out.framebuffer = prepared.framebuffer;
	out.savedRegions.swap(prepared.savedRegions);
	return true;
}

void OldTotRenderer::commitState(OldTotRendererPreparedState &in) {
	_vm->_draw->_backSurface->blit(*in.framebuffer);
	_savedRegions.swap(in.savedRegions);

	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 0, 0,
		in.framebuffer->getWidth() - 1, in.framebuffer->getHeight() - 1);
}

void OldTotRenderer::setResourceBanks(const OldIxxBank *localBank,
									  const OldIxxBank *sharedBank) {
	_localBank = localBank;
	_sharedBank = sharedBank;
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

	return true;
}

void OldTotRenderer::fillScreen(byte color) {
	if (!_vm || !_vm->_draw || !_vm->_draw->_backSurface)
		return;

	Surface &back = *_vm->_draw->_backSurface;
	back.fillRect(0, 0, back.getWidth() - 1, back.getHeight() - 1, color);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 0, 0, back.getWidth() - 1,
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
							 (uint32)desc.width * desc.height, payload))
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
									(uint32)desc.width * desc.height, payload)) {
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
}

void PreGob_Emmanuelle::textNode(uint16 nodeOffset) {
	if (!handleProtectionTextNode(nodeOffset))
		renderTextPlan(nodeOffset);
}

bool PreGob_Emmanuelle::renderAnimationFrame(
	const OldTotAnimationSetup &setup, uint16 step, byte frame,
	int16 baseX, int16 baseY) {
	if (!_tot || !_scriptVM ||
		!setup.hasDescriptor || setup.slot >= 20)
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
	const bool composited = _active.composite(
		setup.slot, *cell, framePlan.x, framePlan.y, 0xFF);
	return composited;
}

void PreGob_Emmanuelle::animationNode(uint16 nodeOffset) {
	OldTotAnimationSetup setup;
	if (!_tot || !_scriptVM ||
		!_tot->decodeAnimationSetup(nodeOffset, setup))
		return;

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
		for (uint16 step = 0; step < setup.stepCount && !_vm->shouldQuit();
			 ++step) {
			// Known animations use up to 8 frames; a wider counter also
			// handles the inclusive byte limit without wrapping.
			for (uint16 frame = 1;
				 frame <= setup.frameMax && !_vm->shouldQuit(); ++frame) {
				const bool rendered = renderAnimationFrame(
					setup, step, (byte)frame, baseX, baseY);
				if (_platformTag == kOldTotPlatformDOS && rendered &&
					!_introSkip.fastForwarding()) {
					presentFrame(true);
					processGameEvents();
				}

				if (_vm->shouldQuit())
					break;
				if (setup.frameDelay) {
					const uint16 units = (uint16)setup.frameDelay * 10;
					delay(units);
				}
				if (_introSkip.fastForwarding())
					processGameEvents();
			}
		}
	}
	if (setup.flags & kAnimClear) {
		_active.restore(setup.slot);
	}
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
