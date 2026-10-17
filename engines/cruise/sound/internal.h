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

#ifndef CRUISE_SOUND_INTERNAL_H
#define CRUISE_SOUND_INTERNAL_H

#include "common/scummsys.h"

namespace Cruise {
namespace SoundInternal {

enum {
	kMusicTimerHz = 100,
	kH32TimbrePayloadSize = 246,
	kAdLibInstrumentSize = 58,
	kH32PresetInstrumentSize = 23,
	kH32CustomInstrumentSize = 279,
	kCorpNumOrdersOffset = 470,
	kCorpTempoOffset = 471,
	kCorpOrderTableOffset = 472,
	kCorpPatternDataOffset = 2400,
	kCorpPatternSize = 1024
};

inline bool isCorpModuleValid(const byte *data, uint32 size) {
	if (!data || size < kCorpPatternDataOffset + kCorpPatternSize)
		return false;

	const uint32 numOrders = data[kCorpNumOrdersOffset];
	if (numOrders == 0 || kCorpOrderTableOffset + numOrders > kCorpPatternDataOffset)
		return false;

	for (uint32 i = 0; i < numOrders; ++i) {
		const uint32 patternEnd = kCorpPatternDataOffset +
				((uint32)data[kCorpOrderTableOffset + i] + 1) * kCorpPatternSize;
		if (patternEnd > size)
			return false;
	}
	return true;
}

inline bool isAdLibInstrumentValid(const byte *data, uint32 size) {
	return data && size >= kAdLibInstrumentSize &&
			(data[0] == 0 || (data[1] >= 6 && data[1] <= 10));
}

inline bool isH32InstrumentValid(const byte *data, uint32 size) {
	if (!data || size < kH32PresetInstrumentSize)
		return false;
	return data[22] < 0x80 || size >= kH32CustomInstrumentSize;
}

inline bool isHPEffectValid(const byte *data, uint32 size) {
	if (!data || size < 6)
		return false;
	const uint16 count = data[0] | ((uint16)data[1] << 8);
	return count != 0 && size >= 6 + (uint32)count * 4;
}

inline int prepareAdLibMusicVolume(int volume, int fadeOut) {
	if (volume != 0 && volume < 0x50)
		volume = 0x50;

	volume -= fadeOut;
	if (volume < 0)
		volume = 0;

	volume += volume / 4;
	return volume < 0x7f ? volume : 0x7f;
}

inline byte calculateH32Checksum(byte address0, byte address1, byte address2,
		const byte *data, uint16 length) {
	uint32 checksum = address0 + address1 + address2;
	for (uint16 i = 0; i < length; ++i)
		checksum += data[i];
	return (byte)(0x80 - (checksum & 0x7f));
}

inline uint16 calculateSpeakerMode0Divisor(int period) {
	if (period == 0)
		period = 0x78;
	if (period < 0)
		return 1;

	// The first quotient is narrowed to a signed word before the second division.
	const int16 intermediateHz = (int16)(249920 / period);
	if (intermediateHz == 0)
		return 1;
	return (uint16)(1193180 / intermediateHz);
}

inline uint16 calculateSpeakerMode1Divisor(int note) {
	uint32 intermediateHz = (note + 0x10) * 2;
	if (intermediateHz == 0)
		intermediateHz = 0x7d0;
	const uint32 divisor = 1193180U / intermediateHz;
	return (uint16)(divisor < 0xffff ? divisor : 0xffff);
}

} // End of namespace SoundInternal
} // End of namespace Cruise

#endif
