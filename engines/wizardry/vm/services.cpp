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
	uint16 selector, uint16 request, NativeRandom::Algorithm algorithm,
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

bool NativeServiceReplay::executeInput(const Common::Array<Event> &events, uint32 &position,
	uint16 selector, uint16 unit, uint16 count, Common::Array<byte> &bytes, Common::String &error) {
	uint16 value;
	if (!readEvent(events, position, kCall, value, error))
		return false;
	if (value != selector || (selector != kReadInput && selector != kFlushInput)) {
		error = "Native input replay call does not match the requested operation";
		return false;
	}
	if (!readEvent(events, position, kArgument, value, error))
		return false;
	if (value != unit || (selector == kReadInput ? unit != 2 : unit != 1 && unit != 2)) {
		error = "Native input replay requires the verified keyboard unit";
		return false;
	}
	if (selector == kReadInput) {
		if (!readEvent(events, position, kArgument, value, error))
			return false;
		if (!count || value != count) {
			error = "Native input replay character count differs from UNITREAD";
			return false;
		}
		bytes.resize(count);
		for (uint i = 0; i < count; ++i) {
			if (!readEvent(events, position, kBiosAX, value, error))
				return false;
			const byte character = value & 255;
			if (!character || character > 127) {
				error = "Native input replay supports the verified ASCII read path only";
				return false;
			}
			bytes[i] = character;
		}
	} else {
		for (;;) {
			if (!readEvent(events, position, kKey, value, error))
				return false;
			if (!value)
				break;
			if (!readEvent(events, position, kBiosAX, value, error))
				return false;
		}
	}
	return readEvent(events, position, kEnd, value, error);
}

static bool inputCharacter(uint16 word, byte &character, Common::String &error) {
	character = word & 255;
	if (character)
		return true;
	switch (word >> 8) {
	case 0x03:
		character = 0;
		return true;
	case 0x48:
		character = 11;
		return true;
	case 0x4b:
		character = 9;
		return true;
	case 0x4d:
		character = 12;
		return true;
	case 0x50:
		character = 10;
		return true;
	default:
		error = "Native input replay does not support this BIOS scan-code branch";
		return false;
	}
}

bool NativeServiceReplay::executeModeInput(const Common::Array<Event> &events, uint32 &position,
	uint16 unit, uint16 count, uint16 mode, uint16 address, uint16 terminator,
	Common::Array<byte> &bytes, Common::String &error, bool checkEffects) {
	const uint16 expected[] = {kReadInputMode, unit, count, mode, terminator};
	for (uint i = 0; i < 5; ++i) {
		uint16 value;
		if (!readEvent(events, position, i ? kArgument : kCall, value, error))
			return false;
		if (value != expected[i]) {
			error = "Mode-aware native input replay arguments differ from UNITREAD";
			return false;
		}
	}
	uint16 consumed;
	if (!readEvent(events, position, kArgument, consumed, error))
		return false;
	if ((unit != 1 && unit != 2) || !count || terminator > 255 || !consumed || consumed > count) {
		error = "Invalid mode-aware native input replay extent";
		return false;
	}
	byte flag = mode & 8 ? 0 : 255;
	bool stopped = false;
	bytes.resize(consumed);
	for (uint i = 0; i < consumed; ++i) {
		uint16 word;
		if (!readEvent(events, position, kBiosAX, word, error) || !inputCharacter(word, bytes[i], error))
			return false;
		if (checkEffects) {
			// The original stores AL before rereading the flag and terminator.
			const uint16 destination = uint16(address + i);
			if (destination == 0x0623)
				flag = bytes[i];
			if (destination == 0xe130)
				terminator = bytes[i];
			if (flag && bytes[i] == terminator) {
				if (i + 1 != consumed) {
					error = "Native input replay contains keys after its terminator";
					return false;
				}
				stopped = true;
			}
		}
	}
	if (checkEffects && !stopped && consumed != count) {
		error = "Native input replay ends before its count or terminator";
		return false;
	}
	uint16 value;
	return readEvent(events, position, kEnd, value, error);
}

bool NativeServiceReplay::executeGraphicsInit(const Common::Array<Event> &events, uint32 &position,
	bool keyboard, bool first, Common::String &error) {
	uint16 value;
	if (!readEvent(events, position, kCall, value, error))
		return false;
	if (value != kGraphicsInit) {
		error = "Native input replay expected graphics initialization";
		return false;
	}
	if (first) {
		if (keyboard) {
			if (!readEvent(events, position, kKey, value, error))
				return false;
			if (value) {
				error = "Native graphics replay requires the verified empty keyboard path";
				return false;
			}
		}
		if (!readEvent(events, position, kModifiers, value, error))
			return false;
		if (value != 0x40) {
			error = "Native graphics replay requires the CGA modifier state";
			return false;
		}
	}
	return readEvent(events, position, kEnd, value, error);
}

bool NativeServiceReplay::configure(const Common::Array<Event> &events, uint32 position,
									Common::String &error, bool inputMode) {
	error.clear();
	if (events.size() > kMaxEvents || position > events.size()) {
		error = "Native service replay exceeds its bounded event storage";
		return false;
	}
	for (uint i = 0; i < events.size(); ++i) {
		const Event &event = events[i];
		if (event.type < kCall || event.type > (inputMode ? kCheckpoint : kEnd) ||
			(event.type == kCall && event.value != 11 && event.value != 19 &&
			 (!inputMode || (event.value != kReadInput && event.value != kFlushInput &&
							 (event.value != kGraphicsInit && event.value != kReadInputMode)))) ||
			(event.type == kKey && event.value > 1) ||
			(event.type == kModifiers && event.value > 255) ||
			(event.type == kEnd && event.value) || (event.type == kCheckpoint && !event.value)) {
			error = "Invalid native service replay event";
			return false;
		}
	}
	uint32 cursor = 0;
	uint32 consumedCalls = 0;
	uint16 checkpoint = 0;
	bool extendedInput = false;
	bool unit1Input = false;
	bool boundary = position == 0;
	uint16 rng[4] = {};
	bool available;
	Common::Array<byte> seen(inputMode ? 65536 : 0, 0);
	while (cursor < events.size()) {
		if (events[cursor].type == kCheckpoint) {
			const uint16 id = events[cursor].value;
			if (!inputMode || !cursor || events[cursor - 1].type != kEnd || seen[id]) {
				error = "Native input checkpoint must be unique and follow a complete call";
				return false;
			}
			seen[id] = 1;
			++cursor;
			if (cursor <= position)
				checkpoint = id;
			if (cursor == position)
				boundary = true;
			continue;
		}
		const uint16 selector = events[cursor].value;
		const uint16 request = cursor + 1 < events.size() ? events[cursor + 1].value : 0;
		if (inputMode && selector == kGraphicsInit) {
			const byte next = cursor + 1 < events.size() ? events[cursor + 1].type : 0;
			if (!executeGraphicsInit(events, cursor, next == kKey, next != kEnd, error))
				return false;
			extendedInput = true;
		} else if (inputMode && selector == kReadInputMode) {
			const uint16 count = cursor + 2 < events.size() ? events[cursor + 2].value : 0;
			const uint16 mode = cursor + 3 < events.size() ? events[cursor + 3].value : 0;
			const uint16 terminator = cursor + 4 < events.size() ? events[cursor + 4].value : 0;
			Common::Array<byte> bytes;
			if (!executeModeInput(events, cursor, request, count, mode, 0, terminator, bytes, error, false))
				return false;
			extendedInput = true;
			unit1Input |= request == 1;
		} else if (inputMode && (selector == kReadInput || selector == kFlushInput)) {
			const uint16 count = cursor + 2 < events.size() ? events[cursor + 2].value : 0;
			Common::Array<byte> bytes;
			if (!executeInput(events, cursor, selector, request, count, bytes, error))
				return false;
		} else if (!execute(events, cursor, selector, request, NativeRandom::kFourWord, rng, available, error))
			return false;
		if (cursor <= position)
			++consumedCalls;
		if (cursor == position)
			boundary = true;
	}
	if (inputMode && (events.empty() || events.back().type != kCheckpoint)) {
		error = "Native input replay requires a terminal checkpoint";
		return false;
	}
	if (!boundary) {
		error = "Saved native replay cursor is inside a service call";
		return false;
	}
	_events = events;
	_position = position;
	_calls = consumedCalls;
	_checkpoint = checkpoint;
	_enabled = true;
	_inputMode = inputMode;
	_extendedInput = extendedInput;
	_unit1Input = unit1Input;
	return true;
}

bool NativeServiceReplay::call(uint16 selector, uint16 request, NativeRandom::Algorithm algorithm,
	uint16 rng[4], bool &available, Common::String &error) {
	uint32 position = _position;
	uint16 next[4];
	memcpy(next, rng, sizeof(next));
	bool ready;
	if (!_enabled || !execute(_events, position, selector, request, algorithm, next, ready, error))
		return false;
	_position = position;
	++_calls;
	memcpy(rng, next, sizeof(next));
	available = ready;
	return true;
}

bool NativeServiceReplay::readInput(uint16 unit, uint16 count, Common::Array<byte> &bytes, Common::String &error,
	uint16 mode, uint16 address, byte terminator) {
	uint32 position = _position;
	Common::Array<byte> next;
	if (!_enabled || !_inputMode)
		return false;
	if (modeAwareRead()) {
		if (!executeModeInput(_events, position, unit, count, mode, address, terminator, next, error, true))
			return false;
	} else if (!executeInput(_events, position, kReadInput, unit, count, next, error))
		return false;
	_position = position;
	++_calls;
	bytes = Common::move(next);
	return true;
}

bool NativeServiceReplay::flushInput(uint16 unit, Common::String &error) {
	uint32 position = _position;
	Common::Array<byte> ignored;
	if (!_enabled || !_inputMode || !executeInput(_events, position, kFlushInput, unit, 0, ignored, error))
		return false;
	_position = position;
	++_calls;
	return true;
}

bool NativeServiceReplay::graphicsInit(byte revision, bool first, Common::String &error) {
	if (!_extendedInput)
		return true;
	if (revision != 1 && revision != 4 && revision != 5) {
		error = "Native graphics replay requires a verified interpreter revision";
		return false;
	}
	uint32 position = _position;
	if (!executeGraphicsInit(_events, position, revision >= 4, first, error))
		return false;
	_position = position;
	++_calls;
	return true;
}

bool NativeServiceReplay::checkpoint() {
	if (!_inputMode || nextType() != kCheckpoint)
		return false;
	_checkpoint = _events[_position++].value;
	return true;
}

} // namespace Wizardry
