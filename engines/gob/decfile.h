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

#ifndef GOB_DECFILE_H
#define GOB_DECFILE_H

#include "common/system.h"
#include "common/array.h"
#include "common/ptr.h"

namespace Common {
	class String;
	class SeekableReadStreamEndian;
	struct Rect;
}

namespace Gob {

class GobEngine;
class Surface;
class CMPFile;
class CMPFileProvider;

/** A DEC file, describing a "decal" (background).
 *
 *  Used in hardcoded "actiony" parts of gob games.
 *  The principle is similar to a Static in Scenery (see scenery.cpp), but
 *  instead of referencing indices in the sprites array, DECs reference sprites
 *  directly by filename.
 */
class DECFile {
public:
	DECFile(GobEngine *vm, const Common::String &fileName,
	        uint16 width, uint16 height, uint8 bpp = 1, CMPFileProvider *sheets = nullptr);

	/** Draw the background, including all default layer parts. */
	void draw(Surface &dest, uint16 scene = 0) const;

	/** Explicitly draw the backdrop. */
	void drawBackdrop(Surface &dest, int16 x = 0, int16 y = 0) const;

	/** Draw this scene's parts in their native priority order. */
	void drawParts(Surface &dest, uint16 scene = 0) const;

	/** Restore the foreground intersecting an actor's area after drawing it. */
	void drawForeground(Surface &dest, uint16 scene, const Common::Rect &area, int16 depth) const;

	bool setPartX(uint16 scene, uint16 part, int16 x);

	/** Explicitly draw a layer part. */
	void drawLayer(Surface &dest, uint16 layer, uint16 part,
	               uint16 x, uint16 y, int32 transp = -1) const;

private:
	struct Part {
		uint8 layer;
		uint8 part;
		uint8 priority;

		uint16 x;
		uint16 y;
		bool transp;
	};

	typedef Common::SharedPtr<const CMPFile> Layer;
	typedef Common::Array<Layer>     LayerArray;
	typedef Common::Array<Part>      PartArray;
	typedef Common::Array<PartArray> SceneArray;

	GobEngine *_vm;

	uint16 _width;
	uint16 _height;
	uint8  _bpp;

	byte _hasPadding;

	Layer _backdrop;

	LayerArray _layers;
	SceneArray _scenes;


	void load(Common::SeekableReadStreamEndian &dec, const Common::String &fileName, CMPFileProvider *sheets);

	Layer loadLayer(Common::SeekableReadStreamEndian &dec, CMPFileProvider *sheets);

	void loadParts(PartArray &parts, Common::SeekableReadStreamEndian &dec);
	void loadPart(Part &part, Common::SeekableReadStreamEndian &dec);
};

} // End of namespace Gob

#endif // GOB_DECFILE_H
