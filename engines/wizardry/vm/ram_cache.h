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
 */

#ifndef WIZARDRY_VM_RAM_CACHE_H
#define WIZARDRY_VM_RAM_CACHE_H

#include "common/array.h"
#include "wizardry/vm/host.h"

namespace Wizardry {

class RamDiskSource {
public:
	virtual ~RamDiskSource() {}
	virtual bool readUncached(uint16 unit, uint16 block, uint16 count, byte *external,
							  uint16 address, uint16 physicalBase, uint16 &result, Common::String &error) = 0;
	virtual bool readRetained(uint16 unit, uint16 block, uint16 count, byte *external,
							  uint16 address, uint16 physicalBase, uint16, uint16 &result, Common::String &error) {
		return readUncached(unit, block, count, external, address, physicalBase, result, error);
	}
};

/** Optional interpreter RAM disk. WR25 owns the map; only unit 4 reads use it. */
class RamDiskCache {
public:
	enum { kMapBlocks = 2000,
		   kMaxSlots = 818 };
	RamDiskCache() { initialize(0, false); }
	void initialize(byte revision, bool resident);
	bool configured() const { return _capacity != 0; }
	bool enabled() const { return _enabled; }
	uint16 capacity() const { return _capacity; }
	uint16 used() const { return _data.size() / 512; }
	uint16 remaining() const { return _capacity - used(); }
	uint16 slot(uint block) const { return block < kMapBlocks ? _map[block] : 0xffff; }
	const Common::Array<byte> &data() const { return _data; }
	const Common::Array<byte> &backing() const { return _backing; }
	bool control(MachineHost &host, RamDiskSource &source, uint16 address,
				 uint16 &result, Common::String &error);
	bool read(MachineHost &host, RamDiskSource &source, uint16 unit, uint16 block,
			  uint16 count, byte *external, uint16 address, uint16 &result, Common::String &error) const;

private:
	bool _enabled = false;
	uint16 _capacity = 0, _physicalBase = 0;
	uint16 _map[kMapBlocks];
	Common::Array<byte> _data, _backing;
	void reset();
};

} // namespace Wizardry
#endif
