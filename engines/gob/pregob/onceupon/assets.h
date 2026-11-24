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

#ifndef GOB_PREGOB_ONCEUPON_ASSETS_H
#define GOB_PREGOB_ONCEUPON_ASSETS_H

#include "common/array.h"
#include "common/ptr.h"
#include "common/str.h"
#include "gob/pregob/seqfile.h"

namespace Gob {
class SoundDesc;
class Surface;

namespace OnceUpon {

/** One ordered palette transform; source images are never mutated. */
struct ColorMap {
	byte colors[256];
	ColorMap();
	void replace(byte from, byte to);
	bool identity() const;
	bool operator==(const ColorMap &other) const;
};

/** Immutable assets for one game run.
 *
 * Entries are weak; live scenes own the data they use. Game resources must
 * remain fixed during a run. invalidate() drops lookups when the run ends.
 */
class Assets : public SEQFile::ResourceProvider {
public:
	typedef Common::SharedPtr<const ANIFile> Animation;
	typedef Common::SharedPtr<const DECFile> Terrain;
	typedef Common::SharedPtr<const Surface> Image;

	explicit Assets(GobEngine *vm);
	~Assets() override;
	Animation animation(const Common::String &name, const ColorMap &colors = ColorMap());
	Animation characterAnimation(byte hair, byte jacket, byte trousers);
	Terrain terrain(const Common::String &name, bool forestCorrections = false);
	Image image(const Common::String &name, uint16 width = 320, uint16 height = 200,
		const ColorMap &colors = ColorMap());
	Image forestEnergyIcons();
	/** Immutable artwork for composing the native interfaces. */
	Image forestWeapons();
	void drawFramedBackground(Surface &dest, bool ending);
	static bool loadSample(GobEngine *vm, SoundDesc &sample, const char *name);
	Animation acquireAnimation(const Common::String &name) override;
	Terrain acquireBackground(const Common::String &name) override;
	void invalidate();

private:
	struct AnimationEntry {
		Common::String name;
		ColorMap colors;
		Common::WeakPtr<const ANIFile> value;
	};
	struct TerrainEntry {
		Common::String name;
		bool forestCorrections;
		Common::WeakPtr<const DECFile> value;
	};
	struct ImageEntry {
		Common::String name;
		uint16 width, height;
		ColorMap colors;
		Common::WeakPtr<const Surface> value;
	};

	GobEngine *_vm;
	Common::Array<AnimationEntry> _animations;
	Common::Array<TerrainEntry> _terrain;
	Common::Array<ImageEntry> _images;
};

} // End of namespace OnceUpon
} // End of namespace Gob

#endif
