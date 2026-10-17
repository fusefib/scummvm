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

// Presentation estimates, not emulated CPU time. All versions use these units.
static const uint kWorkloadPercent = 80;
static const uint kSpritePrepareNsPerPixel = 3450;
static const uint kSpriteBlitNsPerPixel = 2050;
static const uint kTextNsPerPixel = 2650;
static const uint kCopyNsPerByte = 168;

void WorkBudget::add(uint32 count, uint32 nanosecondsPerUnit) {
	const uint64 limit = (uint64)0x7fffffffU * 1000000;
	const uint64 cost = (uint64)count * nanosecondsPerUnit;
	_nanoseconds += MIN(cost, limit - _nanoseconds);
}

void WorkBudget::addPixels(int width, int height, uint32 nanosecondsPerPixel) {
	if (width > 0 && height > 0)
		add((uint32)MIN<uint64>((uint64)width * height, 0xffffffffU), nanosecondsPerPixel);
}

uint32 WorkBudget::millis() const {
	return (uint32)MIN<uint64>((_nanoseconds * kWorkloadPercent + 50000000) / 100000000, 0x7fffffffU);
}

static int clippedLength(int start, int length, int limit) {
	if (length <= 0 || start >= limit)
		return 0;
	if (start < 0)
		return MAX(0, MIN(start + length, limit));
	return MIN(length, limit - start);
}

DrawingWorkload::DrawingWorkload() {
	cancel();
}

void DrawingWorkload::begin(bool enabled, bool captionVisible) {
	_phase = enabled && kWorkloadPercent ? kDrawing : kIdle;
	_captionVisible = captionVisible;
	_work.reset();
}

void DrawingWorkload::cancel() {
	_phase = kIdle;
	_captionVisible = false;
	_work.reset();
}

void DrawingWorkload::countSprite(int width, int height, int x, int y) {
	if (_phase != kDrawing || !_captionVisible)
		return;
	_work.addPixels(width, height, kSpritePrepareNsPerPixel);
	_work.addPixels(clippedLength(x, width, 320), clippedLength(y, height, 200), kSpriteBlitNsPerPixel);
}

void DrawingWorkload::countText(int width, int height) {
	if (_phase == kDrawing)
		_work.addPixels(width, height, kTextNsPerPixel);
}

void DrawingWorkload::countBackgroundCopy(uint32 bytes) {
	if (_phase == kDrawing)
		_work.add(bytes, kCopyNsPerByte);
}

uint32 DrawingWorkload::finishDrawing() {
	if (_phase != kDrawing)
		return 0;
	const uint32 millis = _work.millis();
	_work.reset();
	_phase = kCopy;
	return millis;
}

uint32 DrawingWorkload::finishCopy(uint32 bytes) {
	if (_phase != kCopy)
		return 0;
	cancel();
	WorkBudget work;
	work.add(bytes, kCopyNsPerByte);
	return work.millis();
}

} // End of namespace Cruise
