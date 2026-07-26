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

#ifndef MADS_NEBULAR_RSOUND_H
#define MADS_NEBULAR_RSOUND_H

#include "audio/mididrv.h"
#include "mads/core/sound_manager.h"

namespace MADS {
namespace RexNebular {

/**
 * Shared native implementation of the Rex Nebular Roland sound runtime.
 *
 * The original RSOUND overlays combine 16-bit MPU-401 code, an embedded
 * MT-32 initialization bank and proprietary sequence data. ScummVM replaces
 * the executable code while continuing to consume the original data segment.
 */
class RexRSound : public SoundDriver {
public:
	enum {
		kVoiceCount = 9,
		kOwnedNoteCount = 4,
		kOwnedNoteStorage = 64,
		kSequenceTickUsec = 10000
	};

protected:
	/**
	 * Commands 2-5 use three distinct voice-group layouts in the numbered
	 * overlays. The profile names identify the exact resource family rather
	 * than assigning an unverified music/effect meaning to either group.
	 */
	enum CommonCommandProfile {
		kCommonProfileSection1,
		kCommonProfileSection2,
		kCommonProfileSections3To8
	};

	/**
	 * Native representation of the original 0x22-byte RSOUND voice state.
	 *
	 * The names mirror behavior confirmed in the common overlay interpreter;
	 * the original byte/word offsets are documented beside each field.
	 */
	struct Voice {
		byte delay;                   // +00 event delay / active flag
		int8 pitchStep;               // +01 pitch-bend ramp step
		int8 volumeStep;              // +02 volume ramp step
		int8 panStep;                 // +03 pan ramp step
		byte note;                    // +04 current note
		byte program;                 // +05 current program
		byte velocity;                // +06 note velocity
		byte gateTime;                // +07 gate lead; negative means legato
		byte noteOffDelay;            // +08 note-off countdown
		byte volumeCounter;           // +09 volume-ramp counter
		byte pitchCounter;            // +0A pitch-ramp counter
		byte panCounter;              // +0B pan-ramp counter
		byte volume;                  // +0C controller 7
		byte pitch;                   // +0D pitch-bend MSB, 0x40 centered
		byte pan;                     // +0E controller 10
		byte volumeReload;            // +0F volume-ramp reload
		byte pitchReload;             // +10 pitch-ramp reload
		byte panReload;               // +11 pan-ramp reload
		byte pitchDuration;           // +12 remaining pitch-ramp duration
		byte pendingStop;             // +13 nonzero while fading out
		uint16 sequenceStart;         // +14 restart position
		uint16 position;              // +16 current stream position
		uint16 innerLoopStart;        // +18 FF-loop restart
		uint16 outerLoopStart;        // +1A FE-loop restart
		uint16 innerLoopCount;        // +1C sign-extended FF-loop count
		uint16 outerLoopCount;        // +1E sign-extended FE-loop count
		uint16 identityOffset;        // +20 active-sequence identity

		Voice();
		void initialize(uint16 sequenceOffset);
		bool active() const {
			return delay != 0;
		}
	};

	MidiDriver *_midiDriver;
	Voice _voices[kVoiceCount];

	// Sections 1-2 allocate effects from logical channels 4-8, sections
	// 3-8 use channels 5-8, and the opening overlay uses channels 6-8.
	byte _firstEffectVoice;

	// The original driver uses one flat 64-byte table and indexes it as
	// channel * 4 + note. Most events use four entries, but the opening
	// overlay deliberately contains five-note chord events which spill into
	// the following channel's first slot. Keeping the flat layout preserves
	// that behavior exactly.
	byte _ownedNotes[kOwnedNoteStorage];
	int _commandParam;

	/**
	 * Start a stream on a fixed software voice. Voice indices 0 through 8
	 * correspond to MIDI status-channel nibbles 1 through 9.
	 */
	void startVoice(uint voiceIndex, uint16 sequenceOffset);

	/**
	 * Start on any free melodic voice (indices 0 through 7), reusing a voice
	 * already marked for stopping when necessary.
	 */
	int startAnyVoice(uint16 sequenceOffset);

	/**
	 * Start on the overlay-specific restricted allocator. Sections 1-2 use
	 * indices 3-7, sections 3-8 use indices 4-7, and RSOUND.009 uses 5-7.
	 */
	int startEffectVoice(uint16 sequenceOffset);

	void requestStopVoice(uint voiceIndex);
	void requestStopVoices(const byte *voiceIndices, uint voiceCount);
	void requestStopRange(uint firstVoice, uint voiceCount);
	void stopAndResetRange(uint firstVoice, uint voiceCount);
	void requestStopAll();
	void setUpdateDivider(byte divider);

	void stopVoice(uint voiceIndex);
	void stopVoices(const byte *voiceIndices, uint voiceCount);
	void stopAllVoices();

	void setVoiceVolume(uint voiceIndex, byte volume);

	/**
	 * Send controller 7 without changing the stored Voice::volume value.
	 * RSOUND.003 command 15 deliberately sends one value on channel 2 while
	 * storing it only in channel 1's voice state.
	 */
	void sendUntrackedVoiceVolume(uint voiceIndex, byte volume);

	/** Reset the original 16-bit public tick counter for a valid command. */
	void beginCommand(int param);

	int executeCommonCommand(int commandId,
			CommonCommandProfile profile = kCommonProfileSections3To8);

	/**
	 * Re-send the first embedded DT1 record.
	 *
	 * RSOUND.009 command 2 performs this after resetting the first five
	 * software voices. The record beginning at DS:006F is the first
	 * FF-delimited DT1 payload in the analyzed opening overlay.
	 */
	void sendFirstMt32Record();

	/**
	 * Clear section-owned deferred state during SoundDriver::stop().
	 *
	 * The base implementation is empty. Sections 4, 6 and 9 override this
	 * because their command-0 handlers also clear a callback clock/pointer.
	 */
	virtual void resetSectionState();

	/** Called once per native 100 Hz tick before voice processing. */
	virtual void sectionTimerTick();

	bool isSequenceActive(uint16 sequenceOffset) const;

	Voice &voice(uint voiceIndex);
	const Voice &voice(uint voiceIndex) const;

	byte *sequenceData(uint16 offset, uint32 size = 1);
	const byte *sequenceData(uint16 offset, uint32 size = 1) const;

	uint16 getRandomNumber();

private:
	int _masterVolume;
	bool _bankUploaded;
	bool _muted;
	bool _paused;
	uint16 _randomSeed;
	uint64 _timerAccumulator;
	uint16 _updateCounter;          // CS:0059, reset per valid command
	// RSOUND.009 has no common update-divider gate; its callback clock and all
	// nine voices run on every unpaused 100 Hz tick.
	bool _usesUpdateDivider;
	// The original divider gates the entire voice/pending-stop update,
	// not only the fade loop. A value of zero disables sequence processing.
	byte _updateDivider;
	byte _updateDividerCounter;
	bool _timerInstalled;

	// Logical controller-7 values most recently sent to each MIDI channel.
	// This is deliberately separate from Voice::volume: RSOUND.003 command 15
	// changes channel 2 on the device without changing that voice field, and
	// MIDI reset commands establish controller value 100 on inactive voices.
	byte _channelVolumes[kVoiceCount];

	static int getDataOffset(const Common::Path &filename);
	static int getDataSize(const Common::Path &filename);

	static void timerCallback(void *refCon);
	void onTimer();
	void timerTick();

	void updateVoice(uint voiceIndex);
	void updateRamps(uint voiceIndex);
	void updatePendingStops();

	void sendOwnedNoteOffs(uint voiceIndex);
	void sendNoteOn(uint voiceIndex, byte note, byte velocity);
	void sendNoteOff(uint voiceIndex, byte note);
	void sendProgramChange(uint voiceIndex, byte program);

	/** Update the logical channel-volume shadow and transmit scaled CC7. */
	void sendVolume(uint voiceIndex, byte volume);

	/** Transmit the shadowed logical volume without changing it. */
	void writeVolume(uint voiceIndex, byte volume);

	void sendPitchBend(uint voiceIndex, byte pitch);
	void sendPan(uint voiceIndex, byte pan);

	/**
	 * Ordinary note pairs and F5 chord events use different gate arithmetic in
	 * the DOS sequencer. Keep the paths separate.
	 */
	byte calculateSingleNoteOffDelay(byte duration, byte gateTime) const;
	byte calculateChordNoteOffDelay(byte duration, byte gateTime) const;
	void initializeOwnedNotes();

	uint32 findMt32Bank() const;
	void sendMt32Payload(const byte *payload, uint32 payloadSize);
	void resetMidiRange(uint firstVoice, uint voiceCount);

	void sendMidi(byte status, byte data1 = 0, byte data2 = 0);
	byte scaleVolume(byte volume) const;

public:
	RexRSound(Audio::Mixer *mixer, OPL::OPL *opl, MidiDriver *midiDriver,
			const Common::Path &filename, byte firstEffectVoice = 4,
			bool usesUpdateDivider = true);
	~RexRSound() override;

	int command(int commandId, int param) override;
	int stop() override;
	int poll() override;
	void noise() override;
	void setVolume(int volume) override;

	/**
	 * Upload the FF-delimited MT-32 bank embedded in the loaded RSOUND data
	 * segment. Each record is framed as a Roland DT1 message and receives a
	 * freshly calculated checksum.
	 */
	void uploadMt32Bank();

	bool bankUploaded() const {
		return _bankUploaded;
	}

	uint16 updateCounter() const {
		return _updateCounter;
	}
};

} // namespace RexNebular
} // namespace MADS

#endif
