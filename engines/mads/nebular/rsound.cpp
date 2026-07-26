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
 *
 */

#include "common/debug.h"
#include "common/file.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "mads/nebular/rsound.h"

namespace MADS {
namespace RexNebular {

namespace {

struct OverlayLayout {
	uint32 dataOffset;
	uint32 dataSize;
};

OverlayLayout readOverlayLayout(const Common::Path &filename) {
	Common::File file;
	if (!file.open(filename))
		error("Could not open file - %s", filename.toString().c_str());

	const int64 fileSize = file.size();
	if (fileSize < 0x2e)
		error("RSOUND overlay is too small - %s", filename.toString().c_str());

	file.seek(0);
	if (file.readUint16LE() != 0x5a4d)
		error("RSOUND overlay is not an MZ executable - %s",
				filename.toString().c_str());

	file.seek(8);
	const uint32 imageOffset = file.readUint16LE() * 16;
	if (imageOffset + 0x2c > (uint64)fileSize)
		error("Invalid RSOUND MZ header - %s", filename.toString().c_str());

	file.seek(imageOffset + 0x2a);
	const uint32 dataSegmentOffset = file.readUint16LE() * 16;
	const uint32 dataOffset = imageOffset + dataSegmentOffset;

	if (dataOffset >= (uint64)fileSize)
		error("Invalid RSOUND data-segment offset - %s",
				filename.toString().c_str());

	OverlayLayout result;
	result.dataOffset = dataOffset;
	result.dataSize = (uint32)(fileSize - dataOffset);
	return result;
}

const byte kMt32BankSignature[] = {
	0xf0, 0x41, 0x10, 0x16, 0x12, 0xff
};

const byte kDt1Header[] = {
	0x41, 0x10, 0x16, 0x12
};

const uint32 kMaxSysExSize = 268;
// Implementation sentinel. The DOS request-stop helper writes an 0xFF
// marker into the low byte of the identity field; no analyzed sequence uses
// either representation as a valid identity.
const uint16 kInvalidSequenceOffset = 0xffff;
const uint32 kMaxOperationsPerTick = 1024;

byte addByteAndSigned(byte value, int8 delta) {
	return (byte)((value + delta) & 0xff);
}

// Logical voice indices correspond to MIDI channels 1-9. The DOS voice
// structures are physically ordered 1,9,2,3,4,5,6,7,8, which is why these
// command groups are not always contiguous in the executable data segment.
const byte kVoices1239[] = { 0, 1, 2, 8 };
const byte kVoices45678[] = { 3, 4, 5, 6, 7 };
const byte kVoices1234[] = { 0, 1, 2, 3 };
const byte kVoices12345[] = { 0, 1, 2, 3, 4 };
const byte kVoices12349[] = { 0, 1, 2, 3, 8 };
const byte kVoices5678[] = { 4, 5, 6, 7 };
const byte kVoices56789[] = { 4, 5, 6, 7, 8 };
const byte kVoices6789[] = { 5, 6, 7, 8 };

void debugMt32TimbrePayload(const byte *payload, uint32 payloadSize) {
	// Roland timbre-memory records begin with address 08 xx xx. The first
	// ten data bytes are the original sound designer's timbre name.
	if (payloadSize < 13 || payload[0] != 0x08)
		return;

	char name[11];
	for (uint index = 0; index < 10; ++index) {
		const byte value = payload[3 + index];
		name[index] = value >= 0x20 && value <= 0x7e ? (char)value : '.';
	}
	name[10] = '\0';

	debug(2, "Rex MT-32 timbre at %02x %02x %02x: \"%s\"",
			payload[0], payload[1], payload[2], name);
}

} // namespace

RexRSound::Voice::Voice() :
		delay(0),
		pitchStep(0),
		volumeStep(0),
		panStep(0),
		note(0),
		program(0),
		velocity(0),
		gateTime(0),
		noteOffDelay(0),
		volumeCounter(0),
		pitchCounter(0),
		panCounter(0),
		volume(0),
		pitch(0x40),
		pan(0x40),
		volumeReload(0xff),
		pitchReload(0),
		panReload(0),
		pitchDuration(0),
		pendingStop(0),
		sequenceStart(0),
		position(0),
		innerLoopStart(0),
		outerLoopStart(0),
		innerLoopCount(0),
		outerLoopCount(0),
		identityOffset(kInvalidSequenceOffset) {
}

void RexRSound::Voice::initialize(uint16 sequenceOffset) {
	pitchStep = 0;
	volumeStep = 0;
	panStep = 0;
	gateTime = 0;
	pendingStop = 0;
	innerLoopCount = 0;
	outerLoopCount = 0;
	volumeReload = 0xff;
	pitch = 0x40;
	pan = 0x40;
	sequenceStart = sequenceOffset;
	position = sequenceOffset;
	innerLoopStart = sequenceOffset;
	outerLoopStart = sequenceOffset;
	identityOffset = sequenceOffset;

	// RSOUND.001 image 0x0769 writes only the fields represented above and
	// intentionally leaves note, program, velocity, volume and live ramp
	// counters intact. Group-stop helpers are more defensive in the native
	// port; see KNOWN_CAVEATS_CHECKLIST.md before changing either behavior.
}

int RexRSound::getDataOffset(const Common::Path &filename) {
	return readOverlayLayout(filename).dataOffset;
}

int RexRSound::getDataSize(const Common::Path &filename) {
	return readOverlayLayout(filename).dataSize;
}

RexRSound::RexRSound(Audio::Mixer *mixer, OPL::OPL *opl,
		MidiDriver *midiDriver, const Common::Path &filename,
		byte firstEffectVoice, bool usesUpdateDivider) :
		SoundDriver(mixer, opl, filename, getDataOffset(filename),
				getDataSize(filename)),
		_midiDriver(midiDriver),
		_firstEffectVoice(firstEffectVoice),
		_commandParam(0),
		_masterVolume(255),
		_bankUploaded(false),
		_muted(false),
		_paused(false),
		_randomSeed(0x005c),
		_timerAccumulator(0),
		_updateCounter(0),
		_usesUpdateDivider(usesUpdateDivider),
		_updateDivider(1),
		_updateDividerCounter(0),
		_timerInstalled(false) {
	if (!_midiDriver || !_midiDriver->isOpen())
		error("Rex MT-32 driver was not opened");

	// MIDI reset establishes controller-7 value 100. Keep a separate device
	// shadow because Voice::volume intentionally does not always track it.
	for (uint voiceIndex = 0; voiceIndex < kVoiceCount; ++voiceIndex)
		_channelVolumes[voiceIndex] = 100;

	initializeOwnedNotes();

	// RSOUND.001 invokes command 0 before walking the full bank, which sends
	// the first DT1 record twice. The native path uploads the bank once pending
	// a DOS MIDI timing/LCD comparison.
	uploadMt32Bank();

	_midiDriver->setTimerCallback(this, timerCallback);
	_timerInstalled = true;
}

RexRSound::~RexRSound() {
	if (_timerInstalled) {
		_midiDriver->setTimerCallback(nullptr, nullptr);
		_timerInstalled = false;
	}

	stop();
}

int RexRSound::command(int commandId, int param) {
	Common::StackLock lock(_driverMutex);

	if (commandId < 0 || commandId > 8) {
		debug(2, "Rex RSOUND command %d, param %d is not implemented yet",
				commandId, param);
		return 0;
	}

	beginCommand(param);
	return executeCommonCommand(commandId);
}

int RexRSound::stop() {
	Common::StackLock lock(_driverMutex);

	// SoundManager::stop() calls this method directly, unlike closeDriver(),
	// which sends command 0 first. Clear any section callback state here so a
	// pending one-shot cannot restart playback after the stop operation.
	resetSectionState();
	stopAllVoices();
	_muted = false;
	_paused = false;
	_timerAccumulator = 0;

	if (!_midiDriver || !_midiDriver->isOpen())
		return 0;

	_midiDriver->stopAllNotes(true);
	resetMidiRange(0, kVoiceCount);
	return 0;
}

int RexRSound::poll() {
	// Playback advances from the MidiDriver timer at exactly 100 Hz.
	return 0;
}

void RexRSound::noise() {
	// The Roland driver does not expose the AdLib noise generator.
}

void RexRSound::setVolume(int volume) {
	Common::StackLock lock(_driverMutex);

	if (volume < 0)
		volume = 0;
	else if (volume > 255)
		volume = 255;

	_masterVolume = volume;

	// Controller state exists independently of whether a software voice is
	// active. Re-scale all nine channels so reset-default volume 100 and the
	// RSOUND.003 untracked channel-2 write follow the MADS master volume too.
	for (uint voiceIndex = 0; voiceIndex < kVoiceCount; ++voiceIndex)
		writeVolume(voiceIndex, _channelVolumes[voiceIndex]);
}


RexRSound::Voice &RexRSound::voice(uint voiceIndex) {
	if (voiceIndex >= kVoiceCount)
		error("Invalid Rex MT-32 voice index %u", voiceIndex);
	return _voices[voiceIndex];
}

const RexRSound::Voice &RexRSound::voice(uint voiceIndex) const {
	if (voiceIndex >= kVoiceCount)
		error("Invalid Rex MT-32 voice index %u", voiceIndex);
	return _voices[voiceIndex];
}

byte *RexRSound::sequenceData(uint16 offset, uint32 size) {
	if ((uint32)offset + size > _soundData.size())
		error("Rex MT-32 sequence access outside the data segment");
	return &_soundData[offset];
}

const byte *RexRSound::sequenceData(uint16 offset, uint32 size) const {
	if ((uint32)offset + size > _soundData.size())
		error("Rex MT-32 sequence access outside the data segment");
	return &_soundData[offset];
}

void RexRSound::initializeOwnedNotes() {
	for (uint noteIndex = 0; noteIndex < kOwnedNoteStorage; ++noteIndex)
		_ownedNotes[noteIndex] = 0xff;
}

void RexRSound::startVoice(uint voiceIndex, uint16 sequenceOffset) {
	Voice &state = voice(voiceIndex);
	sequenceData(sequenceOffset);

	state.initialize(sequenceOffset);
	state.delay = 1;
	sendPitchBend(voiceIndex, 0x40);
}

int RexRSound::startAnyVoice(uint16 sequenceOffset) {
	for (uint voiceIndex = 0; voiceIndex < 8; ++voiceIndex) {
		if (!_voices[voiceIndex].active()) {
			startVoice(voiceIndex, sequenceOffset);
			return (int)voiceIndex;
		}
	}

	for (int voiceIndex = 7; voiceIndex >= 0; --voiceIndex) {
		if (_voices[voiceIndex].pendingStop == 0xff) {
			startVoice((uint)voiceIndex, sequenceOffset);
			return voiceIndex;
		}
	}

	return -1;
}

int RexRSound::startEffectVoice(uint16 sequenceOffset) {
	// RSOUND.001/.002 scan channels 4-8, RSOUND.003-.008 scan 5-8,
	// and the opening overlay's allocator at image 0x0605 scans 6-8.
	for (uint voiceIndex = _firstEffectVoice; voiceIndex < 8; ++voiceIndex) {
		if (!_voices[voiceIndex].active()) {
			startVoice(voiceIndex, sequenceOffset);
			return (int)voiceIndex;
		}
	}

	for (int voiceIndex = 7; voiceIndex >= _firstEffectVoice; --voiceIndex) {
		if (_voices[voiceIndex].pendingStop == 0xff) {
			startVoice((uint)voiceIndex, sequenceOffset);
			return voiceIndex;
		}
	}

	return -1;
}

void RexRSound::requestStopVoice(uint voiceIndex) {
	Voice &state = voice(voiceIndex);
	if (!state.active())
		return;

	state.pendingStop = 0xff;
	state.identityOffset = kInvalidSequenceOffset;
}

void RexRSound::requestStopRange(uint firstVoice, uint voiceCount) {
	if (firstVoice >= kVoiceCount || voiceCount > kVoiceCount - firstVoice)
		error("Invalid Rex RSOUND voice range");

	// Stop requests restore the original update divider to one, matching
	// the shared helper reached by the numbered overlays and opening code.
	setUpdateDivider(1);
	for (uint voiceIndex = firstVoice;
			voiceIndex < firstVoice + voiceCount; ++voiceIndex)
		requestStopVoice(voiceIndex);
}

void RexRSound::stopAndResetRange(uint firstVoice, uint voiceCount) {
	if (firstVoice >= kVoiceCount || voiceCount > kVoiceCount - firstVoice)
		error("Invalid Rex RSOUND voice range");

	// The DOS overlay clears the corresponding 0x22-byte voice structures
	// before sending channel reset controllers. stopVoice() also emits owned
	// note-offs, which is intentionally more defensive but MIDI-equivalent
	// after the following all-notes-off/reset-controller sequence.
	for (uint voiceIndex = firstVoice;
			voiceIndex < firstVoice + voiceCount; ++voiceIndex)
		stopVoice(voiceIndex);
	resetMidiRange(firstVoice, voiceCount);
}

void RexRSound::requestStopVoices(const byte *voiceIndices,
		uint voiceCount) {
	setUpdateDivider(1);
	for (uint index = 0; index < voiceCount; ++index) {
		if (voiceIndices[index] >= kVoiceCount)
			error("Invalid Rex RSOUND voice index");
		requestStopVoice(voiceIndices[index]);
	}
}

void RexRSound::requestStopAll() {
	setUpdateDivider(1);
	for (uint voiceIndex = 0; voiceIndex < kVoiceCount; ++voiceIndex)
		requestStopVoice(voiceIndex);
}

void RexRSound::setUpdateDivider(byte divider) {
	// RSOUND.003 command 9 writes only the reload byte at image 0x0749.
	// The live counter is left untouched and may underflow to 0xFF before
	// the signed `jg` test reloads it.
	_updateDivider = divider;
}

void RexRSound::stopVoice(uint voiceIndex) {
	// DOS group resets generally clear only bytes +00..+03 and manipulate the
	// flat ownership table separately. Full-state replacement is safer against
	// stuck notes but can erase inherited program/controller state; this is a
	// documented compatibility divergence pending command-level MIDI captures.
	sendOwnedNoteOffs(voiceIndex);
	_voices[voiceIndex] = Voice();
}

void RexRSound::stopVoices(const byte *voiceIndices, uint voiceCount) {
	for (uint index = 0; index < voiceCount; ++index) {
		if (voiceIndices[index] >= kVoiceCount)
			error("Invalid Rex RSOUND voice index");
		stopVoice(voiceIndices[index]);
	}
}

void RexRSound::stopAllVoices() {
	for (uint voiceIndex = 0; voiceIndex < kVoiceCount; ++voiceIndex)
		stopVoice(voiceIndex);
}

void RexRSound::setVoiceVolume(uint voiceIndex, byte volume) {
	Voice &state = voice(voiceIndex);
	state.volume = volume;
	sendVolume(voiceIndex, volume);
}

void RexRSound::sendUntrackedVoiceVolume(uint voiceIndex, byte volume) {
	sendVolume(voiceIndex, volume);
}

void RexRSound::beginCommand(int param) {
	_commandParam = param;
	_updateCounter = 0;
}

void RexRSound::resetMidiRange(uint firstVoice, uint voiceCount) {
	if (!_midiDriver || !_midiDriver->isOpen())
		return;

	for (uint voiceIndex = firstVoice;
			voiceIndex < firstVoice + voiceCount; ++voiceIndex) {
		const byte channel = (byte)(voiceIndex + 1);
		sendMidi(MidiDriver::MIDI_COMMAND_CONTROL_CHANGE | channel,
				MidiDriver::MIDI_CONTROLLER_ALL_NOTES_OFF, 0);
		sendMidi(MidiDriver::MIDI_COMMAND_CONTROL_CHANGE | channel,
				MidiDriver::MIDI_CONTROLLER_RESET_ALL_CONTROLLERS, 0);
		// Preserve the original logical reset value while applying the MADS
		// master-volume policy to the value transmitted to the device.
		sendVolume(voiceIndex, 100);
		sendMidi(MidiDriver::MIDI_COMMAND_CONTROL_CHANGE | channel,
				MidiDriver::MIDI_CONTROLLER_PANNING, 0x40);
	}
}

int RexRSound::executeCommonCommand(int commandId,
		CommonCommandProfile profile) {
	switch (commandId) {
	case 0:
		stopAllVoices();
		_muted = false;
		_paused = false;
		setUpdateDivider(0);
		resetMidiRange(0, kVoiceCount);
		sendFirstMt32Record();
		return 0;

	case 1:
		requestStopAll();
		return 0;

	case 2:
		// Every numbered overlay resets MIDI channels 1-4 here, but the
		// software-voice state cleared by the DOS helper varies by section.
		switch (profile) {
		case kCommonProfileSection1:
			stopVoices(kVoices1234, ARRAYSIZE(kVoices1234));
			break;
		case kCommonProfileSection2:
			stopVoices(kVoices12345, ARRAYSIZE(kVoices12345));
			break;
		case kCommonProfileSections3To8:
			stopVoices(kVoices12349, ARRAYSIZE(kVoices12349));
			break;
		}
		resetMidiRange(0, 4);
		return 0;

	case 3:
		// Section 1 fades channels 1-4; section 2 substitutes rhythm
		// channel 9 for channel 4; sections 3-8 use channels 1-4 and 9.
		switch (profile) {
		case kCommonProfileSection1:
			requestStopVoices(kVoices1234, ARRAYSIZE(kVoices1234));
			break;
		case kCommonProfileSection2:
			requestStopVoices(kVoices1239, ARRAYSIZE(kVoices1239));
			break;
		case kCommonProfileSections3To8:
			requestStopVoices(kVoices12349, ARRAYSIZE(kVoices12349));
			break;
		}
		return 0;

	case 4:
		// The companion MIDI reset covers channels 5-9 in every numbered
		// overlay even though the cleared software-voice set differs.
		switch (profile) {
		case kCommonProfileSection1:
			stopVoices(kVoices56789, ARRAYSIZE(kVoices56789));
			break;
		case kCommonProfileSection2:
			stopVoices(kVoices6789, ARRAYSIZE(kVoices6789));
			break;
		case kCommonProfileSections3To8:
			stopVoices(kVoices5678, ARRAYSIZE(kVoices5678));
			break;
		}
		resetMidiRange(4, 5);
		return 0;

	case 5:
		switch (profile) {
		case kCommonProfileSection1:
			requestStopVoices(kVoices56789, ARRAYSIZE(kVoices56789));
			break;
		case kCommonProfileSection2:
			requestStopVoices(kVoices45678, ARRAYSIZE(kVoices45678));
			break;
		case kCommonProfileSections3To8:
			requestStopVoices(kVoices5678, ARRAYSIZE(kVoices5678));
			break;
		}
		return 0;

	case 6:
		_muted = true;
		_paused = true;
		for (uint voiceIndex = 0; voiceIndex < kVoiceCount; ++voiceIndex)
			writeVolume(voiceIndex, _channelVolumes[voiceIndex]);
		return 0;

	case 7:
		_muted = false;
		_paused = false;
		for (uint voiceIndex = 0; voiceIndex < kVoiceCount; ++voiceIndex)
			writeVolume(voiceIndex, _channelVolumes[voiceIndex]);
		return 0;

	case 8:
		for (uint voiceIndex = 0; voiceIndex < kVoiceCount; ++voiceIndex) {
			if (_voices[voiceIndex].active())
				return 1;
		}
		return 0;

	default:
		return 0;
	}
}

void RexRSound::resetSectionState() {
	// Most overlays have no deferred state outside the common voice array.
}

void RexRSound::sectionTimerTick() {
}

bool RexRSound::isSequenceActive(uint16 sequenceOffset) const {
	// The original routine checks the eight melodic voices and excludes the
	// rhythm voice.
	for (uint voiceIndex = 0; voiceIndex < 8; ++voiceIndex) {
		if (_voices[voiceIndex].active() &&
				_voices[voiceIndex].identityOffset == sequenceOffset)
			return true;
	}

	return false;
}

uint16 RexRSound::getRandomNumber() {
	const uint16 value = (uint16)(0x9249 + _randomSeed);
	_randomSeed = (uint16)((value >> 3) | (value << 13));
	return _randomSeed;
}

void RexRSound::timerCallback(void *refCon) {
	RexRSound *driver = static_cast<RexRSound *>(refCon);
	if (driver)
		driver->onTimer();
}

void RexRSound::onTimer() {
	Common::StackLock lock(_driverMutex);

	if (!_midiDriver || !_midiDriver->isOpen())
		return;

	_timerAccumulator += _midiDriver->getBaseTempo();
	while (_timerAccumulator >= kSequenceTickUsec) {
		_timerAccumulator -= kSequenceTickUsec;
		timerTick();
	}
}

void RexRSound::timerTick() {
	// Every analyzed overlay advances its pseudo-random source first, even
	// while command 6 has paused the musical state. The pause word is tested
	// before the public counter, section callback clocks and voice updates.
	getRandomNumber();
	if (_paused)
		return;

	++_updateCounter;
	sectionTimerTick();

	if (_usesUpdateDivider) {
		if (!_updateDivider)
			return;

		// Numbered overlays decrement an eight-bit counter, use a signed
		// `jg`, reload it, and only then process all voices. RSOUND.009
		// has no equivalent gate and falls through on every unpaused tick.
		_updateDividerCounter = (byte)(_updateDividerCounter - 1);
		if ((int8)_updateDividerCounter > 0)
			return;
		_updateDividerCounter = _updateDivider;
	}

	for (uint voiceIndex = 0; voiceIndex < kVoiceCount; ++voiceIndex)
		updateVoice(voiceIndex);

	updatePendingStops();
}

byte RexRSound::calculateSingleNoteOffDelay(byte duration,
		byte gateTime) const {
	// Ordinary-note path: RSOUND.001 image 0x1019 performs raw eight-bit
	// subtraction. The same instruction pattern occurs in all nine overlays.
	return (byte)(duration - gateTime);
}

byte RexRSound::calculateChordNoteOffDelay(byte duration,
		byte gateTime) const {
	// F5 chord path: RSOUND.001 image 0x0F74 has separate boundary rules.
	if ((int8)gateTime < 0)
		return duration + 1;

	if (duration < gateTime)
		return duration;

	return duration - gateTime;
}

void RexRSound::updateVoice(uint voiceIndex) {
	Voice &state = _voices[voiceIndex];

	if (!state.active()) {
		state.pitchStep = 0;
		state.volumeStep = 0;
		state.panStep = 0;
		return;
	}

	if (state.noteOffDelay && --state.noteOffDelay == 0)
		sendOwnedNoteOffs(voiceIndex);

	if (--state.delay != 0) {
		updateRamps(voiceIndex);
		return;
	}

	bool reachedBoundary = false;

	for (uint operation = 0; operation < kMaxOperationsPerTick; ++operation) {
		const byte opcode = *sequenceData(state.position);

		if (opcode < 0x80) {
			const byte *event = sequenceData(state.position, 2);
			const byte note = event[0];
			const byte duration = event[1];

			state.note = note;
			state.delay = duration;
			state.position += 2;

			if (!note || !duration) {
				// Image 0x0CCE tests the note and duration independently.
				// A zero note with a nonzero duration is a timed rest: delay
				// remains nonzero, so the voice resumes at the next pair later.
				// A zero duration leaves delay zero and therefore ends playback.
				sendOwnedNoteOffs(voiceIndex);
				reachedBoundary = true;
				break;
			}

			state.noteOffDelay =
					calculateSingleNoteOffDelay(duration, state.gateTime);

			const uint ownedNoteBase = (voiceIndex + 1) * kOwnedNoteCount;
			const bool heldLegatoNote =
					(int8)state.gateTime < 0 &&
					_ownedNotes[ownedNoteBase] == note;

			if (!heldLegatoNote) {
				sendOwnedNoteOffs(voiceIndex);
				sendNoteOn(voiceIndex, note, state.velocity);
			}

			_ownedNotes[ownedNoteBase] = note;
			reachedBoundary = true;
			break;
		}

		if (opcode < 0xf1) {
			warning("Unknown Rex MT-32 sequence byte 0x%02x at 0x%04x",
					opcode, state.position);
			stopVoice(voiceIndex);
			return;
		}

		switch (opcode) {
		case 0xf1:
			// Present in the dispatch table but intentionally ignored.
			sequenceData(state.position, 2);
			state.position += 2;
			break;

		case 0xf2: {
			const byte count = sequenceData(state.position, 2)[1];
			if (!count)
				error("Invalid zero-length Rex random table");

			const uint16 tableOffset = state.position + 2;
			sequenceData(tableOffset, count + 1);

			const byte randomIndex =
					(byte)((count - 1) & getRandomNumber());
			const byte selected = _soundData[tableOffset + randomIndex];
			const byte destinationIndex = _soundData[tableOffset + count];
			const uint32 destination =
					(uint32)tableOffset + count + destinationIndex + 1;

			if (destination >= _soundData.size())
				error("Rex random-table mutation outside the data segment");

			_soundData[destination] = selected;
			state.position += count + 3;
			break;
		}

		case 0xf3: {
			const byte *args = sequenceData(state.position, 3);
			state.panReload = args[1];
			state.panStep = (int8)args[2];
			state.panCounter = 1;
			state.position += 3;
			break;
		}

		case 0xf4: {
			const byte pan = sequenceData(state.position, 2)[1];
			state.pan = pan;
			sendPan(voiceIndex, pan);
			state.position += 2;
			break;
		}

		case 0xf5: {
			const byte count = sequenceData(state.position, 2)[1];
			const uint ownedNoteBase = (voiceIndex + 1) * kOwnedNoteCount;
			if (ownedNoteBase + count > kOwnedNoteStorage)
				error("Rex chord note table extends outside ownership storage");

			const byte *args =
					sequenceData(state.position, (uint32)count + 3);

			for (uint noteIndex = 0; noteIndex < count; ++noteIndex) {
				const byte note = args[2 + noteIndex];
				if (_ownedNotes[ownedNoteBase + noteIndex] != note) {
					sendNoteOn(voiceIndex, note, state.velocity);
					_ownedNotes[ownedNoteBase + noteIndex] = note;
				}
			}

			for (uint noteIndex = count;
					noteIndex < kOwnedNoteCount; ++noteIndex)
				_ownedNotes[ownedNoteBase + noteIndex] = 0xff;

			const byte duration = args[count + 2];
			state.delay = duration;
			state.noteOffDelay =
					calculateChordNoteOffDelay(duration, state.gateTime);
			state.position += count + 3;
			reachedBoundary = true;
			break;
		}

		case 0xf6: {
			const byte volume = sequenceData(state.position, 2)[1];
			if (!state.pendingStop || state.volume >= volume) {
				state.volume = volume;
				sendVolume(voiceIndex, volume);
			}
			state.position += 2;
			break;
		}

		case 0xf7: {
			const byte pitch = sequenceData(state.position, 2)[1];
			state.pitch = pitch;
			sendPitchBend(voiceIndex, pitch);
			state.position += 2;
			break;
		}

		case 0xf8: {
			const byte *args = sequenceData(state.position, 3);
			if (!state.pendingStop) {
				// Image 0x0E87 stores args[1] into both +0F (reload)
				// and +09 (current counter); all nine overlays match.
				state.volumeReload = args[1];
				state.volumeCounter = args[1];
				state.volumeStep = (int8)args[2];
			}
			state.position += 3;
			break;
		}

		case 0xf9:
			state.velocity = sequenceData(state.position, 2)[1];
			state.position += 2;
			break;

		case 0xfa: {
			const byte *args = sequenceData(state.position, 4);
			state.pitchReload = args[1];
			state.pitchStep = (int8)args[2];
			state.pitchDuration = args[3];
			state.pitchCounter = 1;
			state.position += 4;
			break;
		}

		case 0xfb:
			state.gateTime = sequenceData(state.position, 2)[1];
			state.position += 2;
			break;

		case 0xfc: {
			const byte program = sequenceData(state.position, 2)[1];
			state.program = program;
			sendProgramChange(voiceIndex, program);
			state.position += 2;
			break;
		}

		case 0xfd: {
			const byte pendingStop = state.pendingStop;
			const uint16 sequenceStart = state.sequenceStart;
			state.initialize(sequenceStart);
			state.pendingStop = pendingStop;
			break;
		}

		case 0xfe: {
			if (!state.outerLoopCount) {
				const byte count = sequenceData(state.position, 2)[1];
				if (!count) {
					state.position += 2;
					state.outerLoopStart = state.position;
					state.innerLoopStart = state.position;
					state.innerLoopCount = 0;
					state.outerLoopCount = 0;
				} else {
					// Image 0x0E08 uses CBW before storing the FE count.
					state.outerLoopCount =
							(uint16)(int16)(int8)count;
					state.innerLoopStart = state.outerLoopStart;
					state.position = state.innerLoopStart;
				}
			} else if (--state.outerLoopCount) {
				state.innerLoopStart = state.outerLoopStart;
				state.position = state.innerLoopStart;
			} else {
				state.position += 2;
				state.outerLoopStart = state.position;
				state.innerLoopStart = state.position;
			}
			break;
		}

		case 0xff: {
			if (!state.innerLoopCount) {
				const byte count = sequenceData(state.position, 2)[1];
				if (!count) {
					state.position += 2;
					state.innerLoopStart = state.position;
					state.innerLoopCount = 0;
				} else {
					// Image 0x0DC5 uses CBW before storing the FF count.
					state.innerLoopCount =
							(uint16)(int16)(int8)count;
					state.position = state.innerLoopStart;
				}
			} else if (--state.innerLoopCount) {
				state.position = state.innerLoopStart;
			} else {
				state.position += 2;
				state.innerLoopStart = state.position;
			}
			break;
		}
		}

		if (reachedBoundary)
			break;
	}

	if (!reachedBoundary)
		error("Rex MT-32 sequence exceeded the per-tick opcode limit");

	updateRamps(voiceIndex);
}

void RexRSound::updateRamps(uint voiceIndex) {
	Voice &state = _voices[voiceIndex];

	if (state.volumeStep) {
		if (--state.volumeCounter == 0) {
			state.volumeCounter = state.volumeReload;

			const byte next =
					addByteAndSigned(state.volume, state.volumeStep);
			if ((int8)next < 0) {
				state.volume = 0;
				state.volumeStep = 0;
			} else if (next >= 0x7f) {
				state.volume = 0x7f;
				state.volumeStep = 0;
			} else {
				state.volume = next;
			}

			sendVolume(voiceIndex, state.volume);
		}
	}

	if (state.pitchStep) {
		if (--state.pitchCounter == 0) {
			state.pitchCounter = state.pitchReload;
			state.pitch = addByteAndSigned(state.pitch, state.pitchStep);
			sendPitchBend(voiceIndex, state.pitch);
		}

		if (--state.pitchDuration == 0)
			state.pitchStep = 0;
	}

	if (state.panStep) {
		if (--state.panCounter == 0) {
			state.panCounter = state.panReload;
			byte next = addByteAndSigned(state.pan, state.panStep);

			if (next > 0x7f) {
				next = (next ^ 0x7f) & 0x7f;
				// This is the original driver's behavior: it clears the
				// counter rather than the step.
				state.panCounter = 0;
			}

			state.pan = next;
			sendPan(voiceIndex, state.pan);
		}
	}
}

void RexRSound::updatePendingStops() {
	// Pending-stop attenuation runs on every processed update. The original
	// update divider already gated entry to this routine.
	for (uint voiceIndex = 0; voiceIndex < kVoiceCount; ++voiceIndex) {
		Voice &state = _voices[voiceIndex];

		if (!state.active() || !state.pendingStop)
			continue;

		if (!state.volume) {
			stopVoice(voiceIndex);
		} else {
			--state.volume;
			sendVolume(voiceIndex, state.volume);
		}
	}
}

void RexRSound::sendOwnedNoteOffs(uint voiceIndex) {
	const uint ownedNoteBase = (voiceIndex + 1) * kOwnedNoteCount;

	for (uint noteIndex = 0; noteIndex < kOwnedNoteCount; ++noteIndex) {
		const uint slot = ownedNoteBase + noteIndex;
		const byte note = _ownedNotes[slot];
		if (note == 0xff)
			continue;

		sendNoteOff(voiceIndex, note);
		_ownedNotes[slot] = 0xff;
	}
}

void RexRSound::sendNoteOn(uint voiceIndex, byte note, byte velocity) {
	const byte channel = (byte)(voiceIndex + 1);
	sendMidi(MidiDriver::MIDI_COMMAND_NOTE_ON | channel, note, velocity);
}

void RexRSound::sendNoteOff(uint voiceIndex, byte note) {
	const byte channel = (byte)(voiceIndex + 1);

	// The original driver uses note-on with velocity zero for note release.
	sendMidi(MidiDriver::MIDI_COMMAND_NOTE_ON | channel, note, 0);
}

void RexRSound::sendProgramChange(uint voiceIndex, byte program) {
	const byte channel = (byte)(voiceIndex + 1);
	sendMidi(MidiDriver::MIDI_COMMAND_PROGRAM_CHANGE | channel, program, 0);
}

void RexRSound::sendVolume(uint voiceIndex, byte volume) {
	if (voiceIndex >= kVoiceCount)
		error("Invalid Rex MT-32 volume channel %u", voiceIndex);

	_channelVolumes[voiceIndex] = volume;
	writeVolume(voiceIndex, volume);
}

void RexRSound::writeVolume(uint voiceIndex, byte volume) {
	const byte channel = (byte)(voiceIndex + 1);
	sendMidi(MidiDriver::MIDI_COMMAND_CONTROL_CHANGE | channel,
			MidiDriver::MIDI_CONTROLLER_VOLUME, scaleVolume(volume));
}

void RexRSound::sendPitchBend(uint voiceIndex, byte pitch) {
	const byte channel = (byte)(voiceIndex + 1);
	sendMidi(MidiDriver::MIDI_COMMAND_PITCH_BEND | channel, 0, pitch);
}

void RexRSound::sendPan(uint voiceIndex, byte pan) {
	const byte channel = (byte)(voiceIndex + 1);
	sendMidi(MidiDriver::MIDI_COMMAND_CONTROL_CHANGE | channel,
			MidiDriver::MIDI_CONTROLLER_PANNING, pan);
}

byte RexRSound::scaleVolume(byte volume) const {
	if (_muted)
		return 0;
	return (byte)((volume * _masterVolume + 127) / 255);
}

void RexRSound::sendMidi(byte status, byte data1, byte data2) {
	_midiDriver->send(status, data1, data2);
}

uint32 RexRSound::findMt32Bank() const {
	if (_soundData.size() < sizeof(kMt32BankSignature))
		error("RSOUND data segment is too small");

	const uint32 lastOffset =
			_soundData.size() - sizeof(kMt32BankSignature);

	for (uint32 offset = 0; offset <= lastOffset; ++offset) {
		bool match = true;
		for (uint32 index = 0; index < sizeof(kMt32BankSignature); ++index) {
			if (_soundData[offset + index] != kMt32BankSignature[index]) {
				match = false;
				break;
			}
		}

		if (match)
			return offset;
	}

	error("Could not locate the Rex MT-32 initialization bank");
}

void RexRSound::sendMt32Payload(const byte *payload, uint32 payloadSize) {
	if (payloadSize < 3)
		error("Invalid Rex MT-32 DT1 payload");

	debugMt32TimbrePayload(payload, payloadSize);

	const uint32 messageSize =
			sizeof(kDt1Header) + payloadSize + 1;
	if (messageSize > kMaxSysExSize)
		error("Rex MT-32 SysEx record is too large");

	Common::Array<byte> message;
	message.resize(messageSize);

	uint32 outputOffset = 0;
	for (uint32 index = 0; index < sizeof(kDt1Header); ++index)
		message[outputOffset++] = kDt1Header[index];

	byte checksum = 0;
	for (uint32 index = 0; index < payloadSize; ++index) {
		const byte value = payload[index];
		if (value & 0x80)
			error("Invalid eight-bit value in Rex MT-32 DT1 payload");

		message[outputOffset++] = value;
		checksum = (checksum + value) & 0x7f;
	}

	message[outputOffset] = (-checksum) & 0x7f;

	const uint16 delay =
			_midiDriver->sysExNoDelay(&message[0], (uint16)message.size());
	if (delay)
		g_system->delayMillis(delay);
}

void RexRSound::sendFirstMt32Record() {
	uint32 cursor = findMt32Bank() + sizeof(kMt32BankSignature);
	uint32 end = cursor;
	while (end < _soundData.size() && _soundData[end] != 0xff)
		++end;

	if (end == _soundData.size())
		error("Unterminated Rex MT-32 display record");

	sendMt32Payload(&_soundData[cursor], end - cursor);
}

void RexRSound::uploadMt32Bank() {
	Common::StackLock lock(_driverMutex);

	if (_bankUploaded)
		return;

	uint32 cursor = findMt32Bank() + sizeof(kMt32BankSignature);
	uint32 messageCount = 0;

	while (cursor < _soundData.size()) {
		if (_soundData[cursor] == 0xff)
			break;

		uint32 end = cursor;
		while (end < _soundData.size() && _soundData[end] != 0xff)
			++end;

		if (end == _soundData.size())
			error("Unterminated Rex MT-32 DT1 payload");

		sendMt32Payload(&_soundData[cursor], end - cursor);
		++messageCount;
		cursor = end + 1;
	}

	if (!messageCount)
		error("Rex MT-32 initialization bank is empty");

	_bankUploaded = true;
	debug(1, "Uploaded %u Rex MT-32 DT1 records", messageCount);
}

} // namespace RexNebular
} // namespace MADS
