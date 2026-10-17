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
#include "common/config-manager.h"
#include "common/mutex.h"

#include "cruise/sound/dos_driver.h"

namespace Cruise {

const int PCSoundDriver::_noteTable[] = {
	0xEEE, 0xE17, 0xD4D, 0xC8C, 0xBD9, 0xB2F, 0xA8E, 0x9F7,
	0x967, 0x8E0, 0x861, 0x7E8, 0x777, 0x70B, 0x6A6, 0x647,
	0x5EC, 0x597, 0x547, 0x4FB, 0x4B3, 0x470, 0x430, 0x3F4,
	0x3BB, 0x385, 0x353, 0x323, 0x2F6, 0x2CB, 0x2A3, 0x27D,
	0x259, 0x238, 0x218, 0x1FA, 0x1DD, 0x1C2, 0x1A9, 0x191,
	0x17B, 0x165, 0x151, 0x13E, 0x12C, 0x11C, 0x10C, 0x0FD,
	0x0EE, 0x0E1, 0x0D4, 0x0C8, 0x0BD, 0x0B2, 0x0A8, 0x09F,
	0x096, 0x08E, 0x086, 0x07E, 0x077, 0x070, 0x06A, 0x064,
	0x05E, 0x059, 0x054, 0x04F, 0x04B, 0x047, 0x043, 0x03F,
	0x03B, 0x038, 0x035, 0x032, 0x02F, 0x02C, 0x02A, 0x027,
	0x025, 0x023, 0x021, 0x01F, 0x01D, 0x01C, 0x01A, 0x019,
	0x017, 0x016, 0x015, 0x013, 0x012, 0x011, 0x010, 0x00F
};

const int PCSoundDriver::_noteTableCount = ARRAYSIZE(_noteTable);

int PCSoundDriver::prepareMusicVolume(int volume, int fadeOut) const {
	volume -= fadeOut;
	return volume > 0 ? volume : 0;
}

void PCSoundDriver::setUpdateCallback(UpdateCallback upCb, void *ref) {
	Common::StackLock lock(_callbackMutex);
	_upCb = upCb;
	_upRef = ref;
}

void PCSoundDriver::invokeUpdateCallback() {
	Common::StackLock lock(_callbackMutex);
	if (_upCb && !_paused)
		(*_upCb)(_upRef);
}

void PCSoundDriver::pause(bool paused) {
	Common::StackLock lock(_callbackMutex);
	_paused = paused;
}

void PCSoundDriver::toggleSound() {
	Common::StackLock lock(_controlMutex);
	_soundDisabled = !_soundDisabled;
}

bool PCSoundDriver::soundDisabled() const {
	Common::StackLock lock(_controlMutex);
	return _soundDisabled;
}

void PCSoundDriver::findNote(int freq, int *note, int *oct) const {
	// Native octave selection precedes the note search: gaps clamp to note 11.
	*oct = 0;
	while (*oct < _noteTableCount / 12 - 1 && freq <= _noteTable[(*oct + 1) * 12])
		++*oct;
	*note = 0;
	while (*note < 11 && freq < _noteTable[*oct * 12 + *note])
		++*note;
}

void PCSoundDriver::syncSounds() {
	bool mute = false;
	if (ConfMan.hasKey("mute"))
		mute = ConfMan.getBool("mute");

	bool music_mute = mute;
	bool sfx_mute = mute;

	if (!mute) {
		music_mute = ConfMan.getBool("music_mute");
		sfx_mute = ConfMan.getBool("sfx_mute");
	}

	// Get the new music and sfx volumes
	_musicVolume = music_mute ? 0 : MIN(255, ConfMan.getInt("music_volume"));
	_sfxVolume = sfx_mute ? 0 : MIN(255, ConfMan.getInt("sfx_volume"));
}


} // End of namespace Cruise
