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

#include "audio/mididrv.h"

#include "common/config-manager.h"
#include "common/textconsole.h"

#include "cruise/sound.h"
#include "cruise/sound/atari_players.h"

namespace Cruise {

namespace {

class AtariMidiDriverSink : public AtariMidiSink {
public:
	AtariMidiDriverSink(MidiDriver *driver) : _driver(driver) {}
	~AtariMidiDriverSink() override {
		_driver->close();
		delete _driver;
	}

	void send(byte status, byte first, byte second) override {
		_driver->send(status, first, second);
	}

	void sysEx(const byte *data, uint16 size) override {
		_driver->sysEx(data, size);
	}

private:
	MidiDriver *_driver;
};

} // End of anonymous namespace

Sound *createAtariSound(Audio::Mixer *mixer, CruiseEngine *vm) {
	AtariMidiSink *sink = nullptr;
	const Common::String selection = ConfMan.hasKey("music_driver") ?
			ConfMan.get("music_driver") : Common::String("auto");
	if (!selection.empty() && selection != "auto" && selection != "null") {
		const MidiDriver::DeviceHandle device = MidiDriver::getDeviceHandle(selection);
		const MusicType musicType = MidiDriver::getMusicType(device);
		if (shouldUseAtariMidi(musicType, ConfMan.getBool("native_mt32"), true)) {
			const bool available = MidiDriver::checkDevice(device, MDCK_NONE, false);
			if (!shouldCreateAtariMidi(musicType,
					ConfMan.getBool("native_mt32"), true, available)) {
				warning("Atari MT-32 output is unavailable; falling back to YM2149");
			} else {
				MidiDriver *midi = MidiDriver::createMidi(device);
				const int result = midi ? midi->open() : MidiDriver::MERR_DEVICE_NOT_AVAILABLE;
				if (result == 0) {
					sink = new AtariMidiDriverSink(midi);
				} else {
					warning("Unable to open Atari MT-32 output (%s); falling back to YM2149",
							MidiDriver::getErrorName(result));
					delete midi;
				}
			}
		}
	}

	warning("Cruise: using Atari ST %s music and Atari ST YM2149 effects",
		sink ? "MT-32" : "YM2149");
	return createAtariSound(mixer, vm, sink);
}

} // End of namespace Cruise
