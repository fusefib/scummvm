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

#include "audio/chip.h"

#include "common/array.h"
#include "common/mutex.h"
#include "common/scummsys.h"

namespace Audio {

/**
 * Renders the output of an 8254-compatible PIT channel in mode 3.
 *
 * Unlike PCSpeakerStream, this class accepts the raw counter writes and gate
 * control changes made by a DOS sound driver. Counter reloads are deferred
 * until the next half-cycle, matching the behavior needed by programs which
 * repeatedly reprogram channel 2.
 */
class PCSpeakerPITRenderer : public EmulatedChip {
public:
	enum OutputProfile {
		/** DOSBox-compatible impulse output without its optional filters. */
		kUnfiltered,
		/** DOSBox-compatible impulse output with PC speaker filters. */
		kPCSpeakerFiltered,
		/** Lossless reconstruction for analysis; retains a permanent DC rail. */
		kRawReconstruction
	};

	PCSpeakerPITRenderer(uint32 sampleRate, uint32 pitClock = 1193182);
	PCSpeakerPITRenderer(uint32 sampleRate, OutputProfile profile,
			uint32 pitClock = 1193182);
	~PCSpeakerPITRenderer() override;

	void reset();
	void writeMode3Count(uint16 count);
	void setControl(bool timerGate, bool speakerEnabled);
	void setVolume(byte volume);
	bool isStereo() const override { return false; }
	int getRate() const override { return _sampleRate; }

protected:
	void generateSamples(int16 *buffer, int numSamples) override;

private:
	class PCSpeakerOutputStage;

	enum {
		kFractionalPhases = 32,
		kImpulseDurationUs = 3125,
		kSampleFracBits = 24,
		kCoefficientFracBits = 30,
		kReferenceSampleRate = 48000
	};

	void initializeImpulse();
	void addTransition(int level, uint64 elapsedPitClocks = 0);
	void advanceCounter();
	int16 generateSampleUnlocked(byte volume);
	bool isUndersampled(uint16 count) const;
	int outputLevel() const;

	Common::Mutex _mutex;
	uint32 _sampleRate;
	uint32 _pitClock;
	byte _volume;
	uint64 _phase;
	uint64 _sampleCounter;
	uint16 _count;
	uint16 _pendingCount;
	bool _hasPendingCount;
	bool _counterLoaded;
	bool _timerGate;
	bool _speakerEnabled;
	bool _high;
	bool _undersampled;
	bool _hasUndersampledReload;
	uint64 _lastUndersampledReloadSample;

	uint32 _impulseLength;
	uint32 _impulseHead;
	Common::Array<int32> _impulseLut;
	Common::Array<int32> _impulseBuffer;
	int32 _reconstructedLevel;
	int _targetLevel;
	PCSpeakerOutputStage *_outputStage;

	PCSpeakerPITRenderer(const PCSpeakerPITRenderer &);
	PCSpeakerPITRenderer &operator=(const PCSpeakerPITRenderer &);
};

} // End of namespace Audio

#endif // AUDIO_SOFTSYNTH_PCSPK_PIT_H
