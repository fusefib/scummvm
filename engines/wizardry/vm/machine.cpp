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
#include "common/endian.h"
#include "common/md5.h"
#include "common/memstream.h"
#include "common/util.h"
#include "wizardry/data/ucsd_disk.h"
#include "wizardry/vm/console.h"
#include "wizardry/vm/disk_transport.h"
#include "wizardry/vm/loader_keyboard.h"
#include "wizardry/vm/timing.h"

namespace Wizardry {

static uint16 packedFieldMask(uint16 width, byte revision) {
	if (width)
		return (1U << width) - 1;
	// Index zero aliases the STP jump displacement before the mask table.
	switch (revision) {
	case 1:
		return 0xced0;
	case 4:
		return 0xcdbd;
	case 5:
		return 0xccde;
	default:
		return 0;
	}
}

static bool identifyNativeRevision(const Common::Array<byte> &disk, byte &revision, Common::String &error,
	byte *firstByte = nullptr) {
	revision = 0;
	if (firstByte)
		*firstByte = 0;
	UcsdDisk volume;
	Common::String ignored;
	if (!volume.loadBytes(disk, ignored) || !volume.find("SYSTEM.INTERP"))
		return true;
	Common::Array<byte> interpreter;
	if (!volume.readFile("SYSTEM.INTERP", interpreter, error))
		return false;
	Common::MemoryReadStream stream(interpreter.data(), interpreter.size());
	const Common::String digest = Common::computeStreamMD5AsString(stream);
	if (interpreter.size() == 16384) {
		if (digest == "1b83a5e540a9c4556729b79cd5222c8c")
			revision = 1;
		else if (digest == "0ad1552cdd9316f943fa9935a04bec95")
			revision = 4;
		else if (digest == "9459ba0c1ece8616fb408ff09ba746e8")
			revision = 5;
	}
	if (!revision) {
		error = "Unrecognized SYSTEM.INTERP revision; native service profile is unknown";
		return false;
	}
	if (firstByte)
		*firstByte = interpreter[0];
	return true;
}

PCodeMachine::PCodeMachine() {
	memset(_memory, 0, sizeof(_memory));
	_console = new PCodeConsole(*this);
}

PCodeMachine::~PCodeMachine() {
	delete _console;
}

bool PCodeMachine::boot(const Common::Array<byte> &code, const Common::Array<byte> &disk, Common::String &error) {
	return bootInternal(code, disk, nullptr, error);
}

bool PCodeMachine::boot(const Common::Array<byte> &code, const Common::Array<byte> &disk,
	const Common::Array<byte> &saveDisk, Common::String &error) {
	return bootInternal(code, disk, &saveDisk, error);
}

bool PCodeMachine::loadCampaignImages(const Common::Array<byte> &master,
	const Common::Array<byte> &save, Common::String &error) {
	error.clear();
	if (_instructions || _state != kRunning || _frames.size() != 1 || _services.enabled() ||
		master.size() != _disk.size() || save.size() != _saveDisk.size()) {
		error = "Campaign images can only be installed before a matching fresh boot executes";
		return false;
	}
	if (master.size() < 5 || READ_LE_UINT16(master.data() + 3) != READ_LE_UINT16(_disk.data() + 3)) {
		error = "Campaign disk geometry differs from the installed game";
		return false;
	}
	UcsdDisk source, working;
	if (!source.loadBytes(_disk, error) || !working.loadBytes(master, error))
		return false;
	Common::Array<byte> program;
	const char *const files[] = {"SYSTEM.PASCAL", "SYSTEM.INTERP"};
	for (uint i = 0; i < ARRAYSIZE(files); ++i) {
		const char *name = files[i];
		Common::Array<byte> first, second;
		const UcsdDisk::Entry *a = source.find(name), *b = working.find(name);
		if (!a || !b || a->firstBlock != b->firstBlock || a->size() != b->size() ||
			!source.readFile(name, first, error) || !working.readFile(name, second, error) ||
			first.size() != second.size() || memcmp(first.data(), second.data(), first.size())) {
			error = "Campaign program or interpreter differs from the installed game";
			return false;
		}
		if (!strcmp(name, "SYSTEM.PASCAL"))
			program = first;
	}
	if (_diskPolicy == kResidentCOM && !validateDiskPair(program, master, save, _nativeRevision, error))
		return false;
	// Keep immutable source CRCs so ordinary ScummVM snapshots retain their identity.
	_disk = master;
	_saveDisk = save;
	return true;
}

bool PCodeMachine::validateDiskPair(const Common::Array<byte> &code, const Common::Array<byte> &disk,
	const Common::Array<byte> &saveDisk, byte revision, Common::String &error) const {
	const uint32 masterSize = (revision == 5 ? 1280 : 640) * 512;
	if (!_originalMemory || (revision != 1 && revision != 4 && revision != 5) ||
		disk.size() != masterSize || saveDisk.size() != 1280 * 512 ||
		READ_LE_UINT16(disk.data() + 3) != 0x0880 || READ_LE_UINT16(saveDisk.data() + 3) != 0x0880) {
		error = "Resident paired disks require original memory and verified I-V image geometry";
		return false;
	}
	UcsdDisk master, secondary;
	if (!master.loadBytes(disk, error) || !secondary.loadBytes(saveDisk, error))
		return false;
	const char *const files[] = {"SYSTEM.PASCAL", "SYSTEM.INTERP"};
	for (uint i = 0; i < ARRAYSIZE(files); ++i) {
		const char *name = files[i];
		const UcsdDisk::Entry *a = master.find(name), *b = secondary.find(name);
		Common::Array<byte> first, second;
		if (!a || !b || a->firstBlock != b->firstBlock || a->size() != b->size() ||
			!master.readFile(name, first, error) || !secondary.readFile(name, second, error) ||
			first.size() != second.size() || memcmp(first.data(), second.data(), first.size()) ||
			(!strcmp(name, "SYSTEM.PASCAL") && (first.size() != code.size() || memcmp(first.data(), code.data(), code.size())))) {
			error = "Paired disks must contain the same program and interpreter at matching blocks";
			return false;
		}
	}
	return true;
}

bool PCodeMachine::validateDiskPolicy(Common::String &error) const {
	if (_diskPolicy == kResidentCOM) {
		byte revision;
		if (!identifyNativeRevision(_disk, revision, error))
			return false;
		if (revision != _nativeRevision) {
			error = "Paired saved master has a different native interpreter profile";
			return false;
		}
		UcsdDisk volume;
		Common::Array<byte> program;
		if (!volume.loadBytes(_disk, error) || !volume.readFile("SYSTEM.PASCAL", program, error))
			return false;
		if (wizardryCrc32(program.data(), program.size()) != _code.checksum()) {
			error = "Paired saved master has a different program";
			return false;
		}
		if (!validateDiskPair(program, _disk, _saveDisk, _nativeRevision, error))
			return false;
	} else {
		if (_diskPolicy != _defaultDiskPolicy || _diskSelectedSave || !_saveDisk.empty() || _sourceSaveDiskCRC) {
			error = "Invalid legacy disk policy or noncanonical secondary image";
			return false;
		}
	}
	return true;
}

bool PCodeMachine::bootInternal(const Common::Array<byte> &code, const Common::Array<byte> &disk,
	const Common::Array<byte> *saveDisk, Common::String &error) {
	PCodeFile parsed;
	if (!parsed.load(code, error))
		return false;
	if (!parsed.procedure(1, 1) || disk.size() < 6 * 512 || disk.size() > 2 * 1024 * 1024 || (disk.size() % 512)) {
		error = "Invalid Wizardry program or linear disk image";
		return false;
	}
	byte revision, originalWordHighInitial;
	if (!identifyNativeRevision(disk, revision, error, &originalWordHighInitial))
		return false;
	if (!revision && parsed.segment(1)->name == "WIZARDRY") {
		error = "Wizardry boot requires a verified SYSTEM.INTERP native service profile";
		return false;
	}
	if (saveDisk && !validateDiskPair(code, disk, *saveDisk, revision, error))
		return false;
	_code = Common::move(parsed);
	_nativeRevision = revision;
	_originalWordHighInitial = originalWordHighInitial;
	_originalWordHighByte = originalWordHighInitial;
	_defaultDiskPolicy = revision >= 4 && READ_LE_UINT16(disk.data() + 3) == 0x0880 ? kRaw0880 : kLegacy632;
	_diskPolicy = saveDisk ? kResidentCOM : _defaultDiskPolicy;
	_services.clear();
	_disk = disk;
	_saveDisk = saveDisk ? *saveDisk : Common::Array<byte>();
	_diskSelectedSave = false;
	_diskDMABuffer.clear();
	_ramCache.initialize(_originalMemory ? revision : 0, _diskPolicy == kResidentCOM);
	_graphicsResidue.initialize(_originalMemory ? revision : 0);
	_sourceSaveDiskCRC = saveDisk ? wizardryCrc32(saveDisk->data(), saveDisk->size()) : 0;
	_sourceDiskCRC = wizardryCrc32(disk.data(), disk.size());
	_campaignID = 0;
	memset(_memory, 0, sizeof(_memory));
	_stack.clear();
	_frames.clear();
	_literals.clear();
	clearKeys();
	_loaderKeys.clear();
	_loaderData.clear();
	_transfer = TransferDisks();
	_error.clear();
	_heap = 0x6000;
	_np = 0xf000;
	_literalNext = 0x0800;
	_ioResult = _delayTicks = 0;
	_waitRemainingUs = 0;
	_soundPattern = 0xffff;
	_instructions = 0;
	_input = PendingInput();
	_idleSnapshot = false;
	_state = kRunning;
	delete _console;
	_console = new PCodeConsole(*this, revision >= 4 ? NativeRandom::kThreeWord : NativeRandom::kFourWord,
		(revision == 1 || revision == 4) && _originalMemory);
	// Compatibility memory lacks the original optional native-cache ABI.
	uint count = READ_LE_UINT16(_disk.data() + 1024 + 16);
	if (count > 77) {
		error = "Invalid UCSD directory count";
		_state = kFault;
		return false;
	}
	for (uint i = 0; !_ramCache.configured() && i < count; ++i) {
		uint off = 1024 + 26 * (i + 1);
		if (_disk[off + 6] == 9 && !memcmp(_disk.data() + off + 7, "HAS.CACHE", 9)) {
			_disk[off + 4] = _disk[off + 5] = 0;
		}
	}
	if (_originalMemory && !initializeOriginalMemory(code, error)) {
		_state = kFault;
		return false;
	}
	_segment = 1;
	_ip = 0;
	if (_originalMemory && !loadSegment(1)) {
		error = _error;
		return false;
	}
	push(_originalMemory ? segmentBase(1) : 0);
	push(_originalMemory ? segmentBase(1) : 0);
	enter(1, 1, kRoot);
	if (_state == kFault) {
		error = _error;
		return false;
	}
	writeWord(_frames[0].base, _originalMemory ? 0xe0de : 0x0700);
	if (_originalMemory) {
		writeWord(0x0600, _frames[0].base - 12);
		writeWord(0x060a, readWord(0x0608));
	} else
		writeWord(_frames[0].base + 2, 0);
	error.clear();
	return true;
}

uint16 PCodeMachine::readWord(uint16 address) const {
	const byte high = _originalMemory && address == 0xffff ? _originalWordHighByte : _memory[uint16(address + 1)];
	return uint16(_memory[address] | (uint16(high) << 8));
}

void PCodeMachine::writeWord(uint16 address, uint16 value) {
	_memory[address] = byte(value);
	if (_originalMemory && address == 0xffff)
		_originalWordHighByte = byte(value >> 8);
	else
		_memory[uint16(address + 1)] = byte(value >> 8);
	if (_originalMemory) {
		if (address >= 0x0601 && address <= 0x0603)
			_np = readWord(0x0602);
		if (address >= 0x0603 && address <= 0x0605)
			_heap = readWord(0x0604);
		if (address >= 0xe0dd && address <= 0xe0df)
			_ioResult = readWord(0xe0de);
	}
}

void PCodeMachine::writeByte(uint16 address, byte value) {
	_memory[address] = value;
	if (_originalMemory) {
		if (address == 0x0602 || address == 0x0603)
			_np = readWord(0x0602);
		if (address == 0x0604 || address == 0x0605)
			_heap = readWord(0x0604);
		if (address == 0xe0de || address == 0xe0df)
			_ioResult = readWord(0xe0de);
	}
}

void PCodeMachine::setIoResult(uint16 value) {
	_ioResult = value;
	if (_originalMemory)
		writeWord(0xe0de, value);
}

bool PCodeMachine::readDisk(uint32 block, uint32 size, byte *destination) {
	return readDiskAt(block, size, destination, 0, 4);
}

bool PCodeMachine::readDiskAt(uint32 block, uint32 size, byte *destination, uint16 physicalOffset, uint16 unit) {
	Common::String error;
	uint16 result = 0;
	const bool retained = _graphicsResidue.contains(physicalOffset, size);
	byte *buffer = retained ? _graphicsResidue.data(physicalOffset) : destination;
	if (block > 65535 || size > 65535 || !_ramCache.read(*this, *this, unit, block, size, buffer, physicalOffset, result, error)) {
		fail(error.empty() ? "Native graphics disk request is too large" : error);
		return false;
	}
	setIoResult(result);
	if (!result && retained && size) {
		_graphicsResidue.establish(physicalOffset, size);
		memcpy(destination, buffer, size);
	}
	return result == 0;
}

bool PCodeMachine::cacheControl(uint16 address) {
	Common::String error;
	uint16 result = 0;
	if (!_ramCache.configured() || !_ramCache.control(*this, *this, address, result, error)) {
		fail(error.empty() ? "RAM-cache control requires a verified original-memory profile" : error);
		return false;
	}
	setIoResult(result);
	return true;
}

bool PCodeMachine::readGuestDisk(uint16 unit, uint16 block, uint16 count, uint16 address,
	uint16 &result, Common::String &error) {
	return _ramCache.read(*this, *this, unit, block, count, nullptr, address, result, error);
}

bool PCodeMachine::readUncached(uint16 unit, uint16 block, uint16 count, byte *external,
	uint16 address, uint16 physicalBase, uint16 &result, Common::String &error) {
	return readRetained(unit, block, count, external, address, physicalBase, 0, result, error);
}

bool PCodeMachine::readRetained(uint16 unit, uint16 block, uint16 count, byte *external,
	uint16 address, uint16 physicalBase, uint16 knownPrefix, uint16 &result, Common::String &error) {
	error.clear();
	result = 0;
	if (_diskPolicy == kResidentCOM) {
		ResidentDiskTransport transport(*this, _disk, _saveDisk, _diskSelectedSave, activeTransferDisk(), &_diskDMABuffer);
		if (physicalBase)
			return transport.readCacheBuffer(block, count, external, physicalBase, result, error, knownPrefix);
		if (external && _graphicsResidue.contains(address, count))
			return transport.readBuffer(unit, block, count, external, address, result, error,
				_graphicsResidue.known(address));
		return external ? transport.readBuffer(unit, block, count, external, address, result, error) : transport.unitIO(false, unit, address, count, block, 0, result, error);
	}
	const uint32 offset = uint32(block) * 512;
	const uint32 available = offset < _disk.size() ? MIN<uint32>(count, _disk.size() - offset) : 0;
	if (external) {
		if (available != count) {
			result = 10;
			return true;
		}
		if (count)
			memcpy(external, _disk.data() + offset, count);
		return true;
	}
	if (_diskPolicy == kRaw0880) {
		if (available != count && (count & 511)) {
			error = "Partial-sector disk EOF requires an unmodeled original bounce buffer";
			return false;
		}
	} else if (available != count || block >= 632 || offset + count > 632U * 512) {
		result = 10;
		return true;
	}
	for (uint32 i = 0; i < available; ++i)
		writeByte(uint16(address + i), _disk[offset + i]);
	return true;
}

void PCodeMachine::queueKey(byte key) {
	if (_services.inputMode())
		return;
	if (!key)
		return;
	if (_keyPosition == _keys.size())
		clearKeys();
	// Bound the stored queue as well as the unread suffix. Retaining consumed
	// prefixes across partial refills could otherwise exceed the snapshot's
	// 4096-byte limit even though at most 4096 keys remained pending.
	if (_keyPosition && _keys.size() >= 4096) {
		const uint32 unread = _keys.size() - _keyPosition;
		memmove(_keys.data(), _keys.data() + _keyPosition, unread);
		_keys.resize(unread);
		_keyPosition = 0;
	}
	if (_keys.size() - _keyPosition < 4096)
		_keys.push_back(key);
	_idleSnapshot = false;
}

void PCodeMachine::clearKeys() {
	_keys.clear();
	_keyPosition = 0;
}

bool PCodeMachine::loaderKeyboardProfile() const {
	return _diskPolicy == kResidentCOM && _originalMemory && (_nativeRevision == 4 || _nativeRevision == 5);
}

byte PCodeMachine::transferProfile() const {
	if (_diskPolicy != kResidentCOM || !_originalMemory || (_nativeRevision != 1 && _nativeRevision != 5))
		return 0;
	const byte scenario = TransferDisks::scenario(_code.checksum());
	return scenario == 2 || scenario == 3 || scenario == 5 ? scenario : 0;
}

byte PCodeMachine::keyboardMatchProfile() const {
	const byte transfer = transferProfile();
	return transfer ? transfer : _nativeRevision;
}

bool PCodeMachine::loadTransferSource(byte source, const Common::Array<byte> &image, Common::String &error) {
	if (_instructions || _state != kRunning) {
		error = "Transfer sources can only be supplied before the fresh game executes";
		return false;
	}
	return _transfer.attach(transferProfile(), source, image, error);
}

bool PCodeMachine::loadCampaignTransfers(const TransferDisks &transfer, Common::String &error) {
	if (_instructions || _state != kRunning || !transfer.validate(transferProfile(), true, error)) {
		if (error.empty())
			error = "Campaign transfer disks require a matching fresh game";
		return false;
	}
	_transfer = transfer;
	return true;
}

bool PCodeMachine::transferOwnerActive() const {
	for (uint i = 0; i < _frames.size(); ++i)
		if (_frames[i].segment == 21 && _frames[i].procedure == (transferProfile() == 5 ? 2 : 16))
			return true;
	return false;
}

void PCodeMachine::finishTransferTransaction() {
	// GAMEUTIL:16 (II/III) or :2 (V) retains records after closing SOURCE. Destination
	// rejection can still reopen SOURCE and restore them. Commit only after
	// the owning guest procedure has returned or executed its nonlocal EXIT.
	if (_transfer.transaction && !_transfer.active && !_transfer.choosing && !transferOwnerActive())
		_transfer.transaction = false;
}

void PCodeMachine::consumeTransferChoice(byte key) {
	assert(_transfer.choosing && (transferProfile() == 3 || transferProfile() == 5));
	byte source = 0;
	if (!LoaderKeyboard::transferChoice(transferProfile(), key, source, _loaderKeys))
		return;
	_transfer.choosing = false;
	if (_transfer.present(source))
		_transfer.active = source;
	else if (transferProfile() != 5)
		_transfer.active = 0;
	if (source)
		_diskSelectedSave = true;
}

bool PCodeMachine::keyboardScreenProfile() const {
	// The classic COMs share one exit predicate, including compatibility mode.
	// Their other loader actions and queues are not part of this profile.
	return _nativeRevision == 1 || loaderKeyboardProfile();
}

bool PCodeMachine::loadLoaderCom(const Common::Array<byte> &com, Common::String &error) {
	error.clear();
	if (!loaderKeyboardProfile() || _instructions || !_loaderKeys.empty()) {
		error = "Resident challenge data can only be supplied at fresh paired IV/V startup";
		return false;
	}
	return LoaderKeyboard::extractData(_nativeRevision, com, _loaderData, error);
}

bool PCodeMachine::prepareLoaderKeys() {
	if (!keyboardScreenProfile() || !_loaderKeys.empty())
		return true;
	if (_transfer.choosing) {
		if (_services.inputMode())
			return true; // Physical Q events are applied in replay order.
		while (_transfer.choosing && _keyPosition < _keys.size())
			consumeTransferChoice(_keys[_keyPosition++]);
		if (_transfer.choosing) {
			yieldForInput();
			return false;
		}
		return true;
	}
	const LoaderKeyboard::Action action = LoaderKeyboard::match(keyboardMatchProfile(), _console->keyboardVram());
	if (action == LoaderKeyboard::kTransferSource || action == LoaderKeyboard::kAuxiliary) {
		if (!transferProfile()) {
			fail("Resident transfer requires an authenticated original program");
			return false;
		}
		_transfer.transaction = true;
		if (transferProfile() == 3 || transferProfile() == 5) {
			_transfer.choosing = true;
			_console->drawTransferChoice(transferProfile());
			return prepareLoaderKeys();
		}
		_transfer.active = _transfer.present(1) ? 1 : 0;
		_loaderKeys.push_back(0x1c0d); // DOS open failure also queues Return.
		return true;
	}
	if (action == LoaderKeyboard::kTransferClose) {
		_transfer.active = 0;
		if (transferProfile() == 3 || _nativeRevision == 5)
			_diskSelectedSave = true;
		_loaderKeys.push_back(0x1c0d);
		finishTransferTransaction();
		return true;
	}
	if (action == LoaderKeyboard::kValidationCode || action == LoaderKeyboard::kMagicWord) {
		if (_loaderData.empty()) {
			fail("Resident challenge requires the matching WIZ4.COM or WIZ5.COM at startup");
			return false;
		}
		LoaderKeyboard::challengeKeys(_nativeRevision, _console->keyboardVram(), _loaderData, _loaderKeys);
		return true;
	}
	if (action == LoaderKeyboard::kExit) {
		// The original INT 16h hook commits/closes its files and terminates
		// through DOS 4C00h without returning to the pending guest service.
		// Our images are private; the frontend commits them on normal exit.
		_state = kHalted;
		return false;
	}
	LoaderKeyboard::enqueue(action, _loaderKeys);
	if (action == LoaderKeyboard::kSelectSave)
		_diskSelectedSave = true;
	return true;
}

bool PCodeMachine::replayLoaderEvents(uint32 begin, uint32 end) {
	if (!keyboardScreenProfile())
		return true;
	const Common::Array<NativeServiceReplay::Event> &events = _services.events();
	for (uint32 i = begin; i < end; ++i) {
		const NativeServiceReplay::Event &event = events[i];
		if (event.type == NativeServiceReplay::kTransferKey) {
			if (!prepareLoaderKeys())
				return false;
			if (!_transfer.choosing) {
				fail("Transfer choice replay is outside the original III chooser");
				return false;
			}
			consumeTransferChoice(byte(event.value));
			continue;
		}
		if (event.type != NativeServiceReplay::kKey && event.type != NativeServiceReplay::kBiosAX)
			continue;
		if (!prepareLoaderKeys())
			return false;
		if (_transfer.choosing) {
			fail("Original III keyboard replay lacks a physical transfer choice");
			return false;
		}
		if (_loaderKeys.empty())
			continue;
		if (event.type == NativeServiceReplay::kKey) {
			if (!event.value) {
				fail("Original keyboard replay hides a resident loader key");
				return false;
			}
		} else {
			if (event.value != _loaderKeys[0]) {
				fail("Original keyboard replay differs from the resident loader BIOS word");
				return false;
			}
			_loaderKeys.remove_at(0);
		}
	}
	return true;
}

byte PCodeMachine::inputKey() const {
	return _loaderKeys.empty() ? _keys[_keyPosition] : byte(_loaderKeys[0]);
}

void PCodeMachine::consumeInputKey() {
	if (_loaderKeys.empty())
		++_keyPosition;
	else
		_loaderKeys.remove_at(0);
}

bool PCodeMachine::flushLoaderKeys() {
	if (!keyboardScreenProfile())
		return true;
	// INT16/AH=1 reruns the matcher whenever its queue is empty. A flush
	// cannot terminate while a matching screen keeps reinserting keys.
	_loaderKeys.clear();
	if (!prepareLoaderKeys())
		return false;
	if (!_loaderKeys.empty()) {
		fail("Resident keyboard flush cannot finish while a loader prompt remains visible");
		return false;
	}
	return true;
}

void PCodeMachine::fail(const Common::String &message) {
	if (_state == kFault)
		return;
	_error = Common::String::format("%s (segment %u, procedure %u, IP 0x%04x)",
		message.c_str(), _segment, currentProcedure(), _ip);
	_state = kFault;
}

void PCodeMachine::yieldForInput() {
	if (!executionStopped()) {
		_state = kWaitingForInput;
		if (!_idleSnapshot && _observer) {
			_observer->beforeInput(0);
			_idleSnapshot = true;
		}
	}
}

void PCodeMachine::delay(uint16 ticks) {
	if (executionStopped())
		return;
	if (_observer)
		_observer->onDelay(ticks);
	_delayTicks = ticks;
	if (_services.enabled()) {
		uint16 rng[4];
		_console->getRngState(rng);
		bool available;
		Common::String error;
		const uint32 begin = _services.position();
		if (!_services.call(19, ticks, _console->rngAlgorithm(), rng, available, error)) {
			fail(error);
			return;
		}
		if (!replayLoaderEvents(begin, _services.position()))
			return;
		_console->setRngState(rng);
		_waitRemainingUs = 0;
		_soundPattern = 0xffff;
		_state = kDelayed;
		return;
	}
	if (!prepareLoaderKeys()) {
		if (_transfer.choosing) {
			_input.active = true;
			_input.address = ticks;
			_input.count = 1;
			_input.position = 0;
			_input.mode = 8;
			_input.kind = 6;
		}
		return;
	}
	if (_state != kFault) {
		_waitRemainingUs = NativeTiming::delayMicroseconds(ticks);
		_soundPattern = 0xffff;
		_state = kDelayed;
	}
}

bool PCodeMachine::originalServiceProfile() const {
	return (_nativeRevision == 1 && _console->rngAlgorithm() == NativeRandom::kFourWord) ||
		((_nativeRevision == 4 || _nativeRevision == 5) && _console->rngAlgorithm() == NativeRandom::kThreeWord);
}

bool PCodeMachine::validateServiceProfile(Common::String &error) const {
	if ((!_loaderData.empty() && (!loaderKeyboardProfile() || !LoaderKeyboard::validData(_nativeRevision, _loaderData))) ||
		!LoaderKeyboard::validQueue(_loaderKeys, _nativeRevision, _loaderData) ||
		(!_loaderKeys.empty() && !loaderKeyboardProfile() && !transferProfile())) {
		error = "Invalid resident loader keyboard queue or interpreter policy";
		return false;
	}
	if (!_transfer.validate(transferProfile(), false, error))
		return false;
	if (transferProfile() && transferProfile() != 5 && !_loaderKeys.empty() && (_loaderKeys.size() != 1 || _loaderKeys[0] != 0x1c0d)) {
		error = "Classic transfer loader queue must contain one original Return";
		return false;
	}
	if (_services.transferInput() && transferProfile() != 3 && transferProfile() != 5) {
		error = "Physical transfer choices require an original III or V paired profile";
		return false;
	}
	if (_transfer.choosing && (!_input.active || _state != kWaitingForInput || _input.kind < 2 || _input.kind > 6 || !_loaderKeys.empty())) {
		error = "Resident transfer chooser requires a suspended original input service";
		return false;
	}
	if (_input.kind >= 4 && (_input.kind > 6 || !_input.active || !_transfer.choosing || _input.count != 1 || _input.position || _input.mode != 8 || (_input.kind == 5 && _input.address != 1 && _input.address != 2))) {
		error = "Invalid suspended resident transfer service";
		return false;
	}
	const NativeRandom::Algorithm algorithm = _console->rngAlgorithm();
	if ((_nativeRevision < 4 && algorithm != NativeRandom::kFourWord) ||
		(_services.enabled() && !originalServiceProfile())) {
		error = "Native service replay and saved RNG profile do not match the interpreter revision";
		return false;
	}
	if ((_input.kind == 3 || _services.unit1Input()) && (!_originalMemory || !originalServiceProfile())) {
		error = "Original unit-1 input requires a recognized original-memory profile";
		return false;
	}
	if (_input.kind == 3 && (!_input.count || (!_input.active && _input.position != _input.count))) {
		error = "Invalid original unit-1 input progress";
		return false;
	}
	if (_services.inputMode() && ((!_console->portraitPolicy() && _nativeRevision == 5) || !_keys.empty() || _input.active || (_services.complete() && _state != kWaitingForInput))) {
		error = "Native input replay requires the current portrait profile, empty legacy keyboard state and valid checkpoint state";
		return false;
	}
	return true;
}

bool PCodeMachine::setServiceReplay(const Common::Array<NativeServiceReplay::Event> &events, Common::String &error) {
	if (!canSuspend() || !originalServiceProfile()) {
		error = "Native service replay requires the verified interpreter's original RNG profile";
		return false;
	}
	return _services.configure(events, 0, error);
}

bool PCodeMachine::setInputReplay(const Common::Array<NativeServiceReplay::Event> &events, Common::String &error) {
	if (!canSuspend() || !originalServiceProfile() || (_nativeRevision == 5 && !_console->portraitPolicy()) ||
		_input.active || keyAvailable()) {
		error = "Native input replay requires the verified original RNG and portrait profiles and an empty keyboard boundary";
		return false;
	}
	NativeServiceReplay next;
	if (!next.configure(events, 0, error, true))
		return false;
	if (next.unit1Input() && !_originalMemory) {
		error = "Original unit-1 replay requires original memory";
		return false;
	}
	if (next.transferInput() && transferProfile() != 3 && transferProfile() != 5) {
		error = "Physical transfer choice replay requires original paired Wizardry III or V";
		return false;
	}
	_services = Common::move(next);
	clearKeys();
	return true;
}

bool PCodeMachine::graphicsInit(bool first) {
	Common::String error;
	const uint32 begin = _services.position();
	if (!_services.graphicsInit(_nativeRevision, first, error)) {
		fail(error);
		return false;
	}
	return replayLoaderEvents(begin, _services.position());
}

bool PCodeMachine::pollKeyboard() {
	if (executionStopped())
		return false;
	if (!_services.enabled())
		return prepareLoaderKeys() && keyAvailable();
	uint16 rng[4];
	_console->getRngState(rng);
	bool available;
	Common::String error;
	const uint32 begin = _services.position();
	if (!_services.call(11, 0, _console->rngAlgorithm(), rng, available, error)) {
		fail(error);
		return false;
	}
	if (!replayLoaderEvents(begin, _services.position()))
		return false;
	_console->setRngState(rng);
	return available;
}

void PCodeMachine::sound(uint16, uint16 pattern, uint16) {
	if (_realtime && _state != kFault && pattern <= 2 && !(_biosModifiers & 0x40)) {
		_soundPattern = pattern;
		_waitRemainingUs = NativeTiming::soundMicroseconds(pattern);
		_state = kPlayingSound;
	}
}

void PCodeMachine::advanceTime(uint32 milliseconds) {
	if (_state != kDelayed && _state != kPlayingSound)
		return;
	if (_state == kDelayed && (keyAvailable() || (_biosModifiers & 0x1f))) {
		_waitRemainingUs = 0;
		return;
	}
	const uint64 elapsed = uint64(milliseconds) * 1000;
	_waitRemainingUs = elapsed >= _waitRemainingUs ? 0 : _waitRemainingUs - uint32(elapsed);
}

uint32 PCodeMachine::soundElapsedUs() const {
	return _state == kPlayingSound ? NativeTiming::soundMicroseconds(_soundPattern) - _waitRemainingUs : 0;
}

byte PCodeMachine::currentProcedure() const {
	return _frames.empty() ? 0 : _frames.back().procedure;
}

const PCodeProcedure *PCodeMachine::activeProcedure() const {
	return _frames.empty() ? nullptr : _code.procedure(_frames.back().segment, _frames.back().procedure);
}

byte PCodeMachine::fetchByte() {
	const PCodeSegment *s = _code.segment(_segment);
	if (!s || _ip >= s->code.size() || (_originalMemory && !segmentReferences(_segment))) {
		fail("Instruction fetch outside segment");
		return 0;
	}
	return _originalMemory ? readByte(uint16(segmentBase(_segment) + _ip++)) : s->code[_ip++];
}

uint16 PCodeMachine::fetchWord() {
	const uint lo = fetchByte();
	return lo | (uint16(fetchByte()) << 8);
}

uint16 PCodeMachine::big() {
	uint b = fetchByte();
	return (b & 0x80) ? ((b & 0x7f) << 8) | fetchByte() : b;
}

bool PCodeMachine::requireStack(uint32 count) {
	if (_stack.size() < count) {
		fail("Evaluation stack underflow");
		return false;
	}
	return true;
}

uint16 PCodeMachine::pop() {
	if (!requireStack(1))
		return 0;
	uint16 value = _stack.back();
	_stack.pop_back();
	return value;
}

void PCodeMachine::push(uint16 value) {
	if (_stack.size() >= (_originalMemory ? (0xf7feU - 0xe01cU) / 2 : 16384U)) {
		fail("Evaluation stack overflow");
		return;
	}
	_stack.push_back(value);
	if (_originalMemory)
		writeWord(originalStackPointer(), value);
}

void PCodeMachine::enter(byte sn, byte pn, StaticMode mode, bool external) {
	if (_originalMemory) {
		enterOriginal(sn, pn, mode, external);
		return;
	}
	const PCodeProcedure *p = _code.procedure(sn, pn);
	if (!p) {
		fail("Call to missing procedure");
		return;
	}
	const uint bytes = uint(p->parameterBytes) + p->localBytes;
	if (bytes > _np || _np - bytes < uint(_heap) + 64 || _frames.size() >= 512) {
		fail("Activation stack overflow");
		return;
	}
	if (!requireStack(p->parameterBytes / 2))
		return;
	Frame f;
	f.segment = sn;
	f.procedure = pn;
	f.returnSegment = _segment;
	f.returnIP = _ip;
	f.savedNP = _np;
	_np -= bytes;
	f.base = _np;
	for (uint i = 0; i < p->parameterBytes / 2; ++i)
		writeWord(f.base + 2 * i, pop());
	// The abstract allocator can clear locals because it does not overlay
	// code in the same memory, unlike the original interpreter.
	if (!_preserveLocals) {
		for (uint i = p->parameterBytes; i < bytes; ++i)
			_memory[uint16(f.base + i)] = 0;
	}
	f.stackBase = _stack.size();
	if (mode == kLocal)
		f.staticFrame = int32(_frames.size()) - 1;
	else if (mode == kGlobal)
		f.staticFrame = _frames.empty() ? -1 : 0;
	else if (mode == kSearch) {
		const int want = int(p->level) - 1;
		for (int i = int(_frames.size()) - 1; i >= 0; --i) {
			const PCodeProcedure *candidate = _code.procedure(_frames[i].segment, _frames[i].procedure);
			if (candidate && candidate->level == want) {
				f.staticFrame = i;
				break;
			}
		}
	}
	_frames.push_back(f);
	_segment = sn;
	_ip = p->entry;
}

void PCodeMachine::leave(byte nret) {
	if (_originalMemory) {
		leaveOriginal(nret);
		return;
	}
	if (_frames.empty()) {
		fail("Return with no activation frame");
		return;
	}
	const Frame f = _frames.back();
	const PCodeProcedure *p = activeProcedure();
	if (!p || 2U * nret > uint(p->parameterBytes) + p->localBytes || f.stackBase > _stack.size()) {
		fail("Invalid procedure return");
		return;
	}
	_stack.resize(f.stackBase);
	for (int i = nret - 1; i >= 0; --i)
		push(readWord(f.base + 2 * i));
	_np = f.savedNP;
	_frames.pop_back();
	_ip = f.returnIP;
	_segment = f.returnSegment;
	if (_frames.empty())
		_state = kHalted;
}

int32 PCodeMachine::walk(byte levels) {
	int32 f = int32(_frames.size()) - 1;
	while (levels-- && f >= 0) {
		if (_originalMemory) {
			const uint16 target = readWord(_frames[f].base - 12);
			f = -1;
			for (uint i = 0; i < _frames.size(); ++i)
				if (_frames[i].base - 12 == target) {
					f = i;
					break;
				}
		} else
			f = _frames[f].staticFrame;
	}
	if (f < 0 || f >= int32(_frames.size())) {
		fail("Invalid static link");
		return -1;
	}
	return f;
}

uint16 PCodeMachine::localAddress(int32 frame, uint16 word) {
	if (frame < 0 || frame >= int32(_frames.size()) || !word) {
		fail("Invalid local/global address");
		return 0;
	}
	const Frame &f = _frames[frame];
	return uint16(f.base + 2 * (word - 1));
}

uint16 PCodeMachine::literal(uint16 offset, uint16 length) {
	if (_originalMemory) {
		const PCodeSegment *s = _code.segment(_segment);
		if (!s || offset > s->code.size() || length > s->code.size() - offset || !segmentReferences(_segment)) {
			fail("Invalid resident literal extent");
			return 0;
		}
		return uint16(segmentBase(_segment) + offset);
	}
	for (uint i = 0; i < _literals.size(); ++i)
		if (_literals[i].segment == _segment && _literals[i].offset == offset)
			return _literals[i].address;
	const PCodeSegment *s = _code.segment(_segment);
	const uint32 next = uint32(_literalNext) + ((uint32(length) + 2) & ~1U);
	if (!s || offset > s->code.size() || length > s->code.size() - offset || next > 0x6000) {
		fail("Invalid or exhausted literal pool");
		return 0;
	}
	Literal lit;
	lit.segment = _segment;
	lit.offset = offset;
	lit.length = length;
	lit.address = _literalNext;
	memcpy(_memory + lit.address, s->code.data() + offset, length);
	_literalNext = next;
	_literals.push_back(lit);
	return lit.address;
}

void PCodeMachine::moveBytes(uint16 dst, uint16 src, uint32 size, bool backward) {
	if (size > 65536) {
		fail("Oversized memory move");
		return;
	}
	// Original MOVELEFT/SAS use REP MOVSB; MOVERIGHT sets DF first. A
	// temporary buffer (or memmove) loses the original overlap behavior.
	for (uint32 n = 0; n < size; ++n) {
		const uint32 i = backward ? size - n - 1 : n;
		writeByte(uint16(dst + i), readByte(uint16(src + i)));
	}
}

void PCodeMachine::moveWords(uint16 dst, uint16 src, uint16 count) {
	// MOV is REP MOVSW, not a sequence of byte copies. The distinction is
	// observable for odd overlapping addresses: each word is read first.
	for (uint32 i = 0; i < count; ++i)
		writeWord(uint16(dst + 2 * i), readWord(uint16(src + 2 * i)));
}

void PCodeMachine::fillMemory(uint16 dst, byte value, uint32 size) {
	if (size > 65536) {
		fail("Oversized memory fill");
		return;
	}
	for (uint32 i = 0; i < size; ++i)
		writeByte(uint16(dst + i), value);
}

Common::String PCodeMachine::pascalString(uint16 address) const {
	Common::String s;
	const uint size = readByte(address);
	for (uint i = 0; i < size; ++i)
		s += char(readByte(uint16(address + 1 + i)));
	return s;
}

PCodeMachine::State PCodeMachine::run(uint32 budget) {
	if (_state == kHalted || _state == kFault)
		return _state;
	if (_services.inputMode()) {
		if (_services.complete()) {
			if (_state != kWaitingForInput)
				yieldForInput();
			return _state;
		}
		if (_services.checkpoint()) {
			_idleSnapshot = false;
			yieldForInput();
			return _state;
		}
	}
	if (_realtime && (_state == kDelayed || _state == kPlayingSound)) {
		advanceTime(0);
		if (_waitRemainingUs)
			return _state;
	}
	_state = kRunning;
	_delayTicks = 0;
	_waitRemainingUs = 0;
	_soundPattern = 0xffff;
	if (_input.active)
		completeInput();
	for (uint32 n = 0; n < budget && _state == kRunning; ++n) {
		const byte op = fetchByte();
		if (_state == kFault)
			break;
		++_instructions;
		if (op < 0x80)
			push(op);
		else if (op >= 0xd8 && op <= 0xe7)
			push(readWord(localAddress(int32(_frames.size()) - 1, op - 0xd8 + 1)));
		else if (op >= 0xe8 && op <= 0xf7)
			push(readWord(localAddress(0, op - 0xe8 + 1)));
		else if (op >= 0xf8)
			push(readWord(uint16(pop() + 2 * (op - 0xf8))));
		else
			execute(op);
		finishTransferTransaction();
		if (!executionStopped() && _services.checkpoint()) {
			_idleSnapshot = false;
			yieldForInput();
		}
	}
	return _state;
}

void PCodeMachine::execute(byte op) {
	uint16 a = 0, b = 0, c = 0, d = 0;
	int32 sa = 0, sb = 0;
	switch (op) {
	case 0x80:
		sa = int16(pop());
		push(sa < 0 ? -sa : sa);
		break; // ABI
	case 0x82:
		b = pop();
		a = pop();
		push(a + b);
		break;
	case 0x84:
		b = pop();
		a = pop();
		push(a & b);
		break;
	case 0x86: // DVI: truncate toward zero, including INT16_MIN / -1.
	case 0x8e: // MODI: signed remainder follows the dividend.
		sb = int16(pop());
		sa = int16(pop());
		if (!sb) {
			fail("Division by zero");
			break;
		}
		push(op == 0x86 ? sa / sb : sa % sb);
		break;
	case 0x88: // CHK keeps the index on the stack.
		c = pop();
		b = pop();
		a = pop();
		if (int16(a) < int16(b) || int16(a) > int16(c))
			fail("Range check failed");
		push(a);
		break;
	case 0x8b: // INN: count, reversed set words, then the member below them.
		b = pop();
		if (!requireStack(uint32(b) + 1))
			break;
		a = _stack[_stack.size() - b - 1];
		c = (a < 512 && a < uint32(b) * 16) ? ((_stack[_stack.size() - 1 - a / 16] >> (a & 15)) & 1) : 0;
		_stack.resize(_stack.size() - b - 1);
		push(c);
		break;
	case 0x8d:
		b = pop();
		a = pop();
		push(a | b);
		break;
	case 0x8f:
		sb = int16(pop());
		sa = int16(pop());
		push(sa * sb);
		break;
	case 0x91:
		sa = int16(pop());
		push(-sa);
		break;
	case 0x93:
		push(~pop());
		break;
	case 0x95:
		b = pop();
		a = pop();
		push(a - b);
		break;
	case 0x98:
		sa = int16(pop());
		push(sa * sa);
		break;
	case 0x9a:
		b = pop();
		a = pop();
		writeWord(a, b);
		break;
	case 0x9b:
		b = pop();
		a = pop();
		if (!b || b > 255 || b > readByte(a))
			fail("String index out of range");
		push(a);
		push(b);
		break;
	case 0x9e:
		standardProcedure(fetchByte());
		break;
	case 0x9f:
		push(0);
		break;
	case 0xa1:
		a = pop();
		jump(!(a & 1));
		break;
	case 0xa2:
		b = big();
		a = pop();
		push(a + 2 * b);
		break;
	case 0xa3:
		b = big();
		a = pop();
		push(readWord(uint16(a + 2 * b)));
		break;
	case 0xa4:
		c = big();
		sb = int16(pop());
		a = pop();
		push(a + 2 * int32(c) * sb);
		break;
	case 0xa5:
		push(localAddress(0, big()));
		break;
	case 0xa6: {
		uint32 offset = _ip;
		uint size = fetchByte();
		push(literal(offset, size + 1));
		_ip += size;
	} break;
	case 0xa8:
		c = big();
		b = pop();
		a = pop();
		moveWords(a, b, c);
		break;
	case 0xa9:
		push(readWord(localAddress(0, big())));
		break;
	case 0xaa:
		c = fetchByte();
		b = pop();
		a = pop();
		if (b < 256) {
			if (c < 1)
				fail("SAS character exceeds target capacity");
			writeByte(a, 1);
			writeByte(uint16(a + 1), b);
		} else {
			d = readByte(b);
			if (d > c) {
				fail("SAS string exceeds target capacity");
				break;
			}
			moveBytes(a, b, d + 1);
		}
		break;
	case 0xab:
		a = localAddress(0, big());
		writeWord(a, pop());
		break;
	case 0xac:
		caseJump();
		break;
	case 0xad:
		leave(fetchByte());
		break;
	case 0xae:
		enter(_segment, fetchByte(), kSearch);
		break;
	case 0xaf:
	case 0xb0:
	case 0xb1:
	case 0xb4:
	case 0xb5:
	case 0xb7:
		compare(op);
		break;
	case 0xb2:
	case 0xb6:
	case 0xb8: {
		int32 frame = walk(fetchByte());
		a = localAddress(frame, big());
		if (op == 0xb2)
			push(a);
		else if (op == 0xb6)
			push(readWord(a));
		else
			writeWord(a, pop());
	} break;
	case 0xb3: { // LDC: word-aligned literal words, pushed in file order.
		a = fetchByte();
		_ip = (_ip + 1) & ~1U;
		const PCodeSegment *segment = _code.segment(_segment);
		if (!a || !segment || _ip > segment->code.size() || 2U * a > segment->code.size() - _ip) {
			fail("Invalid multiword constant extent");
			break;
		}
		if (_stack.size() + a > 16384) {
			fail("Evaluation stack overflow");
			break;
		}
		for (uint i = 0; i < a; ++i)
			push(fetchWord());
		break;
	}
	case 0xb9:
		jump(true);
		break;
	case 0xba:
		c = pop();
		b = pop();
		a = pop();
		if (b > 16 || c > 15 || b + c > 16) {
			fail("Invalid packed field");
			break;
		}
		push((readWord(a) >> c) & packedFieldMask(b, _originalMemory ? _nativeRevision : 0));
		break;
	case 0xbb:
		d = pop();
		c = pop();
		b = pop();
		a = pop();
		if (b > 16 || c > 15 || b + c > 16) {
			fail("Invalid packed field");
			break;
		}
		{
			uint32 mask = uint32(packedFieldMask(b, _originalMemory ? _nativeRevision : 0)) << c;
			writeWord(a, (readWord(a) & ~mask) | ((uint32(d) << c) & mask));
		}
		break;
	case 0xbe:
		b = pop();
		a = pop();
		push(readByte(uint16(a + b)));
		break;
	case 0xbf:
		c = pop();
		b = pop();
		a = pop();
		writeByte(uint16(a + b), c);
		break;
	case 0xc0:
		c = fetchByte();
		d = fetchByte();
		b = pop();
		a = pop();
		if (!c || !d || d > 16 || c * d > 16) {
			fail("Invalid packed-array descriptor");
			break;
		}
		push(a + 2 * (b / c));
		push(d);
		push((b % c) * d);
		break;
	case 0xc1:
		// INTERP op_RBP restores the previous base context, then shares
		// RNP's return sequence. The I-III programs use only RBP 0
		// in WIZARDRY:1, returning to the host rather than another Pascal
		// base program. Other base-return contexts need a separate model.
		a = fetchByte();
		if (_state == kFault)
			break;
		if (a || _frames.size() != 1 || _frames[0].segment != 1 || _frames[0].procedure != 1) {
			fail("Unsupported base-program return context");
			break;
		}
		leave(0);
		break;
	case 0xc3:
		b = pop();
		a = pop();
		push(a == b);
		break;
	case 0xc4:
		sb = int16(pop());
		sa = int16(pop());
		push(sa >= sb);
		break;
	case 0xc5:
		sb = int16(pop());
		sa = int16(pop());
		push(sa > sb);
		break;
	case 0xc6:
		push(localAddress(int32(_frames.size()) - 1, big()));
		break;
	case 0xc7:
		push(fetchWord());
		break;
	case 0xc8:
		sb = int16(pop());
		sa = int16(pop());
		push(sa <= sb);
		break;
	case 0xc9:
		sb = int16(pop());
		sa = int16(pop());
		push(sa < sb);
		break;
	case 0xca:
		push(readWord(localAddress(int32(_frames.size()) - 1, big())));
		break;
	case 0xcb:
		b = pop();
		a = pop();
		push(a != b);
		break;
	case 0xcc:
		a = localAddress(int32(_frames.size()) - 1, big());
		writeWord(a, pop());
		break;
	case 0xcd:
		a = fetchByte();
		b = fetchByte();
		enter(_originalMemory ? a : (a ? a : _segment), b, kSearch, a != 0);
		break;
	case 0xce:
		enter(_segment, fetchByte(), kLocal);
		break;
	case 0xcf:
		enter(_segment, fetchByte(), kGlobal);
		break;
	case 0xd2:
	case 0xd7:
		break;
	case 0xd6:
		_state = kHalted;
		break;
	default:
		fail(Common::String::format("Unimplemented opcode 0x%02x", op));
		break;
	}
}

void PCodeMachine::jump(bool take) {
	const byte displacement = fetchByte();
	if (!take)
		return;
	if (displacement < 0x80) {
		_ip += displacement;
	} else {
		const PCodeProcedure *p = activeProcedure();
		const PCodeSegment *s = _code.segment(_segment);
		const int anchor = _originalMemory ? int(readWord(0x0608)) - segmentBase(_segment) : (p ? p->anchor : -1);
		const int slot = anchor >= 0 ? anchor + int(displacement) - 256 : -1;
		if (!s || slot < 0 || slot + 2 > int(s->code.size())) {
			fail("Invalid backward jump slot");
			return;
		}
		const int target = slot - codeWord(_segment, slot);
		if (target < 0) {
			fail("Invalid backward jump target");
			return;
		}
		_ip = target;
	}
}

void PCodeMachine::caseJump() {
	if (_ip & 1)
		++_ip;
	const int32 low = int16(fetchWord());
	const int32 high = int16(fetchWord());
	const int32 value = int16(pop());
	if (value < low || value > high)
		return; // Execute the default UJP at the current IP.
	const PCodeSegment *s = _code.segment(_segment);
	const uint32 slot = _ip + 2 + 2 * (value - low);
	if (!s || slot + 2 > s->code.size()) {
		fail("Invalid case jump table");
		return;
	}
	const uint32 relative = codeWord(_segment, slot);
	if (relative > slot) {
		fail("Invalid case jump target");
		return;
	}
	_ip = slot - relative;
}

void PCodeMachine::compare(byte opcode) {
	const byte type = fetchByte();
	int order = 0;
	if (type == 4) {
		const uint16 b = pop(), a = pop();
		Common::String left = a < 256 ? Common::String(char(a)) : pascalString(a);
		Common::String right = b < 256 ? Common::String(char(b)) : pascalString(b);
		const uint length = MIN(left.size(), right.size());
		for (uint i = 0; i < length && !order; ++i)
			order = int(byte(left[i])) - int(byte(right[i]));
		if (!order)
			order = int(left.size()) - int(right.size());
	} else if (type == 6) {
		const uint16 b = pop() & 1, a = pop() & 1;
		order = int(a) - int(b);
	} else if (type == 10 || type == 12) {
		const uint32 size = uint32(big()) * (type == 12 ? 2 : 1);
		const uint16 b = pop(), a = pop();
		if (size > 65536) {
			fail("Oversized array comparison");
			return;
		}
		if (type == 12) {
			// The original uses unsigned CMPSW ordering. Comparing the
			// little-endian byte encoding reverses e.g. 0100h versus 00ffh.
			for (uint32 i = 0; i < size && !order; i += 2)
				order = int(readWord(uint16(a + i))) - int(readWord(uint16(b + i)));
		} else {
			for (uint32 i = 0; i < size && !order; ++i)
				order = int(readByte(uint16(a + i))) - int(readByte(uint16(b + i)));
		}
	} else {
		fail(Common::String::format("Unimplemented comparison type %u", type));
		return;
	}
	switch (opcode) {
	case 0xaf:
		push(order == 0);
		break;
	case 0xb7:
		push(order != 0);
		break;
	case 0xb4:
		push(order <= 0);
		break;
	case 0xb5:
		push(order < 0);
		break;
	case 0xb0:
		push(order >= 0);
		break;
	case 0xb1:
		push(order > 0);
		break;
	default:
		break;
	}
}

void PCodeMachine::standardProcedure(byte n) {
	uint16 a = 0, b = 0, c = 0, d = 0, e = 0;
	switch (n) {
	case 1:
		if (_originalMemory) {
			flushOriginalHeap();
			if (_state == kFault)
				break;
			b = pop();
			a = pop();
			const uint32 next = uint32(_heap) + uint16(2U * b);
			if (next >= _np) {
				fail("Heap/arena collision in NEW");
				break;
			}
			writeWord(a, _heap);
			setHeapTop(next);
			break;
		}
		b = pop();
		a = pop();
		if (uint32(_heap) + 2U * b + 64 > _np) {
			fail("Heap/stack collision in NEW");
			break;
		}
		writeWord(a, _heap);
		_heap += 2 * b;
		break;
	case 2:
	case 3:
		e = pop();
		d = pop();
		c = pop();
		b = pop();
		a = pop();
		if (_originalMemory && n == 2 && uint16(a - uint16(c + d)) == 2 && !(e & 1)) {
			// Preserve the original MOVELEFT shortcut, including its unusual
			// continuation. It compares the *base* source argument, fills
			// words from LODSW, then reaches fetch with SI still at source+2.
			// Ordinary MOVELEFT, including odd counts, restores its old SI.
			const uint16 value = readWord(uint16(a + b));
			for (uint32 i = 0; i < e; i += 2)
				writeWord(uint16(c + d + i), value);
			_ip = uint16(a + b + 2 - segmentBase(_segment));
		} else {
			moveBytes(uint16(c + d), uint16(a + b), e, n == 3);
		}
		break;
	case 4: {
		b = pop();
		a = pop();
		int index = int(_frames.size()) - 1;
		while (index >= 0 && !(_frames[index].segment == a && _frames[index].procedure == b))
			--index;
		if (index < 0) {
			fail("EXIT target not on dynamic call chain");
			break;
		}
		if (_originalMemory) {
			for (int i = int(_frames.size()) - 1; i > index; --i) {
				const Frame &caller = _frames[i - 1];
				PCodeProcedure p;
				if (!originalProcedure(caller.segment, caller.procedure, p)) {
					fail("Invalid resident EXIT procedure");
					break;
				}
				_frames[i].returnIP = p.exit;
				writeWord(_frames[i].base - 4, segmentBase(caller.segment) + p.exit);
			}
			PCodeProcedure p;
			if (_state == kFault)
				break;
			if (!originalProcedure(_segment, currentProcedure(), p)) {
				fail("Invalid resident EXIT procedure");
				break;
			}
			_ip = p.exit;
			break;
		}
		while (int(_frames.size()) - 1 > index) {
			_np = _frames.back().savedNP;
			_segment = _frames.back().returnSegment;
			_frames.pop_back();
		}
		const PCodeProcedure *p = activeProcedure();
		if (p)
			_ip = p->exit;
		else
			fail("Missing EXIT procedure");
	} break;
	case 5:
		unitIO(false);
		break;
	case 6:
		unitIO(true);
		break;
	case 10:
		d = pop();
		c = pop();
		b = pop();
		a = pop();
		if (int16(c) > 0)
			fillMemory(uint16(a + b), byte(d), c);
		break;
	case 21:
	case 22:
		if (!_originalMemory) {
			fail("Explicit segment lifetime requires original memory mode");
			break;
		}
		a = pop();
		if (a >= 32) {
			fail("Invalid explicit code segment number");
			break;
		}
		if (n == 21)
			loadSegment(a);
		else
			releaseSegment(a);
		break;
	case 32:
		if (_originalMemory)
			flushOriginalHeap();
		if (_state != kFault)
			writeWord(pop(), _heap);
		break;
	case 33:
		a = readWord(pop());
		// WIZARDRY:18 also uses RELEASE to move the heap frontier forward.
		// It is a frontier assignment, not necessarily a monotonic free.
		if (a < (_originalMemory ? 0x0732 : 0x6000) || uint32(a) + (_originalMemory ? 0 : 64) > _np) {
			fail("Invalid RELEASE mark");
			break;
		}
		setHeapTop(a);
		if (_originalMemory)
			writeWord(0xe0e6, 0);
		break;
	case 34:
		push(_originalMemory ? readWord(0xe0de) : _ioResult);
		break;
	case 38:
		a = pop();
		if (a == 1 || a == 2) {
			if (_services.inputMode()) {
				Common::String error;
				const uint32 begin = _services.position();
				if (!_services.flushInput(a, error)) {
					fail(error);
					break;
				}
				if (!replayLoaderEvents(begin, _services.position()))
					break;
				setIoResult(0);
			} else if (!_scriptedInput || loaderKeyboardProfile()) {
				if (!flushLoaderKeys()) {
					if (_transfer.choosing) {
						_input.active = true;
						_input.address = a;
						_input.count = 1;
						_input.position = 0;
						_input.mode = 8;
						_input.kind = 5;
					}
					break;
				}
				if (!_scriptedInput)
					clearKeys();
				setIoResult(0);
			} else if (_nativeRevision == 1) {
				// Scripted input retains type-ahead and IORESULT.
				prepareLoaderKeys();
				if (_transfer.choosing) {
					_input.active = true;
					_input.address = a;
					_input.count = 1;
					_input.position = 0;
					_input.mode = 8;
					_input.kind = 5;
				}
			}
		}
		break;
	case 40:
		a = _originalMemory && readWord(0xe0e6) ? readWord(0xe0e6) : _heap;
		push(uint16(_np - a) / 2);
		break;
	default:
		fail(Common::String::format("Unimplemented standard procedure %u", n));
		break;
	}
}

void PCodeMachine::unitIO(bool write) {
	if (!requireStack(6))
		return;
	const uint16 w = pop(), z = pop(), y = pop(), x = pop(), ptr = pop(), unit = pop();
	const uint16 address = ptr + x;
	if (unit == 13) {
		if (!_console->call(write, address, y, z, w) && _transfer.choosing && !write && y == 11) {
			_input.active = true;
			_input.address = address;
			_input.count = 1;
			_input.position = 0;
			_input.mode = 8;
			_input.kind = 4;
		}
	} else if (unit == 4 || unit == 5 || (unit >= 9 && unit <= 12)) {
		if (!write && _ramCache.configured()) {
			Common::String error;
			uint16 result = 0;
			if (!readGuestDisk(unit, z, y, address, result, error)) {
				fail(error);
				return;
			}
			setIoResult(result);
			return;
		}
		if (_diskPolicy == kResidentCOM) {
			Common::String error;
			uint16 result;
			ResidentDiskTransport transport(*this, _disk, _saveDisk, _diskSelectedSave, activeTransferDisk(), &_diskDMABuffer);
			if (!transport.unitIO(write, unit, address, y, z, w, result, error)) {
				fail(error);
				return;
			}
			setIoResult(result);
			return;
		}
		const uint32 offset = uint32(z) * 512;
		const uint32 available = offset < _disk.size() ? MIN<uint32>(y, _disk.size() - offset) : 0;
		if (_diskPolicy == kRaw0880 && !write) {
			// Original full-sector EOF reads leave the uncopied guest bytes
			// intact and return IORESULT=0. A partial-sector EOF additionally
			// depends on the DOS bounce buffer, which is not modeled here.
			if (available != y && (y & 511)) {
				fail("Partial-sector disk EOF requires an unmodeled original bounce buffer");
				return;
			}
			for (uint32 i = 0; i < available; ++i)
				writeByte(uint16(address + i), _disk[offset + i]);
			setIoResult(0);
			return;
		}
		// Writes stay within the private image instead of extending the DOS
		// file or ignoring its write errors.
		if (offset > _disk.size() || available != y || (_diskPolicy == kLegacy632 && (z >= 632 || offset + y > 632U * 512))) {
			setIoResult(10);
			return;
		}
		for (uint32 i = 0; i < y; ++i) {
			if (write)
				_disk[offset + i] = readByte(uint16(address + i));
			else
				writeByte(uint16(address + i), _disk[offset + i]);
		}
		if (write && y)
			diskWritten();
		setIoResult(0);
	} else if (unit == 1 || unit == 2) {
		if (write && _originalMemory && originalServiceProfile()) {
			writeConsoleOutput(unit, address, y, w);
			return;
		}
		if (!write && _services.inputMode()) {
			if (!y) {
				setIoResult(0);
				return;
			}
			Common::Array<byte> bytes;
			Common::String error;
			const bool modeAware = _services.modeAwareRead();
			const uint32 replayBegin = _services.position();
			if (unit == 1 && modeAware) {
				NativeServiceReplay next = _services;
				if (!next.readInput(unit, y, bytes, error, w, address, readByte(0xe130))) {
					fail(error);
					return;
				}
				PendingInput request;
				request.active = true;
				request.address = address;
				request.count = y;
				request.kind = 3;
				request.mode = w;
				uint consumed;
				if (keyboardScreenProfile()) {
					Common::Array<uint16> biosWords;
					Common::Array<NativeServiceReplay::Event> keyboardEvents;
					for (uint32 i = replayBegin; i < next.position(); ++i) {
						if (_services.events()[i].type == NativeServiceReplay::kBiosAX)
							biosWords.push_back(_services.events()[i].value);
						if (_services.events()[i].type == NativeServiceReplay::kBiosAX ||
							_services.events()[i].type == NativeServiceReplay::kTransferKey)
							keyboardEvents.push_back(_services.events()[i]);
					}
					if (!validateConsoleInput(request, true, &bytes, &biosWords, &keyboardEvents))
						return;
					uint keyIndex = 0;
					for (uint32 i = replayBegin; i < next.position(); ++i) {
						if (_services.events()[i].type == NativeServiceReplay::kTransferKey) {
							if (!replayLoaderEvents(i, i + 1))
								return;
							continue;
						}
						if (_services.events()[i].type != NativeServiceReplay::kBiosAX)
							continue;
						if (!replayLoaderEvents(i, i + 1))
							return;
						Common::Array<byte> one;
						one.push_back(bytes[keyIndex]);
						if (!consumeConsoleInput(keyIndex ? _input : request, one, !keyIndex, false, consumed))
							return;
						++keyIndex;
					}
					if (_input.active || keyIndex != bytes.size()) {
						fail("Incomplete resident unit-1 input replay");
						return;
					}
				} else if (!consumeConsoleInput(request, bytes, true, true, consumed))
					return;
				_services = Common::move(next);
				return;
			}
			if (!_services.readInput(unit, y, bytes, error, w, address, readByte(0xe130))) {
				fail(error);
				return;
			}
			if (!replayLoaderEvents(replayBegin, _services.position()))
				return;
			if (modeAware)
				prepareInput(w);
			for (uint i = 0; i < bytes.size(); ++i) {
				if (_observer)
					_observer->beforeInput(bytes[i]);
				if (modeAware)
					setIoResult(0);
				writeByte(uint16(address + i), bytes[i]);
				_idleSnapshot = false;
			}
			if (modeAware)
				finishInput();
			if (!modeAware)
				setIoResult(0);
			return;
		}
		if (!write && unit == 1 && _originalMemory && originalServiceProfile() && y) {
			PendingInput request;
			request.active = true;
			request.address = address;
			request.count = y;
			request.kind = 3;
			request.mode = w;
			if (keyboardScreenProfile()) {
				if (!validateConsoleInput(request, true))
					return;
				Common::Array<byte> empty;
				uint consumed;
				if (consumeConsoleInput(request, empty, true, false, consumed))
					completeInput();
				return;
			}
			const uint available = MIN<uint>(y, _keys.size() - _keyPosition);
			Common::Array<byte> bytes;
			for (uint i = 0; i < available; ++i)
				bytes.push_back(_keys[_keyPosition + i]);
			uint consumed;
			if (!consumeConsoleInput(request, bytes, true, false, consumed))
				return;
			_keyPosition += consumed;
			if (_input.active)
				yieldForInput();
			return;
		}
		if (write || !y || unit == 1 || !_originalMemory)
			setIoResult(0);
		if (!write) {
			if (!y)
				return;
			_input.active = true;
			_input.address = address;
			_input.count = y;
			_input.position = 0;
			_input.kind = unit;
			_input.mode = unit == 2 ? w : 8;
			if (unit == 2)
				prepareInput(w);
			completeInput();
		}
	} else {
		setIoResult(2);
	}
}

void PCodeMachine::prepareInput(uint16 mode) {
	const byte flags = readByte(0xe21c);
	writeByte(0x0630, flags);
	writeByte(0xe21c, flags | ((flags >> 4) & 3));
	writeByte(0x0623, mode & 8 ? 0 : 255);
	writeByte(0x0622, mode & 4 ? 0 : 255);
}

void PCodeMachine::finishInput() {
	writeByte(0xe21c, readByte(0x0630));
}

class StagedConsoleHost : public MachineHost {
public:
	StagedConsoleHost(Common::Array<byte> &memory, byte revision) : _memory(memory), _revision(revision) {}
	byte readByte(uint16 address) const override { return _memory[address]; }
	uint16 readWord(uint16 address) const override { return readByte(address) | uint16(readByte(uint16(address + 1))) << 8; }
	void writeByte(uint16 address, byte value) override { _memory[address] = value; }
	void writeWord(uint16 address, uint16 value) override {
		writeByte(address, value);
		writeByte(uint16(address + 1), value >> 8);
	}
	bool readDisk(uint32, uint32, byte *) override {
		fail("Unexpected disk access while staging console output");
		return false;
	}
	bool keyAvailable() const override { return false; }
	byte nativeRevision() const override { return _revision; }
	void setIoResult(uint16 value) override { writeWord(0xe0de, value); }
	void yieldForInput() override {}
	void delay(uint16) override {}
	void sound(uint16, uint16, uint16) override {}
	void fail(const Common::String &message) override {
		if (error.empty())
			error = message;
	}
	Common::String error;

private:
	Common::Array<byte> &_memory;
	byte _revision;
};

class ConsoleIO {
public:
	ConsoleIO(MachineHost &host, PCodeConsole &console, MachineObserver *observer = nullptr) : _host(host), _console(console), _observer(observer) {}

	uint read(uint16 address, uint16 count, uint16 mode, uint16 &position,
		bool &active, const Common::Array<byte> &keys, bool begin);
	void write(uint16 unit, uint16 address, uint16 count, uint16 mode);

private:
	MachineHost &_host;
	PCodeConsole &_console;
	MachineObserver *_observer;

	void initialize(uint16 mode);
	void finish();
	void outputCharacter(byte character, uint16 unit);
	void emitCharacter(byte character);
};

void ConsoleIO::initialize(uint16 mode) {
	const byte flags = _host.readByte(0xe21c);
	_host.writeByte(0x0630, flags);
	_host.writeByte(0xe21c, flags | ((flags >> 4) & 3));
	_host.writeByte(0x0623, mode & 8 ? 0 : 255);
	_host.writeByte(0x0622, mode & 4 ? 0 : 255);
}

void ConsoleIO::finish() {
	_host.writeByte(0xe21c, _host.readByte(0x0630));
}

void ConsoleIO::emitCharacter(byte character) {
	if (character) {
		if (_console.graphicsOutput()) {
			_console.graphicsCharacter(character);
		} else {
			_console.teletypeCharacter(character);
			if (_observer && _observer->capturesTeletype())
				_observer->onTeletype(character);
		}
	}
	_host.writeByte(0xe0de, 0);
	_host.writeByte(0xe0df, 0);
}

void ConsoleIO::outputCharacter(byte character, uint16 unit) {
	if (_host.readByte(0x0623) && _host.readByte(uint16(0x0624 + unit))) {
		for (uint spaces = byte(character - 0x20); spaces; --spaces)
			emitCharacter(0x20);
		_host.writeByte(uint16(0x0624 + unit), 0);
	} else if (_host.readByte(0x0623) && character == 0x10) {
		_host.writeByte(uint16(0x0624 + unit), 255);
	} else {
		if (_host.readByte(0x0622) && character == 13) {
			emitCharacter(13);
			character = 10;
		}
		emitCharacter(character);
	}
}

uint ConsoleIO::read(uint16 address, uint16 count, uint16 mode, uint16 &position,
	bool &active, const Common::Array<byte> &keys, bool begin) {
	if (begin)
		initialize(mode);
	uint consumed = 0;
	while (consumed < keys.size() && position < count) {
		const byte key = keys[consumed++];
		if (_observer)
			_observer->beforeInput(key);
		_host.writeByte(0xe0de, 0);
		_host.writeByte(0xe0df, 0);
		const uint16 destination = uint16(address + position);
		_host.writeByte(destination, key);
		if (_host.readByte(0x0623) && key == _host.readByte(0xe130)) {
			for (; position < count; ++position)
				_host.writeByte(uint16(address + position), 0);
			break;
		}
		outputCharacter(key, 1);
		++position;
	}
	active = position < count;
	if (!active)
		finish();
	return consumed;
}

bool PCodeMachine::consumeConsoleInput(const PendingInput &request, const Common::Array<byte> &keys,
	bool begin, bool requireComplete, uint &consumed) {
	Common::Array<byte> staged(_memory, sizeof(_memory));
	StagedConsoleHost host(staged, _nativeRevision);
	PCodeConsole console(host, *_console);
	ConsoleIO stagedIO(host, console);
	PendingInput next = request;
	consumed = stagedIO.read(next.address, next.count, next.mode, next.position, next.active, keys, begin);
	if (!host.error.empty()) {
		fail(host.error);
		return false;
	}
	if (requireComplete && (next.active || consumed != keys.size())) {
		fail("Original unit-1 replay does not end at its count or terminator");
		return false;
	}
	_input = request;
	ConsoleIO liveIO(*this, *_console, _observer);
	liveIO.read(_input.address, _input.count, _input.mode, _input.position, _input.active, keys, begin);
	if (consumed)
		_idleSnapshot = false;
	return true;
}

bool PCodeMachine::validateConsoleInput(const PendingInput &request, bool begin,
	const Common::Array<byte> *replayBytes, const Common::Array<uint16> *biosWords,
	const Common::Array<NativeServiceReplay::Event> *keyboardEvents) {
	if (replayBytes && (!biosWords || biosWords->size() != replayBytes->size())) {
		fail("Original unit-1 replay has an inconsistent BIOS word count");
		return false;
	}
	Common::Array<byte> staged(_memory, sizeof(_memory));
	StagedConsoleHost host(staged, _nativeRevision);
	PCodeConsole console(host, *_console);
	ConsoleIO stagedIO(host, console);
	Common::Array<uint16> queued(_loaderKeys);
	PendingInput next = request;
	uint position = 0;
	uint physical = _keyPosition, replayEvent = 0;
	bool choosing = _transfer.choosing;
	bool first = begin;
	// A failed later echo must not expose an earlier key or a DLE prefix.
	// Stage the whole available sequence, including screen-dependent keys.
	while (next.position < next.count) {
		if (queued.empty() && !choosing) {
			const LoaderKeyboard::Action action = LoaderKeyboard::match(keyboardMatchProfile(), console.keyboardVram());
			if (action == LoaderKeyboard::kExit)
				return true;
			if ((action == LoaderKeyboard::kTransferSource && transferProfile() == 3) ||
				(action == LoaderKeyboard::kAuxiliary && transferProfile() == 5)) {
				choosing = true;
				console.drawTransferChoice(transferProfile());
			} else if (action == LoaderKeyboard::kTransferSource || action == LoaderKeyboard::kTransferClose) {
				queued.push_back(0x1c0d);
			}
			if (action == LoaderKeyboard::kAuxiliary && transferProfile() != 5) {
				fail("Resident transfer requires an authenticated original program");
				return false;
			}
			if (action == LoaderKeyboard::kValidationCode || action == LoaderKeyboard::kMagicWord) {
				if (_loaderData.empty()) {
					fail("Resident challenge requires the matching WIZ4.COM or WIZ5.COM at startup");
					return false;
				}
				LoaderKeyboard::challengeKeys(_nativeRevision, console.keyboardVram(), _loaderData, queued);
			} else if (queued.empty()) {
				LoaderKeyboard::enqueue(action, queued);
			}
		}
		if (replayBytes && keyboardEvents) {
			while (replayEvent < keyboardEvents->size() &&
				(*keyboardEvents)[replayEvent].type == NativeServiceReplay::kTransferKey) {
				if (!choosing) {
					fail("Transfer choice replay is outside an original source chooser");
					return false;
				}
				const byte choice = (*keyboardEvents)[replayEvent++].value;
				byte source = 0;
				if (LoaderKeyboard::transferChoice(transferProfile(), choice, source, queued))
					choosing = false;
			}
		} else if (choosing) {
			while (choosing && physical < _keys.size()) {
				const byte choice = _keys[physical++];
				byte source = 0;
				if (LoaderKeyboard::transferChoice(transferProfile(), choice, source, queued))
					choosing = false;
			}
		}
		if (choosing) {
			if (replayBytes) {
				fail("Original keyboard replay lacks a physical source choice");
				return false;
			}
			break;
		}
		byte key;
		if (replayBytes) {
			if (position == replayBytes->size())
				break;
			if (keyboardEvents)
				++replayEvent;
			if (!queued.empty() && (*biosWords)[position] != queued[0]) {
				fail("Original keyboard replay differs from the resident loader BIOS word");
				return false;
			}
			key = (*replayBytes)[position++];
		} else if (!queued.empty()) {
			key = byte(queued[0]);
		} else {
			if (physical == _keys.size())
				break;
			key = _keys[physical++];
		}
		Common::Array<byte> one;
		one.push_back(key);
		stagedIO.read(next.address, next.count, next.mode, next.position, next.active, one, first);
		first = false;
		if (!host.error.empty()) {
			fail(host.error);
			return false;
		}
		if (!queued.empty())
			queued.remove_at(0);
	}
	if (replayBytes && (next.active || position != replayBytes->size())) {
		fail("Original unit-1 replay does not end at its count or terminator");
		return false;
	}
	return true;
}

void ConsoleIO::write(uint16 unit, uint16 address, uint16 count, uint16 mode) {
	initialize(mode);
	for (uint32 i = 0; i < count; ++i)
		outputCharacter(_host.readByte(uint16(address + i)), unit);
	finish();
}

bool PCodeMachine::writeConsoleOutput(uint16 unit, uint16 address, uint16 count, uint16 mode) {
	if (!count) {
		setIoResult(0);
		return true;
	}
	Common::Array<byte> staged(_memory, sizeof(_memory));
	StagedConsoleHost host(staged, _nativeRevision);
	PCodeConsole console(host, *_console);
	ConsoleIO stagedIO(host, console);
	stagedIO.write(unit, address, count, mode);
	if (!host.error.empty()) {
		fail(host.error);
		return false;
	}
	ConsoleIO liveIO(*this, *_console, _observer);
	liveIO.write(unit, address, count, mode);
	return true;
}

void PCodeMachine::completeInput() {
	if (_input.kind >= 4) {
		const PendingInput pending = _input;
		if (!prepareLoaderKeys())
			return;
		_input = PendingInput();
		if (pending.kind == 4)
			_console->call(false, pending.address, 11, 0, 0);
		else if (pending.kind == 6)
			delay(pending.address);
		else if (!_scriptedInput || loaderKeyboardProfile()) {
			if (!flushLoaderKeys()) {
				if (_transfer.choosing)
					_input = pending;
				return;
			}
			if (!_scriptedInput)
				clearKeys();
			setIoResult(0);
		}
		return;
	}
	if (_input.kind == 3) {
		if (keyboardScreenProfile()) {
			if (!validateConsoleInput(_input, false))
				return;
			while (_input.active) {
				if (!prepareLoaderKeys())
					return;
				if (!keyAvailable()) {
					yieldForInput();
					return;
				}
				Common::Array<byte> one;
				one.push_back(inputKey());
				uint consumed;
				if (!consumeConsoleInput(_input, one, false, false, consumed))
					return;
				if (consumed)
					consumeInputKey();
			}
			return;
		}
		const uint available = MIN<uint>(_input.count - _input.position, _keys.size() - _keyPosition);
		Common::Array<byte> bytes;
		for (uint i = 0; i < available; ++i)
			bytes.push_back(_keys[_keyPosition + i]);
		uint consumed;
		if (!consumeConsoleInput(_input, bytes, false, false, consumed))
			return;
		_keyPosition += consumed;
		if (_input.active)
			yieldForInput();
		return;
	}
	while (_input.position < _input.count) {
		if (!prepareLoaderKeys())
			return;
		if (!keyAvailable()) {
			yieldForInput();
			return;
		}
		const byte key = inputKey();
		consumeInputKey();
		if (_observer)
			_observer->beforeInput(key);
		if (_input.kind == 2)
			setIoResult(0);
		writeByte(uint16(_input.address + _input.position++), key);
		_idleSnapshot = false;
		if (_input.kind == 2 && readByte(0x0623) && key == readByte(0xe130))
			break;
	}
	if (_input.kind == 2)
		finishInput();
	_input.active = false;
	if (_input.kind != 2)
		setIoResult(0);
}

} // namespace Wizardry
