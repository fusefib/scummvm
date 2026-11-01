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

#ifndef WIZARDRY_VM_MACHINE_H
#define WIZARDRY_VM_MACHINE_H

#include "common/stream.h"
#include "wizardry/vm/codefile.h"
#include "wizardry/vm/graphics_residue.h"
#include "wizardry/vm/host.h"
#include "wizardry/vm/ram_cache.h"
#include "wizardry/vm/services.h"
#include "wizardry/vm/transfer.h"

namespace Wizardry {

class PCodeConsole;
class PCodeMachine;

/** Host persistence and optional diagnostics, independent of the frontend. */
class MachineObserver {
public:
	enum TransferSourceResult {
		kTransferUnhandled,
		kTransferSelected,
		kTransferCancelled
	};
	virtual ~MachineObserver() {}
	virtual void beforeInput(byte) {}
	virtual void onDelay(uint16) {}
	virtual bool capturesTeletype() const { return false; }
	virtual void onTeletype(byte) {}
	virtual void onDiskWrite() {}
	// Called before starting a new transfer transaction, never while reopening
	// SOURCE for rollback. Unhandled retains the original disk prompts.
	// An empty image selects an existing private source without replacing it.
	virtual TransferSourceResult onTransferSourceRequest(byte, const TransferDisks &, byte &,
		Common::Array<byte> &) { return kTransferUnhandled; }
	virtual bool onStateLoad(const PCodeMachine &, Common::String &) { return true; }
	virtual bool canCreateScenarioDisk() const { return true; }
	virtual bool onScenarioDiskCreated(const Common::Array<byte> &,
		const Common::Array<byte> &, Common::String &) { return true; }
};

/** Cooperative p-machine for the Wizardry I-V DOS interpreters. */
class PCodeMachine : public MachineHost, private RamDiskSource {
public:
	enum State {
		kRunning,
		kWaitingForInput,
		kDelayed,
		kHalted,
		kFault,
		kPlayingSound
	};
	enum DiskPolicy {
		kLegacy632,
		kRaw0880,
		kResidentCOM
	};
	PCodeMachine();
	~PCodeMachine() override;
	bool boot(const Common::Array<byte> &code, const Common::Array<byte> &disk, Common::String &error);
	bool boot(const Common::Array<byte> &code, const Common::Array<byte> &disk,
		const Common::Array<byte> &saveDisk, Common::String &error);
	bool loadCampaignImages(const Common::Array<byte> &master, const Common::Array<byte> &save,
		Common::String &error);
	bool loadLoaderCom(const Common::Array<byte> &com, Common::String &error);
	bool loadTransferSource(byte source, const Common::Array<byte> &image, Common::String &error);
	bool loadCampaignTransfers(const TransferDisks &transfer, Common::String &error);
	const TransferDisks &transferDisks() const { return _transfer; }
	bool transferInProgress() const { return _transfer.transaction || _transfer.active || _transfer.choosing || _transferCancelPending || transferOwnerActive(); }
	bool discardIdleTransferSources();
	bool cancellingTransfer() const { return _transferCancelPending; }
	bool atScenarioMenu() const;
	bool scenarioInProgress() const { return !_scenarioDisk.empty(); }
	byte transferProfile() const;
	State run(uint32 instructionBudget = 20000);
	void queueKey(byte key);
	void clearKeys();
	void setScriptedInput(bool scripted) { _scriptedInput = scripted; }
	// Memory policy is selected before boot and restored from snapshots.
	void setPreserveLocals(bool preserve) { _preserveLocals = preserve; }
	void setOriginalMemory(bool original) {
		if (_frames.empty())
			_originalMemory = original;
	}
	bool originalMemory() const { return _originalMemory; }
	byte nativeRevision() const override { return _nativeRevision; }
	bool setServiceReplay(const Common::Array<NativeServiceReplay::Event> &events, Common::String &error);
	bool setInputReplay(const Common::Array<NativeServiceReplay::Event> &events, Common::String &error);
	bool serviceReplayEnabled() const { return _services.enabled(); }
	uint32 serviceReplayPosition() const { return _services.position(); }
	uint32 serviceReplaySize() const { return _services.events().size(); }
	bool serviceReplayComplete() const { return _services.complete(); }
	uint32 serviceReplayCalls() const { return _services.calls(); }
	uint32 serviceReplayCallEnd() const { return _services.callEnd(); }
	uint16 serviceReplayCheckpoint() const { return _services.checkpointID(); }
	uint16 serviceReplayNextCall() const { return _services.nextType() == NativeServiceReplay::kCall ? _services.nextValue() : 0; }
	uint16 evaluationStackPointer() const { return _originalMemory ? originalStackPointer() : 0; }
	uint16 currentFrameBase() const { return _frames.empty() ? 0 : _frames.back().base; }
	uint16 arenaTop() const { return _np; }
	uint16 heapTop() const { return _heap; }
	uint16 segmentBase(byte segment) const;
	uint16 segmentReferences(byte segment) const;
	void setRealtime(bool realtime) { _realtime = realtime; }
	void setBiosModifiers(byte flags) { _biosModifiers = flags; }
	void advanceTime(uint32 milliseconds);
	uint32 waitRemainingUs() const { return _waitRemainingUs; }
	uint16 soundPattern() const { return _soundPattern; }
	uint32 soundElapsedUs() const;
	void setObserver(MachineObserver *observer) { _observer = observer; }
	State state() const { return _state; }
	const Common::String &error() const { return _error; }
	uint64 instructions() const { return _instructions; }
	byte currentSegment() const { return _segment; }
	uint32 currentIP() const { return _ip; }
	byte currentProcedure() const;
	PCodeConsole &console() { return *_console; }
	const PCodeConsole &console() const { return *_console; }
	const Common::Array<byte> &disk() const { return _disk; }
	const Common::Array<byte> &saveDisk() const { return _saveDisk; }
	DiskPolicy diskPolicy() const { return _diskPolicy; }
	bool diskSelectedSave() const { return _diskSelectedSave; }
	const Common::Array<byte> &diskDMABuffer() const { return _diskDMABuffer; }
	const RamDiskCache &ramCache() const { return _ramCache; }
	const GraphicsResidue &graphicsResidue() const { return _graphicsResidue; }
	const Common::Array<uint16> &loaderKeys() const { return _loaderKeys; }
	const Common::Array<byte> &loaderData() const { return _loaderData; }
	uint32 sourceDiskCRC() const { return _sourceDiskCRC; }
	uint32 sourceSaveDiskCRC() const { return _sourceSaveDiskCRC; }
	uint16 campaignID() const { return _campaignID; }
	void setCampaignID(uint16 id) { _campaignID = id; }
	const PCodeFile &codeFile() const { return _code; }
	bool saveState(Common::WriteStream &out, Common::String &error) const;
	bool loadState(Common::SeekableReadStream &in, Common::String &error,
		const Common::Array<byte> *expectedSaveDisk = nullptr);
	bool canSuspend() const {
		return !_transferCancelPending && !scenarioInProgress() && _state != kFault && _state != kHalted && !_frames.empty() &&
			(!_input.active || _state == kWaitingForInput);
	}

	byte readByte(uint16 address) const override { return _memory[address]; }
	uint16 readWord(uint16 address) const override;
	void writeByte(uint16 address, byte value) override;
	void writeWord(uint16 address, uint16 value) override;
	bool readDisk(uint32 block, uint32 size, byte *destination) override;
	bool readDiskAt(uint32 block, uint32 size, byte *destination, uint16 physicalOffset, uint16 unit) override;
	bool cacheControl(uint16 address) override;
	uint16 diskCapacity() const override { return scenarioInProgress() || (_diskPolicy == kLegacy632 && !_transfer.active) ? 632 : 1272; }
	uint16 formatScenarioDisk(uint16 unit) override;
	bool keyAvailable() const override { return !_loaderKeys.empty() || _keyPosition < _keys.size(); }
	bool pollKeyboard() override;
	bool executionStopped() const override { return _state == kHalted || _state == kFault; }
	bool keyboardBlocked() const override { return _transfer.choosing || _transferCancelPending; }
	bool tracedInput() const override { return _services.inputMode(); }
	bool graphicsInit(bool first) override;
	void diskWritten() override {
		if (_observer)
			_observer->onDiskWrite();
	}
	void setIoResult(uint16 value) override;
	void yieldForInput() override;
	void delay(uint16 ticks) override;
	void sound(uint16, uint16 pattern, uint16) override;
	void fail(const Common::String &message) override;

private:
	enum StaticMode {
		kLocal,
		kSearch,
		kGlobal,
		kRoot
	};
	struct Frame {
		byte segment = 0;
		byte procedure = 0;
		byte returnSegment = 0;
		uint16 base = 0;
		uint16 returnIP = 0;
		uint16 savedNP = 0;
		int32 staticFrame = -1;
		uint32 stackBase = 0;
	};
	struct Literal {
		byte segment = 0;
		uint16 offset = 0;
		uint16 length = 0;
		uint16 address = 0;
	};
	struct PendingInput {
		bool active = false;
		uint16 address = 0;
		uint16 count = 0;
		uint16 position = 0;
		uint16 mode = 8;
		byte kind = 0; // 0-3: reads; 4: console poll; 5: flush; 6: delay, suspended in III's chooser.
	};
	PCodeFile _code;
	Common::Array<byte> _disk;
	Common::Array<byte> _saveDisk;
	Common::Array<byte> _masterDisk; // Immutable installed data for a new campaign.
	Common::Array<byte> _scenarioDisk;
	byte _scenarioWritten[79];
	uint16 _scenarioWrittenCount = 0;
	uint16 _scenarioUnit = 0;
	Common::Array<byte> _diskDMABuffer;
	TransferDisks _transfer;
	bool _transferCancelPending = false;
	bool _transferSourceRequested = false;
	RamDiskCache _ramCache;
	GraphicsResidue _graphicsResidue;
	uint32 _sourceDiskCRC = 0;
	uint32 _sourceSaveDiskCRC = 0;
	uint16 _campaignID = 0;
	DiskPolicy _diskPolicy = kLegacy632;
	DiskPolicy _defaultDiskPolicy = kLegacy632;
	bool _diskSelectedSave = false;
	byte _memory[65536];
	// DS = CS - 0x1000 in the DOS interpreters. A native word at
	// DS:ffff reaches this physical CS:0000 byte; two byte operations wrap.
	byte _originalWordHighByte = 0;
	byte _originalWordHighInitial = 0;
	Common::Array<uint16> _stack;
	Common::Array<Frame> _frames;
	Common::Array<Literal> _literals;
	Common::Array<byte> _keys;
	Common::Array<uint16> _loaderKeys; // Original COM synthetic BIOS words, before physical input.
	Common::Array<byte> _loaderData;   // Optional authenticated IV/V challenge tables and glyphs.
	uint32 _keyPosition = 0;
	uint32 _ip = 0;
	uint16 _heap = 0x6000;
	uint16 _np = 0xf000;
	uint16 _literalNext = 0x0800;
	uint16 _ioResult = 0;
	uint16 _delayTicks = 0;
	byte _segment = 0;
	uint64 _instructions = 0;
	State _state = kHalted;
	PendingInput _input;
	bool _scriptedInput = false;
	bool _preserveLocals = false;
	bool _originalMemory = false;
	byte _nativeRevision = 0;
	NativeServiceReplay _services;
	bool _realtime = false;
	byte _biosModifiers = 0;
	uint16 _soundPattern = 0xffff;
	uint32 _waitRemainingUs = 0;
	bool _idleSnapshot = false;
	Common::String _error;
	PCodeConsole *_console = nullptr;
	MachineObserver *_observer = nullptr;

	byte fetchByte();
	uint16 fetchWord();
	uint16 big();
	uint16 pop();
	void push(uint16 value);
	bool requireStack(uint32 count);
	const PCodeProcedure *activeProcedure() const;
	void enter(byte segment, byte procedure, StaticMode mode, bool external = false);
	void leave(byte returnWords);
	bool initializeOriginalMemory(const Common::Array<byte> &code, Common::String &error);
	bool loadSegment(byte segment);
	bool releaseSegment(byte segment);
	void enterOriginal(byte segment, byte procedure, StaticMode mode, bool external);
	void leaveOriginal(byte returnWords);
	void syncOriginalRegisters();
	void setArenaTop(uint16 value);
	void setHeapTop(uint16 value);
	bool checkOriginalSpace(uint32 frontier);
	void flushOriginalHeap();
	uint16 originalStackPointer() const;
	uint16 codeWord(byte segment, uint16 offset) const;
	bool originalProcedure(byte segment, byte procedure, PCodeProcedure &result) const;
	bool validateOriginalState(Common::String &error) const;
	bool validateServiceProfile(Common::String &error) const;
	bool originalServiceProfile() const;
	bool loaderKeyboardProfile() const;
	bool keyboardScreenProfile() const;
	byte keyboardMatchProfile() const;
	bool transferOwnerActive() const;
	int transferOwnerFrame() const;
	bool transferRollbackActive() const;
	bool transferRollbackPrompt() const;
	byte transferRollbackSource(Common::String &error) const;
	void selectTransferSource(byte source, bool reopening = false);
	MachineObserver::TransferSourceResult requestTransferSource();
	void cancelTransfer();
	void finishTransferTransaction();
	bool scenarioOwnerActive() const;
	bool scenarioUnitIO(bool write, uint16 unit, uint16 address, uint16 count, uint16 block);
	void discardScenarioDisk();
	void consumeTransferChoice(byte key);
	Common::Array<byte> *activeTransferDisk() { return _transfer.active ? &_transfer.images[_transfer.active - 1] : nullptr; }
	bool prepareLoaderKeys();
	bool replayLoaderEvents(uint32 begin, uint32 end);
	byte inputKey() const;
	void consumeInputKey();
	bool flushLoaderKeys();
	int32 walk(byte levels);
	uint16 localAddress(int32 frame, uint16 word);
	uint16 literal(uint16 offset, uint16 length);
	void execute(byte opcode);
	void jump(bool take);
	void caseJump();
	void compare(byte opcode);
	void standardProcedure(byte number);
	void unitIO(bool write);
	bool writeConsoleOutput(uint16 unit, uint16 address, uint16 count, uint16 mode);
	void completeInput();
	bool consumeConsoleInput(const PendingInput &request, const Common::Array<byte> &keys,
		bool begin, bool requireComplete, uint &consumed);
	bool validateConsoleInput(const PendingInput &request, bool begin,
		const Common::Array<byte> *replayBytes = nullptr,
		const Common::Array<uint16> *biosWords = nullptr,
		const Common::Array<NativeServiceReplay::Event> *keyboardEvents = nullptr);
	void prepareInput(uint16 mode);
	void finishInput();
	void moveBytes(uint16 destination, uint16 source, uint32 size, bool backward = false);
	void moveWords(uint16 destination, uint16 source, uint16 count);
	void fillMemory(uint16 destination, byte value, uint32 size);
	Common::String pascalString(uint16 address) const;
	bool deserializePayload(Common::SeekableReadStream &in, Common::String &error);
	bool bootInternal(const Common::Array<byte> &code, const Common::Array<byte> &disk,
		const Common::Array<byte> *saveDisk, Common::String &error);
	bool validateDiskPair(const Common::Array<byte> &code, const Common::Array<byte> &disk,
		const Common::Array<byte> &saveDisk, byte revision, Common::String &error) const;
	bool validateDiskPolicy(Common::String &error) const;
	bool readUncached(uint16 unit, uint16 block, uint16 count, byte *external,
		uint16 address, uint16 physicalBase, uint16 &result, Common::String &error) override;
	bool readRetained(uint16 unit, uint16 block, uint16 count, byte *external,
		uint16 address, uint16 physicalBase, uint16 knownPrefix, uint16 &result, Common::String &error) override;
	bool readGuestDisk(uint16 unit, uint16 block, uint16 count, uint16 address,
		uint16 &result, Common::String &error);
};

} // namespace Wizardry
#endif
