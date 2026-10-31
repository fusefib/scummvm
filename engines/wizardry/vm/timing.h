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

#ifndef WIZARDRY_VM_TIMING_H
#define WIZARDRY_VM_TIMING_H

#include "common/array.h"

namespace Wizardry {

/** Timing profile for the I–III native BIOS delay and PC-speaker routines. */
class NativeTiming {
public:
	static const uint32 kPitFrequency = 1193182;
	static const uint32 kSpeakerRate = 44100;
	static const uint16 kSpeakerDivisor = 0x0215;

	static uint32 delayMicroseconds(uint16 request);
	static uint32 soundMicroseconds(uint16 pattern);
	// Signed, little-endian 16-bit mono samples; elapsed time permits resume.
	static void renderSound(uint16 pattern, uint32 elapsedUs, Common::Array<byte> &pcm);

private:
	static uint32 loopMicroseconds(uint32 count);
	static uint phases(uint16 pattern);
	static uint32 phaseMicroseconds(uint16 pattern, uint phase);
};

} // namespace Wizardry
#endif
