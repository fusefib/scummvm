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
#include "common/textconsole.h"

#include "cruise/sound/dos_driver.h"
#include "cruise/sound/state.h"

namespace Cruise {
namespace SoundInternal {

MusicState::MusicState() : loaded(0), started(0), looping(0), row(0), order(0), playing(0) {
	memset(name, 0, sizeof(name));
}

void MusicState::retire() {
	memset(name, 0, sizeof(name));
	loaded = started = playing = row = order = 0;
}

void MusicState::sync(Common::Serializer &s) {
	s.syncBytes((byte *)name, sizeof(name));
	s.syncAsSint16LE(loaded);
	s.syncAsSint16LE(started);
	s.syncAsSint16LE(looping);
	s.syncAsSint16LE(row);
	s.syncAsSint16LE(order);
	s.syncAsSint16LE(playing);
}

bool shouldUseNativeMT32(MusicType musicType, bool nativeMT32) {
	return musicType == MT_MT32 || (musicType == MT_GM && nativeMT32);
}

void dispatchCorpPeriodEvent(PCSoundDriver *driver, int channel, int16 period) {
	// The native Ctrl+S latch is tested even on rows without a note.
	if (driver->soundDisabled()) {
		driver->stopChannel(channel);
		return;
	}
	if (period == 0)
		return;

	driver->stopChannel(channel);
	if (period > 0)
		driver->setChannelFrequency(channel, period);
}

void stopMusicDriver(PCSoundDriver *driver) {
	driver->setMusicPlaybackEnabled(false);
	for (int i = 0; i < 4; ++i)
		driver->stopChannel(i);
	driver->stopAll();
}

ReplacementWaitAction checkReplacementWait(int previousFade, int currentFade,
		uint32 elapsed, uint32 stallTimeout) {
	if (currentFade == 0 || currentFade >= 100)
		return kReplacementWaitComplete;
	if (currentFade != previousFade || elapsed < stallTimeout)
		return kReplacementWaitContinue;
	return kReplacementWaitForceStop;
}

PCSpeakerState::PCSpeakerState()
	: _musicEnabled(false), _muxChannel(0), _timerParity(0),
	  _effectActive(false), _effectSegmentsRemaining(0), _effectTicksRemaining(0),
	  _effectDivisor(1), _nextEffectCommand(0), _programmedDivisor(1),
	  _speakerGate(false), _toneSource(kNoTone) {
	Common::fill(_channelDivisors, _channelDivisors + ARRAYSIZE(_channelDivisors), 1);
}

void PCSpeakerState::resetMusic() {
	_musicEnabled = false;
	Common::fill(_channelDivisors, _channelDivisors + ARRAYSIZE(_channelDivisors), 1);
}

void PCSpeakerState::setMusicEnabled(bool enabled) {
	_musicEnabled = enabled;
	if (enabled)
		setGate(true);
}

void PCSpeakerState::setChannelDivisor(int channel, uint16 divisor) {
	assert(channel >= 0 && channel < 4);
	_channelDivisors[channel] = divisor;
}

uint16 PCSpeakerState::channelDivisor(int channel) const {
	assert(channel >= 0 && channel < 4);
	return _channelDivisors[channel];
}

bool PCSpeakerState::programTone(int32 divisor, ToneSource source) {
	const bool changed = _programmedDivisor != divisor || _toneSource != source;
	_programmedDivisor = divisor;
	_toneSource = source;
	return _speakerGate && changed;
}

bool PCSpeakerState::setGate(bool enabled) {
	if (_speakerGate == enabled)
		return false;
	_speakerGate = enabled;
	return true;
}

void PCSpeakerState::startEffect(const byte *data, uint16 count) {
	const uint32 commandSize = (uint32)count * 4;
	_effectCommands.resize(commandSize);
	Common::copy(data + 6, data + 6 + commandSize, _effectCommands.begin());

	_effectActive = true;
	_effectSegmentsRemaining = count;
	_effectDivisor = READ_LE_UINT16(data + 2);
	_effectTicksRemaining = READ_LE_UINT16(_effectCommands.begin());
	_effectDivisor = (uint16)(_effectDivisor + (int16)READ_LE_UINT16(_effectCommands.begin() + 2));
	_nextEffectCommand = 4;
	programTone(_effectDivisor, kEffectTone);
	setGate(true);
}

void PCSpeakerState::stopEffect() {
	_effectActive = false;
	_effectCommands.clear();
	_nextEffectCommand = 0;
	setGate(false);
}

void PCSpeakerState::stopAll() {
	resetMusic();
	stopEffect();
}

bool PCSpeakerState::updateEffect() {
	if (!_effectActive)
		return false;

	--_effectTicksRemaining;
	if (_effectTicksRemaining != 0)
		return false;

	if (_effectSegmentsRemaining > 0)
		--_effectSegmentsRemaining;
	if (_effectSegmentsRemaining == 0) {
		_effectActive = false;
		return setGate(false);
	}

	if (_nextEffectCommand + 4 > _effectCommands.size()) {
		warning("Truncated Cruise HP effect command stream");
		_effectActive = false;
		return setGate(false);
	}

	const byte *command = &_effectCommands[_nextEffectCommand];
	_effectTicksRemaining = READ_LE_UINT16(command);
	_effectDivisor = (uint16)(_effectDivisor + (int16)READ_LE_UINT16(command + 2));
	_nextEffectCommand += 4;
	return programTone(_effectDivisor, kEffectTone);
}

bool PCSpeakerState::timerTick(bool &invokeMusicCallback) {
	bool outputChanged = updateEffect();

	_timerParity ^= 1;
	invokeMusicCallback = _musicEnabled;
	if (_musicEnabled && (_timerParity & 1)) {
		_muxChannel = (_muxChannel + 1) & 3;
		const int32 divisor = _channelDivisors[_muxChannel];
		if (divisor == 1) {
			outputChanged |= setGate(false);
		} else {
			outputChanged |= setGate(true);
			outputChanged |= programTone(divisor, kMusicTone);
		}
	}

	return outputChanged;
}

byte PCSpeakerState::renderedVolume(byte musicVolume, byte sfxVolume) const {
	if (_toneSource == kMusicTone)
		return musicVolume;
	if (_toneSource == kEffectTone)
		return sfxVolume;
	return 0;
}

} // End of namespace SoundInternal
} // End of namespace Cruise
