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

#include "common/endian.h"
#include "common/memstream.h"
#include "common/util.h"
#include "wizardry/vm/console.h"
#include "wizardry/vm/machine.h"
#include "wizardry/vm/timing.h"

namespace Wizardry {

static const uint32 kStateMagic = 0x575a564d; // WZVM
static const uint16 kStateVersion = 13;
static const uint32 kResidueExtensionMagic = 0x44533133; // DS13
static const uint32 kResidueStateMagic = 0x52543133;     // RT13
static const uint32 kMaxStateBytes = 6 * 1024 * 1024;
static const uint32 kMaxCacheStateBytes = 4014 + 818 * 512;
static const uint32 kMaxGraphicsStateBytes = 19592;

bool PCodeMachine::saveState(Common::WriteStream &out, Common::String &error) const {
	error.clear();
	if (!canSuspend()) {
		error = "The p-machine cannot be saved in this state";
		return false;
	}
	if (_originalMemory && !validateOriginalState(error))
		return false;
	if (!validateServiceProfile(error))
		return false;
	if (!validateDiskPolicy(error))
		return false;
	if (!_ramCache.validate(_originalMemory ? _nativeRevision : 0, _diskPolicy == kResidentCOM, error))
		return false;
	if (!_graphicsResidue.validate(_originalMemory ? _nativeRevision : 0, error))
		return false;
	if ((!_diskDMABuffer.empty() && (_diskDMABuffer.size() != 512 || _diskPolicy != kResidentCOM)) ||
		(!_originalMemory && _originalWordHighByte != _originalWordHighInitial)) {
		error = "Invalid original interpreter memory state";
		return false;
	}
	if (!_console->validateClassicState(error))
		return false;
	if ((_console->errorPrefix() || _console->textOutput()) && !_originalMemory) {
		error = "Original output state requires original memory";
		return false;
	}
	Common::MemoryWriteStreamDynamic body(DisposeAfterUse::YES);
	body.writeByte(_state);
	body.writeByte(_segment);
	body.writeUint32LE(_ip);
	body.writeUint16LE(_heap);
	body.writeUint16LE(_np);
	body.writeUint16LE(_literalNext);
	body.writeUint16LE(_ioResult);
	body.writeUint16LE(_delayTicks);
	body.writeUint64LE(_instructions);
	body.writeByte(_scriptedInput ? 1 : 0);
	body.writeByte(_idleSnapshot ? 1 : 0);
	body.writeByte(_input.active ? 1 : 0);
	body.writeUint16LE(_input.address);
	body.writeUint16LE(_input.count);
	body.writeUint16LE(_input.position);
	body.write(_memory, sizeof(_memory));
	body.writeUint32LE(_disk.size());
	body.write(_disk.data(), _disk.size());
	body.writeUint32LE(_stack.size());
	for (uint i = 0; i < _stack.size(); ++i)
		body.writeUint16LE(_stack[i]);
	body.writeUint32LE(_frames.size());
	for (uint i = 0; i < _frames.size(); ++i) {
		const Frame &f = _frames[i];
		body.writeByte(f.segment);
		body.writeByte(f.procedure);
		body.writeByte(f.returnSegment);
		body.writeUint16LE(f.base);
		body.writeUint16LE(f.returnIP);
		body.writeUint16LE(f.savedNP);
		body.writeSint32LE(f.staticFrame);
		body.writeUint32LE(f.stackBase);
	}
	body.writeUint32LE(_literals.size());
	for (uint i = 0; i < _literals.size(); ++i) {
		const Literal &lit = _literals[i];
		body.writeByte(lit.segment);
		body.writeUint16LE(lit.offset);
		body.writeUint16LE(lit.length);
		body.writeUint16LE(lit.address);
	}
	body.writeUint32LE(_keys.size());
	body.writeUint32LE(_keyPosition);
	if (!_keys.empty())
		body.write(_keys.data(), _keys.size());
	if (!_console->saveState(body)) {
		error = "Could not serialize console state";
		return false;
	}
	body.writeByte(_preserveLocals ? 1 : 0);
	body.writeByte(_realtime ? 1 : 0);
	body.writeUint32LE(_waitRemainingUs);
	body.writeUint16LE(_soundPattern);
	body.writeByte(_originalMemory ? 1 : 0);
	body.writeByte(_nativeRevision);
	body.writeByte(_services.enabled() ? 1 : 0);
	body.writeUint32LE(_services.position());
	body.writeUint32LE(_services.events().size());
	for (uint i = 0; i < _services.events().size(); ++i) {
		body.writeByte(_services.events()[i].type);
		body.writeUint16LE(_services.events()[i].value);
	}
	body.writeByte(_services.inputMode() ? 1 : 0);
	body.writeUint16LE(_input.mode);
	body.writeByte(_input.kind);
	body.writeUint32BE(kResidueExtensionMagic);
	body.writeByte(_diskPolicy);
	body.writeByte(_diskSelectedSave ? 1 : 0);
	body.writeUint32LE(_sourceSaveDiskCRC);
	body.writeUint32LE(_saveDisk.size());
	if (!_saveDisk.empty())
		body.write(_saveDisk.data(), _saveDisk.size());
	body.writeByte(_loaderKeys.size());
	for (uint i = 0; i < _loaderKeys.size(); ++i)
		body.writeUint16LE(_loaderKeys[i]);
	body.writeUint32LE(_loaderData.size());
	if (!_loaderData.empty())
		body.write(_loaderData.data(), _loaderData.size());
	body.writeUint32LE(_transfer.size());
	if (!_transfer.save(body)) {
		error = "Could not serialize transfer disks";
		return false;
	}
	const uint32 cacheBytes = _ramCache.needsPersistence() ? _ramCache.size() : 0;
	const uint32 graphicsBytes = _graphicsResidue.needsPersistence() ? _graphicsResidue.size() : 0;
	body.writeUint32BE(kResidueStateMagic);
	body.writeByte(_originalWordHighByte);
	body.writeUint16LE(_diskDMABuffer.size());
	if (!_diskDMABuffer.empty())
		body.write(_diskDMABuffer.data(), _diskDMABuffer.size());
	body.writeUint32LE(cacheBytes);
	if (cacheBytes && !_ramCache.save(body)) {
		error = "Could not serialize RAM cache";
		return false;
	}
	body.writeUint32LE(graphicsBytes);
	if (graphicsBytes && !_graphicsResidue.save(body)) {
		error = "Could not serialize graphics residue";
		return false;
	}
	body.writeUint32LE(44 + _saveDisk.size() + 2 * _loaderKeys.size() + _loaderData.size() +
					   _transfer.size() + _diskDMABuffer.size() + cacheBytes + graphicsBytes);
	body.writeUint16LE(_campaignID);
	if (body.err() || body.size() > kMaxStateBytes) {
		error = "Could not serialize the complete p-machine state";
		return false;
	}
	out.writeUint32BE(kStateMagic);
	out.writeUint16LE(kStateVersion);
	out.writeUint16LE(1); // Campaign identity is part of the checked payload.
	out.writeUint32LE(_code.checksum());
	out.writeUint32LE(_sourceDiskCRC);
	out.writeUint32LE(body.size());
	out.writeUint32LE(wizardryCrc32(body.getData(), body.size()));
	out.write(body.getData(), body.size());
	if (out.err()) {
		error = "Could not write the complete p-machine state";
		return false;
	}
	return true;
}

bool PCodeMachine::deserializePayload(Common::SeekableReadStream &in, Common::String &error) {
	const byte state = in.readByte();
	_segment = in.readByte();
	_ip = in.readUint32LE();
	_heap = in.readUint16LE();
	_np = in.readUint16LE();
	_literalNext = in.readUint16LE();
	_ioResult = in.readUint16LE();
	_delayTicks = in.readUint16LE();
	_instructions = in.readUint64LE();
	const byte scripted = in.readByte(), idle = in.readByte(), pending = in.readByte();
	_input.address = in.readUint16LE();
	_input.count = in.readUint16LE();
	_input.position = in.readUint16LE();
	if ((state > kDelayed && state != kPlayingSound) || scripted > 1 || idle > 1 || pending > 1 ||
		_literalNext < 0x0800 || _literalNext > 0x6000 || (_literalNext & 1) ||
		_input.position > _input.count || (pending && (_input.position == _input.count || state != kWaitingForInput))) {
		error = "Invalid machine registers in saved state";
		return false;
	}
	_state = State(state);
	_scriptedInput = scripted != 0;
	_idleSnapshot = idle != 0;
	_input.active = pending != 0;
	if (in.read(_memory, sizeof(_memory)) != sizeof(_memory)) {
		error = "Truncated saved memory";
		return false;
	}
	const uint32 diskSize = in.readUint32LE();
	if (diskSize != _disk.size() || diskSize > uint64(in.size() - in.pos())) {
		error = "Saved disk size does not match this game image";
		return false;
	}
	if (in.read(_disk.data(), diskSize) != diskSize) {
		error = "Truncated saved disk";
		return false;
	}
	const uint32 stackSize = in.readUint32LE();
	if (stackSize > 16384 || uint64(stackSize) * 2 > uint64(in.size() - in.pos())) {
		error = "Invalid saved evaluation stack";
		return false;
	}
	_stack.resize(stackSize);
	for (uint i = 0; i < stackSize; ++i)
		_stack[i] = in.readUint16LE();
	const uint32 frames = in.readUint32LE();
	if (!frames || frames > 512 || uint64(frames) * 17 > uint64(in.size() - in.pos())) {
		error = "Invalid saved activation stack";
		return false;
	}
	_frames.resize(frames);
	for (uint i = 0; i < frames; ++i) {
		Frame &f = _frames[i];
		f.segment = in.readByte();
		f.procedure = in.readByte();
		f.returnSegment = in.readByte();
		f.base = in.readUint16LE();
		f.returnIP = in.readUint16LE();
		f.savedNP = in.readUint16LE();
		f.staticFrame = in.readSint32LE();
		f.stackBase = in.readUint32LE();
		const PCodeProcedure *p = _code.procedure(f.segment, f.procedure);
		const PCodeSegment *ret = _code.segment(f.returnSegment);
		if (!p ||
			f.staticFrame < -1 || f.staticFrame >= int32(i) || f.stackBase > stackSize ||
			!ret || f.returnIP >= ret->code.size() || (i && f.returnSegment != _frames[i - 1].segment)) {
			error = "Invalid saved activation frame or return address";
			return false;
		}
	}
	const PCodeSegment *active = _code.segment(_segment);
	if (_frames[0].segment != 1 || _frames[0].procedure != 1 || _frames[0].staticFrame != -1 ||
		_segment != _frames.back().segment || !active || _ip >= active->code.size()) {
		error = "Invalid saved execution position";
		return false;
	}
	const uint32 literals = in.readUint32LE();
	if (literals > 11264 || uint64(literals) * 7 > uint64(in.size() - in.pos())) {
		error = "Invalid saved literal table";
		return false;
	}
	_literals.resize(literals);
	uint32 nextAddress = 0x0800;
	for (uint i = 0; i < literals; ++i) {
		Literal &lit = _literals[i];
		lit.segment = in.readByte();
		lit.offset = in.readUint16LE();
		lit.length = in.readUint16LE();
		lit.address = in.readUint16LE();
		const PCodeSegment *s = _code.segment(lit.segment);
		if (!s || !lit.length || lit.address != nextAddress ||
			lit.offset > s->code.size() || lit.length > s->code.size() - lit.offset ||
			uint32(lit.address) + lit.length > _literalNext) {
			error = "Invalid saved literal address";
			return false;
		}
		nextAddress += (uint32(lit.length) + 2) & ~1U;
	}
	if (nextAddress != _literalNext) {
		error = "Inconsistent saved literal allocation";
		return false;
	}
	const uint32 keyCount = in.readUint32LE();
	_keyPosition = in.readUint32LE();
	if (keyCount > 4096 || _keyPosition > keyCount || keyCount > uint64(in.size() - in.pos())) {
		error = "Invalid saved keyboard buffer";
		return false;
	}
	_keys.resize(keyCount);
	if (keyCount && in.read(_keys.data(), keyCount) != keyCount) {
		error = "Truncated saved keyboard buffer";
		return false;
	}
	if (!_console->loadState(in, error))
		return false;
	const byte preserve = in.readByte(), realtime = in.readByte();
	_waitRemainingUs = in.readUint32LE();
	_soundPattern = in.readUint16LE();
	const uint32 duration = _state == kDelayed ? NativeTiming::delayMicroseconds(_delayTicks) : (_state == kPlayingSound ? NativeTiming::soundMicroseconds(_soundPattern) : 0);
	if (preserve > 1 || realtime > 1 || _waitRemainingUs > duration ||
		(_state == kPlayingSound ? (!realtime || _soundPattern > 2 || _delayTicks) : _soundPattern != 0xffff)) {
		error = "Invalid saved native timing state";
		return false;
	}
	_realtime = realtime != 0;
	_preserveLocals = preserve != 0;
	const byte original = in.readByte();
	if (original > 1) {
		error = "Invalid saved memory policy";
		return false;
	}
	_originalMemory = original != 0;
	if ((_console->classicPortraitCache() || _console->errorPrefix() || _console->textOutput()) && !_originalMemory) {
		error = "Original console state requires original memory";
		return false;
	}
	if (!_console->validateClassicState(error))
		return false;
	_services.clear();
	const byte revision = in.readByte(), replay = in.readByte();
	const uint32 position = in.readUint32LE(), count = in.readUint32LE();
	if (revision != _nativeRevision || replay > 1 || count > NativeServiceReplay::kMaxEvents ||
		uint64(count) * 3 > uint64(in.size() - in.pos()) || (!replay && (position || count))) {
		error = "Invalid saved native service profile or replay extent";
		return false;
	}
	Common::Array<NativeServiceReplay::Event> events(count);
	for (uint i = 0; i < count; ++i) {
		events[i].type = in.readByte();
		events[i].value = in.readUint16LE();
	}
	const byte inputMode = in.readByte();
	if (inputMode > 1 || (inputMode && !replay)) {
		error = "Invalid saved input replay mode";
		return false;
	}
	if (replay && !_services.configure(events, position, error, inputMode != 0))
		return false;
	_input.mode = in.readUint16LE();
	_input.kind = in.readByte();
	if (_input.kind > 6 || (_input.kind < 2 && _input.mode != 8)) {
		error = "Invalid saved pending input policy";
		return false;
	}
	const uint32 extensionSize = in.size() - in.pos();
	if (extensionSize < 42 || in.readUint32BE() != kResidueExtensionMagic) {
		error = "Missing saved disk transport state";
		return false;
	}
	const byte policy = in.readByte(), selected = in.readByte();
	_sourceSaveDiskCRC = in.readUint32LE();
	const uint32 saveSize = in.readUint32LE();
	if (policy > kResidentCOM || selected > 1 ||
		(policy == kResidentCOM ? saveSize != 1280 * 512 : (saveSize || selected || _sourceSaveDiskCRC)) ||
		uint64(saveSize) + 60 > uint64(in.size() - in.pos())) {
		error = "Invalid saved disk transport extent or flags";
		return false;
	}
	_diskPolicy = DiskPolicy(policy);
	_diskSelectedSave = selected != 0;
	_saveDisk.resize(saveSize);
	if (saveSize && in.read(_saveDisk.data(), saveSize) != saveSize) {
		error = "Truncated secondary disk";
		return false;
	}
	const byte keys = in.readByte();
	if (keys > 5 || uint64(keys) * 2 + 59 > uint64(in.size() - in.pos())) {
		error = "Invalid saved resident loader keyboard extent";
		return false;
	}
	_loaderKeys.clear();
	for (uint i = 0; i < keys; ++i)
		_loaderKeys.push_back(in.readUint16LE());
	const uint32 dataSize = in.readUint32LE();
	if (dataSize > 16998 || uint64(dataSize) + 55 > uint64(in.size() - in.pos())) {
		error = "Invalid saved resident challenge data extent";
		return false;
	}
	_loaderData.resize(dataSize);
	if (dataSize && in.read(_loaderData.data(), dataSize) != dataSize) {
		error = "Truncated challenge data";
		return false;
	}
	const uint32 transferSize = in.readUint32LE();
	if (transferSize < 32 || transferSize > 32 + 3 * 1280 * 512 ||
		uint64(transferSize) + 19 > uint64(in.size() - in.pos())) {
		error = "Invalid saved transfer extent";
		return false;
	}
	Common::Array<byte> transfer(transferSize);
	if (in.read(transfer.data(), transferSize) != transferSize) {
		error = "Truncated transfer state";
		return false;
	}
	Common::MemoryReadStream transferInput(transfer.data(), transfer.size());
	if (!_transfer.load(transferInput, transferProfile(), false, error))
		return false;
	if (in.readUint32BE() != kResidueStateMagic) {
		error = "Missing saved runtime state";
		return false;
	}
	_originalWordHighByte = in.readByte();
	const uint16 dmaSize = in.readUint16LE();
	if ((dmaSize != 0 && dmaSize != 512) || (dmaSize && _diskPolicy != kResidentCOM) ||
		uint64(dmaSize) + 12 > uint64(in.size() - in.pos()) ||
		(!_originalMemory && _originalWordHighByte != _originalWordHighInitial)) {
		error = "Invalid saved interpreter memory extent";
		return false;
	}
	_diskDMABuffer.resize(dmaSize);
	if (dmaSize && in.read(_diskDMABuffer.data(), dmaSize) != dmaSize) {
		error = "Truncated DMA buffer";
		return false;
	}
	_ramCache.initialize(_originalMemory ? _nativeRevision : 0, _diskPolicy == kResidentCOM);
	const uint32 cacheSize = in.readUint32LE();
	if (cacheSize > kMaxCacheStateBytes || (cacheSize && cacheSize < 4014) ||
		uint64(cacheSize) + 8 > uint64(in.size() - in.pos())) {
		error = "Invalid saved RAM-cache extent";
		return false;
	}
	if (cacheSize) {
		Common::Array<byte> cache(cacheSize);
		if (in.read(cache.data(), cacheSize) != cacheSize) {
			error = "Truncated RAM cache";
			return false;
		}
		Common::MemoryReadStream cacheInput(cache.data(), cache.size());
		if (!_ramCache.load(cacheInput, _originalMemory ? _nativeRevision : 0, _diskPolicy == kResidentCOM, error))
			return false;
		if (!_ramCache.needsPersistence()) {
			error = "Noncanonical empty RAM cache";
			return false;
		}
	}
	_graphicsResidue.initialize(_originalMemory ? _nativeRevision : 0);
	const uint32 graphicsSize = in.readUint32LE();
	if (graphicsSize > kMaxGraphicsStateBytes || uint64(graphicsSize) + 6 != uint64(in.size() - in.pos())) {
		error = "Invalid saved graphics residue extent";
		return false;
	}
	if (graphicsSize) {
		Common::Array<byte> graphics(graphicsSize);
		if (in.read(graphics.data(), graphicsSize) != graphicsSize) {
			error = "Truncated graphics residue";
			return false;
		}
		Common::MemoryReadStream graphicsInput(graphics.data(), graphics.size());
		if (!_graphicsResidue.load(graphicsInput, _originalMemory ? _nativeRevision : 0, error))
			return false;
		if (!_graphicsResidue.needsPersistence()) {
			error = "Noncanonical empty graphics residue";
			return false;
		}
	}
	if (in.readUint32LE() != extensionSize) {
		error = "Invalid transport state length";
		return false;
	}
	_campaignID = in.readUint16LE();
	if (!validateDiskPolicy(error))
		return false;
	if (!validateServiceProfile(error))
		return false;
	if (_originalMemory) {
		if (!validateOriginalState(error))
			return false;
	} else {
		if (_heap < 0x6000 || uint32(_heap) + 64 > _np || _np > 0xf000 || _np != _frames.back().base) {
			error = "Invalid saved compatibility arena";
			return false;
		}
		for (uint i = 0; i < _frames.size(); ++i) {
			const Frame &f = _frames[i];
			const PCodeProcedure *p = _code.procedure(f.segment, f.procedure);
			const uint32 top = i ? _frames[i - 1].base : 0xf000;
			const uint32 bytes = uint32(p->localBytes) + p->parameterBytes;
			if (f.savedNP != top || bytes > top || f.base != top - bytes) {
				error = "Invalid saved compatibility activation frame";
				return false;
			}
		}
	}
	if (in.err() || in.eos() || in.pos() != in.size()) {
		error = "Truncated state or unexpected trailing payload";
		return false;
	}
	return true;
}

bool PCodeMachine::loadState(Common::SeekableReadStream &in, Common::String &error,
							 const Common::Array<byte> *expectedSaveDisk) {
	error.clear();
	if (in.size() - in.pos() < 24 || in.readUint32BE() != kStateMagic) {
		error = "Unrecognized Wizardry saved-state header";
		return false;
	}
	const uint16 version = in.readUint16LE();
	const uint16 flags = in.readUint16LE();
	if (version != kStateVersion || flags != 1) {
		error = "Unrecognized Wizardry saved-state header";
		return false;
	}
	if (in.readUint32LE() != _code.checksum() || in.readUint32LE() != _sourceDiskCRC) {
		error = "This saved state belongs to a different game image or program revision";
		return false;
	}
	const uint32 size = in.readUint32LE(), crc = in.readUint32LE();
	if (size < 65536 || size > kMaxStateBytes || size > uint64(in.size() - in.pos())) {
		error = "Invalid saved-state payload size";
		return false;
	}
	Common::Array<byte> payload(size);
	if (in.read(payload.data(), size) != size || wizardryCrc32(payload.data(), size) != crc) {
		error = "Saved-state checksum mismatch or truncated data";
		return false;
	}
	// Validate everything in an independent machine before touching the live
	// session. Its console keeps its own host reference until final commit.
	PCodeMachine candidate;
	candidate._code = _code;
	candidate._disk = _disk;
	candidate._sourceDiskCRC = _sourceDiskCRC;
	candidate._nativeRevision = _nativeRevision;
	candidate._defaultDiskPolicy = _defaultDiskPolicy;
	candidate._originalWordHighInitial = _originalWordHighInitial;
	candidate._originalWordHighByte = _originalWordHighInitial;
	Common::MemoryReadStream body(payload.data(), payload.size());
	if (!candidate.deserializePayload(body, error))
		return false;
	if (expectedSaveDisk && (candidate._diskPolicy != kResidentCOM || expectedSaveDisk->size() != 1280 * 512 ||
							 wizardryCrc32(expectedSaveDisk->data(), expectedSaveDisk->size()) != candidate._sourceSaveDiskCRC)) {
		error = "Supplied SAVE image does not match this snapshot's original secondary image";
		return false;
	}
	Common::MemoryWriteStreamDynamic consoleBytes(DisposeAfterUse::YES);
	if (!candidate._console->saveState(consoleBytes)) {
		error = "Could not stage console state";
		return false;
	}
	PCodeConsole *nextConsole = new PCodeConsole(*this);
	Common::MemoryReadStream consoleInput(consoleBytes.getData(), consoleBytes.size());
	if (!nextConsole->loadState(consoleInput, error)) {
		delete nextConsole;
		return false;
	}
	if (_observer && !_observer->onStateLoad(candidate, error)) {
		delete nextConsole;
		return false;
	}
	_disk = Common::move(candidate._disk);
	_saveDisk = Common::move(candidate._saveDisk);
	_transfer = Common::move(candidate._transfer);
	_transferCancelPending = false;
	_transferSourceRequested = false;
	_ramCache = Common::move(candidate._ramCache);
	_graphicsResidue = Common::move(candidate._graphicsResidue);
	_diskDMABuffer = Common::move(candidate._diskDMABuffer);
	_originalWordHighByte = candidate._originalWordHighByte;
	_originalWordHighInitial = candidate._originalWordHighInitial;
	_sourceSaveDiskCRC = candidate._sourceSaveDiskCRC;
	_campaignID = candidate._campaignID;
	discardScenarioDisk();
	_diskPolicy = candidate._diskPolicy;
	_diskSelectedSave = candidate._diskSelectedSave;
	memcpy(_memory, candidate._memory, sizeof(_memory));
	_stack = Common::move(candidate._stack);
	_frames = Common::move(candidate._frames);
	_literals = Common::move(candidate._literals);
	_keys = Common::move(candidate._keys);
	_loaderKeys = Common::move(candidate._loaderKeys);
	_loaderData = Common::move(candidate._loaderData);
	_keyPosition = candidate._keyPosition;
	_ip = candidate._ip;
	_heap = candidate._heap;
	_np = candidate._np;
	_literalNext = candidate._literalNext;
	_ioResult = candidate._ioResult;
	_delayTicks = candidate._delayTicks;
	_realtime = candidate._realtime;
	_preserveLocals = candidate._preserveLocals;
	_originalMemory = candidate._originalMemory;
	_nativeRevision = candidate._nativeRevision;
	_services = Common::move(candidate._services);
	_waitRemainingUs = candidate._waitRemainingUs;
	_soundPattern = candidate._soundPattern;
	_segment = candidate._segment;
	_instructions = candidate._instructions;
	_state = candidate._state;
	_input = candidate._input;
	_scriptedInput = candidate._scriptedInput;
	_idleSnapshot = candidate._idleSnapshot;
	_error.clear();
	delete _console;
	_console = nextConsole;
	return true;
}

} // namespace Wizardry
