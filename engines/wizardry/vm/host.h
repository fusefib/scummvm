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

#ifndef WIZARDRY_VM_HOST_H
#define WIZARDRY_VM_HOST_H

#include "common/scummsys.h"
#include "common/str.h"

namespace Wizardry {

/** Interface between the native console driver and the p-machine.
 * Byte addresses wrap at 16 bits. Native word accesses preserve the
 * interpreter's segment-boundary behavior. Disk writes affect private images.
 */
class MachineHost {
public:
	virtual ~MachineHost() {}
	virtual byte readByte(uint16 address) const = 0;
	virtual uint16 readWord(uint16 address) const = 0;
	virtual void writeByte(uint16 address, byte value) = 0;
	virtual void writeWord(uint16 address, uint16 value) = 0;
	virtual bool readDisk(uint32 block, uint32 size, byte *destination) = 0;
	virtual bool keyAvailable() const = 0;
	virtual bool pollKeyboard() { return keyAvailable(); }
	virtual void setIoResult(uint16 value) = 0;
	// An availability poll is complete before this cooperative yield.
	virtual void yieldForInput() = 0;
	// Native delay request: testing can advance without wall-clock sleeping.
	virtual void delay(uint16 ticks) = 0;
	virtual void sound(uint16 first, uint16 second, uint16 third) = 0;
	virtual void fail(const Common::String &message) = 0;
};

} // namespace Wizardry
#endif
