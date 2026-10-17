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

#include "audio/mixer.h"

#include "common/algorithm.h"
#include "common/config-manager.h"
#include "common/endian.h"
#include "common/str.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "cruise/cruise.h"
#include "cruise/cruise_main.h"
#include "cruise/sound.h"
#include "cruise/sound/amiga_paula.h"
#include "cruise/sound/resources.h"
#include "cruise/vars.h"

namespace Cruise {

namespace {

enum {
	kAmigaCiaClock = 709379,
	kFadeTerminal = 0x40
};

byte configuredVolume(const char *muteKey, const char *volumeKey) {
	if ((ConfMan.hasKey("mute") && ConfMan.getBool("mute")) ||
			(ConfMan.hasKey(muteKey) && ConfMan.getBool(muteKey)))
		return 0;
	return MIN(255, ConfMan.getInt(volumeKey));
}

class AmigaSound : public Sound {
public:
	AmigaSound(Audio::Mixer *mixer, CruiseEngine *vm);
	~AmigaSound() override;

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
	void prepareLoad() override;
	void retireMusic() override { _stream->retire(); }
	void pause(bool paused) override { _stream->pause(paused); }
	void doSync(Common::Serializer &s) override;
	const char *musicName() override;
	bool songLoaded() const override;
	bool songPlayed() const override;
	void fadeSong() override;
	int16 trackPosition() const override;
	uint8 numOrders() const override;
	void setNumOrders(uint8 v) override;
	void setPattern(int offset, uint8 value) override;
	int16 musicLooping() const override;
	void musicLoop(int16 v) override;
	void syncSounds() override;
	const char *soundEffectExtension() const override { return ".SPL"; }
	void setVolume(int volume) override;
	void resetForGame() override;

private:
	Audio::Mixer *_mixer;
	CruiseEngine *_vm;
	AmigaPaulaStream *_stream;
	Audio::SoundHandle _handle;
	bool _restartAfterLoad;
};

} // End of anonymous namespace

AmigaPaulaStream::Instrument::Instrument()
	: lengthWords(0), volume(0), loopOffset(0), loopLengthWords(0) {
}

AmigaPaulaStream::ChannelState::ChannelState()
	: active(false), effect(false), nativeVolume(0) {
}

AmigaPaulaStream::AmigaPaulaStream(int outputRate, ResourceLoader loader,
		ResourceFreer freer, void *context, bool dosControls)
	: Audio::Paula(true, outputRate, 1, Audio::Paula::kFilterModeNone),
	  _outputRate(outputRate), _loader(loader), _freer(freer),
	  _resourceContext(context), _dosControls(dosControls), _started(false),
	  _retirementPending(false), _looping(true), _fade(0), _rowOffset(0),
	  _orderIndex(0), _orderCount(0), _ciaLatch(0), _ciaAccumulator(0),
	  _interruptDivider(0), _musicAttenuation(0), _musicVolume(255),
	  _sfxVolume(255) {
	_name[0] = '\0';
	_silence[0] = 0;
	_silence[1] = 0;
	setChannelPanning(0, 0);
	setChannelPanning(1, 255);
	setChannelPanning(2, 255);
	setChannelPanning(3, 0);
	startPaula();
	Common::fill(_channelInstrument, _channelInstrument + kNumChannels, -1);
	Common::fill(_channelVolume, _channelVolume + kNumChannels, 0);
}

AmigaPaulaStream::~AmigaPaulaStream() {
	unload();
}

bool AmigaPaulaStream::loadInternal(const char *name) {
	uint32 size = 0;
	byte *data = (*_loader)(_resourceContext, name, &size);
	if (!data)
		return false;

	bool valid = size >= kPatternDataOffset && size >= kOrderTableOffset + 1;
	uint8 orderCount = valid ? data[kOrderCountOffset] : 0;
	valid = valid && orderCount > 0 && orderCount <= 128 &&
			kOrderTableOffset + orderCount <= kPatternDataOffset;
	uint32 requiredSize = kPatternDataOffset;
	if (valid) {
		for (int i = 0; i < orderCount; ++i)
			requiredSize = MAX(requiredSize, (uint32)kPatternDataOffset +
					(uint32)(data[kOrderTableOffset + i] + 1) * kPatternSize);
		valid = requiredSize <= size;
	}
	if (!valid) {
		warning("Malformed Amiga Cruise CORP module '%s' (%u bytes)", name, size);
		(*_freer)(data);
		return false;
	}

	_song.resize(size);
	Common::copy(data, data + size, _song.begin());
	(*_freer)(data);

	for (int i = 0; i < kNumInstruments; ++i) {
		const uint32 record = kInstrumentTableOffset + i * kInstrumentRecordSize;
		char instrument[kInstrumentNameSize + 1];
		Common::copy(_song.begin() + record,
				_song.begin() + record + kInstrumentNameSize, instrument);
		instrument[kInstrumentNameSize] = '\0';

		Instrument &target = _instruments[i];
		target.data.clear();
		target.lengthWords = READ_BE_UINT16(_song.begin() + record +
				kInstrumentParamsOffset);
		target.volume = READ_BE_UINT16(_song.begin() + record +
				kInstrumentParamsOffset + 2);
		target.loopOffset = READ_BE_UINT16(_song.begin() + record +
				kInstrumentParamsOffset + 4);
		target.loopLengthWords = READ_BE_UINT16(_song.begin() + record +
				kInstrumentParamsOffset + 6);

		if (instrument[0] == '\0')
			continue;

		uint32 instrumentSize = 0;
		byte *instrumentData = (*_loader)(_resourceContext, instrument, &instrumentSize);
		const uint32 sampleLength = (uint32)target.lengthWords * 2;
		const uint32 loopLength = (uint32)target.loopLengthWords * 2;
		if (!instrumentData || (instrumentSize & 1) != 0 ||
				sampleLength > instrumentSize ||
				(loopLength != 0 && (uint32)target.loopOffset + loopLength > sampleLength)) {
			warning("Malformed or missing Amiga Cruise instrument '%s'", instrument);
			if (instrumentData)
				(*_freer)(instrumentData);
			unloadInternal();
			return false;
		}

		target.data.resize(instrumentSize);
		Common::copy(instrumentData, instrumentData + instrumentSize,
				target.data.begin());
		(*_freer)(instrumentData);
	}

	Common::strlcpy(_name, name, sizeof(_name));
	_orderCount = orderCount;
	_ciaLatch = READ_BE_UINT16(_song.begin() + 4);
	if (_ciaLatch == 0) {
		warning("Amiga Cruise CORP module '%s' has a zero CIA latch", name);
		unloadInternal();
		return false;
	}
	return true;
}

bool AmigaPaulaStream::load(const char *name) {
	Common::StackLock lock(_mutex);
	stopInternal();
	unloadInternal();
	return loadInternal(name);
}

void AmigaPaulaStream::play() {
	Common::StackLock lock(_mutex);
	if (_song.empty())
		return;

	_orderCount = _song[kOrderCountOffset];
	for (int i = 0; i < kNumChannels; ++i)
		clearVoice(i);
	for (int i = 0; i < kNumInstruments; ++i) {
		Common::Array<byte> &data = _instruments[i].data;
		if (!data.empty())
			Common::fill(data.begin(), data.begin() + MIN<uint32>(4, data.size()), 0);
	}
	Common::fill(_channelInstrument, _channelInstrument + kNumChannels, -1);
	Common::fill(_channelVolume, _channelVolume + kNumChannels, 0);
	_rowOffset = 0;
	_orderIndex = 0;
	_ciaAccumulator = 0;
	_interruptDivider = 0;
	_fade = 0;
	_started = true;
	_retirementPending = false;
}

void AmigaPaulaStream::stopInternal() {
	_retirementPending = false;
	if (_started) {
		for (int i = 0; i < kNumChannels; ++i)
			clearVoice(i);
	}
	_started = false;
	_fade = 0;
}

void AmigaPaulaStream::stop() {
	Common::StackLock lock(_mutex);
	stopInternal();
}

void AmigaPaulaStream::pause(bool paused) {
	Common::StackLock lock(_mutex);
	Audio::Paula::pausePlay(paused);
}

void AmigaPaulaStream::unloadInternal() {
	stopInternal();
	_song.clear();
	for (int i = 0; i < kNumInstruments; ++i)
		_instruments[i].data.clear();
	_name[0] = '\0';
	_orderCount = 0;
	_rowOffset = 0;
	// Native TrackPos survives unloading and loading until the next play.
	if (_dosControls)
		_orderIndex = 0;
	_ciaLatch = 0;
}

void AmigaPaulaStream::unload() {
	Common::StackLock lock(_mutex);
	unloadInternal();
}

void AmigaPaulaStream::retire() {
	Common::StackLock lock(_mutex);
	if (!_retirementPending)
		return;
	// Paula rendering shares this lock; clear its voice references first.
	unloadInternal();
}

void AmigaPaulaStream::resetForLoad() {
	Common::StackLock lock(_mutex);
	for (int i = 0; i < kNumChannels; ++i)
		clearVoice(i);
	unloadInternal();
	_orderIndex = 0;
}

void AmigaPaulaStream::fade() {
	Common::StackLock lock(_mutex);
	// Replacement preserves an active fade (1991: 0x21574).
	if (_started && _fade == 0)
		_fade = 1;
}

void AmigaPaulaStream::requestFade() {
	Common::StackLock lock(_mutex);
	// Script FadeSong restarts the fade, or retires a stopped module
	// (1991: 0x2154c; Kixx: 0x214ee).
	if (_song.empty())
		return;
	if (_started)
		_fade = 1;
	else
		_retirementPending = true;
}

void AmigaPaulaStream::clearVoice(int channel) {
	if (channel < 0 || channel >= kNumChannels)
		return;
	_channels[channel] = ChannelState();
	disableChannel(channel);
	setChannelPeriod(channel, 0);
	setChannelVolume(channel, 0);
}

void AmigaPaulaStream::applyChannelVolume(int channel) {
	if (channel < 0 || channel >= kNumChannels)
		return;
	const ChannelState &state = _channels[channel];
	const byte hostVolume = state.effect ? _sfxVolume : _musicVolume;
	setChannelVolume(channel, (byte)(MIN<uint16>(state.nativeVolume, 64) *
			hostVolume / 255));
}

void AmigaPaulaStream::startMusicVoice(int channel, int instrument,
		uint16 period, uint16 volume) {
	if (channel < 0 || channel >= kNumChannels || instrument < 0 ||
			instrument >= kNumInstruments)
		return;

	const Instrument &source = _instruments[instrument];
	if (source.data.empty() || source.lengthWords == 0) {
		clearVoice(channel);
		return;
	}
	const uint32 initialLength = MIN<uint32>((uint32)source.lengthWords * 2,
			source.data.size());
	const uint32 loopOffset = MIN<uint32>(source.loopOffset, initialLength);
	const uint32 loopLength = MIN<uint32>((uint32)source.loopLengthWords * 2,
			initialLength - loopOffset);
	const int8 *initial = (const int8 *)source.data.begin();
	const int8 *repeat = loopLength ? initial + loopOffset : _silence;
	const uint32 repeatLength = loopLength ? loopLength : 2;

	ChannelState &state = _channels[channel];
	state.active = true;
	state.effect = false;
	state.nativeVolume = volume;
	setChannelData(channel, initial, repeat, initialLength, repeatLength);
	setChannelPeriod(channel, period);
	applyChannelVolume(channel);
}

void AmigaPaulaStream::processChannel(int channel, const byte *event) {
	const int16 period = (int16)READ_BE_UINT16(event);
	if (period == -3)
		return;

	const int instrument = (event[2] >> 4) - 1;
	if (instrument >= 0 && instrument < kNumInstruments) {
		_channelInstrument[channel] = instrument;
		int volume = _instruments[instrument].volume;
		switch (event[2] & 0x0f) {
		case 5:
			volume = MIN(0x40, volume + event[3]);
			break;
		case 6:
			volume = MAX(0, volume - event[3]);
			break;
		default:
			break;
		}
		volume = MAX(0, volume - _fade - _musicAttenuation);
		_channelVolume[channel] = volume;
		if (_channels[channel].active && !_channels[channel].effect) {
			_channels[channel].nativeVolume = volume;
			applyChannelVolume(channel);
		}
	}

	if (period == -2) {
		clearVoice(channel);
	} else if (period != 0 && _channelInstrument[channel] >= 0) {
		startMusicVoice(channel, _channelInstrument[channel],
				(uint16)period * 2, _channelVolume[channel]);
	}
}

void AmigaPaulaStream::trackerTick() {
	if (!_started || _song.empty() || _orderCount == 0)
		return;

	if (_orderIndex < 0 || _orderIndex >= _orderCount ||
			_rowOffset < 0 || _rowOffset >= kPatternSize)
		return;
	const uint32 pattern = _song[kOrderTableOffset + _orderIndex];
	const uint32 row = kPatternDataOffset + pattern * kPatternSize + _rowOffset;
	if (row + kRowSize > _song.size()) {
		stopInternal();
		return;
	}

	for (int channel = 0; channel < kNumChannels; ++channel)
		processChannel(channel, _song.begin() + row + channel * 4);

	_rowOffset += kRowSize;
	if (_rowOffset >= kPatternSize) {
		_rowOffset = 0;
		++_orderIndex;
		if (_orderIndex >= _orderCount) {
			if (!_dosControls && !_looping)
				stopInternal();
			_orderIndex = 0;
		}
	}
}

void AmigaPaulaStream::interrupt() {
	if (!_started || _ciaLatch == 0)
		return;

	_ciaAccumulator += kAmigaCiaClock;
	// Continuous CIA timers take one E-clock to reload between countdowns.
	const uint64 threshold = (uint64)_outputRate * ((uint32)_ciaLatch + 1);
	while (_ciaAccumulator >= threshold) {
		_ciaAccumulator -= threshold;
		if (++_interruptDivider != 6)
			continue;
		_interruptDivider = 0;
		if (_fade != 0) {
			++_fade;
			if (_fade >= kFadeTerminal) {
				for (int i = 0; i < kNumChannels; ++i)
					clearVoice(i);
				_started = false;
				_fade = kFadeTerminal;
				_retirementPending = true;
				return;
			}
		}
		trackerTick();
	}
}

bool AmigaPaulaStream::loaded() const {
	Common::StackLock lock(_mutex);
	return !_song.empty();
}

bool AmigaPaulaStream::started() const {
	Common::StackLock lock(_mutex);
	return _started;
}

int AmigaPaulaStream::fadeLevel() const {
	Common::StackLock lock(_mutex);
	return _fade;
}

uint8 AmigaPaulaStream::orderCount() const {
	Common::StackLock lock(_mutex);
	return _orderCount;
}

uint8 AmigaPaulaStream::moduleOrderCount() const {
	Common::StackLock lock(_mutex);
	return _song.empty() ? 0 : _song[kOrderCountOffset];
}

void AmigaPaulaStream::setModuleOrderCount(uint8 count) {
	Common::StackLock lock(_mutex);
	if (_song.empty() || !count || count > 128)
		return;
	for (int i = 0; i < count; ++i) {
		if (kPatternDataOffset + ((uint32)_song[kOrderTableOffset + i] + 1) *
				kPatternSize > _song.size())
			return;
	}
	_song[kOrderCountOffset] = count;
}

int AmigaPaulaStream::rowOffset() const {
	Common::StackLock lock(_mutex);
	return _rowOffset;
}

int AmigaPaulaStream::orderIndex() const {
	Common::StackLock lock(_mutex);
	return _orderIndex;
}

byte AmigaPaulaStream::orderPattern(int order) const {
	Common::StackLock lock(_mutex);
	if (order < 0 || order >= _orderCount)
		return 0;
	return _song[kOrderTableOffset + order];
}

byte AmigaPaulaStream::instrumentByte(int instrument, uint32 offset) const {
	Common::StackLock lock(_mutex);
	if (instrument < 0 || instrument >= kNumInstruments ||
			offset >= _instruments[instrument].data.size())
		return 0;
	return _instruments[instrument].data[offset];
}

void AmigaPaulaStream::setOrderPattern(int offset, byte value) {
	Common::StackLock lock(_mutex);
	if (!_song.empty() && offset >= 0 && offset < 128 &&
			(uint32)kPatternDataOffset + ((uint32)value + 1) * kPatternSize <= _song.size())
		_song[kOrderTableOffset + offset] = value;
}

bool AmigaPaulaStream::validPosition(int rowOffset, int orderIndex) const {
	return rowOffset >= 0 && rowOffset < kPatternSize &&
			(rowOffset % kRowSize) == 0 && orderIndex >= 0 &&
			orderIndex < _orderCount;
}

void AmigaPaulaStream::restoreStoppedPosition(int rowOffset, int orderIndex) {
	Common::StackLock lock(_mutex);
	_retirementPending = false;
	_rowOffset = validPosition(rowOffset, orderIndex) ? rowOffset : 0;
	_orderIndex = validPosition(rowOffset, orderIndex) ? orderIndex : 0;
	_fade = 0;
	// WORKAROUND: Version 1 lacks Paula voice, sample and timer phase state.
	// Restore stopped; the host restarts active music from row zero after
	// resource reconstruction instead of resuming with incomplete voice state.
	_started = false;
	_ciaAccumulator = 0;
	_interruptDivider = 0;
	Common::fill(_channelInstrument, _channelInstrument + kNumChannels, -1);
	Common::fill(_channelVolume, _channelVolume + kNumChannels, 0);
	for (int i = 0; i < kNumChannels; ++i)
		clearVoice(i);
}

SoundInternal::ReplacementWaitAction AmigaPaulaStream::recheckReplacementWait(
		int previousFade, uint32 elapsed, uint32 stallTimeout,
		int &currentFade) const {
	Common::StackLock lock(_mutex);
	currentFade = _started ? _fade : 0;
	return SoundInternal::checkReplacementWait(previousFade, currentFade,
			elapsed, stallTimeout);
}

void AmigaPaulaStream::getSaveState(SoundInternal::MusicState &state) const {
	Common::StackLock lock(_mutex);
	Common::strlcpy(state.name, _name, sizeof(state.name));
	state.loaded = !_song.empty();
	state.started = _started;
	state.playing = _started && _fade == 0;
	state.looping = _looping;
	state.row = state.loaded ? _rowOffset : 0;
	state.order = state.loaded ? _orderIndex : 0;
	if (_retirementPending)
		state.retire();
}

void AmigaPaulaStream::setLooping(int16 looping) {
	Common::StackLock lock(_mutex);
	_looping = looping;
}

void AmigaPaulaStream::resetLooping() {
	Common::StackLock lock(_mutex);
	_looping = true;
}

int16 AmigaPaulaStream::looping() const {
	Common::StackLock lock(_mutex);
	return _looping;
}

const char *AmigaPaulaStream::name() const {
	return _name;
}

bool AmigaPaulaStream::canReuse(const char *name) const {
	Common::StackLock lock(_mutex);
	return !_song.empty() && !strcmp(_name, name) && !_fade && !_retirementPending;
}

void AmigaPaulaStream::stopChannel(int channel) {
	Common::StackLock lock(_mutex);
	if (channel >= 0 && channel < kNumChannels)
		clearVoice(channel);
}

void AmigaPaulaStream::stopAllChannels() {
	Common::StackLock lock(_mutex);
	for (int i = 0; i < kNumChannels; ++i)
		clearVoice(i);
}

void AmigaPaulaStream::updateChannel(int channel, int period, int volume) {
	Common::StackLock lock(_mutex);
	if (channel < 0 || channel >= kNumChannels)
		return;
	ChannelState &state = _channels[channel];
	if (period != -1)
		setChannelPeriod(channel, period);
	if (volume != -1) {
		state.nativeVolume = MAX(0, MIN(64, volume));
		applyChannelVolume(channel);
	}
}

bool AmigaPaulaStream::startEffect(const byte *data, uint32 size, int channel,
		int period, int volume, bool loop) {
	if (!data || channel < 0 || channel >= kNumChannels || period <= 0 ||
			size < 2 || (size & 1) != 0)
		return false;

	Common::StackLock lock(_mutex);
	Common::Array<byte> &owned = _effectData[channel];
	owned.resize(size);
	Common::copy(data, data + size, owned.begin());
	owned[0] = 0;
	owned[1] = 0;
	owned[size - 2] = 0;
	owned[size - 1] = 0;

	const int8 *initial = (const int8 *)owned.begin();
	const int8 *repeat = loop ? initial : _silence;
	const uint32 repeatLength = loop ? size : 2;
	ChannelState &state = _channels[channel];
	state.active = true;
	state.effect = true;
	state.nativeVolume = MAX(0, MIN(64, volume));
	setChannelData(channel, initial, repeat, size, repeatLength);
	setChannelPeriod(channel, period);
	applyChannelVolume(channel);
	return true;
}

void AmigaPaulaStream::syncVolumes(byte musicVolume, byte sfxVolume) {
	Common::StackLock lock(_mutex);
	_musicVolume = musicVolume;
	_sfxVolume = sfxVolume;
	for (int i = 0; i < kNumChannels; ++i)
		if (_channels[i].active)
			applyChannelVolume(i);
}

void AmigaPaulaStream::setMusicAttenuation(int attenuation) {
	Common::StackLock lock(_mutex);
	_musicAttenuation = MAX(0, attenuation);
}

static byte *readAmigaSoundFile(void *, const char *name, uint32 *size) {
	return readBundleSoundFile(name, size);
}

AmigaSound::AmigaSound(Audio::Mixer *mixer, CruiseEngine *vm)
	: _mixer(mixer), _vm(vm), _restartAfterLoad(false) {
	_stream = new AmigaPaulaStream(_mixer->getOutputRate(), readAmigaSoundFile,
			MemoryFree);
	_mixer->playStream(Audio::Mixer::kPlainSoundType, &_handle, _stream, -1,
			Audio::Mixer::kMaxChannelVolume, 0, DisposeAfterUse::NO, true);
	syncSounds();
}

AmigaSound::~AmigaSound() {
	_mixer->stopHandle(_handle);
	delete _stream;
}

void AmigaSound::loadMusic(const char *name) {
	char song[33];
	Common::strlcpy(song, name, sizeof(song));
	if (_stream->started()) {
		_stream->fade();
		int lastFade = _stream->fadeLevel();
		uint32 lastProgress = _vm->presentationMillis();
		while (_stream->started() && !_vm->soundWaitCancelled()) {
			if (!_vm->waitForSound(50))
				break;
			const int fade = _stream->fadeLevel();
			const uint32 now = _vm->presentationMillis();
			if (fade != lastFade) {
				lastFade = fade;
				lastProgress = now;
			} else if (now - lastProgress >= 2000) {
				int checkedFade = fade;
				const SoundInternal::ReplacementWaitAction action =
						_stream->recheckReplacementWait(lastFade,
								now - lastProgress, 2000, checkedFade);
				if (action == SoundInternal::kReplacementWaitContinue) {
					if (checkedFade != lastFade) {
						lastFade = checkedFade;
						lastProgress = now;
					}
					continue;
				}
				if (action == SoundInternal::kReplacementWaitForceStop)
					_stream->stop();
				break;
			}
		}
	}
	if (_vm->soundWaitCancelled()) {
		_stream->stop();
		_stream->unload();
		return;
	}
	_stream->stop();
	_stream->unload();
	if (!_stream->load(song))
		warning("Unable to load Amiga Cruise song '%s'", song);
}

void AmigaSound::playMusic() {
	_stream->play();
}

void AmigaSound::stopMusic() {
	_stream->stop();
}

void AmigaSound::removeMusic() {
	_stream->unload();
}

void AmigaSound::fadeOutMusic() {
	_stream->requestFade();
}

void AmigaSound::playEffect(int sample, int channel, int period, int volume) {
	if (sample < 0 || sample >= NUM_FILE_ENTRIES ||
			!filesDatabase[sample].subData.ptr)
		return;
	_stream->startEffect(filesDatabase[sample].subData.ptr,
			getSoundResourceSize(sample), channel, period, volume, false);
}

void AmigaSound::loopEffect(int sample, int channel, int period, int volume) {
	if (sample < 0 || sample >= NUM_FILE_ENTRIES || channel < 0 ||
			channel >= AmigaPaulaStream::kNumChannels ||
			!filesDatabase[sample].subData.ptr)
		return;

	SoundEntry &effect = soundList[channel];
	effect.frameNum = sample;
	effect.frequency = period;
	effect.volume = volume;
	_stream->startEffect(filesDatabase[sample].subData.ptr,
			getSoundResourceSize(sample), channel, period, volume, true);
}

void AmigaSound::updateEffect(int sample, int channel, int period, int volume) {
	if (sample < 0 || sample >= NUM_FILE_ENTRIES || channel < 0 ||
			channel >= AmigaPaulaStream::kNumChannels ||
			!filesDatabase[sample].subData.ptr)
		return;

	SoundEntry &effect = soundList[channel];
	if (period != -1)
		effect.frequency = period;
	if (volume != -1)
		effect.volume = volume;
	_stream->updateChannel(channel, period, volume);
}

void AmigaSound::stopEffect(int channel) {
	if (channel == -1) {
		for (int i = 0; i < AmigaPaulaStream::kNumChannels; ++i)
			soundList[i].frameNum = -1;
		if (!_stream->started())
			_stream->stopAllChannels();
		return;
	}
	if (channel < 0 || channel >= AmigaPaulaStream::kNumChannels)
		return;
	soundList[channel].frameNum = -1;
	if (!_stream->started())
		_stream->stopChannel(channel);
}

void AmigaSound::restoreEffects() {
	for (int channel = 0; channel < AmigaPaulaStream::kNumChannels; ++channel) {
		const SoundEntry &effect = soundList[channel];
		if (effect.frameNum < 0 || effect.frameNum >= NUM_FILE_ENTRIES ||
				!filesDatabase[effect.frameNum].subData.ptr)
			continue;
		_stream->startEffect(filesDatabase[effect.frameNum].subData.ptr,
				getSoundResourceSize(effect.frameNum), channel, effect.frequency,
				effect.volume, true);
	}
}

void AmigaSound::finalizeLoad() {
	if (_restartAfterLoad && _stream->loaded())
		_stream->play();
	_restartAfterLoad = false;
	restoreEffects();
}

void AmigaSound::prepareLoad() {
	_restartAfterLoad = false;
	_stream->resetForLoad();
}

void AmigaSound::doSync(Common::Serializer &s) {
	SoundInternal::MusicState state;
	_stream->getSaveState(state);
	if (s.isLoading())
		_stream->resetForLoad();
	state.sync(s);
	s.syncAsSint16LE(_genVolume);
	if (s.isLoading()) {
		if (state.fading())
			state.retire();
		if (state.loaded && (!memchr(state.name, 0, sizeof(state.name)) || !_stream->load(state.name)))
			state.retire();
		_restartAfterLoad = state.loaded && state.started && state.playing;
		_stream->setLooping(state.looping);
		_stream->setMusicAttenuation(_genVolume);
		_stream->restoreStoppedPosition(state.row, state.order);
	}
}

const char *AmigaSound::musicName() {
	return _stream->name();
}

bool AmigaSound::songLoaded() const {
	return _stream->loaded();
}

bool AmigaSound::songPlayed() const {
	return _stream->started();
}

void AmigaSound::fadeSong() {
	_stream->requestFade();
}

int16 AmigaSound::trackPosition() const {
	return _stream->orderIndex();
}

uint8 AmigaSound::numOrders() const {
	return _stream->moduleOrderCount();
}

void AmigaSound::setNumOrders(uint8 v) {
	// No SongSize calls occur in the inspected English Amiga scripts.
	// The registered native opcode edits the count latched by the next play.
	_stream->setModuleOrderCount(v);
}

void AmigaSound::setPattern(int offset, uint8 value) {
	// No SetPattern calls occur in the inspected English Amiga scripts.
	// Native edits may address stored orders outside the current play span.
	_stream->setOrderPattern(offset, value);
}

int16 AmigaSound::musicLooping() const {
	return _stream->looping();
}

void AmigaSound::musicLoop(int16 v) {
	_stream->setLooping(v);
}

void AmigaSound::syncSounds() {
	_stream->syncVolumes(configuredVolume("music_mute", "music_volume"),
			configuredVolume("sfx_mute", "sfx_volume"));
}

void AmigaSound::setVolume(int volume) {
	_genVolume = volume;
	_stream->setMusicAttenuation(volume);
}

void AmigaSound::resetForGame() {
	_restartAfterLoad = false;
	_stream->resetForLoad();
	_stream->resetLooping();
}

Sound *createAmigaSound(Audio::Mixer *mixer, CruiseEngine *vm) {
	return new AmigaSound(mixer, vm);
}

} // End of namespace Cruise
