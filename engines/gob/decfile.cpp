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

#include "common/rect.h"
#include "common/str.h"
#include "common/stream.h"
#include "common/substream.h"

#include "gob/gob.h"
#include "gob/util.h"
#include "gob/dataio.h"
#include "gob/surface.h"
#include "gob/video.h"
#include "gob/cmpfile.h"
#include "gob/decfile.h"

namespace Gob {

DECFile::DECFile(GobEngine *vm, const Common::String &fileName,
				 uint16 width, uint16 height, uint8 bpp) : _vm(vm),
	_width(width), _height(height), _bpp(bpp), _hasPadding(false), _backdrop(nullptr) {

	bool bigEndian = false;
	Common::String endianFileName = fileName;

	if ((_vm->getEndiannessMethod() == kEndiannessMethodAltFile) &&
	    !_vm->_dataIO->hasFile(fileName)) {
		// If the game has alternate big-endian files, look if one exist

		Common::String alternateFileName = fileName;
		alternateFileName.setChar('_', 0);

		if (_vm->_dataIO->hasFile(alternateFileName)) {
			bigEndian      = true;
			endianFileName = alternateFileName;
		}
	} else if ((_vm->getEndiannessMethod() == kEndiannessMethodBE) ||
	           ((_vm->getEndiannessMethod() == kEndiannessMethodSystem) &&
	            (_vm->getEndianness() == kEndiannessBE)))
		// Game always little endian or it follows the system and it is big endian
		bigEndian = true;

	Common::SeekableReadStream *ani = _vm->_dataIO->getFile(endianFileName);
	if (ani) {
		Common::SeekableReadStreamEndianWrapper sub(ani, bigEndian, DisposeAfterUse::YES);

		// The big endian version pads a few fields to even size
		_hasPadding = bigEndian;

		load(sub, fileName);
		return;
	}

	warning("DECFile::DECFile(): No such file \"%s\" (\"%s\")", endianFileName.c_str(), fileName.c_str());
}

DECFile::~DECFile() {
	delete _backdrop;

	for (LayerArray::iterator l = _layers.begin(); l != _layers.end(); ++l)
		delete *l;
}

void DECFile::load(Common::SeekableReadStreamEndian &dec, const Common::String &fileName) {
	dec.skip(2); // Unused

	uint16 sceneCount   = dec.readUint16();
	int16 layerCount    = dec.readUint16();

	// Sanity checks
	if (layerCount < 1) {
		warning("DECFile::load(): Less than one layer (%d) in file \"%s\"",
		        layerCount, fileName.c_str());
		return;
	}

	// Load the backdrop
	loadBackdrop(dec);

	// Load the layers
	_layers.reserve(MAX(0, layerCount - 1));
	for (int i = 0; i < layerCount - 1; i++)
		_layers.push_back(loadLayer(dec));

	// Each scene has its own parts, sharing the backdrop and layer files.
	_scenes.resize(sceneCount);
	for (uint i = 0; i < sceneCount; ++i)
		loadParts(_scenes[i], dec);
}

void DECFile::loadBackdrop(Common::SeekableReadStreamEndian &dec) {
	// Interestingly, DEC files reference "FOO.LBM" instead of "FOO.CMP"
	Common::String file = Util::setExtension(Util::readString(dec, 13), "");
	if (_hasPadding)
		dec.skip(1);

	if (!file.empty())
		_backdrop = new CMPFile(_vm, file, _width, _height, _bpp);
}

CMPFile *DECFile::loadLayer(Common::SeekableReadStreamEndian &dec) {
	Common::String file = Util::setExtension(Util::readString(dec, 13), "");
	if (_hasPadding)
		dec.skip(1);

	return file.empty() ? nullptr : new CMPFile(_vm, file, _width, _height, _bpp);
}

void DECFile::loadParts(PartArray &parts, Common::SeekableReadStreamEndian &dec) {
	dec.skip(13); // Name
	if (_hasPadding)
		dec.skip(1);

	dec.skip(13); // File?
	if (_hasPadding)
		dec.skip(1);

	uint16 partCount = dec.readUint16();

	parts.resize(partCount);
	for (PartArray::iterator p = parts.begin(); p != parts.end(); ++p)
		loadPart(*p, dec);
}

void DECFile::loadPart(Part &part, Common::SeekableReadStreamEndian &dec) {
	// The 68000 layout places the word coordinates before the byte fields.
	if (_hasPadding) {
		part.x = dec.readUint16();
		part.y = dec.readUint16();
	}
	part.layer    = dec.readByte() - 1;
	part.part     = dec.readByte();
	part.priority = dec.readByte();
	if (!_hasPadding) {
		part.x = dec.readUint16();
		part.y = dec.readUint16();
	}

	part.transp = dec.readByte() != 0;
}

void DECFile::draw(Surface &dest, uint16 scene) const {
	drawBackdrop(dest);
	if (scene >= _scenes.size())
		return;

	const PartArray &parts = _scenes[scene];
	for (PartArray::const_iterator p = parts.begin(); p != parts.end(); ++p)
		drawLayer(dest, p->layer, p->part, p->x, p->y, p->transp ? 0 : -1);
}

void DECFile::drawParts(Surface &dest, uint16 scene) const {
	if (scene >= _scenes.size())
		return;

	const PartArray &parts = _scenes[scene];
	for (uint priority = 0; priority < 10; ++priority)
		for (PartArray::const_iterator p = parts.begin(); p != parts.end(); ++p)
			if (p->priority == priority)
				drawLayer(dest, p->layer, p->part, p->x, p->y, p->transp ? 0 : -1);
}

void DECFile::drawBackdrop(Surface &dest, int16 x, int16 y) const {
	if (!_backdrop)
		return;

	_backdrop->draw(dest, 0, x, y);
}

bool DECFile::getPartPosition(uint16 scene, uint16 part, int16 &x, int16 &y) const {
	if (scene >= _scenes.size() || part >= _scenes[scene].size())
		return false;
	x = _scenes[scene][part].x;
	y = _scenes[scene][part].y;
	return true;
}

bool DECFile::setPartPosition(uint16 scene, uint16 part, int16 x, int16 y) {
	if (scene >= _scenes.size() || part >= _scenes[scene].size())
		return false;
	_scenes[scene][part].x = x;
	_scenes[scene][part].y = y;
	return true;
}

void DECFile::drawForeground(Surface &dest, uint16 scene, const Common::Rect &area, int16 depth) const {
	if (scene >= _scenes.size() || area.isEmpty())
		return;
	int16 priority = 10;
	const PartArray &parts = _scenes[scene];
	for (PartArray::const_iterator p = parts.begin(); p != parts.end(); ++p) {
		if (p->layer >= _layers.size() || !_layers[p->layer] || !p->priority)
			continue;
		uint16 left, top, right, bottom;
		const CMPFile &layer = *_layers[p->layer];
		if (!layer.getCoordinates(p->part, left, top, right, bottom))
			continue;
		int16 x = p->x, y = p->y;
		int16 partRight = x + right - left;
		int16 partBottom = y + bottom - top;
		if (depth >= partBottom && priority > p->priority)
			continue;
		// Native occlusion carries this priority across non-intersecting parts.
		priority = MIN<int16>(priority, p->priority);
		int16 l = MAX<int16>(area.left, x);
		int16 t = MAX<int16>(area.top, y);
		int16 r = MIN<int16>(area.right - 1, partRight);
		int16 b = MIN<int16>(area.bottom - 1, partBottom);
		if (l > r || t > b)
			continue;
		layer.draw(dest, left + l - x, top + t - y, right + r - partRight, bottom + b - partBottom,
			l, t, p->transp ? 0 : -1);
	}
}

void DECFile::drawLayer(Surface &dest, uint16 layer, uint16 part,
						uint16 x, uint16 y, int32 transp) const {

	if (layer >= _layers.size() || !_layers[layer])
		return;

	_layers[layer]->draw(dest, part, x, y, transp);
}

} // End of namespace Gob
