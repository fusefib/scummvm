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

#include "common/array.h"
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
	enum OutputProfile {
		kUnfiltered,
		kPCSpeakerFiltered
	};

	PCSpeakerPITRenderer(uint32 sampleRate, uint32 pitClock = 1193182);
	PCSpeakerPITRenderer(uint32 sampleRate, OutputProfile profile,
			uint32 pitClock = 1193182);
	~PCSpeakerPITRenderer();

	void reset();
	void writeMode3Count(uint16 count);
	void setGate(bool enabled);
	int16 generateSample(byte volume);

private:
	class PCSpeakerOutputFilter;

	enum {
		kFractionalPhases = 32,
		kImpulseDurationUs = 3125
	};

	void initializeImpulse();
	void addTransition(int level, double sampleFraction);
	void advanceCounter();
	bool isUndersampled(uint16 count) const;

	uint32 _sampleRate;
	uint32 _pitClock;
	uint64 _phase;
	uint64 _sampleCounter;
	uint16 _count;
	uint16 _pendingCount;
	bool _hasPendingCount;
	bool _counterLoaded;
	bool _gate;
	bool _high;
	bool _undersampled;
	bool _hasUndersampledReload;
	uint64 _lastUndersampledReloadSample;

	uint32 _impulseLength;
	uint32 _impulseHead;
	Common::Array<double> _impulseLut;
	Common::Array<double> _impulseBuffer;
	double _reconstructedLevel;
	int _targetLevel;
	PCSpeakerOutputFilter *_outputFilter;

	PCSpeakerPITRenderer(const PCSpeakerPITRenderer &);
	PCSpeakerPITRenderer &operator=(const PCSpeakerPITRenderer &);
};

} // End of namespace Audio

#endif // AUDIO_SOFTSYNTH_PCSPK_PIT_H
