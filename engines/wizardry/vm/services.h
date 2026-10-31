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

#ifndef WIZARDRY_VM_SERVICES_H
#define WIZARDRY_VM_SERVICES_H

#include "common/array.h"
#include "common/str.h"
#include "wizardry/vm/random.h"

namespace Wizardry {

/** Finite BIOS inputs for original keyboard-poll and delay service replay. */
class NativeServiceReplay {
public:
	enum Type { kCall = 1,
				kArgument,
				kTick,
				kKey,
				kModifiers,
				kEnd };
	enum { kMaxEvents = 262144 };
	struct Event {
		byte type;
		uint16 value;
		Event(byte t = 0, uint16 v = 0) : type(t), value(v) {}
	};
	bool configure(const Common::Array<Event> &events, uint32 position, Common::String &error);
	void clear() {
		_enabled = false;
		_events.clear();
		_position = 0;
	}
	bool enabled() const { return _enabled; }
	uint32 position() const { return _position; }
	const Common::Array<Event> &events() const { return _events; }
	bool complete() const { return !_enabled || _position == _events.size(); }
	bool call(byte selector, uint16 request, NativeRandom::Algorithm algorithm,
			  uint16 rng[4], bool &available, Common::String &error);

private:
	bool _enabled = false;
	uint32 _position = 0;
	Common::Array<Event> _events;
	static bool execute(const Common::Array<Event> &events, uint32 &position,
						byte selector, uint16 request, NativeRandom::Algorithm algorithm,
						uint16 rng[4], bool &available, Common::String &error);
};

} // namespace Wizardry
#endif
