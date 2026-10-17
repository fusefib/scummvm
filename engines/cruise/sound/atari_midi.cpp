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

#include "common/algorithm.h"
#include "common/endian.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/timer.h"

#include "cruise/sound/atari_players.h"
#include "cruise/sound/internal.h"

namespace Cruise {

namespace {

enum {
	kAtariMfpClock = 2457600,
	kSequencerPrescale = 200
};

class AtariSystemTimerSink : public AtariTimerSink {
public:
	bool install(TimerProc proc, uint32 interval, void *ref,
			const char *name) override {
		return g_system->getTimerManager()->installTimerProc(proc, interval,
				ref, name);
	}
	void remove(TimerProc proc) override {
		g_system->getTimerManager()->removeTimerProc(proc);
	}
};

} // End of anonymous namespace

AtariMidiPlayer::AtariMidiPlayer(AtariMidiSink *sink, ResourceLoader loader,
		ResourceFreer freer, AtariTimerSink *timer)
	: _sink(sink), _timer(timer), _loader(loader), _freer(freer),
	  _timerInstalled(false), _paused(false),
	  _started(false), _retirementPending(false), _looping(false), _fade(0),
	  _rowOffset(0), _orderIndex(0), _orderCount(0), _timerData(1),
	  _timerDivider(0), _musicVolume(255) {
	assert(_sink);
	if (!_timer)
		_timer = new AtariSystemTimerSink();
	_name[0] = '\0';
	Common::fill(_channelInstrument, _channelInstrument + kNumChannels, -1);
}

AtariMidiPlayer::~AtariMidiPlayer() {
	stop();
	unload();
	delete _timer;
	delete _sink;
}

void AtariMidiPlayer::sendDT1(byte address0, byte address1, byte address2,
		const byte *data, uint16 size) {
	byte message[kH32CustomDataSize + 8];
	SoundInternal::buildH32DT1(message, address0, address1, address2, data, size);
	_sink->sysEx(message, size + 8);
}

void AtariMidiPlayer::uploadCustomTimbre(int instrument) {
	if (instrument < 0 || instrument >= kNumInstruments ||
			_instruments[instrument].size() < kH32CustomDataOffset + kH32CustomDataSize)
		return;
	sendDT1(0x08, instrument * 2, 0x00,
			_instruments[instrument].begin() + kH32CustomDataOffset,
			kH32CustomDataSize);
}

int AtariMidiPlayer::renderedVolume(int nativeVolume) const {
	return MIN(100, (nativeVolume * _musicVolume + 127) / 255);
}

void AtariMidiPlayer::sendPatchTemporary(int channel, int group, int number,
		int volume) {
	byte patch[16] = {
		(byte)group, (byte)number, 0x18, 0x32, 0x0c, 0x03, 0x01, 0x00,
		(byte)renderedVolume(volume), 0x07,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	sendDT1(0x03, 0x00, channel * 0x10, patch, sizeof(patch));
}

bool AtariMidiPlayer::loadInternal(const char *name) {
	uint32 size = 0;
	byte *data = (*_loader)(name, &size);
	if (!data)
		return false;

	bool valid = size >= kPatternDataOffset &&
			size >= kInstrumentNameOffset + kNumInstruments * kInstrumentRecordSize;
	const uint8 orderCount = valid ? data[kOrderCountOffset] : 0;
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
		warning("Malformed Atari Cruise MIDI CORP module '%s' (%u bytes)", name, size);
		(*_freer)(data);
		return false;
	}

	_song.resize(size);
	Common::copy(data, data + size, _song.begin());
	(*_freer)(data);

	for (int instrument = 0; instrument < kNumInstruments; ++instrument) {
		const uint32 record = kInstrumentNameOffset +
				instrument * kInstrumentRecordSize;
		char instrumentName[kInstrumentNameSize + 1];
		Common::copy(_song.begin() + record,
				_song.begin() + record + kInstrumentNameSize, instrumentName);
		instrumentName[kInstrumentNameSize] = '\0';
		_instruments[instrument].clear();
		if (!instrumentName[0])
			continue;

		uint32 instrumentSize = 0;
		byte *instrumentData = (*_loader)(instrumentName, &instrumentSize);
		const bool instrumentValid = instrumentData &&
				(instrumentSize == kH32SelectorOffset + 1 ||
				 instrumentSize == kH32CustomResourceSize) &&
				(instrumentData[kH32SelectorOffset] < 0x80 ||
				 (instrumentData[kH32SelectorOffset] < 0xc0 &&
				  instrumentSize == kH32CustomResourceSize));
		if (!instrumentValid) {
			warning("Malformed or missing Atari Cruise H32 instrument '%s'", instrumentName);
			if (instrumentData)
				(*_freer)(instrumentData);
			unloadInternal();
			return false;
		}
		_instruments[instrument].resize(instrumentSize);
		Common::copy(instrumentData, instrumentData + instrumentSize,
				_instruments[instrument].begin());
		(*_freer)(instrumentData);
	}

	Common::strlcpy(_name, name, sizeof(_name));
	_orderCount = orderCount;
	// SUB.B wraps; an MFP timer data byte of zero means 256 ticks.
	const byte timerByte = (byte)(244 - _song[kTempoOffset]);
	_timerData = timerByte ? timerByte : 256;
	for (int instrument = 0; instrument < kNumInstruments; ++instrument) {
		if (!_instruments[instrument].empty() &&
				_instruments[instrument][kH32SelectorOffset] >= 0x80)
			uploadCustomTimbre(instrument);
	}
	return true;
}

bool AtariMidiPlayer::load(const char *name) {
	stop();
	Common::StackLock lock(_mutex);
	unloadInternal();
	return loadInternal(name);
}

void AtariMidiPlayer::timerCallback(void *ref) {
	static_cast<AtariMidiPlayer *>(ref)->timerTick();
}

void AtariMidiPlayer::installTimer() {
	if (_timerInstalled)
		return;
	const uint32 interval = (uint32)(((uint64)1000000 * kSequencerPrescale *
			_timerData + kAtariMfpClock / 2) / kAtariMfpClock);
	if (!_timer->install(timerCallback, interval, this, "cruiseAtariMidi"))
		error("Unable to install Cruise Atari MT-32 timer");
	_timerInstalled = true;
}

void AtariMidiPlayer::removeTimer() {
	if (!_timerInstalled)
		return;
	_timer->remove(timerCallback);
	_timerInstalled = false;
}

void AtariMidiPlayer::playInternal() {
	if (_song.empty())
		return;
	Common::fill(_channelInstrument, _channelInstrument + kNumChannels, -1);
	_rowOffset = 0;
	_orderIndex = 0;
	_timerDivider = 0;
	_fade = 0;
	_paused = false;
	_started = true;
	_retirementPending = false;
}

void AtariMidiPlayer::play() {
	{
		Common::StackLock lock(_mutex);
		playInternal();
		if (!_started)
			return;
	}
	installTimer();
}

void AtariMidiPlayer::stopChannel(int channel) {
	_sink->send(0xb1 + channel, MidiDriver::MIDI_CONTROLLER_ALL_NOTES_OFF, 0);
}

void AtariMidiPlayer::stopInternal() {
	_retirementPending = false;
	const bool wasStarted = _started;
	for (int channel = 0; channel < kNumChannels; ++channel) {
		if (wasStarted)
			stopChannel(channel);
		_channelInstrument[channel] = -1;
	}
	_started = false;
	_paused = false;
	_fade = 0;
	_timerDivider = 0;
}

void AtariMidiPlayer::stop() {
	removeTimer();
	Common::StackLock lock(_mutex);
	stopInternal();
}

void AtariMidiPlayer::unloadInternal() {
	_song.clear();
	for (int instrument = 0; instrument < kNumInstruments; ++instrument)
		_instruments[instrument].clear();
	_name[0] = '\0';
	_orderCount = 0;
}

void AtariMidiPlayer::unload() {
	stop();
	Common::StackLock lock(_mutex);
	unloadInternal();
}

void AtariMidiPlayer::retire() {
	Common::Array<byte> song;
	Common::Array<byte> instruments[kNumInstruments];
	{
		Common::StackLock lock(_mutex);
		if (!_retirementPending)
			return;
		stopInternal();
		// Claim this module before releasing the callback lock. Its storage
		// outlives timer removal; a subsequent command cannot be retired here.
		song.swap(_song);
		for (int i = 0; i < kNumInstruments; ++i)
			instruments[i].swap(_instruments[i]);
		_name[0] = '\0';
		_orderCount = 0;
		_rowOffset = _orderIndex = 0;
	}
	removeTimer();
}

void AtariMidiPlayer::fade() {
	Common::StackLock lock(_mutex);
	if (_started && _fade == 0)
		_fade = 1;
}

void AtariMidiPlayer::pause(bool pausedState) {
	Common::StackLock lock(_mutex);
	if (_paused == pausedState)
		return;
	if (pausedState && _started) {
		for (int channel = 0; channel < kNumChannels; ++channel)
			stopChannel(channel);
	}
	_paused = pausedState && _started;
}

void AtariMidiPlayer::programChannel(int channel, int instrument) {
	if (instrument < 0 || instrument >= kNumInstruments)
		return;
	const byte selector = _instruments[instrument].empty() ? 0 :
			_instruments[instrument][kH32SelectorOffset];
	const int group = selector < 0x80 ? selector >> 6 : 2;
	const int number = selector < 0x80 ? selector & 0x3f : instrument;
	const int volume = MAX(0, (int)_song[instrument] - _fade);
	sendPatchTemporary(channel, group, number, volume);
}

int AtariMidiPlayer::periodToNote(uint16 period) const {
	static const uint16 noteTable[72] = {
		1076, 1076, 1076, 1076, 1076, 1076, 1076, 1076, 1076, 1076, 1076, 1076,
		1076, 1016, 960, 906, 856, 808, 762, 720, 678, 640, 604, 570,
		538, 508, 480, 453, 428, 404, 381, 360, 339, 320, 302, 285,
		269, 254, 240, 226, 214, 202, 190, 180, 170, 160, 151, 143,
		135, 127, 120, 113, 113, 113, 113, 113, 113, 113, 113, 113,
		113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113
	};
	int octave = period < 113 ? 5 : period <= 135 ? 4 :
			period <= 269 ? 3 : period <= 538 ? 2 : period <= 1076 ? 1 : 0;
	int note = 0;
	while (note < 11 && period < noteTable[octave * 12 + note])
		++note;
	return MIN(127, (octave + 1) * 12 + note + 12);
}

void AtariMidiPlayer::processChannel(int channel, const byte *event) {
	const int16 period = READ_BE_INT16(event);
	if (period < 0) {
		stopChannel(channel);
		return;
	}
	const int instrument = (event[2] >> 4) - 1;
	if (instrument >= 0 && instrument < kNumInstruments &&
			(instrument != _channelInstrument[channel] || _fade != 0)) {
		_channelInstrument[channel] = instrument;
		programChannel(channel, instrument);
	}
	if (period != 0) {
		stopChannel(channel);
		_sink->send(0x91 + channel, periodToNote(period), 0x7f);
	}
}

void AtariMidiPlayer::trackerRow() {
	if (!_started || _song.empty() || !validPosition(_rowOffset, _orderIndex))
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
		if (_orderIndex >= _orderCount)
			_orderIndex = 0;
	}
}

void AtariMidiPlayer::timerTick() {
	Common::StackLock lock(_mutex);
	if (!_started || _paused || ++_timerDivider != 12)
		return;
	_timerDivider = 0;
	bool terminalFade = false;
	if (_fade != 0) {
		_fade += 2;
		if (_fade > 100) {
			_fade = 100;
			terminalFade = true;
			_retirementPending = true;
		}
	}
	trackerRow();
	if (terminalFade) {
		for (int channel = 0; channel < kNumChannels; ++channel)
			stopChannel(channel);
		_started = false;
	}
}

void AtariMidiPlayer::refreshVolumes() {
	for (int channel = 0; channel < kNumChannels; ++channel) {
		if (_channelInstrument[channel] >= 0)
			programChannel(channel, _channelInstrument[channel]);
	}
}

void AtariMidiPlayer::syncVolume(byte musicVolume) {
	Common::StackLock lock(_mutex);
	_musicVolume = musicVolume;
	refreshVolumes();
}

bool AtariMidiPlayer::validPosition(int rowOffset, int orderIndex) const {
	return rowOffset >= 0 && rowOffset < kPatternSize &&
			(rowOffset % kRowSize) == 0 && orderIndex >= 0 &&
			orderIndex < _orderCount;
}

bool AtariMidiPlayer::loaded() const {
	Common::StackLock lock(_mutex);
	return !_song.empty();
}

bool AtariMidiPlayer::started() const {
	Common::StackLock lock(_mutex);
	return _started;
}

int AtariMidiPlayer::fadeLevel() const {
	Common::StackLock lock(_mutex);
	return _fade;
}

uint8 AtariMidiPlayer::orderCount() const {
	Common::StackLock lock(_mutex);
	return _orderCount;
}

void AtariMidiPlayer::setOrderCount(uint8 count) {
	Common::StackLock lock(_mutex);
	if (!_song.empty() && count > 0 && count <= 128 &&
			kOrderTableOffset + count <= kPatternDataOffset)
		_orderCount = count;
}

void AtariMidiPlayer::setLooping(int16 looping) {
	Common::StackLock lock(_mutex);
	_looping = looping;
}

int16 AtariMidiPlayer::looping() const {
	Common::StackLock lock(_mutex);
	return _looping;
}

bool AtariMidiPlayer::paused() const {
	Common::StackLock lock(_mutex);
	return _paused;
}

const char *AtariMidiPlayer::name() const {
	return _name;
}

void AtariMidiPlayer::getSaveState(SoundInternal::MusicState &state) const {
	Common::StackLock lock(_mutex);
	Common::strlcpy(state.name, _name, sizeof(state.name));
	state.loaded = !_song.empty();
	state.started = _started;
	state.playing = _started && _fade == 0;
	state.looping = _looping;
	state.row = _rowOffset;
	state.order = _orderIndex;
	if (_retirementPending)
		state.retire();
}

void AtariMidiPlayer::restoreStoppedPosition(int rowOffset, int orderIndex) {
	stop();
	Common::StackLock lock(_mutex);
	// WORKAROUND: Version 1 lacks active MIDI notes, instruments and timer
	// phase. Restore stopped; the host restarts active music from the beginning
	// after resource reconstruction instead of resuming incomplete voices.
	_rowOffset = validPosition(rowOffset, orderIndex) ? rowOffset : 0;
	_orderIndex = validPosition(rowOffset, orderIndex) ? orderIndex : 0;
	_started = false;
	_fade = 0;
	_timerDivider = 0;
	Common::fill(_channelInstrument, _channelInstrument + kNumChannels, -1);
}

SoundInternal::ReplacementWaitAction AtariMidiPlayer::recheckReplacementWait(
		int previousFade, uint32 elapsed, uint32 stallTimeout,
		int &currentFade) const {
	Common::StackLock lock(_mutex);
	currentFade = _started ? _fade : 0;
	return SoundInternal::checkReplacementWait(previousFade, currentFade,
			elapsed, stallTimeout);
}


} // End of namespace Cruise
