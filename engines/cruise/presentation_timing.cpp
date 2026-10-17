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

#include "cruise/presentation_timing.h"
#include "common/util.h"

namespace Cruise {

PresentationClock::PresentationClock() {
	reset(0);
}

void PresentationClock::reset(uint32 now) {
	_tickBase = _phaseTime = now;
	_phase = 0;
	_counter = 0;
}

void PresentationClock::update(uint32 now) {
	const uint32 ticks = (now - _tickBase) / 10;
	_counter = (uint16)(_counter + ticks);
	_tickBase += ticks * 10;
}

uint32 PresentationClock::delayToBoundary(uint32 now, uint16 ticks) {
	update(now);
	return _counter < ticks ? 10 - (now - _tickBase) : 0;
}

void PresentationClock::resetCounter(uint32 now) {
	update(now);
	_counter = 0;
}

uint16 PresentationClock::counter(uint32 now) {
	update(now);
	return _counter;
}

uint32 PresentationClock::delayToPhase(uint32 now) {
	_phase = (_phase + ((now - _phaseTime) % 1000) * 70) % 1000;
	_phaseTime = now;
	return (1000 - _phase + 69) / 70;
}

void PixelWorkload::begin(bool enabled) {
	_active = enabled;
	_pixels = 0;
}

void PixelWorkload::countBitmap(int width, int height) {
	if (!_active || width <= 0 || height <= 0)
		return;
	// Installed dimensions are words; larger inputs saturate the estimate.
	const uint32 pixels = width > 0xffff || height > 0xffff ? 0xffffffffU : (uint32)width * height;
	_pixels += MIN(pixels, 0xffffffffU - _pixels);
}

uint32 PixelWorkload::finish(uint32 unitsPerPixel) {
	uint32 units = 0;
	if (unitsPerPixel)
		units = MIN(_pixels, 0xffffffffU / unitsPerPixel) * unitsPerPixel;
	begin(false);
	return 5 * ((units >> 15) + ((units & 0x7fff) >= 0x4000));
}

} // End of namespace Cruise
