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

#ifndef CRUISE_SOUND_AMIGA_H
#define CRUISE_SOUND_AMIGA_H

#include "audio/mods/paula.h"

#include "common/array.h"
#include "common/scummsys.h"

#include "cruise/sound/state.h"

namespace Cruise {

class AmigaPaulaStream : public Audio::Paula {
public:
	typedef byte *(*ResourceLoader)(void *context, const char *name, uint32 *size);
	typedef void (*ResourceFreer)(void *data);

	enum {
		kNumChannels = 4,
		kNumInstruments = 15,
		kInstrumentTableOffset = 20,
		kInstrumentRecordSize = 30,
		kInstrumentNameSize = 22,
		kInstrumentParamsOffset = 22,
		kPatternSize = 1024,
		kRowSize = 16,
		kPatternDataOffset = 600,
		kOrderCountOffset = 470,
		kOrderTableOffset = 472
	};

	AmigaPaulaStream(int outputRate, ResourceLoader loader, ResourceFreer freer,
			void *context = nullptr, bool dosControls = false);
	~AmigaPaulaStream() override;

	bool load(const char *name);
	void play();
	void stop();
	void unload();
	void retire();
	void getSaveState(SoundInternal::MusicState &state) const;
	void resetForLoad();
	void fade();
	void requestFade();
	void pause(bool paused);

	bool loaded() const;
	bool started() const;
	int fadeLevel() const;
	uint8 orderCount() const;
	uint8 moduleOrderCount() const;
	void setModuleOrderCount(uint8 count);
	int rowOffset() const;
	int orderIndex() const;
	byte orderPattern(int order) const;
	byte instrumentByte(int instrument, uint32 offset) const;
	void setOrderPattern(int offset, byte value);
	void restoreStoppedPosition(int rowOffset, int orderIndex);
	SoundInternal::ReplacementWaitAction recheckReplacementWait(int previousFade,
			uint32 elapsed, uint32 stallTimeout, int &currentFade) const;
	void setLooping(int16 looping);
	void resetLooping();
	int16 looping() const;
	const char *name() const;
	bool canReuse(const char *name) const;

	void stopChannel(int channel);
	void stopAllChannels();
	void updateChannel(int channel, int period, int volume);
	bool startEffect(const byte *data, uint32 size, int channel, int period,
			int volume, bool loop);
	void syncVolumes(byte musicVolume, byte sfxVolume);
	void setMusicAttenuation(int attenuation);

private:
	struct Instrument {
		Common::Array<byte> data;
		uint16 lengthWords;
		uint16 volume;
		uint16 loopOffset;
		uint16 loopLengthWords;

		Instrument();
	};

	struct ChannelState {
		bool active;
		bool effect;
		uint16 nativeVolume;

		ChannelState();
	};

	void interrupt() override;
	bool loadInternal(const char *name);
	void stopInternal();
	void unloadInternal();
	void trackerTick();
	void processChannel(int channel, const byte *event);
	void startMusicVoice(int channel, int instrument, uint16 period, uint16 volume);
	void clearVoice(int channel);
	void applyChannelVolume(int channel);
	bool validPosition(int rowOffset, int orderIndex) const;

	int _outputRate;
	ResourceLoader _loader;
	ResourceFreer _freer;
	void *_resourceContext;
	bool _dosControls;
	char _name[33];
	Common::Array<byte> _song;
	Instrument _instruments[kNumInstruments];
	Common::Array<byte> _effectData[kNumChannels];
	ChannelState _channels[kNumChannels];
	int8 _silence[2];
	bool _started;
	bool _retirementPending;
	int16 _looping;
	int _fade;
	int _rowOffset;
	int _orderIndex;
	uint8 _orderCount;
	uint16 _ciaLatch;
	uint64 _ciaAccumulator;
	int _interruptDivider;
	int _channelInstrument[kNumChannels];
	uint16 _channelVolume[kNumChannels];
	int _musicAttenuation;
	byte _musicVolume;
	byte _sfxVolume;
};

} // End of namespace Cruise

#endif
