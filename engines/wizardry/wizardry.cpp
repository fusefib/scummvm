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
#include "common/endian.h"
#include "common/events.h"
#include "common/file.h"
#include "common/memstream.h"
#include "common/savefile.h"
#include "common/system.h"
#include "engines/metaengine.h"
#include "engines/util.h"
#include "graphics/cursorman.h"
#include "graphics/fonts/dosfont.h"
#include "graphics/paletteman.h"
#include "gui/message.h"
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
	delete _campaign;
}

bool WizardryEngine::hasFeature(EngineFeature feature) const {
	return feature == kSupportsReturnToLauncher || feature == kSupportsSavingDuringRuntime ||
		   feature == kSupportsLoadingDuringRuntime;
}

bool WizardryEngine::canSaveGameStateCurrently(Common::U32String *) {
	return _ready && !_transferDialogOpen && _scenarioBackup.empty() && _machine.canSuspend();
}
bool WizardryEngine::canLoadGameStateCurrently(Common::U32String *) {
	return _ready && !_transferDialogOpen && !_machine.cancellingTransfer();
}

bool WizardryEngine::canSaveAutosaveCurrently() {
	return canSaveGameStateCurrently() && _machine.instructions() && !_loadingAutosave;
}

Common::String WizardryEngine::getSaveStateName(int slot) const {
	return getMetaEngine()->getSavegameFile(slot, _targetName.c_str());
}

Common::Error WizardryEngine::loadGameState(int slot) {
	// The framework normally autosaves before loading. Preserve the very
	// autosave being selected, and never autosave an unstarted machine.
	const bool previous = _loadingAutosave;
	_loadingAutosave = slot == getAutosaveSlot();
	Common::Error result = Engine::loadGameState(slot);
	_loadingAutosave = previous;
	return result;
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
	const bool loadingSave = ConfMan.hasKey("save_slot");
	const bool useOriginalSave = !loadingSave && (_description->gameNumber >= 4 ||
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
	if (!loadingSave && _description->gameNumber >= 4) {
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
	if (loadingSave) {
		Common::Error result = loadGameState(ConfMan.getInt("save_slot"));
		if (result.getCode() != Common::kNoError)
			return result;
	} else if (!configureCampaign(message)) {
		warning("Wizardry: campaign initialization failed: %s", message.c_str());
		return Common::Error(Common::kReadingFailed, message);
	}
	debug(1, "Wizardry: memory policy %s, disk policy %u",
		  _machine.originalMemory() ? "original" : "compatibility", unsigned(_machine.diskPolicy()));
	PCodeMachine::State previousState = PCodeMachine::kHalted;
	while (!shouldQuit()) {
		processEvents();
		if (shouldQuit())
			break;
		updateTime();
		PCodeMachine::State state = _machine.run(20000);
		if (!_scenarioBackup.empty() && !_machine.atScenarioMenu())
			_scenarioStarted = true;
		if (!_scenarioBackup.empty() && (_scenarioFailed || (_scenarioStarted && _machine.atScenarioMenu()))) {
			restoreScenarioSession();
			state = _machine.state();
			if (!_scenarioError.empty()) {
				campaignWarning(_scenarioError);
				_scenarioError.clear();
			}
		}
		if (!flushCampaign(message) && !_campaignWarned)
			campaignWarning(message);
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
			flushCampaign(message, true);
			return Common::Error(Common::kUnknownError, _machine.error());
		}
		if (state == PCodeMachine::kHalted)
			break;
		if (state != PCodeMachine::kRunning)
			_system->delayMillis(state == PCodeMachine::kPlayingSound ? 1 : 10);
	}
	if (_description->gameNumber >= 1 && _description->gameNumber <= 5 && shouldQuit())
		saveAutosaveIfEnabled();
	stopSound();
	_ready = false;
	if (!flushCampaign(message, true))
		return Common::Error(Common::kWritingFailed, message);
	return Common::kNoError;
}

bool WizardryEngine::configureCampaign(Common::String &error) {
	error.clear();
	delete _campaign;
	_campaign = nullptr;
	_campaignDirty = _campaignWarned = false;
	_campaignRecoveryPending = false;
	_campaignSelectionPending = false;
	_nextCampaignRetry = 0;
	if (_description->gameNumber < 1 || _description->gameNumber > 5)
		return true;
	if (_description->gameNumber >= 4 && _machine.diskPolicy() != PCodeMachine::kResidentCOM)
		return true;
	CampaignStore::Identity identity = campaignIdentity(_machine);
	if (!CampaignStore::readSelectedCampaignId(*_saveFileMan, _targetName, identity, error))
		return false;
	CampaignStore *candidate = new CampaignStore(*_saveFileMan, _targetName, identity);
	Common::Array<byte> master, save;
	TransferDisks transfer = _machine.transferDisks();
	const CampaignStore::LoadResult loaded = candidate->load(master, save, error, &transfer);
	if (loaded == CampaignStore::kEmpty && identity.campaign)
		error = "The selected Wizardry campaign is missing; restore a ScummVM saved game";
	if (loaded == CampaignStore::kError || !error.empty() || (loaded != CampaignStore::kEmpty && (!_machine.loadCampaignImages(master, save, error) || !_machine.loadCampaignTransfers(transfer, error)))) {
		delete candidate;
		return false;
	}
	_campaign = candidate;
	_machine.setCampaignID(identity.campaign);
	_campaignDirty = loaded == CampaignStore::kEmpty || !_machine.transferDisks().empty();
	debug(1, "Wizardry: campaign profile %u, generation %llu, %s", unsigned(identity.profile),
		  static_cast<unsigned long long>(_campaign->generation()),
		  loaded == CampaignStore::kEmpty ? "new" : "restored");
	if (loaded == CampaignStore::kRecovered) {
		campaignWarning("One Wizardry campaign copy was damaged. The last complete copy has been recovered.");
		_campaignWarned = false;
	}
	return true;
}

CampaignStore::Identity WizardryEngine::campaignIdentity(const PCodeMachine &machine) const {
	CampaignStore::Identity identity;
	identity.gameNumber = _description->gameNumber;
	identity.campaign = machine.campaignID();
	identity.profile = machine.diskPolicy() == PCodeMachine::kResidentCOM ? CampaignStore::kPaired : (machine.originalMemory() ? CampaignStore::kOriginal : CampaignStore::kCompatibility);
	identity.masterCRC = machine.sourceDiskCRC();
	identity.saveCRC = machine.sourceSaveDiskCRC();
	identity.masterSize = machine.disk().size();
	identity.saveSize = machine.saveDisk().size();
	return identity;
}

bool WizardryEngine::onStateLoad(const PCodeMachine &machine, Common::String &error) {
	CampaignStore *candidate = new CampaignStore(*_saveFileMan, _targetName, campaignIdentity(machine));
	Common::Array<byte> master, save;
	const CampaignStore::LoadResult loaded = candidate->load(master, save, error);
	const bool pending = machine.transferInProgress();
	if (!pending) {
		const bool saved = loaded == CampaignStore::kError ? candidate->recoverFromSnapshot(machine.disk(), machine.saveDisk(), error, &machine.transferDisks()) : candidate->commit(machine.disk(), machine.saveDisk(), error, &machine.transferDisks());
		if (!saved || !candidate->select(error)) {
			delete candidate;
			return false;
		}
	}
	delete _campaign;
	_campaign = candidate;
	_campaignRecoveryPending = pending && loaded == CampaignStore::kError;
	_campaignSelectionPending = _campaignDirty = pending;
	_campaignWarned = false;
	_nextCampaignRetry = 0;
	_scenarioBackup.clear();
	_scenarioError.clear();
	_scenarioFailed = false;
	_scenarioStarted = false;
	error.clear();
	return true;
}

bool WizardryEngine::preserveScenarioSession(Common::String &error) {
	if (!flushCampaign(error, true))
		return false;
	int slot = -1;
	for (int i = 0; i <= getMetaEngine()->getMaximumSaveSlot(); ++i) {
		if (i != getAutosaveSlot() && !_saveFileMan->exists(getMetaEngine()->getSavegameFile(i, _targetName.c_str()))) {
			slot = i;
			break;
		}
	}
	if (slot < 0) {
		error = "No unused save slot is available to preserve the current campaign";
		return false;
	}
	const Common::Error result = saveGameState(slot, "Before creating a scenario disk");
	if (result.getCode() != Common::kNoError) {
		error = "Could not preserve the current campaign in a save slot";
		return false;
	}
	Common::InSaveFile *input = _saveFileMan->openForLoading(getMetaEngine()->getSavegameFile(slot, _targetName.c_str()));
	byte header[24];
	bool valid = input && input->read(header, sizeof(header)) == sizeof(header) &&
				 !memcmp(header, "WZVM", 4) && READ_LE_UINT16(header + 4) == 13 && READ_LE_UINT16(header + 6) == 1 &&
				 READ_LE_UINT32(header + 8) == _machine.codeFile().checksum() &&
				 READ_LE_UINT32(header + 12) == _machine.sourceDiskCRC();
	const uint32 size = valid ? READ_LE_UINT32(header + 16) : 0;
	valid = valid && size >= 65536 && size <= 6 * 1024 * 1024 && size <= uint64(input->size() - input->pos());
	if (valid) {
		_scenarioBackup.resize(sizeof(header) + size);
		memcpy(_scenarioBackup.data(), header, sizeof(header));
		valid = input->read(_scenarioBackup.data() + sizeof(header), size) == size && !input->err() &&
				wizardryCrc32(_scenarioBackup.data() + sizeof(header), size) == READ_LE_UINT32(header + 20);
	}
	delete input;
	if (!valid) {
		_scenarioBackup.clear();
		error = "The saved previous campaign could not be verified";
		return false;
	}
	_scenarioFailed = false;
	_scenarioStarted = false;
	_scenarioError.clear();
	return true;
}

bool WizardryEngine::onScenarioDiskCreated(const Common::Array<byte> &master,
										   const Common::Array<byte> &save, Common::String &error) {
	CampaignStore::Identity identity = campaignIdentity(_machine);
	CampaignStore *candidate = nullptr;
	bool saved = CampaignStore::findUnusedCampaignId(*_saveFileMan, _targetName, identity, error);
	if (saved) {
		candidate = new CampaignStore(*_saveFileMan, _targetName, identity);
		Common::Array<byte> unusedMaster, unusedSave;
		saved = candidate->load(unusedMaster, unusedSave, error) == CampaignStore::kEmpty &&
				candidate->commit(master, save, error, &_machine.transferDisks()) && candidate->select(error);
	}
	if (!saved) {
		delete candidate;
		_scenarioError = error.empty() ? "The new Wizardry campaign could not be saved" : error;
		_scenarioFailed = true;
		return false;
	}
	delete _campaign;
	_campaign = candidate;
	_campaignDirty = _campaignWarned = _campaignRecoveryPending = _campaignSelectionPending = false;
	_nextCampaignRetry = 0;
	_machine.setCampaignID(identity.campaign);
	_scenarioBackup.clear();
	return true;
}

void WizardryEngine::restoreScenarioSession() {
	Common::String error;
	Common::MemoryReadStream input(_scenarioBackup.data(), _scenarioBackup.size());
	// Rollback restores the verified pre-operation VM; it must not write or
	// reselect anything after the operation's persistence failure.
	_machine.setObserver(nullptr);
	const bool restored = _machine.loadState(input, error);
	_machine.setObserver(this);
	_scenarioBackup.clear();
	_scenarioFailed = false;
	_scenarioStarted = false;
	if (!restored)
		_machine.fail(error);
	_campaignDirty = false;
	stopSound();
	_lastClock = _system->getMillis();
}

bool WizardryEngine::flushCampaign(Common::String &error, bool force) {
	error.clear();
	if (!_campaign || !_campaignDirty)
		return true;
	// Source deletion and destination insertion are one original operation.
	// Even a forced quit/fault flush must not persist only half of that move.
	if (_machine.transferInProgress() || !_scenarioBackup.empty())
		return true;
	const uint32 now = _system->getMillis();
	if (!force && _nextCampaignRetry && int32(now - _nextCampaignRetry) < 0)
		return true;
	bool saved;
	if (_campaignRecoveryPending) {
		saved = _campaign->recoverFromSnapshot(_machine.disk(), _machine.saveDisk(), error,
											   &_machine.transferDisks());
		// The store retains permission to retry a failed recovery write after
		// preserving the old raw copies. Keep recovery pending until verified.
		if (!saved) {
			_nextCampaignRetry = now + 5000;
			return false;
		}
		_campaignRecoveryPending = false;
	} else {
		saved = _campaign->commit(_machine.disk(), _machine.saveDisk(), error, &_machine.transferDisks());
	}
	if (!saved || (_campaignSelectionPending && !_campaign->select(error))) {
		_nextCampaignRetry = now + 5000;
		return false;
	}
	_campaignSelectionPending = false;
	_campaignDirty = _campaignWarned = false;
	_nextCampaignRetry = 0;
	debug(2, "Wizardry: campaign committed, generation %llu",
		  static_cast<unsigned long long>(_campaign->generation()));
	return true;
}

void WizardryEngine::campaignWarning(const Common::String &message) {
	_campaignWarned = true;
	warning("Wizardry: %s", message.c_str());
	PauseToken pause = pauseEngine();
	const Common::U32String text(message);
	GUI::MessageDialog dialog(text);
	dialog.runModal();
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
		if (key == 'M' && _machine.atScenarioMenu() && _scenarioBackup.empty()) {
			Common::String error;
			if (!preserveScenarioSession(error)) {
				campaignWarning(error);
				continue;
			}
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

Common::Error WizardryEngine::loadGameStream(Common::SeekableReadStream *stream) {
	Common::String message;
	if (!canLoadGameStateCurrently() || !_machine.loadState(*stream, message))
		return Common::Error(Common::kReadingFailed, message);
	ConfMan.setBool("original_save_disk", _machine.diskPolicy() == PCodeMachine::kResidentCOM);
	ConfMan.flushToDisk();
	// Hardware-buffered PCM is not serialized. Discard pre-load audio and
	// resume only the pending guest sound, from its saved modeled offset.
	stopSound();
	_machine.setRealtime(true);
	_lastClock = _system->getMillis();
	if (_machine.state() == PCodeMachine::kPlayingSound)
		startSound();
	present();
	return Common::kNoError;
}

Common::Error WizardryEngine::saveGameStream(Common::WriteStream *stream, bool) {
	Common::String message;
	updateTime();
	if (!canSaveGameStateCurrently() || !_machine.saveState(*stream, message))
		return Common::Error(Common::kWritingFailed, message);
	return Common::kNoError;
}

} // namespace Wizardry
