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

#ifndef GOB_PREGOB_EMMANUELLE_ACTIVE_H
#define GOB_PREGOB_EMMANUELLE_ACTIVE_H
#include "common/array.h"
#include "common/scummsys.h"
#include "gob/pregob/emmanuelle/gameflow.h"
#include "gob/surface.h"
namespace Gob {
class GobEngine;
namespace Emmanuelle {
struct EmmanuelleRendererState;

class ActiveSpriteLayer {
public:
	explicit ActiveSpriteLayer(GobEngine *vm, uint slotCount = 20);
	void restoreAll();
	void discardAll();
	bool active(uint slot) const;
	bool bounds(uint slot, SpriteBounds &out) const;
	uint activeCount() const;
	bool restore(uint slot);
	bool composite(uint slot, const Surface &sprite, int16 x, int16 y, int32 transparent = -1);
	bool compositePair(uint firstSlot, const Surface &firstSprite,
					   int16 firstX, int16 firstY, int32 firstTransparent,
					   uint secondSlot, const Surface &secondSprite,
					   int16 secondX, int16 secondY, int32 secondTransparent);
	bool exportState(EmmanuelleRendererState &out) const;
	bool importState(const EmmanuelleRendererState &in);
	void swapState(ActiveSpriteLayer &other);

private:
	struct Slot {
		Slot() : active(false), x(0), y(0), width(0), height(0) {}
		bool active;
		int16 x, y, width, height;
		SurfacePtr background;
	};
	SurfacePtr capture(int16 x, int16 y, int16 width, int16 height) const;
	void dirty(const SpriteBounds &r);
	GobEngine *_vm;
	Common::Array<Slot> _slots;
};

} // namespace Emmanuelle
} // namespace Gob

#endif // GOB_PREGOB_EMMANUELLE_ACTIVE_H
