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

#include "wizardry/vm/machine.h"

namespace Wizardry {

bool PCodeMachine::atScenarioMenu() const {
	const byte game = TransferDisks::scenario(_code.checksum());
	if (game < 1 || game > 3 || keyAvailable())
		return false;
	// KANJIREA's top-level menu waits in procedure 13. Its other operations
	// have distinct activation frames; an M typed into a name is not a menu.
	return _frames.size() >= 3 && _frames[1].segment == 12 && _frames[1].procedure == 1 &&
		   _frames[2].segment == 12 && _frames[2].procedure == 13;
}

bool PCodeMachine::scenarioOwnerActive() const {
	for (uint i = 0; i < _frames.size(); ++i)
		if (_frames[i].segment == 12 && _frames[i].procedure == 16)
			return true;
	return false;
}

void PCodeMachine::discardScenarioDisk() {
	_scenarioDisk.clear();
	_scenarioWrittenCount = _scenarioUnit = 0;
}

uint16 PCodeMachine::formatScenarioDisk(uint16 unit) {
	const byte game = TransferDisks::scenario(_code.checksum());
	if (game < 1 || game > 3 || _segment != 12 || currentProcedure() != 16 ||
		(unit != 4 && unit != 5) || _masterDisk.size() != 640 * 512 || scenarioInProgress() ||
		(_observer && !_observer->canCreateScenarioDisk()))
		return 10;
	_scenarioDisk.resize((_diskPolicy == kResidentCOM ? 1280 : 640) * 512);
	memset(_scenarioDisk.data(), 0, _scenarioDisk.size());
	memset(_scenarioWritten, 0, sizeof(_scenarioWritten));
	_scenarioWrittenCount = 0;
	_scenarioUnit = unit;
	return 0;
}

bool PCodeMachine::scenarioUnitIO(bool write, uint16 unit, uint16 address, uint16 count, uint16 block) {
	const byte game = TransferDisks::scenario(_code.checksum());
	if (game < 1 || game > 3 || (unit != 4 && unit != 5))
		return false;
	if (scenarioOwnerActive() && _segment == 12 && currentProcedure() == 25 &&
		!write && !block && count == 512) {
		// Report the new virtual medium as blank, never the current campaign.
		setIoResult(10);
		return true;
	}
	if (!scenarioInProgress())
		return false;
	const uint32 offset = uint32(block) * 512;
	if (!write && unit == 4) {
		for (uint i = 0; i < _frames.size(); ++i) {
			if (_frames[i].segment != 12 || _frames[i].procedure != 18)
				continue;
			const uint16 check = readWord(_frames[i].base);
			if (check != 1 && check != 2)
				break;
			const Common::Array<byte> &image = check == 1 ? _scenarioDisk : _masterDisk;
			if (offset > image.size() || count > image.size() - offset) {
				setIoResult(10);
				return true;
			}
			for (uint j = 0; j < count; ++j)
				writeByte(uint16(address + j), image[offset + j]);
			setIoResult(0);
			return true;
		}
	}
	if (_segment != 12 || (currentProcedure() != 27 && currentProcedure() != 28))
		return false;
	if ((!write && unit != 4) || (write && unit != _scenarioUnit) || (count & 511) ||
		offset > 632U * 512 || count > 632U * 512 - offset) {
		setIoResult(10);
		return true;
	}
	for (uint i = 0; i < count; ++i) {
		if (write)
			_scenarioDisk[offset + i] = readByte(uint16(address + i));
		else
			writeByte(uint16(address + i), _masterDisk[offset + i]);
	}
	if (write) {
		const uint16 previousCount = _scenarioWrittenCount;
		for (uint b = block; b < uint32(block) + count / 512; ++b) {
			const byte mask = 1 << (b & 7);
			if (!(_scenarioWritten[b / 8] & mask)) {
				_scenarioWritten[b / 8] |= mask;
				++_scenarioWrittenCount;
			}
		}
		if (previousCount < 632 && _scenarioWrittenCount == 632) {
			// Compatibility mode has no native-cache ABI.
			if (!_ramCache.configured()) {
				const uint entries = _scenarioDisk[1040] | (uint16(_scenarioDisk[1041]) << 8);
				for (uint i = 0; i < entries && i < 77; ++i) {
					const uint off = 1024 + 26 * (i + 1);
					if (_scenarioDisk[off + 6] == 9 && !memcmp(_scenarioDisk.data() + off + 7, "HAS.CACHE", 9))
						_scenarioDisk[off + 4] = _scenarioDisk[off + 5] = 0;
				}
			}
			const bool paired = _diskPolicy == kResidentCOM;
			Common::String error;
			const Common::Array<byte> &master = paired ? _masterDisk : _scenarioDisk;
			const Common::Array<byte> &save = paired ? _scenarioDisk : _saveDisk;
			if (_observer && !_observer->onScenarioDiskCreated(master, save, error)) {
				// Keep further guest writes isolated until the operation exits
				// or the frontend restores its pre-operation snapshot.
				setIoResult(10);
				return true;
			}
			if (paired) {
				_disk = _masterDisk;
				_saveDisk = Common::move(_scenarioDisk);
				_diskSelectedSave = true;
			} else {
				_disk = Common::move(_scenarioDisk);
			}
			discardScenarioDisk();
		}
	}
	setIoResult(0);
	return true;
}

} // namespace Wizardry
