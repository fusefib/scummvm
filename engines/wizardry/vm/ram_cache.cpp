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

#include "wizardry/vm/ram_cache.h"
#include "common/util.h"

namespace Wizardry {

void RamDiskCache::initialize(byte revision, bool resident) {
	_enabled = false;
	_backing.clear();
	_capacity = _physicalBase = 0;
	if (revision == 1 || revision == 4 || revision == 5) {
		const uint16 end = revision == 1 ? 0x99e0 : revision == 4 ? 0x9b30
																  : 0xb5b0;
		const uint16 paragraph = (end >> 4) + 1;
		// The native arena ends at 640 KiB; resident COM uses CS=5000,
		// while the standalone interpreter uses CS=3000.
		_capacity = ((0xa000 - (resident ? 0x5000 : 0x3000) - paragraph) >> 5) - 1;
		_physicalBase = paragraph << 4;
	}
	reset();
}

void RamDiskCache::reset() {
	_data.clear();
	for (uint i = 0; i < kMapBlocks; ++i)
		_map[i] = 0xffff;
}

bool RamDiskCache::control(MachineHost &host, RamDiskSource &source, uint16 address,
						   uint16 &result, Common::String &error) {
	error.clear();
	const uint16 first = host.readWord(address);
	if (first == 0xffff || first == 0xfffe) {
		_enabled = first == 0xfffe;
		result = 0;
		return true;
	}
	if (first == 0xfffd) {
		reset();
		result = 0xffff; // AX remains the value used to clear the original map.
		return true;
	}
	const uint16 count = MIN<uint16>(host.readWord(uint16(address + 2)), remaining());
	if (count) {
		if (uint32(first) + count > kMapBlocks) {
			error = "RAM-cache preload exceeds its original 2000-block map";
			return false;
		}
		const uint16 oldUsed = used();
		_data.resize((uint32(oldUsed) + count) * 512);
		memset(_data.data() + uint32(oldUsed) * 512, 0, uint32(count) * 512);
		if (_backing.size() > uint32(oldUsed) * 512)
			memcpy(_data.data() + uint32(oldUsed) * 512, _backing.data() + uint32(oldUsed) * 512,
				   MIN<uint32>(uint32(count) * 512, _backing.size() - uint32(oldUsed) * 512));
		for (uint i = 0; i < count; ++i)
			_map[first + i] = oldUsed + i;
		for (uint done = 0; done < count;) {
			const uint16 chunk = MIN<uint16>(count - done, 63);
			const uint32 offset = uint32(oldUsed + done) * 512;
			const uint16 known = offset < _backing.size() ? MIN<uint32>(chunk * 512, _backing.size() - offset) : 0;
			if (!source.readRetained(4, first + done, chunk * 512, _data.data() + offset,
									 0, uint16(_physicalBase + offset), known, result, error) ||
				result) {
				if (error.empty())
					error = Common::String::format("RAM-cache preload returned IORESULT %u", result);
				return false;
			}
			if (_backing.size() < offset + uint32(chunk) * 512)
				_backing.resize(offset + uint32(chunk) * 512);
			memcpy(_backing.data() + offset, _data.data() + offset, uint32(chunk) * 512);
			done += chunk;
		}
	}
	result = remaining();
	host.writeWord(address, result);
	return true;
}

bool RamDiskCache::read(MachineHost &host, RamDiskSource &source, uint16 unit, uint16 block,
						uint16 count, byte *external, uint16 address, uint16 &result, Common::String &error) const {
	error.clear();
	result = 0;
	if (!_enabled || unit != 4 || !count)
		return source.readUncached(unit, block, count, external, address, 0, result, error);
	if (uint32(block) + (uint32(count) + 511) / 512 > kMapBlocks || uint32(address) + count > 65536) {
		error = "RAM-cache read escapes its original map or destination segment";
		return false;
	}
	uint32 offset = 0;
	while (count) {
		uint next = block;
		while (next < kMapBlocks && _map[next] == 0xffff)
			++next;
		uint run = 0;
		if (next < kMapBlocks) {
			run = 1;
			// Original SCASW keeps AX=firstSlot+1 throughout the scan.
			while (next + run < kMapBlocks && _map[next + run] == uint16(_map[next] + 1))
				++run;
		}
		if (next != block) {
			const uint16 bytes = next == kMapBlocks ? count : MIN<uint16>(count, uint16((next - block) * 512));
			if (!source.readUncached(unit, block, bytes, external ? external + offset : nullptr,
									 uint16(address + offset), 0, result, error) ||
				result)
				return error.empty();
			count -= bytes;
			block += bytes / 512;
			offset += bytes;
			if (!count)
				break;
		}
		// Keep the found map entry after the original 16-bit gap arithmetic.
		const uint16 bytes = MIN<uint16>(count, run >= 64 ? 0x8000 : run * 512);
		const uint32 sourceOffset = uint32(_map[next]) * 512;
		if (!bytes || sourceOffset + bytes > _data.size()) {
			error = "RAM-cache read refers to an invalid cache slot";
			return false;
		}
		for (uint i = 0; i < bytes; ++i) {
			if (external)
				external[offset + i] = _data[sourceOffset + i];
			else
				host.writeByte(uint16(address + offset + i), _data[sourceOffset + i]);
		}
		count -= bytes;
		block += bytes / 512;
		offset += bytes;
	}
	return true;
}

} // namespace Wizardry
