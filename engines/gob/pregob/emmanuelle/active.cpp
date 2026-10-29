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

#include "gob/pregob/emmanuelle/active.h"
#include "gob/pregob/emmanuelle/saveload.h"
#include "gob/draw.h"
#include "gob/gob.h"
namespace Gob {
namespace Emmanuelle {
ActiveSpriteLayer::ActiveSpriteLayer(GobEngine *vm, uint slotCount)
	: _vm(vm) {
	_slots.resize(slotCount);
}

bool ActiveSpriteLayer::exportState(
		EmmanuelleRendererState &out) const {
	out.activeObjects.clear();
	for (uint i = 0; i < _slots.size(); ++i) {
		const Slot &slot = _slots[i];
		EmmanuelleActiveObjectState state;
		state.active = slot.active;
		state.x = slot.x;
		state.y = slot.y;
		state.width = slot.width;
		state.height = slot.height;
		if (slot.active) {
			if (!slot.background)
				return false;
			exportOldTotSurface(*slot.background, state.background);
		}
		out.activeObjects.push_back(state);
	}
	return true;
}

bool ActiveSpriteLayer::importState(
		const EmmanuelleRendererState &in) {
	if (in.activeObjects.size() != _slots.size() || !_vm || !_vm->_draw ||
		!_vm->_draw->_backSurface)
		return false;
	const int16 screenWidth = _vm->_draw->_backSurface->getWidth();
	const int16 screenHeight = _vm->_draw->_backSurface->getHeight();
	Common::Array<Slot> slots;
	slots.resize(_slots.size());
	for (uint i = 0; i < slots.size(); ++i) {
		const EmmanuelleActiveObjectState &state = in.activeObjects[i];
		Slot &slot = slots[i];
		slot.active = state.active;
		slot.x = state.x;
		slot.y = state.y;
		slot.width = state.width;
		slot.height = state.height;
		if (state.active && (state.x < 0 || state.y < 0 ||
			state.width <= 0 || state.height <= 0 ||
			state.x + state.width > screenWidth ||
			state.y + state.height > screenHeight ||
			!importOldTotSurface(state.background, slot.background) ||
			state.width != state.background.width ||
			state.height != state.background.height))
			return false;
	}
	_slots.swap(slots);
	return true;
}

void ActiveSpriteLayer::swapState(ActiveSpriteLayer &other) {
	_slots.swap(other._slots);
}

void ActiveSpriteLayer::restoreAll() {
	for (uint i = 0; i < _slots.size(); ++i) {
		if (_slots[i].active)
			restore(i);
		_slots[i] = Slot();
	}
}

void ActiveSpriteLayer::discardAll() {
	for (uint i = 0; i < _slots.size(); ++i)
		_slots[i] = Slot();
}

bool ActiveSpriteLayer::bounds(uint slot, SpriteBounds &out) const {
	if (slot >= _slots.size() || !_slots[slot].active)
		return false;
	const Slot &activeSlot = _slots[slot];
	out = SpriteBounds(activeSlot.x, activeSlot.y, activeSlot.width,
		activeSlot.height);
	return true;
}

uint ActiveSpriteLayer::activeCount() const {
	uint count = 0;
	for (uint i = 0; i < _slots.size(); ++i)
		if (_slots[i].active)
			++count;
	return count;
}

SurfacePtr ActiveSpriteLayer::capture(int16 x, int16 y, int16 w, int16 h) const {
	Surface *target = _vm && _vm->_draw ? _vm->_draw->_backSurface.get() : nullptr;
	if (!target || w <= 0 || h <= 0)
		return SurfacePtr();
	SurfacePtr s(new Surface(w, h, target->getBPP()));
	s->clear();
	s->blit(*target, x, y, x + w - 1, y + h - 1, 0, 0);
	return s;
}
void ActiveSpriteLayer::dirty(const SpriteBounds &r) {
	if (_vm && _vm->_draw && _vm->_draw->_backSurface && r.w > 0 && r.h > 0)
		_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, r.x, r.y, r.x + r.w - 1, r.y + r.h - 1);
}
bool ActiveSpriteLayer::restore(uint slot) {
	Surface *target = _vm && _vm->_draw ? _vm->_draw->_backSurface.get() : nullptr;
	if (slot >= _slots.size() || !_slots[slot].active ||
		!_slots[slot].background || !target)
		return false;
	Slot &s = _slots[slot];
	target->blit(*s.background, s.x, s.y);
	dirty(SpriteBounds(s.x, s.y, s.width, s.height));
	s = Slot();
	return true;
}
bool ActiveSpriteLayer::composite(uint slot, const Surface &sprite, int16 x, int16 y, int32 transparent) {
	Surface *target = _vm && _vm->_draw ? _vm->_draw->_backSurface.get() : nullptr;
	if (slot >= _slots.size() || !target)
		return false;
	Slot &s = _slots[slot];
	const int16 w = sprite.getWidth(), h = sprite.getHeight();
	const int16 ox = s.active ? s.x : x, oy = s.active ? s.y : y;
	const ActiveCompositePlan p = planActiveComposite(ox, oy, x, y, w, h);
	if (s.active && s.background)
		target->blit(*s.background, s.x, s.y);
	SurfacePtr bg = capture(x, y, w, h);
	if (!bg)
		return false;
	target->blit(sprite, x, y, transparent);
	dirty(p.alignedUnion);
	s.active = true;
	s.x = x;
	s.y = y;
	s.width = w;
	s.height = h;
	s.background = bg;
	return true;
}

bool ActiveSpriteLayer::compositePair(
	uint firstSlot, const Surface &firstSprite, int16 firstX, int16 firstY,
	int32 firstTransparent, uint secondSlot, const Surface &secondSprite,
	int16 secondX, int16 secondY, int32 secondTransparent) {
	Surface *target = _vm && _vm->_draw ? _vm->_draw->_backSurface.get() : nullptr;
	if (!target || firstSlot >= _slots.size() || secondSlot >= _slots.size() ||
		firstSlot == secondSlot)
		return false;

	Slot &first = _slots[firstSlot];
	Slot &second = _slots[secondSlot];
	const int16 firstWidth = firstSprite.getWidth();
	const int16 firstHeight = firstSprite.getHeight();
	const int16 secondWidth = secondSprite.getWidth();
	const int16 secondHeight = secondSprite.getHeight();
	const ActivePairCompositePlan plan = planActivePairComposite(
		first.active ? first.x : firstX, first.active ? first.y : firstY,
		firstX, firstY, firstWidth, firstHeight,
		second.active ? second.x : secondX, second.active ? second.y : secondY,
		secondX, secondY, secondWidth, secondHeight);

	if (first.active && first.background)
		target->blit(*first.background, first.x, first.y);
	if (second.active && second.background)
		target->blit(*second.background, second.x, second.y);

	SurfacePtr firstBackground = capture(
		firstX, firstY, firstWidth, firstHeight);
	SurfacePtr secondBackground = capture(
		secondX, secondY, secondWidth, secondHeight);
	if (!firstBackground || !secondBackground)
		return false;

	target->blit(firstSprite, firstX, firstY, firstTransparent);
	target->blit(secondSprite, secondX, secondY, secondTransparent);
	dirty(plan.alignedUnion);

	first.active = true;
	first.x = firstX;
	first.y = firstY;
	first.width = firstWidth;
	first.height = firstHeight;
	first.background = firstBackground;
	second.active = true;
	second.x = secondX;
	second.y = secondY;
	second.width = secondWidth;
	second.height = secondHeight;
	second.background = secondBackground;
	return true;
}
} // namespace Emmanuelle
} // namespace Gob
