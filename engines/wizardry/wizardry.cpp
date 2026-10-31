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

#include "wizardry/wizardry.h"
#include "audio/audiostream.h"
#include "audio/decoders/raw.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/events.h"
#include "common/file.h"
#include "common/system.h"
#include "engines/util.h"
#include "graphics/cursorman.h"
#include "graphics/fonts/dosfont.h"
#include "graphics/paletteman.h"
#include "wizardry/data/ucsd_disk.h"
#include "wizardry/vm/console.h"
#include "wizardry/vm/timing.h"

namespace Wizardry {

WizardryEngine::WizardryEngine(OSystem *system, const WizardryGameDescription *description) : Engine(system), _description(description) {
	memset(_pixels, 0, sizeof(_pixels));
	if (_description->gameNumber >= 1 && _description->gameNumber <= 5)
		_machine.setObserver(this);
}

WizardryEngine::~WizardryEngine() {
	stopSound();
}

bool WizardryEngine::hasFeature(EngineFeature feature) const {
	return feature == kSupportsReturnToLauncher;
}

Common::Error WizardryEngine::run() {
	if (_description->gameNumber < 1 || _description->gameNumber > 5)
		return Common::Error(Common::kUnsupportedGameidError, "Unsupported Wizardry game number");
	const Common::String name = Common::String::format("WIZ%u.DSK", _description->gameNumber);
	Common::File file;
	if (!file.open(Common::Path(name)))
		return Common::Error(Common::kNoGameDataFoundError, name);
	if (file.size() < 6 * 512 || file.size() > 2 * 1024 * 1024)
		return Common::Error(Common::kReadingFailed, "Unexpected disk image size");
	Common::Array<byte> bytes(uint32(file.size()));
	if (file.read(bytes.data(), bytes.size()) != bytes.size())
		return Common::kReadingFailed;
	UcsdDisk disk;
	Common::String message;
	Common::Array<byte> program;
	if (!disk.loadBytes(bytes, message) || !disk.readFile("SYSTEM.PASCAL", program, message))
		return Common::Error(Common::kReadingFailed, message);
	const bool useOriginalSave = (_description->gameNumber >= 4 ||
												  (ConfMan.hasKey("original_save_disk") && ConfMan.getBool("original_save_disk")));
	_machine.setOriginalMemory(useOriginalSave);
	if (useOriginalSave) {
		const Common::String saveName = Common::String::format("SAVE%u.DSK", _description->gameNumber);
		Common::File saveFile;
		if (!saveFile.open(Common::Path(saveName))) {
			debug(1, "Wizardry: original save disk rejected: missing %s", saveName.c_str());
			return Common::Error(Common::kNoGameDataFoundError,
								 Common::String::format("Starting this game requires %s", saveName.c_str()));
		}
		if (saveFile.size() != 1280 * 512) {
			debug(1, "Wizardry: original save disk rejected: unexpected size for %s", saveName.c_str());
			return Common::Error(Common::kReadingFailed, "The original save disk has an unexpected size");
		}
		Common::Array<byte> saveBytes(uint32(saveFile.size()));
		if (saveFile.read(saveBytes.data(), saveBytes.size()) != saveBytes.size()) {
			debug(1, "Wizardry: original save disk rejected: could not read all of %s", saveName.c_str());
			return Common::kReadingFailed;
		}
		if (!_machine.boot(program, bytes, saveBytes, message)) {
			debug(1, "Wizardry: original save disk rejected: %s", message.c_str());
			return Common::Error(Common::kReadingFailed, message);
		}
	} else if (!_machine.boot(program, bytes, message)) {
		return Common::Error(Common::kReadingFailed, message);
	}
	// Earlier scenarios are optional, immutable inputs. The original Move
	// Characters command mutates private copies, committed with the target.
	if (_machine.transferProfile()) {
		for (byte source = 1; source <= TransferDisks::sourceCount(_machine.transferProfile()); ++source) {
			const Common::String sourceName = Common::String::format("SAVE%u.DSK", source);
			if (!Common::File::exists(Common::Path(sourceName)))
				continue;
			Common::File sourceFile;
			if (!sourceFile.open(Common::Path(sourceName)) || sourceFile.size() != 1280 * 512)
				return Common::Error(Common::kReadingFailed, "An earlier-scenario transfer disk could not be read");
			Common::Array<byte> sourceBytes(uint32(sourceFile.size()));
			if (sourceFile.read(sourceBytes.data(), sourceBytes.size()) != sourceBytes.size())
				return Common::kReadingFailed;
			if (!_machine.loadTransferSource(source, sourceBytes, message))
				return Common::Error(Common::kReadingFailed, message);
		}
	}
	if (_description->gameNumber >= 4) {
		const Common::String comName = Common::String::format("WIZ%u.COM", _description->gameNumber);
		Common::File comFile;
		if (!comFile.open(Common::Path(comName)))
			return Common::Error(Common::kNoGameDataFoundError,
								 Common::String::format("Starting this game requires %s", comName.c_str()));
		if (comFile.size() <= 0 || comFile.size() > 65536)
			return Common::Error(Common::kReadingFailed, "The resident loader file has an unexpected size");
		Common::Array<byte> comBytes(uint32(comFile.size()));
		if (comFile.read(comBytes.data(), comBytes.size()) != comBytes.size())
			return Common::kReadingFailed;
		if (!_machine.loadLoaderCom(comBytes, message))
			return Common::Error(Common::kReadingFailed, message);
	}
	debug(1, "Wizardry %u: loaded %u p-code segments from %s", _description->gameNumber,
		  _machine.codeFile().segments().size(), name.c_str());
	initGraphics(320, 200);
	CursorMan.showMouse(false);
	const byte palette[] = {0, 0, 0, 0, 170, 170, 170, 0, 0, 255, 255, 255};
	_system->getPaletteManager()->setPalette(palette, 0, 4);
	syncSoundSettings();
	_machine.setRealtime(true);
	_lastClock = _system->getMillis();
	_ready = true;
	PCodeMachine::State previousState = PCodeMachine::kHalted;
	while (!shouldQuit()) {
		processEvents();
		if (shouldQuit())
			break;
		updateTime();
		PCodeMachine::State state = _machine.run(20000);
		_lastClock = _system->getMillis();
		if (state == PCodeMachine::kPlayingSound) {
			if (!_soundActive || _soundInstruction != _machine.instructions())
				startSound();
		}
		// Guest sound waits may expire before the next mixer callback. Let the
		// persistent queue drain naturally, including the 3.645 ms click.
		present();
		if (state != previousState && state == PCodeMachine::kWaitingForInput)
			debug(2, "Wizardry: input wait at segment %u, procedure %u, IP 0x%04x",
				  _machine.currentSegment(), _machine.currentProcedure(), _machine.currentIP());
		previousState = state;
		if (state == PCodeMachine::kFault) {
			return Common::Error(Common::kUnknownError, _machine.error());
		}
		if (state == PCodeMachine::kHalted)
			break;
		if (state != PCodeMachine::kRunning)
			_system->delayMillis(state == PCodeMachine::kPlayingSound ? 1 : 10);
	}
	stopSound();
	_ready = false;
	return Common::kNoError;
}

void WizardryEngine::processEvents() {
	Common::Event event;
	while (_eventMan->pollEvent(event)) {
		if (event.type != Common::EVENT_KEYDOWN)
			continue;
		if (event.kbd.keycode == Common::KEYCODE_F5) {
			openMainMenuDialog();
			continue;
		}
		byte key = 0;
		switch (event.kbd.keycode) {
		case Common::KEYCODE_LEFT:
			key = 0x09;
			break;
		case Common::KEYCODE_DOWN:
			key = 0x0a;
			break;
		case Common::KEYCODE_UP:
			key = 0x0b;
			break;
		case Common::KEYCODE_RIGHT:
			key = 0x0c;
			break;
		case Common::KEYCODE_RETURN:
		case Common::KEYCODE_KP_ENTER:
			key = 0x0d;
			break;
		case Common::KEYCODE_ESCAPE:
			key = 0x1b;
			break;
		case Common::KEYCODE_BACKSPACE:
			key = 0x08;
			break;
		default:
			if (!(event.kbd.flags & (Common::KBD_CTRL | Common::KBD_ALT | Common::KBD_META)) &&
				event.kbd.ascii >= 32 && event.kbd.ascii <= 126) {
				key = event.kbd.ascii;
				if (key >= 'a' && key <= 'z')
					key -= 'a' - 'A';
			}
			break;
		}
		if (key)
			_machine.queueKey(key);
	}
	const int modifiers = _eventMan->getModifierState();
	byte bios = 0;
	if (modifiers & Common::KBD_SHIFT)
		bios |= 3;
	if (modifiers & Common::KBD_CTRL)
		bios |= 4;
	if (modifiers & Common::KBD_ALT)
		bios |= 8;
	if (modifiers & Common::KBD_SCRL)
		bios |= 0x10;
	if (modifiers & Common::KBD_NUM)
		bios |= 0x20;
	if (modifiers & Common::KBD_CAPS)
		bios |= 0x40;
	_machine.setBiosModifiers(bios);
}

void WizardryEngine::updateTime() {
	const uint32 now = _system->getMillis();
	if (_ready && !_paused)
		_machine.advanceTime(now - _lastClock);
	_lastClock = now;
}

void WizardryEngine::pauseEngineIntern(bool pause) {
	if (pause)
		updateTime();
	Engine::pauseEngineIntern(pause);
	_paused = pause;
	_lastClock = _system->getMillis();
}

void WizardryEngine::startSound() {
	_soundActive = true;
	_soundInstruction = _machine.instructions();
	if (!_mixer || !_mixer->isReady())
		return;
	Common::Array<byte> pcm;
	NativeTiming::renderSound(_machine.soundPattern(), _machine.soundElapsedUs(), pcm);
	if (pcm.empty())
		return;
	byte *samples = static_cast<byte *>(malloc(pcm.size()));
	if (!samples)
		return;
	memcpy(samples, pcm.data(), pcm.size());
	if (!_speakerStream) {
		_speakerStream = Audio::makeQueuingAudioStream(NativeTiming::kSpeakerRate, false);
		_speakerStream->queueBuffer(samples, pcm.size(), DisposeAfterUse::YES,
									Audio::FLAG_16BITS | Audio::FLAG_LITTLE_ENDIAN);
		// Keep ownership even if the mixer has no free channel. Its queue
		// remains valid until we detach the handle and delete it explicitly.
		_mixer->playStream(Audio::Mixer::kSFXSoundType, &_speakerHandle, _speakerStream,
						   -1, Audio::Mixer::kMaxChannelVolume, 0, DisposeAfterUse::NO);
		if (!_mixer->isSoundHandleActive(_speakerHandle)) {
			delete _speakerStream;
			_speakerStream = nullptr;
			return;
		}
		if (_paused)
			_mixer->pauseHandle(_speakerHandle, true);
	} else {
		_speakerStream->queueBuffer(samples, pcm.size(), DisposeAfterUse::YES,
									Audio::FLAG_16BITS | Audio::FLAG_LITTLE_ENDIAN);
	}
}

void WizardryEngine::stopSound() {
	if (_mixer)
		_mixer->stopHandle(_speakerHandle);
	delete _speakerStream;
	_speakerStream = nullptr;
	_soundActive = false;
}

void WizardryEngine::present() {
	const PCodeConsole &console = _machine.console();
	const uint16 width = console.textOutput() ? 640 : 320;
	if (_screenWidth != width) {
		initGraphics(width, 200);
		_screenWidth = width;
		const byte foreground = console.textOutput() ? 170 : 255;
		const byte palette[] = {0, 0, 0, 0, 170, 170, 170, 0, 0, foreground, foreground, foreground};
		_system->getPaletteManager()->setPalette(palette, 0, 4);
	}
	if (console.textOutput()) {
		console.renderText(_pixels, width, Graphics::DosFont::fontData_PCBIOS);
	} else {
		const byte *vram = console.vram();
		for (uint y = 0; y < 200; ++y) {
			const uint row = (y & 1) * 0x2000 + (y >> 1) * 80;
			for (uint x = 0; x < 320; ++x)
				_pixels[y * 320 + x] = (vram[row + (x >> 2)] >> (6 - 2 * (x & 3))) & 3;
		}
	}
	_system->copyRectToScreen(_pixels, width, 0, 0, width, 200);
	_system->updateScreen();
}

} // namespace Wizardry
