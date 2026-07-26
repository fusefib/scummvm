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

#ifndef MADS_NEBULAR_RSOUND_SECTIONS_H
#define MADS_NEBULAR_RSOUND_SECTIONS_H

#include "mads/nebular/rsound.h"

namespace MADS {
namespace RexNebular {

/** RSOUND.001: `RLND AGAdemo 9-13-92`; 42 game-facing commands; section-specific bank. */
class RSound1 : public RexRSound {
private:
	byte adjustedCommandParam() const;
	void startCommand111213();

public:
	RSound1(Audio::Mixer *mixer, OPL::OPL *opl, MidiDriver *midiDriver);
	int command(int commandId, int param) override;
};

/** Demo RSOUND.001: `RLND AGAdemo 6-11-92`; 41 game-facing commands. */
class RSoundDemo1 : public RexRSound {
private:
	bool _command23Toggle;

	byte adjustedCommandParam() const;
	void startCommand111213();

public:
	RSoundDemo1(Audio::Mixer *mixer, OPL::OPL *opl, MidiDriver *midiDriver);
	int command(int commandId, int param) override;
};

/** RSOUND.002: `RLND AGA002 09-13-92`; 44 game-facing commands; section-specific bank. */
class RSound2 : public RexRSound {
private:
	byte _command12Value;

public:
	RSound2(Audio::Mixer *mixer, OPL::OPL *opl, MidiDriver *midiDriver);
	int command(int commandId, int param) override;
};


/** RSOUND.003: `RLND AGA003 09-11-92`; 61 game-facing commands; bank byte-identical to RSOUND.004 and RSOUND.005. */
class RSound3 : public RexRSound {
private:
	bool _command16Toggle;
	bool _command39Toggle;

public:
	RSound3(Audio::Mixer *mixer, OPL::OPL *opl, MidiDriver *midiDriver);
	int command(int commandId, int param) override;
};

/** RSOUND.004: `RLND AGA004 09-13-92`; 60 game-facing commands; bank byte-identical to RSOUND.003 and RSOUND.005. */
class RSound4 : public RexRSound {
private:
	enum CallbackAction {
		kCallbackNone,
		kCallbackCommand54,
		kCallbackCommand55,
		kCallbackCommand56
	};

	uint16 _callbackCounter;
	uint16 _callbackPeriod;
	CallbackAction _callbackAction;

	void clearCallback();
	void resetSectionState() override;
	void scheduleCallback(CallbackAction action);
	void runCallback(CallbackAction action);
	void sectionTimerTick() override;

public:
	RSound4(Audio::Mixer *mixer, OPL::OPL *opl, MidiDriver *midiDriver);
	int command(int commandId, int param) override;
};

/** RSOUND.005: `RLND AGA005 09-10-92`; 42 game-facing commands; bank byte-identical to RSOUND.003 and RSOUND.004. */
class RSound5 : public RexRSound {
public:
	RSound5(Audio::Mixer *mixer, OPL::OPL *opl, MidiDriver *midiDriver);
	int command(int commandId, int param) override;
};

/** RSOUND.006: `RLND AGA006 09-10-92`; 30 game-facing commands; section-6 MT-32 bank variant. */
class RSound6 : public RexRSound {
private:
	/** DS:10E4 countdown, DS:10E6 reload, DS:10E8 one-shot callback. */
	enum CallbackAction {
		kCallbackNone,
		kCallbackStartGroup130A,
		kCallbackStartGroup1A38
	};

	uint16 _callbackCounter;
	uint16 _callbackPeriod;
	CallbackAction _callbackAction;

	void clearCallback();
	void resetSectionState() override;
	void scheduleCallback(CallbackAction action);
	void startGroup130A();
	void startGroup1A38();
	void sectionTimerTick() override;

public:
	RSound6(Audio::Mixer *mixer, OPL::OPL *opl, MidiDriver *midiDriver);
	int command(int commandId, int param) override;
};

/** RSOUND.007: `RLND AGA007 09-11-92`; 38 game-facing commands; section-7 MT-32 bank variant. */
class RSound7 : public RexRSound {
public:
	RSound7(Audio::Mixer *mixer, OPL::OPL *opl, MidiDriver *midiDriver);
	int command(int commandId, int param) override;
};

/** RSOUND.008: `RLND AGA008 09-09-92`; 38 commands and 38 distinct
 * handlers; section-8 MT-32 bank variant. */
class RSound8 : public RexRSound {
public:
	RSound8(Audio::Mixer *mixer, OPL::OPL *opl, MidiDriver *midiDriver);
	int command(int commandId, int param) override;
};

/** RSOUND.009: `RLND AGAopen 9-13-92`; 52 commands, 51 unique
 * handlers, opening-only bank, callback clock, and channels-6-8 allocator. */
class RSound9 : public RexRSound {
private:
	enum CallbackAction {
		kCallbackNone,
		kCallbackCommand38,
		kCallbackCommand39,
		kCallbackCommand40,
		kCallbackCommand41,
		kCallbackCommand42,
		kCallbackCommand44_46,
		kCallbackCommand45,
		kCallbackCommand47,
		kCallbackCommand50
	};

	// Native representation of RSOUND.009 DS:0050, DS:0052 and DS:0054:
	// countdown, reload period and a one-shot near callback pointer.
	uint16 _callbackCounter;
	uint16 _callbackPeriod;
	CallbackAction _callbackAction;

	// CallbackAction intentionally models only callback addresses installed by
	// the 52-entry command table. An adjacent parameterized helper at image
	// 0x078B has no confirmed caller and remains outside the native command ABI.

	void clearCallback();
	void resetSectionState() override;
	void scheduleCallback(CallbackAction action);
	void startFixedCue(const uint16 *sequenceOffsets, uint sequenceCount);
	void runCallback(CallbackAction action);
	void setOpeningBranchMarkers(byte value);
	int executeOpeningCommonCommand(int commandId);
	void sectionTimerTick() override;

public:
	RSound9(Audio::Mixer *mixer, OPL::OPL *opl, MidiDriver *midiDriver);
	int command(int commandId, int param) override;
};

/** Demo RSOUND.009: `RLND AGAdemo 6-25-92`; 40 game-facing commands,
 * no callback clock, and a channels-6-8 allocator. */
class RSoundDemo9 : public RexRSound {
private:
	int executeDemoCommonCommand(int commandId);

public:
	RSoundDemo9(Audio::Mixer *mixer, OPL::OPL *opl, MidiDriver *midiDriver);
	int command(int commandId, int param) override;
};

} // namespace RexNebular
} // namespace MADS

#endif
