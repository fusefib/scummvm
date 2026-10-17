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

#ifndef CRUISE_SOUND_STATE_H
#define CRUISE_SOUND_STATE_H

#include "common/array.h"
#include "common/scummsys.h"
#include "common/serializer.h"

#include "audio/mididrv.h"

namespace Cruise {

class PCSoundDriver;

namespace SoundInternal {

// The existing version-1 music fields, captured under the player's lock.
struct MusicState {
	char name[33];
	int16 loaded, started, looping, row, order, playing;

	MusicState();
	void retire();
	void sync(Common::Serializer &s);
	bool fading() const { return loaded && started && !playing; }
};

enum ReplacementWaitAction {
	kReplacementWaitContinue,
	kReplacementWaitComplete,
	kReplacementWaitForceStop
};

bool shouldUseNativeMT32(MusicType musicType, bool nativeMT32);
void dispatchCorpPeriodEvent(PCSoundDriver *driver, int channel, int16 period);
void stopMusicDriver(PCSoundDriver *driver);
ReplacementWaitAction checkReplacementWait(int previousFade, int currentFade,
		uint32 elapsed, uint32 stallTimeout);

class PCSpeakerState {
public:
	enum ToneSource {
		kNoTone,
		kMusicTone,
		kEffectTone
	};

	PCSpeakerState();

	void resetMusic();
	void setMusicEnabled(bool enabled);
	void setChannelDivisor(int channel, uint16 divisor);
	uint16 channelDivisor(int channel) const;

	void startEffect(const byte *data, uint16 count);
	void stopEffect();
	void stopAll();
	bool timerTick(bool &invokeMusicCallback);
	byte renderedVolume(byte musicVolume, byte sfxVolume) const;

	bool musicEnabled() const { return _musicEnabled; }
	bool effectActive() const { return _effectActive; }
	uint16 effectTicksRemaining() const { return _effectTicksRemaining; }
	int32 programmedDivisor() const { return _programmedDivisor; }
	bool speakerGate() const { return _speakerGate; }
	ToneSource toneSource() const { return _toneSource; }

private:
	bool updateEffect();
	bool programTone(int32 divisor, ToneSource source);
	bool setGate(bool enabled);

	bool _musicEnabled;
	uint16 _channelDivisors[4];
	byte _muxChannel;
	byte _timerParity;

	bool _effectActive;
	uint16 _effectSegmentsRemaining;
	uint16 _effectTicksRemaining;
	int32 _effectDivisor;
	Common::Array<byte> _effectCommands;
	uint32 _nextEffectCommand;

	int32 _programmedDivisor;
	bool _speakerGate;
	ToneSource _toneSource;
};

} // End of namespace SoundInternal
} // End of namespace Cruise

#endif
