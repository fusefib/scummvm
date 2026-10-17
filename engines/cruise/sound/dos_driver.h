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

#ifndef CRUISE_SOUND_DRIVER_H
#define CRUISE_SOUND_DRIVER_H

#include "common/mutex.h"
#include "common/scummsys.h"

namespace Audio {
class Mixer;
}

namespace OPL {
class OPL;
}

class MidiDriver;

namespace Cruise {

class PCSoundDriver {
public:
	typedef void (*UpdateCallback)(void *);

	PCSoundDriver() :
		_upCb(nullptr),
		_upRef(nullptr),
		_paused(false),
		_musicVolume(0),
		_sfxVolume(0),
		_soundDisabled(false) {}
	virtual ~PCSoundDriver() {}

	virtual void setupChannel(int channel, const byte *data, int instrument, int volume) = 0;
	virtual int prepareMusicVolume(int volume, int fadeOut) const;
	virtual void setChannelFrequency(int channel, int frequency) = 0;
	virtual void stopChannel(int channel) = 0;
	virtual void playSample(const byte *data, int size, int channel, int volume) = 0;
	virtual void stopAll() = 0;
	virtual void setMusicPlaybackEnabled(bool) {}
	virtual void loadSong(const char *songName, const byte *moduleData) {}
	virtual void unloadSong() {}
	virtual void prepareInstrument(int instrument, const byte *data) {}
	virtual bool isInstrumentDataValid(const byte *data, uint32 size) const { return data != nullptr; }
	virtual bool usesInstrumentFiles() const { return true; }
	virtual bool isPCSpeaker() const { return false; }
	virtual const char *getInstrumentExtension() const { return ""; }
	virtual const char *getSoundEffectExtension() const { return getInstrumentExtension(); }
	virtual void syncSounds();
	virtual void pause(bool paused);

	virtual void setUpdateCallback(UpdateCallback upCb, void *ref);
	void findNote(int freq, int *note, int *oct) const;
	void toggleSound();
	bool soundDisabled() const;

protected:
	void invokeUpdateCallback();

	UpdateCallback _upCb;
	void *_upRef;
	Common::Mutex _callbackMutex;
	bool _paused;
	uint8 _musicVolume;
	uint8 _sfxVolume;
	mutable Common::Mutex _controlMutex;
	bool _soundDisabled;

	static const int _noteTable[];
	static const int _noteTableCount;
};

PCSoundDriver *createAdLibSoundDriver(Audio::Mixer *mixer);
PCSoundDriver *createAdLibSoundDriver(Audio::Mixer *mixer, OPL::OPL *opl);
PCSoundDriver *createMT32SoundDriver(MidiDriver *midi);
PCSoundDriver *createPCSpeakerSoundDriver(Audio::Mixer *mixer);

} // End of namespace Cruise

#endif
