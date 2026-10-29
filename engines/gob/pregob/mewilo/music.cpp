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

#include "common/endian.h"
#include "common/util.h"

#include "gob/pregob/mewilo/music.h"

namespace Gob {
namespace Mewilo {

MewiloCpcMusicData::MewiloCpcMusicData() : _tailSize(0), _valid(false) {
	memset(_channels, 0, sizeof(_channels));
}

MewiloCpcMusicData::MewiloCpcMusicData(Common::SeekableReadStream &stream) : _tailSize(0), _valid(false) {
	memset(_channels, 0, sizeof(_channels));
	load(stream);
}

bool MewiloCpcMusicData::load(Common::SeekableReadStream &stream) {
	// Match the byte-buffer overload even on invalid size or a short read.
	memset(_channels, 0, sizeof(_channels));
	_tailSize = 0;
	_valid = false;
	const int64 size = stream.size();
	if (size < (int64)kPayloadSize || size > 1024 * 1024)
		return false;

	byte *data = new byte[(uint32)size];
	stream.seek(0);
	if (stream.read(data, (uint32)size) != (uint32)size) {
		delete[] data;
		return false;
	}
	const bool ok = load(data, (uint32)size);
	delete[] data;
	return ok;
}

bool MewiloCpcMusicData::load(const byte *data, uint32 size) {
	memset(_channels, 0, sizeof(_channels));
	_tailSize = 0;
	_valid = false;
	if (!data || size < kPayloadSize)
		return false;

	uint32 p = 0;
	for (uint channelIndex = 0; channelIndex < kChannelCount; ++channelIndex) {
		Channel &channel = _channels[channelIndex];
		channel.count = data[p++];
		if (channel.count > kEntryCapacity)
			return false;

		for (uint i = 0; i < kEntryCapacity; ++i, p += 2)
			channel.first[i] = READ_LE_UINT16(&data[p]);
		for (uint i = 0; i < kEntryCapacity; ++i, p += 2)
			channel.second[i] = READ_LE_UINT16(&data[p]);
		for (uint i = 0; i < kEntryCapacity; ++i)
			channel.third[i] = data[p++];
	}

	_tailSize = size - kPayloadSize;
	_valid = true;
	return true;
}

bool renderMewiloCpcMusic(const MewiloCpcMusicData &music, Common::Array<byte> &pcm) {
	pcm.clear();
	if (!music.valid())
		return false;
	const uint32 samplesPerCs = 250, sampleRate = 25000;
	uint32 cycleCs = 0;
	for (uint channel = 0; channel < 3; ++channel) {
		uint32 time = 0;
		const MewiloCpcMusicData::Channel &c = music.channel(channel);
		for (uint i = 0; i < c.count; ++i)
			time += c.second[i];
		cycleCs = MAX(cycleCs, time);
	}
	// Firmware SOUND queues serialize events on the same voice. A new noise
	// request cannot truncate the previous 3 cs burst even with advance < 3.
	const MewiloCpcMusicData::Channel &noise = music.channel(2);
	uint32 noiseStarts[MewiloCpcMusicData::kEntryCapacity];
	uint32 requested = 0, noiseEnd = 0;
	for (uint i = 0; i < noise.count; ++i) {
		noiseStarts[i] = MAX(requested, noiseEnd);
		noiseEnd = noiseStarts[i] + 3;
		requested += noise.second[i];
	}
	cycleCs = MAX(cycleCs, noiseEnd);
	// Both shipped cycles are under 15 seconds. Bound even adversarial data.
	if (!cycleCs || cycleCs > 60000)
		return false;
	pcm.resize(cycleCs * samplesPerCs);
	memset(pcm.data(), 0, pcm.size());
	static const uint16 kAyVolume[16] = {
		0, 513, 828, 1239, 1923, 3238, 4926, 9110,
		10344, 17876, 24682, 30442, 38844, 47270, 56402, 65535};
	const MewiloCpcMusicData::Channel &tone = music.channel(0);
	uint32 dst = 0, phase = 0;
	for (uint i = 0; i < tone.count; ++i) {
		const uint16 period = tone.first[i];
		const uint32 count = uint32(tone.second[i]) * samplesPerCs;
		const byte raw = tone.third[i];
		const uint32 step = period ? uint32((uint64(62500) << 32) / (uint64(period) * sampleRate)) : 0;
		for (uint32 j = 0; j < count; ++j) {
			int value = 0;
			if (period && raw) {
				const uint32 steps = MIN<uint32>(15, (j / samplesPerCs) / 9);
				const byte volume = ((raw & 15) + 16 - steps) & 15;
				const int amplitude = (kAyVolume[volume] * 120U + 32767U) / 65535U;
				value = (phase & 0x80000000U) ? amplitude : -amplitude;
			}
			if (period)
				phase += step;
			pcm[dst++] = byte(int8(value));
		}
	}
	// Record 1's amplitude is deliberately zero, regardless of its third array.
	// AY noise: master 1 MHz / 16 / period(1), bit0 output, bit0 XOR bit3
	// feedback into bit16. Keep the free-running generator alive across gaps.
	uint32 rng = 1, noiseClock = 0;
	uint event = 0;
	for (uint32 i = 0; noise.count && i < pcm.size(); ++i) {
		while (event < noise.count && i >= (noiseStarts[event] + 3) * samplesPerCs)
			++event;
		if (event < noise.count && i >= noiseStarts[event] * samplesPerCs) {
			const int amplitude = (kAyVolume[noise.third[event] & 15] * 120U + 32767U) / 65535U;
			const int value = int(int8(pcm[i])) + ((rng & 1) ? amplitude : -amplitude);
			pcm[i] = byte(int8(CLIP<int>(value, -128, 127)));
		}
		noiseClock += 62500;
		while (noiseClock >= sampleRate) {
			noiseClock -= sampleRate;
			rng = (rng >> 1) | (((rng ^ (rng >> 3)) & 1) << 16);
		}
	}
	return true;
}

uint16 mewiloCpcTonePeriodToHz(uint16 period) {
	if (!period)
		return 0;
	return (uint16)((62500U + period / 2) / period);
}

} // End of namespace Mewilo
} // End of namespace Gob
