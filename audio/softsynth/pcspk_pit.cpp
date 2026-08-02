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

#include "audio/softsynth/pcspk_pit.h"

#include "common/util.h"

#include <math.h>

namespace Audio {

class PCSpeakerPITRenderer::PCSpeakerOutputFilter {
private:
	struct Section {
		double b0;
		double b1;
		double b2;
		double a1;
		double a2;
		double z1;
		double z2;

		Section() :
			b0(0.0), b1(0.0), b2(0.0), a1(0.0), a2(0.0),
			z1(0.0), z2(0.0) {
		}

		double process(double input) {
			const double output = b0 * input + z1;
			z1 = b1 * input - a1 * output + z2;
			z2 = b2 * input - a2 * output;
			return output;
		}

		void clear() {
			z1 = 0.0;
			z2 = 0.0;
		}
	};

	Section _highPassFirst;
	Section _highPassSecond;
	Section _lowPassFirst;
	Section _lowPassSecond;

	static void configureFirstOrder(Section &section, double sampleRate,
			double cutoff, bool highPass) {
		const double k = tan(M_PI * cutoff / sampleRate);
		const double normalization = 1.0 / (1.0 + k);

		section.b0 = (highPass ? 1.0 : k) * normalization;
		section.b1 = (highPass ? -1.0 : k) * normalization;
		section.b2 = 0.0;
		section.a1 = (k - 1.0) * normalization;
		section.a2 = 0.0;
	}

	static void configureSecondOrder(Section &section, double sampleRate,
			double cutoff, bool highPass) {
		// The complex pole pair of a third-order Butterworth filter has Q=1.
		const double k = tan(M_PI * cutoff / sampleRate);
		const double normalization = 1.0 / (1.0 + k + k * k);
		const double b0 = (highPass ? 1.0 : k * k) * normalization;

		section.b0 = b0;
		section.b1 = (highPass ? -2.0 : 2.0) * b0;
		section.b2 = b0;
		section.a1 = 2.0 * (k * k - 1.0) * normalization;
		section.a2 = (1.0 - k + k * k) * normalization;
	}

public:
	PCSpeakerOutputFilter(uint32 sampleRate) {
		const double highPassCutoff = MIN<double>(120.0, sampleRate * 0.1);
		const double lowPassCutoff = MIN<double>(4300.0, sampleRate * 0.45);
		configureFirstOrder(_highPassFirst, sampleRate,
			highPassCutoff, true);
		configureSecondOrder(_highPassSecond, sampleRate,
			highPassCutoff, true);
		configureFirstOrder(_lowPassFirst, sampleRate,
			lowPassCutoff, false);
		configureSecondOrder(_lowPassSecond, sampleRate,
			lowPassCutoff, false);
		reset(-1.0);
	}

	void reset(double inputLevel) {
		_highPassFirst.clear();
		_highPassSecond.clear();
		_lowPassFirst.clear();
		_lowPassSecond.clear();

		// Initialize the first high-pass section at the steady state of the
		// disabled speaker's negative rail. This avoids a synthetic startup
		// edge while still clearing all dynamic filter history.
		_highPassFirst.z1 = -_highPassFirst.b0 * inputLevel;
	}

	double process(double input) {
		double output = _highPassFirst.process(input);
		output = _highPassSecond.process(output);
		output = _lowPassFirst.process(output);
		return _lowPassSecond.process(output);
	}
};

PCSpeakerPITRenderer::PCSpeakerPITRenderer(uint32 sampleRate, uint32 pitClock) :
	PCSpeakerPITRenderer(sampleRate, kUnfiltered, pitClock) {
}

PCSpeakerPITRenderer::PCSpeakerPITRenderer(uint32 sampleRate,
		OutputProfile profile, uint32 pitClock) :
	_sampleRate(sampleRate),
	_pitClock(pitClock),
	_outputFilter(nullptr) {
	assert(_sampleRate);
	assert(_pitClock);
	if (profile == kPCSpeakerFiltered)
		_outputFilter = new PCSpeakerOutputFilter(sampleRate);
	initializeImpulse();
	reset();
}

PCSpeakerPITRenderer::~PCSpeakerPITRenderer() {
	delete _outputFilter;
}

void PCSpeakerPITRenderer::initializeImpulse() {
	_impulseLength = MAX<uint32>(
		2, ((uint64)_sampleRate * kImpulseDurationUs + 999999) / 1000000);
	_impulseLut.resize(_impulseLength * kFractionalPhases, 0.0);
	_impulseBuffer.resize(_impulseLength + 2, 0.0);

	// Keep the reconstruction bandwidth constant at ordinary mixer rates, but
	// stay below Nyquist when a backend requests a lower rate.
	const double cutoff = MIN<double>(14500.0, _sampleRate * 0.45);
	const double center = _impulseLength / 2.0;

	for (uint32 phase = 0; phase < kFractionalPhases; ++phase) {
		const double fraction = (double)phase / kFractionalPhases;
		double sum = 0.0;

		for (uint32 tap = 0; tap < _impulseLength; ++tap) {
			// Samples are emitted at the end of their interval. A transition
			// occurring partway through the current sample is therefore this
			// far in the past when tap zero is emitted.
			const double time = tap + 1.0 - fraction;
			double coefficient = 0.0;
			if (time > 0.0 && time < _impulseLength) {
				const double distance = time - center;
				const double window =
					0.5 * (1.0 + cos(2.0 * M_PI * distance /
						_impulseLength));
				const double argument =
					2.0 * M_PI * cutoff * distance / _sampleRate;
				const double sinc = argument == 0.0 ?
					1.0 : sin(argument) / argument;
				coefficient = window * sinc;
			}

			_impulseLut[phase * _impulseLength + tap] = coefficient;
			sum += coefficient;
		}

		// Normalize every fractional phase independently. The integrated
		// response therefore settles to the requested rail without phase-
		// dependent gain or long-term DC drift.
		if (fabs(sum) < 1e-12) {
			_impulseLut[phase * _impulseLength] = 1.0;
			sum = 1.0;
		}
		for (uint32 tap = 0; tap < _impulseLength; ++tap)
			_impulseLut[phase * _impulseLength + tap] /= sum;
	}
}

void PCSpeakerPITRenderer::reset() {
	_phase = 0;
	_sampleCounter = 0;
	_count = 0;
	_pendingCount = 0;
	_hasPendingCount = false;
	_counterLoaded = false;
	_timerGate = false;
	_speakerEnabled = false;
	_high = true;
	_undersampled = false;
	_hasUndersampledReload = false;
	_lastUndersampledReloadSample = 0;
	_impulseHead = 0;
	for (uint i = 0; i < _impulseBuffer.size(); ++i)
		_impulseBuffer[i] = 0.0;
	_reconstructedLevel = -1.0;
	_targetLevel = -1;
	if (_outputFilter)
		_outputFilter->reset(_reconstructedLevel);
}

bool PCSpeakerPITRenderer::isUndersampled(uint16 count) const {
	const uint32 minimumCount =
		((uint64)2 * _pitClock + _sampleRate - 1) / _sampleRate;
	// Mode 3 formally requires count >= 2. Treat count 1 as undersampled
	// compatibility input rather than inventing specified 8254 behavior.
	return count && count < minimumCount;
}

int PCSpeakerPITRenderer::outputLevel() const {
	if (!_speakerEnabled)
		return -1;

	// Disabling the timer gate forces mode-3 OUT high. Port 0x61 bit 1
	// controls whether that output reaches the physical speaker separately.
	if (!_timerGate)
		return 1;

	return _high ? 1 : -1;
}

void PCSpeakerPITRenderer::addTransition(int level, double sampleFraction) {
	if (level == _targetLevel)
		return;

	const int delta = level - _targetLevel;
	_targetLevel = level;

	sampleFraction = CLIP<double>(sampleFraction, 0.0, 1.0);
	uint32 phase = (uint32)(sampleFraction * kFractionalPhases + 0.5);
	uint32 sampleOffset = 0;
	if (phase == kFractionalPhases) {
		phase = 0;
		sampleOffset = 1;
	}

	for (uint32 tap = 0; tap < _impulseLength; ++tap) {
		const uint32 bufferIndex =
			(_impulseHead + sampleOffset + tap) % _impulseBuffer.size();
		_impulseBuffer[bufferIndex] += delta *
			_impulseLut[phase * _impulseLength + tap];
	}
}

void PCSpeakerPITRenderer::writeMode3Count(uint16 count) {
	if (isUndersampled(count)) {
		// Counts above Nyquist cannot be represented as ordinary oscillation.
		// Rapid reloads are nevertheless used as a noise source, so preserve
		// that compatibility behavior by toggling the last physical level.
		const uint64 sampleGap = _sampleCounter -
			_lastUndersampledReloadSample;
		const bool rapidReload = _hasUndersampledReload &&
			sampleGap * 1000 <= _sampleRate;
		if (!_undersampled)
			_high = false;
		else if (_timerGate && _speakerEnabled && rapidReload)
			_high = !_high;

		_count = count;
		_pendingCount = 0;
		_hasPendingCount = false;
		_counterLoaded = true;
		_phase = 0;
		_undersampled = true;
		_hasUndersampledReload = true;
		_lastUndersampledReloadSample = _sampleCounter;
		addTransition(outputLevel(), 0.0);
		return;
	}

	const bool wasUndersampled = _undersampled;
	_undersampled = false;
	_hasUndersampledReload = false;

	// A mode-3 count written while the counter is running is transferred at
	// the next half-cycle rather than restarting the current waveform.
	if (_timerGate && _counterLoaded && !wasUndersampled) {
		_pendingCount = count;
		_hasPendingCount = true;
	} else {
		_count = count;
		_pendingCount = 0;
		_hasPendingCount = false;
		_counterLoaded = true;
		_phase = 0;
		_high = true;
		addTransition(outputLevel(), 0.0);
	}
}

void PCSpeakerPITRenderer::setGate(bool enabled) {
	setControl(enabled, enabled);
}

void PCSpeakerPITRenderer::setControl(bool timerGate, bool speakerEnabled) {
	const bool timerGateChanged = timerGate != _timerGate;
	_timerGate = timerGate;
	_speakerEnabled = speakerEnabled;

	if (timerGateChanged) {
		if (!timerGate && _hasPendingCount) {
			// A gate stop cancels the current half-cycle. Retain the newest
			// programmed count so the next start does not revert to an older
			// divisor which happened to be active before the stop.
			_count = _pendingCount;
			_pendingCount = 0;
			_hasPendingCount = false;
		}

		_phase = 0;
		// Counts which cannot be represented at the mixer rate remain on the
		// suppressed compatibility level when restarted. Gate-low OUT is
		// nevertheless always high, as represented by outputLevel().
		_high = !timerGate || !_undersampled;
	}

	// Evaluate the two port-0x61 controls together. This prevents a combined
	// update from exposing an intermediate speaker level.
	addTransition(outputLevel(), 0.0);
}

void PCSpeakerPITRenderer::advanceCounter() {
	if (!_timerGate || !_counterLoaded || _undersampled)
		return;

	// Phase is expressed in PIT-clock/output-rate products, avoiding timer
	// drift and preserving each edge's fractional position inside this sample.
	uint64 phaseToAdvance = _pitClock;
	uint64 elapsed = 0;

	while (phaseToAdvance) {
		// A programmed PIT count of zero represents 65536.
		const uint32 effectiveCount = _count ? _count : 0x10000;
		const uint32 halfCount = _high ?
			(effectiveCount + 1) / 2 : effectiveCount / 2;
		// Intel specifies a minimum count of 2 for mode 3. Retain the
		// compatibility behavior for the out-of-range count 1.
		const uint64 halfPeriod =
			(uint64)MAX<uint32>(halfCount, 1) * _sampleRate;
		const uint64 toTransition = halfPeriod - _phase;
		const uint64 advance = MIN<uint64>(phaseToAdvance, toTransition);

		_phase += advance;
		phaseToAdvance -= advance;
		elapsed += advance;

		if (_phase == halfPeriod) {
			_phase = 0;
			_high = !_high;
			if (_hasPendingCount) {
				_count = _pendingCount;
				_pendingCount = 0;
				_hasPendingCount = false;
			}
			addTransition(outputLevel(),
				(double)elapsed / _pitClock);
		}
	}
}

int16 PCSpeakerPITRenderer::generateSample(byte volume) {
	advanceCounter();

	_reconstructedLevel += _impulseBuffer[_impulseHead];
	_impulseBuffer[_impulseHead] = 0.0;
	_impulseHead = (_impulseHead + 1) % _impulseBuffer.size();
	++_sampleCounter;

	// Volume is deliberately applied after reconstruction: a mute or volume
	// change must not alter PIT, impulse-tail, or later filter state.
	const double output = _outputFilter ?
		_outputFilter->process(_reconstructedLevel) : _reconstructedLevel;
	const double scaled = output * 127.0 * volume;
	const int32 rounded = (int32)(scaled < 0.0 ?
		scaled - 0.5 : scaled + 0.5);
	return (int16)CLIP<int32>(rounded, -32768, 32767);
}

} // End of namespace Audio
