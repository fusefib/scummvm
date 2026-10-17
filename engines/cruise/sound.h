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

#ifndef CRUISE_SOUND_H
#define CRUISE_SOUND_H

#include "common/serializer.h"
#include "common/str.h"

namespace Audio {
class Mixer;
}

namespace Cruise {

class CruiseEngine;
class PCSoundDriver;
class PCSoundFxPlayer;
class AmigaMusicResources;

enum MusicReplacement {
	kNoReplacement,
	kAtariMusic,
	kAmigaMusic
};

MusicReplacement getMusicReplacement(const Common::String &domain);

class Sound {
protected:
	int _genVolume;

public:
	Sound() : _genVolume(0) {}
	virtual ~Sound() {}

	virtual void loadMusic(const char *name) = 0;
	virtual void playMusic() = 0;
	virtual void stopMusic() = 0;
	virtual void removeMusic() = 0;
	virtual void fadeOutMusic() = 0;

	virtual void playEffect(int sample, int channel, int period, int volume) = 0;
	virtual void loopEffect(int sample, int channel, int period, int volume) = 0;
	virtual void updateEffect(int sample, int channel, int period, int volume) = 0;
	virtual void stopEffect(int channel) = 0;
	virtual void restoreEffects() = 0;
	virtual void finalizeLoad() = 0;
	virtual void prepareLoad() { stopMusic(); }
	virtual void pause(bool paused) { (void)paused; }
	virtual void service() {}
	virtual void retireMusic() {}
	virtual void toggleSound() {}

	virtual void doSync(Common::Serializer &s) = 0;
	virtual const char *musicName() = 0;
	virtual bool canReuseMusic(const char *name);
	virtual bool isMusicAvailable(const char *name) const;
	virtual bool songLoaded() const = 0;
	virtual bool songPlayed() const = 0;
	virtual void fadeSong() = 0;
	virtual int16 trackPosition() const { return 0; }
	virtual uint8 numOrders() const = 0;
	virtual void setNumOrders(uint8 v) = 0;
	virtual void setPattern(int offset, uint8 value) = 0;
	virtual int16 musicLooping() const = 0;
	virtual void musicLoop(int16 v) = 0;
	virtual void syncSounds() = 0;
	virtual const char *soundEffectExtension() const = 0;
	virtual void resetForGame() {}

	virtual void setVolume(int volume) { _genVolume = volume; }
	virtual uint8 getVolume() const { return _genVolume; }
};

class PCSound : public Sound {
private:
	Audio::Mixer *_mixer;
	CruiseEngine *_vm;
protected:
	PCSoundDriver *_soundDriver;
	PCSoundFxPlayer *_player;
public:
	PCSound(Audio::Mixer *mixer, CruiseEngine *vm);
	~PCSound() override;

	void loadMusic(const char *name) override;
	void playMusic() override;
	void stopMusic() override;
	void removeMusic() override;
	void fadeOutMusic() override;

	void playEffect(int sample, int channel, int period, int volume) override;
	void loopEffect(int sample, int channel, int period, int volume) override;
	void updateEffect(int sample, int channel, int period, int volume) override;
	void stopEffect(int channel) override;
	void restoreEffects() override;
	void finalizeLoad() override;
	void toggleSound() override;
	void pause(bool paused) override;
	void retireMusic() override;

	void doSync(Common::Serializer &s) override;
	const char *musicName() override;
	bool songLoaded() const override;
	bool songPlayed() const override;
	void fadeSong() override;
	uint8 numOrders() const override;
	void setNumOrders(uint8 v) override;
	void setPattern(int offset, uint8 value) override;
	int16 musicLooping() const override;
	void musicLoop(int16 v) override;
	void syncSounds() override;
	const char *soundEffectExtension() const override;
	void setVolume(int volume) override { _genVolume = volume; }
	uint8 getVolume() const override { return _genVolume; }
};

Sound *createSound(Audio::Mixer *mixer, CruiseEngine *vm);
Sound *createDosAtariSound(Audio::Mixer *mixer, CruiseEngine *vm);
Sound *createDosAmigaSound(Audio::Mixer *mixer, CruiseEngine *vm,
		AmigaMusicResources *resources);
Sound *createAmigaSound(Audio::Mixer *mixer, CruiseEngine *vm);
Sound *createAtariSound(Audio::Mixer *mixer, CruiseEngine *vm);

} // End of namespace Cruise

#endif
