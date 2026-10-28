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
 */

#include "gob/pregob/mewilo/thomson_music.h"

#include "common/endian.h"
#include "common/util.h"

namespace Gob {
namespace Mewilo {

static const uint32 kMaxNoteCycles = 10000000;
static const uint32 kMaxMusicCycles = 120000000;

static bool loadNativeMemory(Common::SeekableReadStream &stream,
		Common::Array<byte> &memory, Common::Array<byte> &present) {
	const int64 fileSize = stream.size();
	if (fileSize <= 0 || fileSize > 1024 * 1024 || !stream.seek(0))
		return false;

	Common::Array<byte> bytes;
	bytes.resize(uint32(fileSize));
	if (stream.read(bytes.data(), bytes.size()) != bytes.size())
		return false;
	memory.resize(65536);
	present.resize(65536);
	memset(memory.data(), 0, memory.size());
	memset(present.data(), 0, present.size());

	uint records = 0;
	for (uint32 p = 0; p + 5 <= bytes.size();) {
		const byte type = bytes[p];
		const uint16 size = READ_BE_UINT16(bytes.data() + p + 1);
		const uint16 address = READ_BE_UINT16(bytes.data() + p + 3);
		p += 5;
		if (type == 0xff)
			return size == 0 && p == bytes.size() && records != 0;
		if (type != 0 || !size || address < 0x6000 ||
			uint32(address) + size > 65536 || p + size > bytes.size())
			return false;
		memcpy(memory.data() + address, bytes.data() + p, size);
		memset(present.data() + address, 1, size);
		p += size;
		++records;
	}
	return false;
}

bool readMewiloThomsonMusic(Common::SeekableReadStream &stream, byte command,
		Common::Array<MewiloThomsonNote> &notes) {
	notes.clear();
	const uint16 address = command == 8 ? 0x8931 : (command == 6 ? 0x897f : 0);
	if (!address)
		return false;
	Common::Array<byte> memory, present;
	if (!loadNativeMemory(stream, memory, present) || !present[address])
		return false;
	const uint count = memory[address];
	if (!count || uint32(address) + count + 1 > memory.size())
		return false;

	// BASIC initialization supplies T5, L24, A0, O4 (divisor 2). Both Mewilo
	// streams set their tempo/duration explicitly, but begin before setting O.
	MewiloThomsonNote state = {0, 5, 24, 0, 2};
	Common::Array<MewiloThomsonNote> result;
	for (uint i = 0; i < count; ++i) {
		if (!present[address + i + 1])
			return false;
		const byte token = memory[address + i + 1];
		const byte low = token & 15;
		switch (token >> 4) {
		case 0:
			if (low < 1 || low > 5)
				return false;
			state.octaveDivisor = 1 << (5 - low);
			break;
		case 1:
			if (!low)
				return false; // A zero native counter would wrap to 65536.
			state.tempo = low;
			break;
		case 2:
			state.timbre = low;
			break;
		case 3:
			if (low > 12)
				return false;
			state.key = low;
			result.push_back(state);
			break;
		default:
			if (token == 0x40)
				return false;
			state.duration = token - 0x40;
			break;
		}
	}
	if (result.empty())
		return false;
	notes = result;
	return true;
}

// Mathematical translation of the TO8/TO8D monitor NOTE body ($EDF9), whose
// counted loop is $EE9C. The 13 scalar period counts are the native tuning
// parameters, not an embedded firmware image. A0 holds the duty cycle; A1..15
// progressively move ticks from the low half to the high half on counter carry.
// The test-only firmware oracle checks every DAC write and the return cycle.
class NoteClock {
public:
	NoteClock(const MewiloThomsonNote &note, byte phase) :
		_note(note), _tempo(note.tempo), _duration(note.duration),
		_phase(phase), _carries(0), _cycles(133 + (note.key == 0 ? 2 : 0)) {}

	bool run(MewiloThomsonNoteTrace &out) {
		static const byte kPeriodCounts[13] = {
			0, 119, 112, 105, 98, 92, 86, 80, 75, 70, 65, 60, 56
		};
		byte low = kPeriodCounts[_note.key];
		byte high = low ? low : 80;
		bool done = false;
		while (!done) {
			if (_cycles > kMaxNoteCycles)
				return false;
			_cycles += 8; // Compare low count with timbre.
			if (low > _note.timbre) {
				_cycles += 10;
				if (_carries) {
					high += _note.timbre;
					low -= _note.timbre;
					_carries = 0;
					_cycles += 17;
				}
				_cycles += 6;
				for (uint u = 0; u < _note.octaveDivisor; ++u) {
					_cycles += 17;
					out.writes.push_back(MewiloThomsonDacWrite(_cycles, 0));
					done = countHalf(low);
					_cycles += 10;
					if (done)
						break;
					_cycles += 13;
				}
			}
			if (done)
				break;
			_cycles += 6;
			for (uint u = 0; u < _note.octaveDivisor; ++u) {
				_cycles += 22;
				out.writes.push_back(MewiloThomsonDacWrite(_cycles, 63));
				done = countHalf(high);
				_cycles += 15;
				if (done)
					break;
				_cycles += 13;
			}
			if (!done)
				_cycles += 3;
		}
		out.cycles = _cycles + 69; // Restore PIA controls and saved registers.
		out.phase = _phase;
		return out.cycles <= kMaxNoteCycles;
	}

private:
	bool countHalf(byte step) {
		// Collapse INCA/DECB/BNE (seven cycles per iteration), preserving the
		// scratch byte's carry. Duration is decremented only on that carry;
		// tempo counts duration reloads. No floating-point pitch/time guesses.
		_cycles += 38 + 7 * (step ? uint(step) : 256);
		const byte next = _phase + step;
		if (next < _phase) {
			++_carries;
			_cycles += 15;
			if (!--_duration) {
				_duration = _note.duration;
				_cycles += 15;
				if (!--_tempo) {
					_cycles -= 8; // Native terminal branch skips scratch write.
					return true;
				}
			}
		}
		_phase = next;
		return false;
	}

	const MewiloThomsonNote &_note;
	uint16 _tempo, _duration;
	byte _phase, _carries;
	uint32 _cycles;
};

bool buildMewiloThomsonNote(const MewiloThomsonNote &note, byte phase,
		MewiloThomsonNoteTrace &trace) {
	trace = MewiloThomsonNoteTrace();
	if (note.key > 12 || !note.tempo || note.tempo > 15 || !note.duration ||
		note.duration > 191 || note.timbre > 15 ||
		(note.octaveDivisor != 1 && note.octaveDivisor != 2 &&
		 note.octaveDivisor != 4 && note.octaveDivisor != 8 && note.octaveDivisor != 16))
		return false;
	if (!NoteClock(note, phase).run(trace)) {
		trace = MewiloThomsonNoteTrace();
		return false;
	}
	return true;
}

// 1 MHz / 25 kHz = 40 ticks per sample; averaging is exact integer arithmetic.
class DacSampler {
public:
	explicit DacSampler(Common::Array<byte> &pcm) : _pcm(pcm), _used(0), _area(0) {}
	void hold(uint32 ticks, int level) {
		while (ticks) {
			const uint32 take = MIN<uint32>(ticks, 40 - _used);
			_area += level * int32(take);
			_used += take;
			ticks -= take;
			if (_used == 40) {
				_pcm.push_back(byte(_area / 40));
				_used = 0;
				_area = 0;
			}
		}
	}
	void finish() { if (_used) hold(40 - _used, 0); }
private:
	Common::Array<byte> &_pcm;
	uint32 _used;
	int32 _area;
};

bool renderMewiloThomsonMusic(const Common::Array<MewiloThomsonNote> &notes,
		Common::Array<byte> &pcm) {
	pcm.clear();
	if (notes.empty() || notes.size() > 255)
		return false;
	Common::Array<byte> result;
	DacSampler sampler(result);
	uint32 cycles = 0;
	byte phase = 0;
	int level = 0;
	for (uint i = 0; i < notes.size(); ++i) {
		MewiloThomsonNoteTrace trace;
		if (!buildMewiloThomsonNote(notes[i], phase, trace) ||
			trace.cycles > kMaxMusicCycles - cycles)
			return false;
		cycles += trace.cycles;
		phase = trace.phase;
		if (!notes[i].key) {
			// The firmware holds a constant DAC level for PAUSE. Suppress that
			// DC component instead of sending a fictitious 80-count tone.
			sampler.hold(trace.cycles, 0);
			level = 0;
			continue;
		}
		uint32 previous = 0;
		for (uint j = 0; j < trace.writes.size(); ++j) {
			const MewiloThomsonDacWrite &write = trace.writes[j];
			sampler.hold(write.cycle - previous, level);
			level = write.level ? 96 : -96;
			previous = write.cycle;
		}
		sampler.hold(trace.cycles - previous, level);
	}
	sampler.finish();
	pcm = result;
	return true;
}

} // End of namespace Mewilo
} // End of namespace Gob
