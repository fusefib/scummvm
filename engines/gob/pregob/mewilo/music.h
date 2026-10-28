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
 */

#ifndef GOB_PREGOB_MEWILO_MUSIC_H
#define GOB_PREGOB_MEWILO_MUSIC_H

#include "common/array.h"
#include "common/scummsys.h"
#include "common/stream.h"

namespace Gob {
namespace Mewilo {

/** One firmware SOUND QUEUE event reconstructed from the native CPC player. */
struct MewiloCpcSoundEvent {
	uint32 startCs;
	uint16 advanceCs;
	byte channelStatus;
	byte amplitudeEnvelope;
	byte toneEnvelope;
	uint16 tonePeriod;
	byte noisePeriod;
	byte initialAmplitudeRaw;
	uint16 durationCs;

	MewiloCpcSoundEvent() :
		startCs(0), advanceCs(0), channelStatus(0), amplitudeEnvelope(0),
		toneEnvelope(0), tonePeriod(0), noisePeriod(0), initialAmplitudeRaw(0), durationCs(0) {}
};

/** Native CPC .MUS channel data.
 *
 * The CPC loader reads exactly three records. Each record contains a one-byte
 * used-entry count, followed by three fixed-capacity arrays: 136 LE16 values,
 * 136 LE16 values, and 136 byte values. The shipped resources are 2048 bytes;
 * the three records occupy 2043 bytes and leave five bytes of file tail.
 *
 * The native overlay establishes the following firmware semantics:
 *
 *  - record 0: first=tone period, second=duration/timeline advance, third=raw start
 *    amplitude byte. A nonzero third value selects amplitude envelope 1; the
 *    CPC firmware sound path uses its low amplitude nibble.
 *  - record 1: first=tone period and second=duration/timeline advance, but the
 *    native routine queues start amplitude zero and never reads this record's
 *    third array. This makes the shipped second voice silent; preserve it.
 *  - record 2: generic noise events (tone 0, noise period 1, duration 3 cs,
 *    initial amplitude from third), with second used only as timeline advance.
 *    Both shipped music files have zero record-2 entries.
 *
 * ENV 1 is set once before playback as a single 15-step software envelope:
 * start from the queued amplitude, subtract one every 9 centiseconds.
 */
class MewiloCpcMusicData {
public:
	static const uint kChannelCount = 3;
	static const uint kEntryCapacity = 136;
	static const uint32 kChannelSize = 1 + kEntryCapacity * 2 + kEntryCapacity * 2 + kEntryCapacity;
	static const uint32 kPayloadSize = kChannelCount * kChannelSize;

	struct Channel {
		byte count;
		uint16 first[kEntryCapacity];
		uint16 second[kEntryCapacity];
		byte third[kEntryCapacity];
	};

	MewiloCpcMusicData();
	explicit MewiloCpcMusicData(Common::SeekableReadStream &stream);

	bool load(Common::SeekableReadStream &stream);
	bool load(const byte *data, uint32 size);
	bool valid() const { return _valid; }
	const Channel &channel(uint index) const { return _channels[index]; }
	uint32 tailSize() const { return _tailSize; }

	/** Translate the serialized records to the exact SOUND QUEUE fields used by
	 * the native overlay. Events are appended in channel order; startCs is the
	 * per-channel timeline position.
	 */
	void buildNativeEvents(Common::Array<MewiloCpcSoundEvent> &events) const;

	/** Duration of one audible channel-A cycle in centiseconds. */
	uint32 channelACycleDurationCs() const;

private:
	Channel _channels[kChannelCount];
	uint32 _tailSize;
	bool _valid;
};

/** Render one bounded 25 kHz signed-8 PCM cycle. Preserves the shipped A-only
 * output; B remains silent as in the native overlay. Record C queues 3 cs
 * noise bursts using the AY 17-bit generator. This is a digital queue/PSG
 * reconstruction, not analogue filtering or cycle-accurate firmware emulation.
 * Output is cleared on failure; caller owns it independently of the MUS object.
 */
bool renderMewiloCpcMusic(const MewiloCpcMusicData &music, Common::Array<byte> &pcm);

/** Convert a CPC AY tone period to the nearest whole frequency in Hz.
 *
 * The CPC clocks its AY-3-8912 at 1 MHz; the tone generator divides by 16.
 */
uint16 mewiloCpcTonePeriodToHz(uint16 period);

} // End of namespace Mewilo
} // End of namespace Gob

#endif // GOB_PREGOB_MEWILO_MUSIC_H
