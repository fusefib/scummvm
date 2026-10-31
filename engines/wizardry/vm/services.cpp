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

#include "wizardry/vm/services.h"

namespace Wizardry {

static bool readEvent(const Common::Array<NativeServiceReplay::Event> &events,
					  uint32 &position, byte type, uint16 &value, Common::String &error) {
	if (position == events.size() || events[position].type != type) {
		error = Common::String::format("Native service replay expected event %u at index %u", type, position);
		return false;
	}
	value = events[position++].value;
	return true;
}

bool NativeServiceReplay::execute(const Common::Array<Event> &events, uint32 &position,
								  byte selector, uint16 request, NativeRandom::Algorithm algorithm,
								  uint16 rng[4], bool &available, Common::String &error) {
	uint16 value;
	if (!readEvent(events, position, kCall, value, error))
		return false;
	if (value != selector || (selector != 11 && selector != 19)) {
		error = "Native service replay call does not match the requested service";
		return false;
	}
	available = false;
	if (selector == 11) {
		if (!readEvent(events, position, kKey, value, error))
			return false;
		available = value != 0;
		NativeRandom::poll(algorithm, rng, available);
	} else {
		if (!readEvent(events, position, kArgument, value, error))
			return false;
		if (value != request) {
			error = "Native delay replay request does not match the game argument";
			return false;
		}
		uint16 previous;
		if (!readEvent(events, position, kTick, previous, error))
			return false;
		uint16 remaining = uint16(request * 2U);
		for (;;) {
			NativeRandom::stir(algorithm, rng);
			if (!readEvent(events, position, kKey, value, error))
				return false;
			if (value)
				break;
			if (!readEvent(events, position, kModifiers, value, error))
				return false;
			if (value & 0x1f)
				break;
			if (!readEvent(events, position, kTick, value, error))
				return false;
			if (value != previous) {
				previous = value;
				remaining = uint16(remaining - 1);
				if (!remaining)
					break;
			}
		}
	}
	return readEvent(events, position, kEnd, value, error);
}

bool NativeServiceReplay::configure(const Common::Array<Event> &events, uint32 position,
									Common::String &error) {
	error.clear();
	if (events.size() > kMaxEvents || position > events.size()) {
		error = "Native service replay exceeds its bounded event storage";
		return false;
	}
	for (uint i = 0; i < events.size(); ++i) {
		const Event &event = events[i];
		if (event.type < kCall || event.type > kEnd ||
			(event.type == kCall && event.value != 11 && event.value != 19) ||
			(event.type == kKey && event.value > 1) ||
			(event.type == kModifiers && event.value > 255) ||
			(event.type == kEnd && event.value)) {
			error = "Invalid native service replay event";
			return false;
		}
	}
	uint32 cursor = 0;
	bool boundary = position == 0;
	uint16 rng[4] = {};
	bool available;
	while (cursor < events.size()) {
		const byte selector = events[cursor].value;
		const uint16 request = cursor + 1 < events.size() ? events[cursor + 1].value : 0;
		if (!execute(events, cursor, selector, request, NativeRandom::kFourWord, rng, available, error))
			return false;
		if (cursor == position)
			boundary = true;
	}
	if (!boundary) {
		error = "Saved native replay cursor is inside a service call";
		return false;
	}
	_events = events;
	_position = position;
	_enabled = true;
	return true;
}

bool NativeServiceReplay::call(byte selector, uint16 request, NativeRandom::Algorithm algorithm,
							   uint16 rng[4], bool &available, Common::String &error) {
	uint32 position = _position;
	uint16 next[4];
	memcpy(next, rng, sizeof(next));
	bool ready;
	if (!_enabled || !execute(_events, position, selector, request, algorithm, next, ready, error))
		return false;
	_position = position;
	memcpy(rng, next, sizeof(next));
	available = ready;
	return true;
}

} // namespace Wizardry
