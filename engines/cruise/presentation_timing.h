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

#ifndef CRUISE_PRESENTATION_TIMING_H
#define CRUISE_PRESENTATION_TIMING_H

#include "common/scummsys.h"

namespace Cruise {

// A shared presentation policy, retaining the reference tick and phase.
class PresentationClock {
public:
	PresentationClock();
	void reset(uint32 now);
	uint32 delayToBoundary(uint32 now, uint16 ticks);
	void resetCounter(uint32 now);
	uint16 counter(uint32 now);
	uint32 delayToPhase(uint32 now);

private:
	void update(uint32 now);
	uint32 _tickBase, _phaseTime, _phase;
	uint16 _counter;
};

// Approximate 80% presentation costs, in units of 5/32768 ms per pixel.
enum {
	kSpriteWorkUnits = 29
};

class PixelWorkload {
public:
	PixelWorkload() : _pixels(0), _active(false) {}
	void begin(bool enabled);
	void countBitmap(int width, int height);
	uint32 finish(uint32 unitsPerPixel);

private:
	uint32 _pixels;
	bool _active;
};

} // End of namespace Cruise

#endif
