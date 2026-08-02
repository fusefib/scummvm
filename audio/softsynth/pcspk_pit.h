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

#ifndef AUDIO_SOFTSYNTH_PCSPK_PIT_H
#define AUDIO_SOFTSYNTH_PCSPK_PIT_H

#include "common/scummsys.h"

namespace Audio {

/**
 * Renders the output of an 8254-compatible PIT channel in mode 3.
 *
 * Unlike PCSpeakerStream, this class accepts the raw counter writes and gate
 * changes made by a DOS sound driver. Counter reloads are deferred until the
 * next half-cycle, matching the behavior needed by programs which repeatedly
 * reprogram channel 2.
 */
class PCSpeakerPITRenderer {
public:
	PCSpeakerPITRenderer(uint32 sampleRate, uint32 pitClock = 1193182);

	void reset();
	void writeMode3Count(uint16 count);
	void setGate(bool enabled);
	int16 generateSample(byte volume);

private:
	uint32 _sampleRate;
	uint32 _pitClock;
	uint64 _phase;
	uint16 _count;
	uint16 _pendingCount;
	bool _hasPendingCount;
	bool _counterLoaded;
	bool _gate;
	bool _high;
};

} // End of namespace Audio

#endif // AUDIO_SOFTSYNTH_PCSPK_PIT_H
