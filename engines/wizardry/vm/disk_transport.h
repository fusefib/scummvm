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

#ifndef WIZARDRY_VM_DISK_TRANSPORT_H
#define WIZARDRY_VM_DISK_TRANSPORT_H

#include "common/array.h"
#include "wizardry/vm/host.h"

namespace Wizardry {

/** Resident COM disk callback and interpreter transfer loop.
 * Files are private images. Physical DS/CS bases are 64 KiB aligned;
 * requests cannot escape their segment.
 */
class ResidentDiskTransport {
public:
	ResidentDiskTransport(MachineHost &host, Common::Array<byte> &master,
						  Common::Array<byte> &save, bool &selectedSave) : _host(host), _master(master), _save(save), _selectedSave(selectedSave) {}

	bool unitIO(bool write, uint16 unit, uint16 address, uint16 count,
				uint16 block, uint16 mode, uint16 &ioResult, Common::String &error);
	bool readBuffer(uint16 unit, uint16 block, uint16 count, byte *destination,
					uint16 physicalOffset, uint16 &ioResult, Common::String &error);

private:
	struct Buffer {
		MachineHost &host;
		byte *external;
		uint16 address;
		Buffer(MachineHost &h, uint16 a, byte *p = nullptr) : host(h), external(p), address(a) {}
		byte read(uint32 offset) const { return external ? external[offset] : host.readByte(uint16(address + offset)); }
		void write(uint32 offset, byte value) {
			if (external)
				external[offset] = value;
			else
				host.writeByte(uint16(address + offset), value);
		}
	};
	MachineHost &_host;
	Common::Array<byte> &_master, &_save;
	bool &_selectedSave;

	bool transfer(bool write, uint16 unit, uint16 block, uint16 count,
				  Buffer &buffer, uint16 &ioResult, Common::String &error);
	bool sectors(bool write, byte drive, uint16 block, uint16 count,
				 Buffer &buffer, uint32 offset, uint16 &ioResult, Common::String &error);
	bool track(bool write, byte drive, uint16 block, uint16 count,
			   Buffer &buffer, uint32 offset, uint16 &ioResult, Common::String &error);
	bool callback(bool write, byte drive, uint16 block, uint16 count,
				  Buffer &buffer, uint32 offset, bool dmaBounce, uint16 &ioResult, Common::String &error);
};

} // namespace Wizardry
#endif
