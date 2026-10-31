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

#include "wizardry/vm/disk_transport.h"
#include "common/util.h"

namespace Wizardry {

bool ResidentDiskTransport::unitIO(bool write, uint16 unit, uint16 address,
								   uint16 count, uint16 block, uint16, uint16 &ioResult, Common::String &error) {
	Buffer buffer(_host, address);
	return transfer(write, unit, block, count, buffer, ioResult, error);
}

bool ResidentDiskTransport::readBuffer(uint16 unit, uint16 block, uint16 count,
									   byte *destination, uint16 physicalOffset, uint16 &ioResult, Common::String &error) {
	Buffer buffer(_host, physicalOffset, destination);
	return transfer(false, unit, block, count, buffer, ioResult, error);
}

bool ResidentDiskTransport::transfer(bool write, uint16 unit, uint16 block,
									 uint16 count, Buffer &buffer, uint16 &ioResult, Common::String &error) {
	error.clear();
	ioResult = 0;
	if (!count)
		return true;
	if (unit != 4 && unit != 5) {
		ioResult = 2;
		return true;
	}
	// The original 16-bit round-up overflows for these writes and reaches a
	// zero-byte DOS write. File truncation/growth is outside this private policy.
	if (write && count > 0xfe00) {
		error = "Resident disk write would overflow the original sector count";
		return false;
	}
	const uint32 span = write ? (uint32(count) + 511) & ~511U : count;
	if (uint32(buffer.address) + span > 65536) {
		error = "Resident disk request escapes its 64 KiB segment";
		return false;
	}
	const uint16 full = write ? span / 512 : count / 512;
	const uint16 partial = write ? 0 : count & 511;
	if (uint32(block) + full + (partial ? 1 : 0) > 2048) {
		error = "Resident disk track exceeds the original 8-bit division";
		return false;
	}
	if (full && (!sectors(write, byte(unit - 4), block, full, buffer, 0, ioResult, error) || ioResult))
		return error.empty();
	if (partial) {
		Buffer bounce(_host, 0xe600);
		if (!sectors(false, byte(unit - 4), block + full, 1, bounce, 0, ioResult, error) || ioResult)
			return error.empty();
		// REP MOVSB is forward and can overlap the guest bounce buffer.
		for (uint i = 0; i < partial; ++i)
			buffer.write(uint32(full) * 512 + i, _host.readByte(0xe600 + i));
	}
	return true;
}

bool ResidentDiskTransport::sectors(bool write, byte drive, uint16 block,
									uint16 count, Buffer &buffer, uint32 offset, uint16 &ioResult, Common::String &error) {
	while (count) {
		const uint16 chunk = MIN<uint16>(count, 8 - (block & 7));
		if (!track(write, drive, block, chunk, buffer, offset, ioResult, error) || ioResult)
			return error.empty();
		block += chunk;
		count -= chunk;
		offset += uint32(chunk) * 512;
	}
	return true;
}

bool ResidentDiskTransport::track(bool write, byte drive, uint16 block,
								  uint16 count, Buffer &buffer, uint32 offset, uint16 &ioResult, Common::String &error) {
	const uint32 physical = (uint32(buffer.address) + offset) & 65535;
	if (physical + uint32(count) * 512 <= 65535)
		return callback(write, drive, block, count, buffer, offset, false, ioResult, error);
	const uint16 prefix = (65535 - physical) / 512;
	if (prefix && (!callback(write, drive, block, prefix, buffer, offset, false, ioResult, error) || ioResult))
		return error.empty();
	if (!callback(write, drive, block + prefix, 1, buffer, offset + uint32(prefix) * 512, true, ioResult, error) || ioResult)
		return error.empty();
	const uint16 rest = count - prefix - 1;
	return !rest || callback(write, drive, block + prefix + 1, rest, buffer,
							 offset + uint32(prefix + 1) * 512, false, ioResult, error);
}

bool ResidentDiskTransport::callback(bool write, byte drive, uint16 block,
									 uint16 count, Buffer &buffer, uint32 offset, bool dmaBounce,
									 uint16 &ioResult, Common::String &error) {
	uint16 cylinder = block / 16;
	const uint16 head = (block / 8) & 1, sector = block & 7;
	if (!drive && count == 1 && cylinder == 79) {
		if (!sector) {
			_selectedSave = true;
			ioResult = 64; // Five original retries, including a reset, have this same result.
			return true;
		}
		cylinder = 39;
	}
	Common::Array<byte> &image = drive || _selectedSave ? _save : _master;
	// COM opens WIZ with access mode 0 and masks DOS access-denied errors.
	if (write && !drive && !_selectedSave)
		return true;
	const uint32 position = (uint32(cylinder) * 16 + head * 8 + sector) * 512;
	const uint32 bytes = uint32(count) * 512;
	const uint32 available = position < image.size() ? MIN<uint32>(bytes, image.size() - position) : 0;
	if (write && available != bytes) {
		error = "Resident disk write would grow its private image";
		return false;
	}
	if (!write && dmaBounce && available != bytes) {
		error = "Short DMA disk read requires unmodeled interpreter-CS bounce residue";
		return false;
	}
	if (!write && buffer.external && available != bytes) {
		error = "Short graphics disk read requires unmodeled interpreter-CS residue";
		return false;
	}
	// A complete DMA sector overwrites its temporary CS buffer before copying.
	// For other reads the COM callback ignores short DOS counts: leave the tail.
	for (uint32 i = 0; i < available; ++i) {
		if (write)
			image[position + i] = buffer.read(offset + i);
		else
			buffer.write(offset + i, image[position + i]);
	}
	if (write && available)
		_host.diskWritten();
	return true;
}

} // namespace Wizardry
