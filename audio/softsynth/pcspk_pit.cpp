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

PCSpeakerPITRenderer::PCSpeakerPITRenderer(uint32 sampleRate, uint32 pitClock) :
	_sampleRate(sampleRate),
	_pitClock(pitClock) {
	assert(_sampleRate);
	assert(_pitClock);
	initializeImpulse();
	reset();
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
	_gate = false;
	_high = true;
	_undersampled = false;
	_hasUndersampledReload = false;
	_lastUndersampledReloadSample = 0;
	_impulseHead = 0;
	for (uint i = 0; i < _impulseBuffer.size(); ++i)
		_impulseBuffer[i] = 0.0;
	_reconstructedLevel = -1.0;
	_targetLevel = -1;
}

bool PCSpeakerPITRenderer::isUndersampled(uint16 count) const {
	const uint32 minimumCount =
		((uint64)2 * _pitClock + _sampleRate - 1) / _sampleRate;
	// Mode 3 formally requires count >= 2. Treat count 1 as undersampled
	// compatibility input rather than inventing specified 8254 behavior.
	return count && count < minimumCount;
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
		if (_gate && rapidReload) {
			const int level = -_targetLevel;
			_high = level > 0;
			addTransition(level, 0.0);
		}

		_count = count;
		_pendingCount = 0;
		_hasPendingCount = false;
		_counterLoaded = true;
		_phase = 0;
		_high = true;
		_undersampled = true;
		_hasUndersampledReload = true;
		_lastUndersampledReloadSample = _sampleCounter;
		return;
	}

	const bool wasUndersampled = _undersampled;
	_undersampled = false;
	_hasUndersampledReload = false;

	// A mode-3 count written while the counter is running is transferred at
	// the next half-cycle rather than restarting the current waveform.
	if (_gate && _counterLoaded && !wasUndersampled) {
		_pendingCount = count;
		_hasPendingCount = true;
	} else {
		_count = count;
		_pendingCount = 0;
		_hasPendingCount = false;
		_counterLoaded = true;
		_phase = 0;
		_high = true;
		if (_gate)
			addTransition(1, 0.0);
	}
}

void PCSpeakerPITRenderer::setGate(bool enabled) {
	const bool changed = enabled != _gate;
	_gate = enabled;
	if (enabled) {
		if (changed) {
			_phase = 0;
			_high = true;
			if (!_undersampled)
				addTransition(1, 0.0);
		}
		return;
	}

	// With the speaker disabled the physical output is held at its negative
	// rail. Do not clear the impulse tail: it contains the audible edge and
	// must be allowed to settle naturally.
	if (changed)
		addTransition(-1, 0.0);
	_pendingCount = 0;
	_hasPendingCount = false;
	_phase = 0;
	_high = true;
}

void PCSpeakerPITRenderer::advanceCounter() {
	if (!_gate || !_counterLoaded || _undersampled)
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
			addTransition(_high ? 1 : -1,
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
	const double scaled = _reconstructedLevel * 127.0 * volume;
	const int32 rounded = (int32)(scaled < 0.0 ?
		scaled - 0.5 : scaled + 0.5);
	return (int16)CLIP<int32>(rounded, -32768, 32767);
}

} // End of namespace Audio
