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
				kEnd,
				kBiosAX,
				kCheckpoint };
	enum { kGraphicsInit = 1,
		   kReadInput = 256,
		   kFlushInput = 257,
		   kReadInputMode = 258 };
	enum { kMaxEvents = 262144 };
	struct Event {
		byte type;
		uint16 value;
		Event(byte t = 0, uint16 v = 0) : type(t), value(v) {}
	};
	bool configure(const Common::Array<Event> &events, uint32 position, Common::String &error,
		bool inputMode = false);
	void clear() {
		_enabled = _inputMode = _extendedInput = false;
		_events.clear();
		_position = _calls = 0;
		_checkpoint = 0;
	}
	bool enabled() const { return _enabled; }
	bool inputMode() const { return _inputMode; }
	bool extendedInput() const { return _extendedInput; }
	bool modeAwareRead() const { return _inputMode && nextType() == kCall && nextValue() == kReadInputMode; }
	uint32 position() const { return _position; }
	uint32 calls() const { return _calls; }
	uint16 checkpointID() const { return _checkpoint; }
	byte nextType() const { return _position < _events.size() ? _events[_position].type : 0; }
	uint16 nextValue() const { return _position < _events.size() ? _events[_position].value : 0; }
	uint32 callEnd() const { return _position && _events[_position - 1].type == kCheckpoint ? _position - 1 : _position; }
	const Common::Array<Event> &events() const { return _events; }
	bool complete() const { return !_enabled || _position == _events.size(); }
	bool call(uint16 selector, uint16 request, NativeRandom::Algorithm algorithm,
		uint16 rng[4], bool &available, Common::String &error);
	bool readInput(uint16 unit, uint16 count, Common::Array<byte> &bytes, Common::String &error,
		uint16 mode = 8, uint16 address = 0, byte terminator = 0);
	bool flushInput(uint16 unit, Common::String &error);
	bool graphicsInit(byte revision, bool first, Common::String &error);
	bool checkpoint();

private:
	bool _enabled = false;
	bool _inputMode = false;
	bool _extendedInput = false;
	uint32 _position = 0;
	uint32 _calls = 0;
	uint16 _checkpoint = 0;
	Common::Array<Event> _events;
	static bool execute(const Common::Array<Event> &events, uint32 &position,
		uint16 selector, uint16 request, NativeRandom::Algorithm algorithm,
		uint16 rng[4], bool &available, Common::String &error);
	static bool executeInput(const Common::Array<Event> &events, uint32 &position,
		uint16 selector, uint16 unit, uint16 count, Common::Array<byte> &bytes, Common::String &error);
	static bool executeModeInput(const Common::Array<Event> &events, uint32 &position,
		uint16 unit, uint16 count, uint16 mode, uint16 address, uint16 terminator,
		Common::Array<byte> &bytes, Common::String &error, bool checkEffects);
	static bool executeGraphicsInit(const Common::Array<Event> &events, uint32 &position,
		bool keyboard, bool first, Common::String &error);
};

} // namespace Wizardry
#endif
