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

#include "common/str.h"
#include "common/stream.h"

#include "gob/gob.h"
#include "gob/dataio.h"
#include "gob/draw.h"
#include "gob/decfile.h"
#include "gob/anifile.h"
#include "gob/aniobject.h"

#include "gob/pregob/seqfile.h"

namespace Gob {

SEQFile::SEQFile(GobEngine *vm, const Common::String &fileName, ResourceProvider *resources) : _vm(vm),
	_frame(0), _abortPlay(false), _savedFrameRate(0),
	_playing(false),
	_nextBackgroundKey(0) {
	for (uint i = 0; i < kObjectCount; i++) {
		_objects[i].object = nullptr;
		_objects[i].order = 0;
		_nextAnimationKey[i] = 0;
	}

	Common::SeekableReadStream *seq = _vm->_dataIO->getFile(Util::setExtension(fileName, ".SEQ"));
	if (!seq) {
		warning("SEQFile::SEQFile(): No such file \"%s\"", fileName.c_str());
		return;
	}

	load(*seq, resources);

	delete seq;
}

SEQFile::~SEQFile() {
	endPlay();
	for (uint i = 0; i < kObjectCount; i++)
		delete _objects[i].object;

}

void SEQFile::load(Common::SeekableReadStream &seq, ResourceProvider *resources) {
	const uint16 decCount = (uint16)seq.readByte() + 1;
	const uint16 aniCount = (uint16)seq.readByte() + 1;

	// Load backgrounds
	_backgrounds.reserve(decCount);
	for (uint i = 0; i < decCount; i++) {
		const Common::String dec = Util::readString(seq, 13);

		if (!_vm->_dataIO->hasFile(dec)) {
			warning("SEQFile::load(): No such background \"%s\"", dec.c_str());
			return;
		}

		_backgrounds.push_back(resources ? resources->acquireBackground(dec) :
			Common::SharedPtr<const DECFile>(new DECFile(_vm, dec, 320, 200)));
	}

	// Load animations
	_animations.reserve(aniCount);
	for (uint i = 0; i < aniCount; i++) {
		const Common::String ani = Util::readString(seq, 13);

		if (!_vm->_dataIO->hasFile(ani)) {
			warning("SEQFile::load(): No such animation \"%s\"", ani.c_str());
			return;
		}

		_animations.push_back(resources ? resources->acquireAnimation(ani) :
			Common::SharedPtr<const ANIFile>(new ANIFile(_vm, ani)));
	}

	seq.skip(2); // The scene selects the rate instead of using this header word.

	// Load background change keys

	const uint16 bgKeyCount = seq.readUint16LE();
	_bgKeys.resize(bgKeyCount);

	for (uint16 i = 0; i < bgKeyCount; i++) {
		const uint16 frame = seq.readUint16LE();
		const uint16 index = seq.readUint16LE();

		_bgKeys[i].frame      = frame;
		_bgKeys[i].background = index < _backgrounds.size() ? _backgrounds[index].get() : nullptr;
	}

	// Keep the four tracks in their original file order.
	for (uint i = 0; i < kObjectCount; ++i) {
		const uint16 animKeyCount = seq.readUint16LE();
		AnimationKeys &keys = _animKeys[i];
		keys.resize(animKeyCount);
		for (uint16 j = 0; j < animKeyCount; ++j) {
			AnimationKey &key = keys[j];
			key.frame = seq.readUint16LE();
			const uint16 index = seq.readUint16LE();
			key.ani = findANI(index, key.animation);
			key.x = seq.readSint16LE();
			key.y = seq.readSint16LE();
			key.order = seq.readSint16LE();
		}
	}
}

const ANIFile *SEQFile::findANI(uint16 index, uint16 &animation) {
	animation = 0xFFFF;

	// 0xFFFF = remove animation
	if (index == 0xFFFF)
		return nullptr;

	for (const auto &anim : _animations) {
		if (index < anim->getAnimationCount()) {
			animation = index;
			return anim.get();
		}

		index -= anim->getAnimationCount();
	}

	return nullptr;
}

void SEQFile::beginPlay(uint16 frameRate) {
	endPlay();
	bool hasKeys = !_bgKeys.empty();
	for (uint i = 0; !hasKeys && i < kObjectCount; ++i)
		hasKeys = !_animKeys[i].empty();
	if (!hasKeys)
		return;

	// Init

	_frame = 0;
	_abortPlay = false;
	_nextBackgroundKey = 0;

	for (uint i = 0; i < kObjectCount; i++) {
		delete _objects[i].object;

		_objects[i].object = nullptr;
		_objects[i].order  = 0;
		_nextAnimationKey[i] = 0;
	}

	// Set the frame rate

	_savedFrameRate = _vm->_util->getFrameRate();
	changeFrameRate(frameRate);
	_playing = true;
}

bool SEQFile::stepPlay() {
	if (!_playing || _vm->shouldQuit() || _abortPlay)
		return false;

	playFrame();
	handleFrameEvent();
	_vm->_draw->blitInvalidated();

	// Wait for the frame to end
	waitFrame();

	// Handle input

	_vm->_util->processInput();

	int16 key = _vm->_util->checkKey();

	int16 mouseX, mouseY;
	MouseButtons mouseButtons;
	_vm->_util->getMouseState(&mouseX, &mouseY, &mouseButtons);
	_vm->_util->forceMouseUp();

	handleInput(key, mouseX, mouseY, mouseButtons);

	++_frame;
	return !_vm->shouldQuit() && !_abortPlay;
}

void SEQFile::endPlay() {
	if (_playing)
		changeFrameRate(_savedFrameRate);
	_playing = false;
}

void SEQFile::playFrame() {
	// Remove the current animation frames
	clearAnims();

	// Handle background keys, directly updating the background
	if (_nextBackgroundKey < _bgKeys.size() && _bgKeys[_nextBackgroundKey].frame == _frame) {
		const BackgroundKey &bg = _bgKeys[_nextBackgroundKey++];
		if (bg.background) {
			bg.background->draw(*_vm->_draw->_backSurface);
			_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 0, 0, 319, 199);
		}
	}

	// Handle the animation keys, updating the objects
	for (uint i = 0; i < kObjectCount; ++i) {
		uint &nextKey = _nextAnimationKey[i];
		const AnimationKeys &keys = _animKeys[i];
		if (nextKey >= keys.size())
			continue;
		const AnimationKey &anim = keys[nextKey];
		if (anim.frame != _frame)
			continue;
		++nextKey;

		Object &object = _objects[i];

		delete object.object;
		object.object = nullptr;

		// No valid animation => remove
		if ((anim.animation == 0xFFFF) || !anim.ani)
			continue;

		// Change the animation

		object.object = new ANIObject(*anim.ani);

		object.object->setAnimation(anim.animation);
		object.object->setPosition(anim.x, anim.y);
		object.object->setVisible(true);
		object.object->setPause(false);

		object.order = anim.order;
	}

	// Draw the animations
	drawAnims();
}

// At most four objects; insertion preserves slot order for equal keys.
uint SEQFile::getOrderedObjects(Object *objects[kObjectCount]) {
	uint count = 0;
	for (uint i = 0; i < kObjectCount; ++i) {
		if (!_objects[i].object)
			continue;
		uint at = count;
		while (at && objects[at - 1]->order > _objects[i].order) {
			objects[at] = objects[at - 1];
			--at;
		}
		objects[at] = &_objects[i];
		++count;
	}
	return count;
}

void SEQFile::clearAnims() {
	Object *objects[kObjectCount];
	const uint count = getOrderedObjects(objects);
	// Remove old frames in reverse drawing order. Keys may replace objects
	// after this call, so drawAnims() orders the new set independently.
	for (uint i = count; i > 0; --i) {
		int16 left, top, right, bottom;
		if (objects[i - 1]->object->clear(*_vm->_draw->_backSurface, left, top, right, bottom))
			_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, left, top, right, bottom);
	}
}

void SEQFile::drawAnims() {
	Object *objects[kObjectCount];
	const uint count = getOrderedObjects(objects);
	for (uint i = 0; i < count; ++i) {
		int16 left, top, right, bottom;
		if (objects[i]->object->draw(*_vm->_draw->_backSurface, left, top, right, bottom))
			_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, left, top, right, bottom);
		objects[i]->object->advance();
	}
}

uint16 SEQFile::getFrame() const {
	return _frame;
}

void SEQFile::seekFrame(uint16 frame) {
	_frame = frame;
}

void SEQFile::abortPlay() {
	_abortPlay = true;
}

void SEQFile::changeFrameRate(int16 rate) {
	_vm->_util->setFrameRate(rate);
}

void SEQFile::waitFrame() {
	_vm->_util->waitEndFrame();
}

void SEQFile::handleFrameEvent() {
}

void SEQFile::handleInput(int16 key, int16 mouseX, int16 mouseY, MouseButtons mouseButtons) {
	if (key != 0 || mouseButtons != kMouseButtonsNone)
		abortPlay();
}

} // End of namespace Gob
