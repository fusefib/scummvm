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

#include "wizardry/vm/timing.h"
#include "common/endian.h"

namespace Wizardry {

uint32 NativeTiming::delayMicroseconds(uint16 request) {
	// WR19 doubles a word and decrements after each BIOS clock change.
	uint32 ticks = uint16(request << 1);
	if (!ticks)
		ticks = 65536;
	return uint32((uint64(ticks) * 65536 * 1000000 + kPitFrequency - 1) / kPitFrequency);
}

uint32 NativeTiming::loopMicroseconds(uint32 count) {
	// Nominal 4.77 MHz 8088 profile: LOOP takes 17 clocks, the last takes 5.
	const uint32 clocks = (count - 1) * 17 + 5;
	return uint32((uint64(clocks) * 1000000 + 4 * kPitFrequency - 1) / (4 * kPitFrequency));
}

uint NativeTiming::phases(uint16 pattern) {
	return pattern < 2 ? 1 : (pattern == 2 ? 3 : 0);
}

uint32 NativeTiming::phaseMicroseconds(uint16 pattern, uint phase) {
	return loopMicroseconds(pattern == 0 ? 0x2000 : (phase == 1 ? 0x8000 : 0x0400));
}

uint32 NativeTiming::soundMicroseconds(uint16 pattern) {
	uint32 result = 0;
	for (uint i = 0; i < phases(pattern); ++i)
		result += phaseMicroseconds(pattern, i);
	return result;
}

void NativeTiming::renderSound(uint16 pattern, uint32 elapsedUs, Common::Array<byte> &pcm) {
	pcm.clear();
	const uint32 duration = soundMicroseconds(pattern);
	if (elapsedUs >= duration)
		return;
	const uint32 total = uint32((uint64(duration) * kSpeakerRate + 999999) / 1000000);
	const uint32 first = uint32((uint64(elapsedUs) * kSpeakerRate + 999999) / 1000000);
	pcm.resize((total - first) * 2);
	uint32 endUs = 0, beginSample = 0;
	for (uint phase = 0; phase < phases(pattern); ++phase) {
		endUs += phaseMicroseconds(pattern, phase);
		const uint32 endSample = uint32((uint64(endUs) * kSpeakerRate + 999999) / 1000000);
		for (uint32 sample = first > beginSample ? first : beginSample; sample < endSample; ++sample) {
			int16 value = 0;
			if (phase != 1) {
				const uint32 tick = uint32(uint64(sample - beginSample) * kPitFrequency / kSpeakerRate);
				// PIT mode 3 has one extra high clock for an odd divisor.
				value = (tick % kSpeakerDivisor < (kSpeakerDivisor + 1) / 2) ? 6000 : -6000;
			}
			WRITE_LE_UINT16(pcm.data() + 2 * (sample - first), uint16(value));
		}
		beginSample = endSample;
	}
}

} // namespace Wizardry
