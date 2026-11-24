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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * This file is dual-licensed.
 * In addition to the GPLv3 license mentioned above, this code is also
 * licensed under LGPL 2.1. See LICENSES/COPYING.LGPL file for the
 * full text of the license.
 *
 */

#include "common/endian.h"
#include "common/textconsole.h"
#include "gob/anifile.h"
#include "gob/dataio.h"
#include "gob/decfile.h"
#include "gob/gob.h"
#include "gob/sound/sound.h"
#include "gob/surface.h"
#include "gob/video.h"
#include "gob/pregob/onceupon/assets.h"

namespace Gob {
namespace OnceUpon {

namespace {
// A derived asset keeps the already decoded source alive. Releasing its last
// scene owner releases both; the weak lookup table never owns phase artwork.
template<class T>
struct KeepSource {
	Common::SharedPtr<const T> source;
	explicit KeepSource(const Common::SharedPtr<const T> &value) : source(value) {}
	void operator()(const T *value) {
		delete value;
		// WeakPtr keeps its control block (and this deleter) after the object dies.
		source.reset();
	}
};

template<class T>
void prune(Common::Array<T> &entries) {
	for (uint i = entries.size(); i > 0; --i)
		if (entries[i - 1].value.expired())
			entries.remove_at(i - 1);
}
}

ColorMap::ColorMap() {
	for (uint i = 0; i < ARRAYSIZE(colors); ++i)
		colors[i] = i;
}

void ColorMap::replace(byte from, byte to) {
	if (from == to)
		return;
	for (uint i = 0; i < ARRAYSIZE(colors); ++i)
		if (colors[i] == from)
			colors[i] = to;
}

bool ColorMap::identity() const {
	for (uint i = 0; i < ARRAYSIZE(colors); ++i)
		if (colors[i] != i)
			return false;
	return true;
}

bool ColorMap::operator==(const ColorMap &other) const {
	return !memcmp(colors, other.colors, sizeof(colors));
}

Assets::Assets(GobEngine *vm) : _vm(vm) {}
Assets::~Assets() {}

bool Assets::loadSample(GobEngine *vm, SoundDesc &sample, const char *name) {
	if (vm->getPlatform() != Common::kPlatformDOS)
		return vm->_sound->sampleLoad(&sample, SOUND_SND, name);

	int32 size = 0;
	byte *data = vm->_dataIO->getFile(name, size);
	if (data && size > 6) {
		const uint16 rate = READ_BE_UINT16(data + 4);
		if (sample.load(SOUND_SND, data, size)) {
			// DOS derives the sample timer divisor from the authored rate.
			if (rate && rate < sample._frequency)
				sample._frequency = rate;
			return true;
		}
	}

	delete[] data;
	warning("Once Upon: Failed to load sample \"%s\"", name);
	return false;
}

void Assets::invalidate() {
	_animations.clear();
	_terrain.clear();
	_sheets.clear();
	_images.clear();
}

Assets::Animation Assets::acquireAnimation(const Common::String &name) {
	return animation(name);
}

Assets::Terrain Assets::acquireBackground(const Common::String &name) {
	return terrain(name);
}

Common::SharedPtr<const CMPFile> Assets::acquireSheet(const Common::String &name, uint16 width, uint16 height, uint8 bpp) {
	// Names resolve within this mounted game. Keep the requested dimensions
	// because a sheet without an RXY builds its rectangle from them.
	prune(_sheets);
	for (uint i = 0; i < _sheets.size(); ++i) {
		const SheetEntry &entry = _sheets[i];
		if (entry.width == width && entry.height == height && entry.bpp == bpp && entry.name.equalsIgnoreCase(name))
			return entry.value.lock();
	}

	Common::SharedPtr<const CMPFile> sheet(new CMPFile(_vm, name, width, height, bpp));
	if (!sheet->empty()) {
		SheetEntry entry;
		entry.name = name;
		entry.width = width;
		entry.height = height;
		entry.bpp = bpp;
		entry.value = sheet;
		_sheets.push_back(entry);
	}
	return sheet;
}

Assets::Animation Assets::animation(const Common::String &name, const ColorMap &colors) {
	// DataIO archive members and filesystem fallback both resolve names without case.
	prune(_animations);
	for (uint i = 0; i < _animations.size(); ++i)
		if (_animations[i].name.equalsIgnoreCase(name) && _animations[i].colors == colors)
			return _animations[i].value.lock();

	Animation value;
	if (colors.identity()) {
		value = Animation(new ANIFile(_vm, name, 320, 1, this));
	} else {
		Animation source = animation(name);
		value = Animation(source->cloneRecolored(colors.colors), KeepSource<ANIFile>(source));
	}
	AnimationEntry entry;
	entry.name = name;
	entry.colors = colors;
	entry.value = value;
	_animations.push_back(entry);
	return value;
}

Assets::Animation Assets::characterAnimation(byte hair, byte jacket, byte trousers) {
	ColorMap colors;
	colors.replace(15, 12);
	colors.replace(14, 10);
	colors.replace(8, 9);
	colors.replace(12, hair);
	colors.replace(10, jacket);
	colors.replace(9, trousers);
	return animation("ba.ani", colors);
}

Assets::Terrain Assets::terrain(const Common::String &name, bool forestCorrections) {
	prune(_terrain);
	for (uint i = 0; i < _terrain.size(); ++i)
		if (_terrain[i].name.equalsIgnoreCase(name) && _terrain[i].forestCorrections == forestCorrections)
			return _terrain[i].value.lock();

	DECFile *decoded = new DECFile(_vm, name, 320, 200);
	if (forestCorrections && _vm->getPlatform() == Common::kPlatformAmiga &&
			_vm->getGameType() == kGameTypeAbracadabra && name.equalsIgnoreCase("foret1.dec")) {
		decoded->setPartX(2, 9, 320);
		decoded->setPartX(4, 15, 320);
	}
	Terrain value(decoded);
	TerrainEntry entry;
	entry.name = name;
	entry.forestCorrections = forestCorrections;
	entry.value = value;
	_terrain.push_back(entry);
	return value;
}

Assets::Image Assets::forestEnergyIcons() {
	Image atlas = image("elchoix.cmp");
	Surface *icons = new Surface(7, 15, 1);
	icons->blit(*atlas, 16, 0, 22, 14, 0, 0);
	return Image(icons);
}

Assets::Image Assets::forestWeapons() {
	Image atlas = image(_vm->getGameType() == kGameTypeAbracadabra ? "armabra.cmp" : "armes.cmp");
	// Native ARMABRA ends at row 31; ARMES at row 47. The remainder is zero.
	// Keep only this crop, without pinning the full decoded atlas through it.
	Surface *weapons = new Surface(320, 48, 1);
	weapons->blit(*atlas, 0, 0, 319, 47, 0, 0);
	return Image(weapons);
}

void Assets::drawFramedBackground(Surface &dest, bool ending) {
	Image frame = image("cadre.cmp");
	Image scenery = image(ending ? "fin.cmp" : _vm->getGameType() == kGameTypeBabaYaga ? "present2.cmp" : "present.cmp");
	dest.blit(*frame);
	dest.blit(*scenery, 0, 0, 288, ending ? 137 : 175, 16, ending ? 50 : 12);
}

Assets::Image Assets::image(const Common::String &name, uint16 width, uint16 height, const ColorMap &colors) {
	prune(_images);
	for (uint i = 0; i < _images.size(); ++i)
		if (_images[i].name.equalsIgnoreCase(name) && _images[i].width == width && _images[i].height == height &&
				_images[i].colors == colors)
			return _images[i].value.lock();

	Surface *decoded = new Surface(width, height, 1);
	Image value;
	if (colors.identity()) {
		_vm->_video->drawPackedSprite(name.c_str(), *decoded);
		value = Image(decoded);
	} else {
		Image source = image(name, width, height);
		const uint32 pixels = (uint32)width * height;
		const byte *from = source->getData();
		byte *to = decoded->getData();
		for (uint32 i = 0; i < pixels; ++i)
			to[i] = colors.colors[from[i]];
		value = Image(decoded, KeepSource<Surface>(source));
	}
	ImageEntry entry;
	entry.name = name;
	entry.width = width;
	entry.height = height;
	entry.colors = colors;
	entry.value = value;
	_images.push_back(entry);
	return value;
}

} // End of namespace OnceUpon
} // End of namespace Gob
