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

namespace Audio {

PCSpeakerPITRenderer::PCSpeakerPITRenderer(uint32 sampleRate, uint32 pitClock) :
	_sampleRate(sampleRate),
	_pitClock(pitClock) {
	assert(_sampleRate);
	assert(_pitClock);
	reset();
}

void PCSpeakerPITRenderer::reset() {
	_phase = 0;
	_count = 0;
	_pendingCount = 0;
	_hasPendingCount = false;
	_counterLoaded = false;
	_gate = false;
	_high = true;
}

void PCSpeakerPITRenderer::writeMode3Count(uint16 count) {
	// A mode-3 count written while the counter is running is transferred at
	// the next half-cycle rather than restarting the current waveform.
	if (_gate && _counterLoaded) {
		_pendingCount = count;
		_hasPendingCount = true;
	} else {
		_count = count;
		_pendingCount = 0;
		_hasPendingCount = false;
		_counterLoaded = true;
		_phase = 0;
		_high = true;
	}
}

void PCSpeakerPITRenderer::setGate(bool enabled) {
	_gate = enabled;
	if (enabled)
		return;

	// Preserve the behavior of the original ISOUND-local renderer. A later
	// reconstruction stage can model the physical decay without changing the
	// mode-3 counter semantics kept by this class.
	_pendingCount = 0;
	_hasPendingCount = false;
	_phase = 0;
	_high = true;
}

int16 PCSpeakerPITRenderer::generateSample(byte volume) {
	if (!_gate)
		return 0;

	// Integrate the PIT output level over this PCM sample. Phase is expressed
	// in PIT-clock/output-rate products, avoiding drift and floating point.
	uint64 phaseToAdvance = _pitClock;
	int64 signedArea = 0;

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

		signedArea += _high ? (int64)advance : -(int64)advance;
		_phase += advance;
		phaseToAdvance -= advance;

		if (_phase == halfPeriod) {
			_phase = 0;
			_high = !_high;
			if (_hasPendingCount) {
				_count = _pendingCount;
				_pendingCount = 0;
				_hasPendingCount = false;
			}
		}
	}

	const int amplitude = 127 * volume;
	return (int16)((int64)amplitude * signedArea / _pitClock);
}

} // End of namespace Audio
