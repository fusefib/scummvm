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
 */

#ifndef CRUISE_SOUND_AMIGA_RESOURCES_H
#define CRUISE_SOUND_AMIGA_RESOURCES_H

#include "common/array.h"
#include "common/fs.h"
#include "common/str-array.h"

namespace Cruise {

// Private music source. Neither indexing nor playback touches SearchMan or
// the game's current disk. Cached bytes remain immutable after validation.
class AmigaMusicResources {
public:
	bool open(const Common::Path &directory, const Common::StringArray &modules,
			Common::String &reason);
	bool contains(const char *name) const;
	static byte *load(void *context, const char *name, uint32 *size);
	static void release(void *data);
	static bool isModuleName(const Common::String &name);

private:
	struct Entry {
		Common::FSNode file;
		Common::String name;
		uint32 offset, stored, expanded;
	};
	struct Resource {
		Common::String name;
		Common::Array<byte> data;
	};
	bool index(const Common::Path &directory, Common::String &reason);
	bool cache(const Common::String &name, Common::String &reason);
	bool validateModule(const Common::String &name, Common::String &reason);
	const Resource *find(const char *name) const;
	Common::Array<Entry> _entries;
	Common::Array<Resource> _resources;
};

} // End of namespace Cruise

#endif
