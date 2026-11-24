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

#include "common/stream.h"
#include "common/substream.h"

#include "gob/gob.h"
#include "gob/util.h"
#include "gob/dataio.h"
#include "gob/surface.h"
#include "gob/video.h"
#include "gob/cmpfile.h"
#include "gob/anifile.h"

namespace Gob {

ANIFile::ANIFile(GobEngine *vm, const Common::String &fileName,
				 uint16 width, uint8 bpp, CMPFileProvider *sheets) : _vm(vm),
	_width(width), _bpp(bpp), _hasPadding(false) {

	Common::SharedPtr<Layout> layout(new Layout);
	_layout = layout;
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

		load(sub, fileName, *layout, sheets);
		return;
	}

	warning("ANIFile::ANIFile(): No such file \"%s\" (\"%s\")", endianFileName.c_str(), fileName.c_str());
}

ANIFile::ANIFile(const ANIFile &source, const byte colors[256]) :
	_vm(source._vm), _width(source._width), _bpp(source._bpp), _hasPadding(source._hasPadding),
	_layout(source._layout) {
	assert(_bpp == 1);
	_layers.reserve(source._layers.size());
	for (uint i = 0; i < source._layers.size(); ++i)
		_layers.push_back(Layer(source._layers[i] ? source._layers[i]->cloneRecolored(colors) : nullptr));
}

ANIFile *ANIFile::cloneRecolored(const byte colors[256]) const {
	return new ANIFile(*this, colors);
}

void ANIFile::load(Common::SeekableReadStreamEndian &ani, const Common::String &fileName, Layout &layout,
					CMPFileProvider *sheets) {
	ani.skip(2); // Unused

	uint16 animationCount = ani.readUint16();
	uint16 layerCount     = ani.readUint16();

	if (layerCount < 1)
		warning("ANIFile::load(): Less than one layer (%d) in file \"%s\"",
		        layerCount, fileName.c_str());

		// Load the layers
	if (layerCount > 0) {
		ani.skip(13); // The first layer is ignored?
		if (_hasPadding)
			ani.skip(1);

		_layers.reserve(layerCount - 1);
		for (int i = 0; i < layerCount - 1; i++)
			_layers.push_back(loadLayer(ani, sheets));
	}

	// Load the animations
	layout.animations.resize(animationCount);
	layout.frames.resize(animationCount);

	for (uint16 animation = 0; animation < animationCount; animation++) {
		Animation &definition = layout.animations[animation];
		loadAnimation(definition, layout.frames[animation], layout.chunks, ani);

		layout.maxWidth  = MAX<uint16>(layout.maxWidth , definition.width);
		layout.maxHeight = MAX<uint16>(layout.maxHeight, definition.height);
	}
}

void ANIFile::loadAnimation(Animation &animation, FrameArray &frames, ChunkArray &chunks,
							Common::SeekableReadStreamEndian &ani) {

	// Animation properties

	animation.name = Util::readString(ani, 13);
	if (_hasPadding)
		ani.skip(1);

	ani.skip(13); // The name a second time?!?
	if (_hasPadding)
		ani.skip(1);

	ani.skip(2);  // Unknown

	animation.x      = (int16) ani.readUint16();
	animation.y      = (int16) ani.readUint16();
	animation.deltaX = (int16) ani.readUint16();
	animation.deltaY = (int16) ani.readUint16();

	animation.transp = ani.readByte() != 0;

	if (_hasPadding)
		ani.skip(1);

	uint16 frameCount = ani.readUint16();

	// Load the frames

	frames.resize(MAX<uint16>(1, frameCount));
	loadFrames(frames, chunks, ani);

	animation.frameCount = frames.size();

	animation.width  = 0;
	animation.height = 0;

	// Calculate the areas of each frame

	animation.frameAreas.resize(animation.frameCount);
	for (uint16 i = 0; i < animation.frameCount; i++) {
		const FrameRange &frame = frames[i];
		FrameArea &area = animation.frameAreas[i];

		area.left  = area.top    =  0x7FFF;
		area.right = area.bottom = -0x7FFF;

		for (uint piece = 0; piece < frame.count; ++piece) {
			const AnimationChunk &c = chunks[frame.first + piece];
			uint16 cL, cT, cR, cB;

			if (!getCoordinates(c.layer, c.part, cL, cT, cR, cB))
				continue;

			const uint16 width  = cR - cL + 1;
			const uint16 height = cB - cT + 1;

			const uint16 l = c.x;
			const uint16 t = c.y;
			const uint16 r = l + width  - 1;
			const uint16 b = t + height - 1;

			area.left   = MIN<int16>(area.left  , l);
			area.top    = MIN<int16>(area.top   , t);
			area.right  = MAX<int16>(area.right , r);
			area.bottom = MAX<int16>(area.bottom, b);
		}

		if ((area.left <= area.right) && (area.top <= area.bottom)) {
			animation.width  = MAX<uint16>(animation.width , area.right  - area.left + 1);
			animation.height = MAX<uint16>(animation.height, area.bottom - area.top  + 1);
		}
	}
}

void ANIFile::loadFrames(FrameArray &frames, ChunkArray &chunks, Common::SeekableReadStreamEndian &ani) {
	uint32 curFrame = 0;

	bool end = false;
	while (!end) {
		FrameRange &frame = frames[curFrame];
		if (!frame.count)
			frame.first = chunks.size();
		++frame.count;

		uint8 layerFlags = ani.readByte();

		// Chunk properties
		AnimationChunk chunk;
		chunk.layer = (layerFlags & 0x0F) - 1;
		chunk.part  = ani.readByte();
		chunk.x     = (int8) ani.readByte();
		chunk.y     = (int8) ani.readByte();

		// X multiplier/offset
		int16 xOff = ((layerFlags & 0xC0) >> 6) << 7;
		if (chunk.x >= 0)
			chunk.x += xOff;
		else
			chunk.x -= xOff;

		// Y multiplier/offset
		int16 yOff = ((layerFlags & 0x30) >> 4) << 7;
		if (chunk.y >= 0)
			chunk.y += yOff;
		else
			chunk.y -= yOff;
		chunks.push_back(chunk);

		uint8 multiPart = ani.readByte();
		if      (multiPart == 0xFF) // No more frames in this animation
			end = true;
		else if (multiPart != 0x01) // No more chunks in this frame
			curFrame++;

		// Shouldn't happen, but just to be safe
		if (curFrame >= frames.size())
			frames.resize(curFrame + 1);

		if (_hasPadding)
			ani.skip(1);

		if (ani.eos() || ani.err())
			error("ANIFile::loadFrames(): Read error");
	}
}

ANIFile::Layer ANIFile::loadLayer(Common::SeekableReadStreamEndian &ani, CMPFileProvider *sheets) {
	Common::String file = Util::setExtension(Util::readString(ani, 13), "");
	if (_hasPadding)
		ani.skip(1);

	return sheets ? sheets->acquireSheet(file, _width, 0, _bpp) : Layer(new CMPFile(_vm, file, _width, 0, _bpp));
}

uint16 ANIFile::getAnimationCount() const {
	return _layout->animations.size();
}

void ANIFile::getMaxSize(uint16 &width, uint16 &height) const {
	width  = _layout->maxWidth;
	height = _layout->maxHeight;
}

const ANIFile::Animation &ANIFile::getAnimationInfo(uint16 animation) const {
	assert(animation < _layout->animations.size());

	return _layout->animations[animation];
}

bool ANIFile::getFrameOrigin(uint16 animation, uint16 frame, int16 &x, int16 &y) const {
	const AnimationFrameArray &frames = _layout->frames;
	if (animation >= frames.size() || frame >= frames[animation].size() || !frames[animation][frame].count)
		return false;

	const AnimationChunk &chunk = _layout->chunks[frames[animation][frame].first];
	x = chunk.x;
	y = chunk.y;
	return true;
}

bool ANIFile::getCoordinates(uint16 layer, uint16 part,
							 uint16 &left, uint16 &top, uint16 &right, uint16 &bottom) const {

	if (layer >= _layers.size())
		return false;

	return _layers[layer]->getCoordinates(part, left, top, right, bottom);
}

void ANIFile::draw(Surface &dest, uint16 animation, uint16 frame, int16 x, int16 y) const {
	if (animation >= _layout->animations.size())
		return;

	const Animation &anim = _layout->animations[animation];
	if (frame >= anim.frameCount)
		return;

	const FrameRange &range = _layout->frames[animation][frame];

	for (uint i = 0; i < range.count; ++i) {
		const AnimationChunk &chunk = _layout->chunks[range.first + i];
		drawLayer(dest, chunk.layer, chunk.part, x + chunk.x, y + chunk.y, anim.transp ? 0 : -1);
	}
}

void ANIFile::drawLayer(Surface &dest, uint16 layer, uint16 part,
						int16 x, int16 y, int32 transp) const {

	if (layer >= _layers.size())
		return;

	_layers[layer]->draw(dest, part, x, y, transp);
}

} // End of namespace Gob
