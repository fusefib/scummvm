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

#ifndef GOB_PREGOB_MEWILO_THOMSON_MUSIC_H
#define GOB_PREGOB_MEWILO_THOMSON_MUSIC_H

#include "common/array.h"
#include "common/stream.h"

namespace Gob {
namespace Mewilo {

// Values passed by PARTIE4's sequencer to the monitor NOTE entry point.
struct MewiloThomsonNote {
	byte key; // 0 = pause, 1..12 = chromatic notes
	byte tempo;
	byte duration;
	byte timbre;
	byte octaveDivisor;
};

struct MewiloThomsonDacWrite {
	uint32 cycle;
	byte level;
	MewiloThomsonDacWrite(uint32 c, byte v) : cycle(c), level(v) {}
};

struct MewiloThomsonNoteTrace {
	Common::Array<MewiloThomsonDacWrite> writes;
	uint32 cycles;
	byte phase;
	MewiloThomsonNoteTrace() : cycles(0), phase(0) {}
};

enum { kMewiloThomsonSampleRate = 25000 };

// Only canonical load records are accepted. Failed reads clear the output.
bool readMewiloThomsonMusic(Common::SeekableReadStream &stream, byte command,
	Common::Array<MewiloThomsonNote> &notes);

// Translated NOTE counter/duty-cycle algorithm, not a CPU or ROM interpreter.
// Cycles are 1 MHz E-clock ticks within the native monitor routine. 'phase'
// models its scratch accumulator; no register file or firmware is required.
bool buildMewiloThomsonNote(const MewiloThomsonNote &note, byte phase,
	MewiloThomsonNoteTrace &trace);

// Signed 8-bit mono PCM, 25 kHz. Integrates DAC steps over each sample rather
// than rounding notes to hertz/milliseconds. DC-only PAUSE is audible silence.
bool renderMewiloThomsonMusic(const Common::Array<MewiloThomsonNote> &notes,
	Common::Array<byte> &pcm);

} // End of namespace Mewilo
} // End of namespace Gob

#endif
