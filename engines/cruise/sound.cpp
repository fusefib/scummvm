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
#include "common/events.h"
#include "common/mutex.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "cruise/sound/dos_driver.h"
#include "cruise/sound/internal.h"
#include "cruise/sound/dos_player.h"
#include "cruise/sound/state.h"

namespace Cruise {

PCSoundFxPlayer::PCSoundFxPlayer(PCSoundDriver *driver,
		ResourceLoader resourceLoader, ResourceFreer resourceFreer)
	: _playing(false), _musicStarted(false), _resumeAfterLoad(false),
	  _retirementPending(false), _driver(driver),
	  _resourceLoader(resourceLoader), _resourceFreer(resourceFreer), _mutex() {
	Common::fill(_instrumentsData, _instrumentsData + ARRAYSIZE(_instrumentsData), (byte *)nullptr);
	_sfxData = nullptr;
	_sfxDataSize = 0;
	_musicName[0] = '\0';
	_fadeOutCounter = 0;
	_driver->setUpdateCallback(updateCallback, this);

	_currentPos = 0;
	_currentOrder = 0;
	_numOrders = 0;
	_eventsDelay = 0;
	_looping = false;
	_updateTicksCounter = 0;
}

PCSoundFxPlayer::~PCSoundFxPlayer() {
	// Disconnect first so no callback can retain or reacquire player state.
	_driver->setUpdateCallback(nullptr, nullptr);
	Common::StackLock lock(_mutex);
	stopInternal();
	unloadInternal();
}

bool PCSoundFxPlayer::replacementShouldQuit() const {
	return g_system->getEventManager()->shouldQuit();
}

uint32 PCSoundFxPlayer::replacementMillis() const {
	return g_system->getMillis();
}

bool PCSoundFxPlayer::replacementDelay(uint32 delayMs) {
	g_system->delayMillis(delayMs);
	return true;
}

bool PCSoundFxPlayer::load(const char *song) {
	debug(9, "PCSoundFxPlayer::load('%s')", song);
	char name[33];
	Common::strlcpy(name, song, sizeof(name));

	int lastFade = 0;
	uint32 lastProgress = replacementMillis();
	{
		Common::StackLock lock(_mutex);
		if (_musicStarted && !_driver->isPCSpeaker()) {
			if (_fadeOutCounter == 0) {
				_fadeOutCounter = 1;
				_playing = false;
			}
			lastFade = _fadeOutCounter;
		}
	}

	for (;;) {
		if (replacementShouldQuit()) {
			Common::StackLock lock(_mutex);
			stopInternal();
			unloadInternal();
			return false;
		}

		int currentFade;
		{
			Common::StackLock lock(_mutex);
			if (_fadeOutCounter == 0 || _fadeOutCounter >= 100)
				break;
			currentFade = _fadeOutCounter;
		}
		if (currentFade != lastFade) {
			lastFade = currentFade;
			lastProgress = replacementMillis();
		}

		if (!replacementDelay(REPLACEMENT_DELAY_MS)) {
			Common::StackLock lock(_mutex);
			stopInternal();
			break;
		}
		const uint32 now = replacementMillis();
		if ((uint32)(now - lastProgress) >= REPLACEMENT_STALL_MS) {
			SoundInternal::ReplacementWaitAction action;
			int checkedFade;
			{
				Common::StackLock lock(_mutex);
				checkedFade = _fadeOutCounter;
				action = SoundInternal::checkReplacementWait(lastFade, checkedFade,
						(uint32)(now - lastProgress), REPLACEMENT_STALL_MS);
				if (action == SoundInternal::kReplacementWaitForceStop) {
					warning("Cruise music fade stalled at %d; forcing replacement", lastFade);
					stopInternal();
				}
			}
			if (action == SoundInternal::kReplacementWaitContinue) {
				if (checkedFade != lastFade) {
					lastFade = checkedFade;
					lastProgress = now;
				}
				continue;
			}
			break;
		}
	}

	if (replacementShouldQuit()) {
		Common::StackLock lock(_mutex);
		stopInternal();
		unloadInternal();
		return false;
	}

	Common::StackLock lock(_mutex);
	unloadInternal();
	return loadInternal(name);
}

bool PCSoundFxPlayer::loadInternal(const char *song) {
	if (song != _musicName)
		Common::strlcpy(_musicName, song, sizeof(_musicName));
	_musicStarted = false;
	_playing = false;
	_fadeOutCounter = 0;
	_looping = false;
	uint32 moduleSize = 0;
	_sfxData = (*_resourceLoader)(song, &moduleSize);
	if (!_sfxData) {
		warning("Unable to load soundfx module '%s'", song);
		_musicName[0] = '\0';
		return false;
	}
	if (!SoundInternal::isCorpModuleValid(_sfxData, moduleSize)) {
		warning("Malformed Cruise CORP module '%s' (%u bytes)", song, moduleSize);
		(*_resourceFreer)(_sfxData);
		_sfxData = nullptr;
		_musicName[0] = '\0';
		return false;
	}

	_driver->loadSong(song, _sfxData);
	_sfxDataSize = moduleSize;

	for (int i = 0; i < NUM_INSTRUMENTS; ++i) {
		_instrumentsData[i] = nullptr;
		if (!_driver->usesInstrumentFiles())
			continue;

		char instrument[64];
		Common::fill(instrument, instrument + ARRAYSIZE(instrument), 0);
		Common::copy(_sfxData + INSTRUMENT_NAME_OFFSET + i * INSTRUMENT_RECORD_SIZE,
				_sfxData + INSTRUMENT_NAME_OFFSET + i * INSTRUMENT_RECORD_SIZE + INSTRUMENT_NAME_SIZE,
				instrument);
		instrument[63] = '\0';

		if (instrument[0] != '\0') {
			char *dot = strrchr(instrument, '.');
			if (dot)
				*dot = '\0';
			Common::strlcat(instrument, _driver->getInstrumentExtension(), sizeof(instrument));
			uint32 instrumentSize = 0;
			_instrumentsData[i] = (*_resourceLoader)(instrument, &instrumentSize);
			if (!_instrumentsData[i]) {
				warning("Unable to load soundfx instrument '%s'", instrument);
			} else if (!_driver->isInstrumentDataValid(_instrumentsData[i], instrumentSize)) {
				warning("Malformed soundfx instrument '%s' (%u bytes)", instrument, instrumentSize);
				(*_resourceFreer)(_instrumentsData[i]);
				_instrumentsData[i] = nullptr;
			} else {
				_driver->prepareInstrument(i, _instrumentsData[i]);
			}
		}
	}
	return true;
}

void PCSoundFxPlayer::play() {
	debug(9, "PCSoundFxPlayer::play()");
	Common::StackLock lock(_mutex);
	if (_sfxData) {
		_retirementPending = false;
		for (int i = 0; i < NUM_CHANNELS; ++i)
			_instrumentsChannelTable[i] = -1;
		_currentPos = 0;
		_currentOrder = 0;
		_numOrders = _sfxData[NUM_ORDERS_OFFSET];
		_eventsDelay = (244 - _sfxData[TEMPO_OFFSET]) * 100 / 1060;
		_updateTicksCounter = 0;
		_fadeOutCounter = 0;
		_musicStarted = true;
		_playing = true;
		_driver->setMusicPlaybackEnabled(true);
	}
}

void PCSoundFxPlayer::stopInternal() {
	_retirementPending = false;
	_resumeAfterLoad = false;
	_fadeOutCounter = 0;
	_playing = false;
	_musicStarted = false;
	SoundInternal::stopMusicDriver(_driver);
}

void PCSoundFxPlayer::stop() {
	Common::StackLock lock(_mutex);
	stopInternal();
}

void PCSoundFxPlayer::requestFade() {
	Common::StackLock lock(_mutex);
	fadeOutInternal();
	// DOS FadeSong requests foreground cleanup even for an idle song.
	_retirementPending = true;
}

void PCSoundFxPlayer::fadeOut() {
	Common::StackLock lock(_mutex);
	fadeOutInternal();
}

void PCSoundFxPlayer::fadeOutInternal() {
	if (_sfxData && _musicStarted && _fadeOutCounter == 0) {
		_fadeOutCounter = 1;
		_playing = false;
		if (_driver->isPCSpeaker()) {
			_fadeOutCounter = 100;
			stopInternal();
			_retirementPending = true;
		}
	}
}

void PCSoundFxPlayer::updateCallback(void *ref) {
	static_cast<PCSoundFxPlayer *>(ref)->update();
}

void PCSoundFxPlayer::update() {
	Common::StackLock lock(_mutex);
	if (_playing || (_fadeOutCounter != 0 && _fadeOutCounter < 100)) {
		++_updateTicksCounter;
		if (_updateTicksCounter > _eventsDelay) {
			handleEvents();
			_updateTicksCounter = 0;
		}
	}
}

void PCSoundFxPlayer::handleEvents() {
	if (!_sfxData || _currentOrder < 0 || _currentOrder >= _numOrders ||
			_currentPos < 0 || _currentPos >= PATTERN_SIZE) {
		stopInternal();
		return;
	}
	const byte *patternData = _sfxData + PATTERN_DATA_OFFSET;
	const byte *orderTable = _sfxData + ORDER_TABLE_OFFSET;
	const uint32 patternNum = (uint32)orderTable[_currentOrder] * PATTERN_SIZE;
	if (PATTERN_DATA_OFFSET + patternNum + _currentPos + ROW_SIZE > _sfxDataSize) {
		stopInternal();
		return;
	}

	for (int i = 0; i < NUM_CHANNELS; ++i)
		handlePattern(i, patternData + patternNum + _currentPos + i * CHANNEL_EVENT_SIZE);

	if (_fadeOutCounter != 0 && _fadeOutCounter < 100)
		_fadeOutCounter += 2;
	if (_fadeOutCounter >= 100) {
		stopInternal();
		_retirementPending = true;
		return;
	}

	_currentPos += ROW_SIZE;
	if (_currentPos >= PATTERN_SIZE) {
		_currentPos = 0;
		++_currentOrder;
		if (_currentOrder == _numOrders)
			_currentOrder = 0;
	}
	debug(7, "_currentOrder=%d/%d _currentPos=%d", _currentOrder, _numOrders, _currentPos);
}

void PCSoundFxPlayer::handlePattern(int channel, const byte *patternData) {
	int instrument = patternData[EVENT_INSTRUMENT_OFFSET] >> 4;
	if (instrument != 0) {
		--instrument;
		if (_instrumentsChannelTable[channel] != instrument || _fadeOutCounter != 0) {
			_instrumentsChannelTable[channel] = instrument;
			const int volume = _driver->prepareMusicVolume(_sfxData[VOLUME_TABLE_OFFSET + instrument], _fadeOutCounter);
			_driver->setupChannel(channel, _instrumentsData[instrument], instrument, volume);
		}
	}
	int16 freq = (int16)READ_BE_UINT16(patternData);
	SoundInternal::dispatchCorpPeriodEvent(_driver, channel, freq);
}

void PCSoundFxPlayer::unloadInternal() {
	_retirementPending = false;
	_musicName[0] = '\0';
	if (_musicStarted || _playing || _fadeOutCounter != 0)
		stopInternal();
	if (!_sfxData)
		return;

	_driver->unloadSong();
	for (int i = 0; i < NUM_INSTRUMENTS; ++i) {
		(*_resourceFreer)(_instrumentsData[i]);
		_instrumentsData[i] = nullptr;
	}
	(*_resourceFreer)(_sfxData);
	_sfxData = nullptr;
	_sfxDataSize = 0;
	_musicStarted = false;
	_playing = false;
	_fadeOutCounter = 0;
}

void PCSoundFxPlayer::unload() {
	Common::StackLock lock(_mutex);
	unloadInternal();
	_musicName[0] = '\0';
}

void PCSoundFxPlayer::retire() {
	Common::StackLock lock(_mutex);
	if (!_retirementPending)
		return;
	// Stop callbacks under the same lock before releasing their resources.
	unloadInternal();
	_musicName[0] = '\0';
	_currentPos = _currentOrder = 0;
}

void PCSoundFxPlayer::doSync(Common::Serializer &s) {
	Common::StackLock lock(_mutex);
	SoundInternal::MusicState state;
	Common::strlcpy(state.name, _musicName, sizeof(state.name));
	state.loaded = _sfxData != nullptr;
	state.started = _musicStarted;
	state.playing = _playing;
	state.looping = _looping;
	state.row = _currentPos;
	state.order = _currentOrder;
	if (_retirementPending)
		state.retire();
	state.sync(s);
	if (s.isLoading()) {
		stopInternal();
		unloadInternal();
		_musicName[0] = '\0';
		_currentPos = _currentOrder = 0;
		if (state.loaded && !state.fading() &&
				memchr(state.name, 0, sizeof(state.name)))
			loadInternal(state.name);
		_looping = state.looping;
		if (!_sfxData) {
			_musicName[0] = '\0';
			return;
		}
		for (int i = 0; i < NUM_CHANNELS; ++i)
			_instrumentsChannelTable[i] = -1;
		_numOrders = _sfxData[NUM_ORDERS_OFFSET];
		_eventsDelay = (244 - _sfxData[TEMPO_OFFSET]) * 100 / 1060;
		_updateTicksCounter = 0;
		_currentPos = state.row;
		_currentOrder = state.order;
		if (_currentPos < 0 || _currentPos >= PATTERN_SIZE ||
				(_currentPos % ROW_SIZE) != 0)
			_currentPos = 0;
		if (_numOrders <= 0 || _currentOrder < 0 || _currentOrder >= _numOrders)
			_currentOrder = 0;
		_resumeAfterLoad = state.playing != 0;
		_musicStarted = _resumeAfterLoad;
		_playing = false;
	}
}

void PCSoundFxPlayer::finalizeLoad() {
	Common::StackLock lock(_mutex);
	_playing = _resumeAfterLoad;
	_resumeAfterLoad = false;
	if (_playing)
		_driver->setMusicPlaybackEnabled(true);
}

void PCSoundFxPlayer::syncSounds() {
	Common::StackLock lock(_mutex);
	_driver->syncSounds();
}

bool PCSoundFxPlayer::songLoaded() const {
	Common::StackLock lock(_mutex);
	return _sfxData != nullptr;
}

bool PCSoundFxPlayer::songPlayed() const {
	Common::StackLock lock(_mutex);
	return _musicStarted;
}

uint8 PCSoundFxPlayer::numOrders() const {
	Common::StackLock lock(_mutex);
	assert(_sfxData);
	return _sfxData[NUM_ORDERS_OFFSET];
}

void PCSoundFxPlayer::setNumOrders(uint8 v) {
	Common::StackLock lock(_mutex);
	assert(_sfxData);
	if (v == 0 || ORDER_TABLE_OFFSET + v > AUX_DATA_OFFSET)
		return;
	for (int i = 0; i < v; ++i) {
		if (PATTERN_DATA_OFFSET + ((uint32)_sfxData[ORDER_TABLE_OFFSET + i] + 1) * PATTERN_SIZE > _sfxDataSize)
			return;
	}
	_sfxData[NUM_ORDERS_OFFSET] = v;
}

void PCSoundFxPlayer::setPattern(int offset, uint8 value) {
	Common::StackLock lock(_mutex);
	assert(_sfxData);
	if (offset >= 0 && ORDER_TABLE_OFFSET + offset < AUX_DATA_OFFSET &&
			PATTERN_DATA_OFFSET + ((uint32)value + 1) * PATTERN_SIZE <= _sfxDataSize)
		_sfxData[ORDER_TABLE_OFFSET + offset] = value;
}

const char *PCSoundFxPlayer::musicName() const {
	Common::StackLock lock(_mutex);
	return _musicName;
}

int16 PCSoundFxPlayer::looping() const {
	Common::StackLock lock(_mutex);
	return _looping;
}

void PCSoundFxPlayer::setLooping(int16 v) {
	Common::StackLock lock(_mutex);
	_looping = v;
}


} // End of namespace Cruise
