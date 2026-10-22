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
 *
 * This file is dual-licensed.
 * In addition to the GPLv3 license mentioned above, this code is also
 * licensed under LGPL 2.1. See LICENSES/COPYING.LGPL file for the
 * full text of the license.
 *
 */

#include "gob/pregob/emmanuelle/emmanuelle.h"

#include "common/config-manager.h"
#include "common/events.h"
#include "common/memstream.h"
#include "common/md5.h"
#include "common/rendermode.h"
#include "common/system.h"

#include "graphics/cursorman.h"

#include "gob/console.h"
#include "gob/dataio.h"
#include "gob/gob.h"
#include "gob/sound/sound.h"
#include "gob/util.h"
#include "gob/video.h"

#include "gob/pregob/emmanuelle/atari.h"
#include "gob/pregob/emmanuelle/dos.h"
#include "gob/pregob/emmanuelle/oldixx.h"
#include "gob/pregob/emmanuelle/oldtot.h"
#include "gob/pregob/emmanuelle/runtime.h"

namespace Gob {
namespace Emmanuelle {

static OldTotPlatformTag detectPlatform(const GobEngine *vm) {
	if (vm && vm->getPlatform() == Common::kPlatformAtariST)
		return kOldTotPlatformAtariST;

	return kOldTotPlatformDOS;
}

PreGob_Emmanuelle::PreGob_Emmanuelle(GobEngine *vm,
									 bool inlineLocalGraphics,
									 bool alternateInputGeometry,
									 bool optionalAtariAudio) :
	PreGob(vm), _renderer(vm), _active(vm, 20), _moduleFileStem(),
	_randomState(vm->_rnd.getRandomNumber(0xFFFFFFFFU)),
	_platformTag(detectPlatform(vm)),
	_inlineLocalGraphics(inlineLocalGraphics),
	_alternateInputGeometry(alternateInputGeometry),
	_optionalAtariAudio(optionalAtariAudio),
	_dosInitialFrameCoverage() {
	// Owned scripts, sprite banks, fonts, and execution objects.
	_tot = nullptr;
	_recupTot = nullptr;
	_sharedI01 = nullptr;
	_localI00 = nullptr;
	_scriptVM = nullptr;
	_executor = nullptr;
	_fonts[0] = nullptr;
	_fonts[1] = nullptr;

	_currentModule = kModuleNone;
	_currentUsesLocalGraphics = false;
	_node24Offset = 0;
	for (uint i = 0; i < ARRAYSIZE(_variables); ++i)
		_variables[i] = 0;

	// Remainder from converting host input passes to native timer quanta.
	_schedulerAccumulator = 0;

	// Preserve quanta deferred by the scheduler's callback limit.
	_schedulerDebt = 0;

	// The title's minimum duration starts at its first complete visible frame.
	_initialInitPresentation = false;
	_initialInitComplete = false;
	_initialTitleState = kInitialTitleNone;
	_initialTomahawkVisibleStart = 0;

	// Native DOS draws may need several sprites to complete a frame.
	_dosInitialFrameHasCoverage = false;
	_dosInitialFramePresented = false;

	_enhancedDosFights = false;

	// Cursor ownership is separate from its script-defined appearance.
	_cursorOwned = false;
	_cursorDefined = false;
	_cursorDescriptor = 0;

	_inputContext = kEmmanuelleInputNone;
	_fightActive = false;

	// Menu requests wait for the executor to leave its polling stack.
	_mainMenuActionThisPoll = false;
	_openMainMenuAfterSuspend = false;

	_diagnosticTraceEnabled = false;

	// Playback can continue after a nonblocking sound command returns.
	_audioKind = kOldTotAudioNone;
	_audioSlot = -1;
	_audioFrequency = 0;
	_audioRepeatCount = 0;
	_audioActive = false;
	_audioBlocking = false;

	// Loaded samples outlive individual playback commands.
	_sampleBank = 0;
	_sampleCount = 0;
	_silentAtariAudio = false;

	// -1 means the Atari intro sample sequence is idle.
	_atariIntroPosition = -1;

	_skipIntermediateSequenceNodes = false;
	_colorCycleEnabled = false;
	_colorCycleCounter = 0;

	_renderer.setPlatform(_platformTag);
	if (_platformTag == kOldTotPlatformDOS) {
		_runtimeState.videoMode = dosVideoModeForRenderMode(
			Common::parseRenderMode(ConfMan.get("render_mode")));
		_runtimeState.savedVideoMode = _runtimeState.videoMode;
		_enhancedDosFights =
			ConfMan.getBool("emmanuelle_enhanced_fights");
	}

	if (_vm->_console)
		_vm->_console->registerEmmanuelleDebugger(this);
}

PreGob_Emmanuelle::~PreGob_Emmanuelle() {
	if (_vm->_console)
		_vm->_console->unregisterEmmanuelleDebugger();
	_introSkip.finish();
	setInputContext(kEmmanuelleInputNone);
	stopSampleAudio();
	if (_platformTag == kOldTotPlatformDOS && _vm && _vm->_sound)
		_vm->_sound->speakerOff();
	clearModuleObjects();
	delete _recupTot;
	delete _sharedI01;
	delete _fonts[0];
	delete _fonts[1];
	if (_cursorOwned) {
		hideCursor();
		removeCursor();
	}
}

Common::SeekableReadStream *PreGob_Emmanuelle::openLoose(
	const Common::String &name) const {
	return _vm->_dataIO->getFile(name);
}

OldTotData *PreGob_Emmanuelle::loadScriptResource(
		const Common::String &name, Common::String &reason,
		uint32 *sourceSize, byte *sourceMD5) const {
	Common::SeekableReadStream *stream = openLoose(name);
	if (!stream || stream->size() <= 0 || stream->size() > 0xFFFF) {
		delete stream;
		reason = "script resource is unavailable";
		return nullptr;
	}

	Common::Array<byte> data;
	data.resize((uint32)stream->size());
	if (stream->read(&data[0], data.size()) != data.size()) {
		delete stream;
		reason = "unable to read script resource";
		return nullptr;
	}
	delete stream;

	byte digest[16];
	Common::MemoryReadStream identityStream(&data[0], data.size());
	if (!Common::computeStreamMD5(identityStream, digest)) {
		reason = "unable to identify script resource";
		return nullptr;
	}
	if (sourceSize)
		*sourceSize = data.size();
	if (sourceMD5)
		memcpy(sourceMD5, digest, sizeof(digest));

	Common::MemoryReadStream scriptStream(&data[0], data.size());
	OldTotData *tot = new OldTotData(scriptStream);
	if (!tot->valid()) {
		delete tot;
		reason = "malformed script resource";
		return nullptr;
	}
	reason.clear();
	return tot;
}

bool PreGob_Emmanuelle::shouldQuit() const {
	return _vm->shouldQuit();
}

bool PreGob_Emmanuelle::shouldAbortExecution() const {
	return _vm->shouldQuit();
}

bool PreGob_Emmanuelle::loadRecupAssets() {
	// RECUP stores forty big-endian draw roots at 0x11, after its count byte.
	const char *fileName = "RECUP.TOT";
	Common::String reason;
	_recupTot = loadScriptResource(fileName, reason);
	if (!_recupTot) {
		debugC(1, kDebugFileIO, "Emmanuelle: unable to load %s: %s",
			fileName, reason.c_str());
		return false;
	}
	if (_recupTot->size() < 0x61 ||
		_recupTot->byteAt(0x10) != 40) {
		debugC(1, kDebugFileIO, "Emmanuelle: malformed %s", fileName);
		return false;
	}

	_recupRoots.clear();
	for (uint i = 0; i < 40; ++i) {
		const uint16 offset = _recupTot->uint16BEAt(0x11 + 2 * i);
		if (offset >= _recupTot->size() ||
			_recupTot->byteAt(offset) != 0x41) {
			_recupRoots.clear();
			debugC(1, kDebugFileIO,
				   "Emmanuelle: invalid %s action root %u", fileName, i);
			return false;
		}
		_recupRoots.push_back(offset);
	}
	return true;
}

bool PreGob_Emmanuelle::loadSharedAssets() {
	const char *bankName = "INIT.I01";
	Common::SeekableReadStream *stream = openLoose(bankName);
	if (!stream) {
		debugC(1, kDebugFileIO, "Emmanuelle: %s is missing", bankName);
		return false;
	}

	_sharedI01 = new OldIxxBank(*stream);
	delete stream;
	if (!_sharedI01->valid()) {
		debugC(1, kDebugFileIO, "Emmanuelle: malformed %s", bankName);
		return false;
	}

	const char *fontNames[2] = {"JEUCAR2.LET", "JEUCAR3.LET"};
	for (uint i = 0; i < ARRAYSIZE(fontNames); ++i) {
		stream = openLoose(fontNames[i]);
		if (!stream) {
			debugC(1, kDebugFileIO, "Emmanuelle: %s is missing",
				   fontNames[i]);
			return false;
		}
		_fonts[i] = _renderer.loadLetFont(*stream);
		delete stream;
		if (!_fonts[i]) {
			debugC(1, kDebugFileIO, "Emmanuelle: malformed %s",
				   fontNames[i]);
			return false;
		}
	}

	return loadRecupAssets();
}

void PreGob_Emmanuelle::clearModuleObjects() {
	if (_platformTag == kOldTotPlatformAtariST)
		_renderer.resetAtariModuleState();

	delete _executor;
	_executor = nullptr;
	delete _scriptVM;
	_scriptVM = nullptr;
	delete _localI00;
	_localI00 = nullptr;
	delete _tot;
	_tot = nullptr;
	_renderer.setResourceBanks(nullptr, _sharedI01);
}

bool PreGob_Emmanuelle::loadModule(ModuleId moduleId,
								   bool usesLocalGraphics) {
	const char *stem = moduleFileStem(moduleId);
	if (!stem)
		return false;
	clearModuleObjects();
	hideCursor();
	if (recentersMouseOnModuleLoad(_platformTag))
		_vm->_util->setMousePos(160, 96);
	_moduleFileStem = stem;
	_currentModule = moduleId;
	_currentUsesLocalGraphics = usesLocalGraphics;

	debugC(2, kDebugGameFlow, "Emmanuelle: loading module %s", stem);

	const char *totExtension = ".TOT";
	const char *ixxExtension = ".I00";
	Common::String reason;
	_tot = loadScriptResource(_moduleFileStem + totExtension, reason);
	if (!_tot) {
		debugC(1, kDebugFileIO, "Emmanuelle: unable to load %s%s: %s",
			stem, totExtension, reason.c_str());
		return false;
	}

	if (moduleNeedsLocalGraphicsFile(moduleId, usesLocalGraphics,
									 _inlineLocalGraphics)) {
		Common::SeekableReadStream *stream = openLoose(_moduleFileStem + ixxExtension);
		if (!stream) {
			debugC(1, kDebugFileIO, "Emmanuelle: %s%s is missing", stem,
				   ixxExtension);
			return false;
		}
		_localI00 = new OldIxxBank(*stream);
		delete stream;
		if (!_localI00->valid()) {
			debugC(1, kDebugFileIO, "Emmanuelle: malformed %s%s", stem,
				   ixxExtension);
			return false;
		}
	}

	_renderer.setResourceBanks(_localI00, _sharedI01);
	completeInitialTomahawkHandoff(moduleId);
	if (_vm->shouldQuit())
		return false;
	bool paletteApplied = false;
	if (_platformTag == kOldTotPlatformAtariST) {
			if (_renderer.atariModulePaletteDiffers(_moduleFileStem) &&
				!_renderer.atariBlack()) {
				fadeAtariToBlack();
			}
			paletteApplied = _renderer.applyAtariModulePalette(_moduleFileStem);
		} else
			paletteApplied = _renderer.applyDosPalette(
				_moduleFileStem, _runtimeState.videoMode);
	if (!paletteApplied) {
		debugC(1, kDebugGraphics,
			   "Emmanuelle: no platform palette for module %s", stem);
		return false;
	}

	_scriptVM = new OldTotVM(*_tot, *this);
	_scriptVM->runtimeState() = _runtimeState;
	_scriptVM->loadVariables(_variables, ARRAYSIZE(_variables));
	_executor = new OldTotExecutor(*_tot, *_scriptVM, *this);
	if (_diagnosticTraceEnabled)
		debugC(2, kDebugGameFlow,
			   "EMMA_TRACE event=module_load platform=%u module=%s size=%u",
			   (uint)_platformTag, _moduleFileStem.c_str(), (uint)_tot->size());

	return true;
}

bool PreGob_Emmanuelle::executeLoadedModule() {
	if (!_executor || !_scriptVM)
		return false;

	bool ok = _executor->runRoot();
	while (ok && _executor->suspended() && !_vm->shouldQuit()) {

		ok = _executor->resume();
	}

	_scriptVM->storeVariables(_variables, ARRAYSIZE(_variables));
	_runtimeState = _scriptVM->runtimeState();
	if (_diagnosticTraceEnabled)
		debugC(2, kDebugGameFlow,
			   "EMMA_TRACE event=module_execute_end module=%s ok=%u selector=%u steps=%u reason=%s",
			   _moduleFileStem.c_str(), ok ? 1U : 0U, (uint)_variables[0],
			   (uint)_executor->steps(),
			   oldTotExecutorStopReasonName(_executor->stopReason()));
	if (ok && !_vm->shouldQuit() && _initialInitPresentation &&
		_moduleFileStem.equalsIgnoreCase("INIT"))
		ensureInitialTomahawkHandoff();
	if (ok && !_vm->shouldQuit() && _platformTag != kOldTotPlatformDOS &&
		!_initialInitComplete && _currentModule == kModuleInit) {
		_initialInitComplete = true;
		ok = loadSampleBank(2);
		if (!ok)
			_initialTitleState = kInitialTitleNone;
	}
	if (ok && !_vm->shouldQuit() &&
		_initialInitPresentation &&
		_moduleFileStem.equalsIgnoreCase("INIT"))
		_initialInitPresentation = false;
	return ok;
}

void PreGob_Emmanuelle::unloadModule() {
	clearModuleObjects();
	_mainMenuActionThisPoll = false;
	_openMainMenuAfterSuspend = false;
	_currentModule = kModuleNone;
	_currentUsesLocalGraphics = false;
}

uint16 PreGob_Emmanuelle::randomBounded(uint16 upperExclusive) {
	if (_platformTag == kOldTotPlatformAtariST) {
		return atariStRandomBounded(
			_vm->_rnd.getRandomNumber(0xFFFFFFFFU), upperExclusive);
	}

	return dosRandomBounded(_randomState, upperExclusive);
}

void PreGob_Emmanuelle::schedulerTiming(byte period, bool timingVariant,
										uint16 &reload, uint16 &initialCountdown) {
	if (_platformTag == kOldTotPlatformAtariST) {
		(void)timingVariant;
		getAtariStSchedulerTiming(period, reload, initialCountdown);
		return;
	}

	getDosSchedulerTiming(period, timingVariant, reload, initialCountdown);
}

void PreGob_Emmanuelle::beginIntroFastForward() {
	hideCursor();
	stopSampleAudio();
	if (_platformTag == kOldTotPlatformDOS && _vm->_sound)
		_vm->_sound->speakerOff();
	if (_diagnosticTraceEnabled)
		debugC(2, kDebugGameFlow,
			   "EMMA_TRACE event=intro_skip_begin module=%s",
			   _moduleFileStem.c_str());
}

void PreGob_Emmanuelle::finishIntro() {
	const bool fastForwarding = _introSkip.fastForwarding();
	const bool resolved = !fastForwarding || resolveIntroPresentation();
	_introSkip.finish();
	setInputContext(kEmmanuelleInputNone);
	if (fastForwarding) {
		if (!resolved) {
			if (_platformTag == kOldTotPlatformAtariST)
				servicePresentation();
			presentFrame(false, true);
		}
		discardPendingInput();
	}
	if (_diagnosticTraceEnabled)
		debugC(2, kDebugGameFlow,
			   "EMMA_TRACE event=intro_skip_end module=%s requested=%u",
			   _moduleFileStem.c_str(), fastForwarding ? 1U : 0U);
}

bool PreGob_Emmanuelle::skipIntermediateSequenceNodes() const {
	return _platformTag != kOldTotPlatformDOS && _skipIntermediateSequenceNodes;
}

void PreGob_Emmanuelle::serviceColorCycle() {
	if (_platformTag == kOldTotPlatformDOS)
		return;
	const ColorCycleResult result = advanceColorCycle(
		_colorCycleCounter, _colorCycleEnabled,
		_scriptVM ? _scriptVM->readVariable(0) : 0);
	_colorCycleCounter = result.counter;
	if (result.rotatePalette) {
		if (_platformTag == kOldTotPlatformAtariST)
			_renderer.rotateAtariColorCycleWords();

	}
}

void PreGob_Emmanuelle::serviceWaitColorCycle() {
	if (_platformTag == kOldTotPlatformDOS)
		return;
	_colorCycleCounter = prepareAtariStWaitColorCounter(_colorCycleCounter);
	serviceColorCycle();
}

void PreGob_Emmanuelle::setAudioState(
	OldTotAudioKind kind, int16 slot, uint16 frequency,
	int16 repeatCount, bool blocking, bool active) {
	_audioKind = kind;
	_audioSlot = slot;
	_audioFrequency = frequency;
	_audioRepeatCount = repeatCount;
	_audioBlocking = blocking && active;
	_audioActive = active;
	if (_diagnosticTraceEnabled)
		debugC(3, kDebugSound,
			   "EMMA_TRACE event=audio_%s platform=%u kind=%u slot=%d "
			   "frequency=%u repeat=%d blocking=%u",
			   active ? "start" : "stop", (uint)_platformTag, (uint)kind,
			   (int)slot, (uint)frequency, (int)repeatCount,
			   blocking ? 1U : 0U);
}

bool PreGob_Emmanuelle::loadSampleBank(byte bank) {
	if (_platformTag == kOldTotPlatformDOS ||
		(bank != 1 && bank != 2))
		return false;
	if (_platformTag == kOldTotPlatformAtariST && _silentAtariAudio) {
		_sampleBank = bank;
		_sampleCount = 0;
		return true;
	}

	stopSampleAudio();
	for (uint i = 0; i < ARRAYSIZE(_samples); ++i)
		_vm->_sound->sampleFree(&_samples[i]);

	if (!prepareSampleBank(bank, _samples, _sampleCount)) {
		_sampleBank = 0;
		return false;
	}
	_sampleBank = bank;
	debugC(2, kDebugSound,
		   "Emmanuelle: loaded sample bank %u (%u samples)",
		   bank, _sampleCount);
	return true;
}

const char *const *PreGob_Emmanuelle::sampleBankNames(byte bank, byte &count) const {
	if (bank == 2) {
		static const char *const names[] = {
			"P1.SPL", "P2.SPL", "P3.SPL", "P5.SPL", "P6.SPL", "P7.SPL",
			"P8.SPL", "P9.SPL", "P10.SPL", "P11.SPL", "P12.SPL",
			"P13.SPL", "P14.SPL", "P15.SPL", "P16.SPL", "P17.SPL",
			"P19.SPL", "P20.SPL", "P21.SPL"};
		count = ARRAYSIZE(names);
		return names;
	}
	if (_platformTag == kOldTotPlatformAtariST)
		return atariIntroSampleNames(count);

	count = 0;
	return nullptr;
}

bool PreGob_Emmanuelle::prepareSampleBank(byte bank, SoundDesc *samples,
		byte &count) const {
	byte size;
	const char *const *names = sampleBankNames(bank, size);
	count = 0;
	if (!names)
		return false;
	for (byte i = 0; i < size; ++i) {
		if (!_vm->_sound->sampleLoad(&samples[i], SOUND_SND, names[i])) {
			debugC(1, kDebugSound, "Emmanuelle: failed to load sample %s", names[i]);
			return false;
		}
	}
	count = size;
	return true;
}

void PreGob_Emmanuelle::stopSampleAudio() {
	if (_platformTag == kOldTotPlatformDOS || !_vm || !_vm->_sound)
		return;
	_atariIntroPosition = -1;
	_vm->_sound->blasterStop(0);
	if (_audioActive)
		setAudioState(_audioKind, _audioSlot, 0,
						   _audioRepeatCount, _audioBlocking, false);
}

void PreGob_Emmanuelle::dispatchScheduled(uint16 target) {
	if (_executor)
		_executor->dispatchScheduled(target);
}

void PreGob_Emmanuelle::runFight(
	uint16 leftMeterBonus, uint16 rightMeterBonus, uint16 leftDamageOption,
	uint16 rightDamageOption, uint16 leftBaseMeter, uint16 rightBaseMeter) {
	if (_platformTag == kOldTotPlatformAtariST)
		_skipIntermediateSequenceNodes = false;
	if (!_recupTot || _recupRoots.size() != 40 || !_scriptVM)
		return;

	_fightActive = true;
	setInputContext(kEmmanuelleInputFight);
	FightState state;
	initFightState(state, leftMeterBonus, rightMeterBonus, leftDamageOption, rightDamageOption,
						 leftBaseMeter, rightBaseMeter);
	const uint32 fightStart = g_system->getMillis();
	if (_diagnosticTraceEnabled)
		debugC(2, kDebugGameFlow,
			   "EMMA_TRACE event=fight_start platform=%u module=%s "
			   "meter_a=%d meter_b=%d option_a=%u option_b=%u",
			   (uint)_platformTag, _moduleFileStem.c_str(), (int)state.leftMeter,
			   (int)state.rightMeter, (uint)leftDamageOption, (uint)rightDamageOption);
	renderFightActors(state);
	presentFrame(true);

	if (_platformTag == kOldTotPlatformDOS && !_vm->shouldQuit()) {
		const uint16 initialHold = kDosFightInitialHoldMilliseconds;
		delay(initialHold);
	}
	processGameEvents();
	FightInputState fightInput(_platformTag == kOldTotPlatformDOS);
	byte result = 0;
	while (!state.ended && !_vm->shouldQuit()) {
		const uint32 tickStart = g_system->getMillis();
		fightInput.beginTick();
		int16 mouseX = 0;
		int16 mouseY = 0;
		MouseButtons mouseButtons = kMouseButtonsNone;
		int16 key = takePendingInput(mouseX, mouseY, mouseButtons);
		if (key >= 'a' && key <= 'z')
			key = (int16)(key - 'a' + 'A');
		fightInput.noteKey(key);
		if (state.left.frame >=
			fightFrameMax(false, state.left.action)) {
			if (fightInput.automatic()) {
				chooseFightLeftAI(
					state, *this,
					(uint16)(6 + 3 * _scriptVM->readVariable(50)),
					_platformTag == kOldTotPlatformDOS);
			} else
				setFightLeftInput(state, fightInput.takeActionKey());
		}
		if (state.right.frame >=
			fightFrameMax(true, state.right.action))
			chooseFightRightAI(state, *this, 4);

		advanceFightFrame(state);
		renderFightActors(state);

		if (state.pendingLeftDamage != 0)
			playDosFightTone(440);
		if (state.pendingRightDamage != 0 && !_vm->shouldQuit())
			playDosFightTone(1000);
		if (!_vm->shouldQuit())
			serviceAtariFightFrame(state);

		if (_vm->shouldQuit())
			break;
		result = applyFightDamage(state);
		presentFrame(true);
		processGameEvents();
		if (_platformTag == kOldTotPlatformDOS && !_vm->shouldQuit())
			waitForMinimumDuration(
				tickStart, kDosFightTickMinimumMilliseconds);
		if (result)
			break;
	}
	if (result && !_vm->shouldQuit()) {
		if (result == 4)
			startAtariFightConclusionAudio();
		for (byte frame = 1; frame <= 9 && !_vm->shouldQuit(); ++frame) {
			setFightConclusionFrame(
				state, result, frame, (byte)(fightRandom(8) + 1));
			renderFightActors(state);

			delay(100);
			if (_vm->shouldQuit())
				break;
			presentFrame(true);
			processGameEvents();
		}
		if (result == 4 || _platformTag == kOldTotPlatformAmiga)
			stopSampleAudio();
		if (!_vm->shouldQuit()) {
			const uint16 terminalDelay =
				2000;
			delay(terminalDelay);
		}
		if (!_vm->shouldQuit())
			_scriptVM->writeVariable(39, result);
	}
	_active.restore(0);
	_active.restore(1);
	if (_platformTag == kOldTotPlatformDOS && !_vm->shouldQuit()) {
		OldTotRuntimeState &runtimeState = _scriptVM->runtimeState();
		runtimeState.videoMode = runtimeState.savedVideoMode;
		runtimeState.videoInitMarker = 0;
		reinitializeVideoBackend(runtimeState.videoMode);
		runtimeState.videoInitMarker = 0x28;
		presentFrame(false);
	}
	setInputContext(kEmmanuelleInputNone);
	_fightActive = false;
	if (_diagnosticTraceEnabled)
		debugC(2, kDebugGameFlow,
			   "EMMA_TRACE event=fight_end platform=%u "
			   "elapsed_ms=%u result=%u quit=%u",
			   (uint)_platformTag,
			   (uint)(g_system->getMillis() - fightStart), (uint)result,
			   _vm->shouldQuit() ? 1U : 0U);
}

bool PreGob_Emmanuelle::handleNativeFF0B(
	byte command, OldTotRuntimeState &runtimeState) {

	if (_platformTag == kOldTotPlatformAtariST)
		return handleAtariFF0B(command);

	bool reinitialize = false;
	if (!applyDosNativeCommand(command, runtimeState, reinitialize,
							   !_enhancedDosFights))
		return false;
	if (reinitialize) {
		reinitializeVideoBackend(runtimeState.videoMode);
		runtimeState.videoInitMarker = 0x28;
	}
	return true;
}

void PreGob_Emmanuelle::exportDiagnosticSnapshot(
	OldTotDiagnosticSnapshot &out) const {
	out = OldTotDiagnosticSnapshot();
	out.platform = _platformTag;
	out.module = _moduleFileStem;
	out.input.key = _pendingInput.peek(out.input.mouseX, out.input.mouseY,
		out.input.mouseButtons);
	out.cursorVisible = CursorMan.isVisible();
	out.saveBoundary = saveBoundaryState();
	out.traceEnabled = _diagnosticTraceEnabled;
	out.schedulerAccumulator = _schedulerAccumulator;
	out.schedulerDebt = _schedulerDebt;
	out.variables.resize(ARRAYSIZE(_variables));
	if (_scriptVM) {
		out.hasVM = true;
		_scriptVM->exportTransientState(out.vm);
		_scriptVM->storeVariables(out.variables.data(), out.variables.size());
	} else {
		for (uint i = 0; i < ARRAYSIZE(_variables); ++i)
			out.variables[i] = _variables[i];
	}
	if (_executor) {
		out.hasExecutor = true;
		_executor->exportState(out.executor);
	}
	// Selector resolution updates variables, so use a copy of the snapshot.
	uint16 policyVariables[ARRAYSIZE(_variables)];
	out.selector = out.variables[0];
	for (uint i = 0; i < out.variables.size(); ++i)
		policyVariables[i] = out.variables[i];
	out.postPolicySelector = adjustModuleSelector(
		policyVariables, ARRAYSIZE(policyVariables));
	const ModuleTransition transition =
		moduleTransition(out.postPolicySelector);
	out.nextModule = moduleFileStem(transition.module);

	out.native.audioKind = _audioKind;
	out.native.audioSlot = _audioSlot;
	out.native.audioFrequency = _audioFrequency;
	out.native.audioRepeatCount = _audioRepeatCount;
	out.native.audioActive = isAudioPlaying(_platformTag, _audioActive,
		_vm->_sound->blasterPlayingSound());
	out.native.audioBlocking = _audioBlocking && out.native.audioActive;
	out.native.sampleBank = _sampleBank;
	out.native.sampleCount = _sampleCount;
	out.native.initialInitComplete = _initialInitComplete;
	out.native.skipIntermediateSequenceNodes = _skipIntermediateSequenceNodes;
	out.native.colorCycleEnabled = _colorCycleEnabled;
	out.native.colorCounter = _colorCycleCounter;
	out.native.activeObjectCount = _active.activeCount();
	out.native.savedRegionDepth = _renderer.savedRegionDepth();
	if (_platformTag == kOldTotPlatformAtariST) {
		out.native.presentationMode = _renderer.presentationMode();
		out.native.presentationVariant = _renderer.presentationVariant();
		out.native.presentationPhase = _renderer.presentationPhase();
		out.native.presentationDirty = _renderer.presentationPending();
		out.native.presentationActive = _renderer.presentationActive();
		out.native.paletteBlack = _renderer.atariBlack();
	}
}

bool PreGob_Emmanuelle::queryProtectionHint(
	ProtectionChallenge &out) const {
	uint16 node = 0;
	if (!_tot || !_scriptVM || !_executor ||
		!_executor->activeInputNode(node) || uint32(node) + 6U > _tot->size())
		return false;

	uint16 variables[7];
	for (uint i = 0; i < ARRAYSIZE(variables); ++i)
		variables[i] = _scriptVM->readVariable(i);
	return buildProtectionHint(_moduleFileStem.equalsIgnoreCase("INIT"), true,
		_tot->byteAt(node), _tot->byteAt(node + 5), variables, out);
}

void PreGob_Emmanuelle::setDiagnosticTraceEnabled(bool enabled) {
	_diagnosticTraceEnabled = enabled;
	debugC(1, kDebugGameFlow, "Emmanuelle: diagnostic trace %s",
		   enabled ? "enabled" : "disabled");

}

void PreGob_Emmanuelle::playDosFightTone(uint16 frequency) {
	if (_platformTag != kOldTotPlatformDOS || !_vm || !_vm->_sound)
		return;

	setAudioState(kOldTotAudioDosSpeaker, -1, frequency, 0,
						 true, true);
	_vm->_sound->speakerOn((int16)frequency, -1);
	// Pump events without advancing script timers while the tone plays.
	delay(10);
	_vm->_sound->speakerOff();
	setAudioState(kOldTotAudioDosSpeaker, -1, frequency, 0,
						 true, false);
}

static const char *loopResultDescription(ModuleLoopResult result) {
	switch (result) {
	case kModuleLoopExit:
		return "normal exit";
	case kModuleLoopUnmappedSelector:
		return "unmapped module selector";
	case kModuleLoopLoadError:
		return "resource load error";
	case kModuleLoopExecutionError:
		return "interpreter error";
	case kModuleLoopQuit:
		return "quit requested";
	case kModuleLoopLimit:
		return "module iteration limit";
	default:
		return "unknown result";
	}
}

void PreGob_Emmanuelle::run() {
	debugC(1, kDebugGameFlow,
		   "Emmanuelle: starting %s old-TOT runtime",
		   _platformTag == kOldTotPlatformAmiga
			   ? "Amiga"
			   : (_platformTag == kOldTotPlatformAtariST ? "Atari ST"
														 : "DOS"));
	if (_platformTag != kOldTotPlatformDOS &&
		_platformTag != kOldTotPlatformAtariST &&
		_platformTag != kOldTotPlatformAmiga) {
		debugC(1, kDebugGameFlow,
			   "Emmanuelle: platform frontend is not implemented yet");
		return;
	}
	initScreen();
	addCursor();
	_cursorOwned = true;
	setInputContext(kEmmanuelleInputNone);
	hideCursor();
	_vm->_util->setFrameRate(kHostFrameRate);
	if (_platformTag == kOldTotPlatformAtariST &&
		atariStAudioPolicy(_optionalAtariAudio,
						   hasAnyAtariSoundFile()) == kAtariStAudioSilent) {
		_silentAtariAudio = true;
		warning("Emmanuelle: running the loader-free Atari ST profile "
				"without sound samples");
	}
	bool ready = loadSharedAssets();
	if (ready && _platformTag != kOldTotPlatformDOS)
		ready = loadSampleBank(1);
	if (ready) {
		_variables[0] = 0;
		_initialTitleState = kInitialTitleNone;
		_initialTomahawkVisibleStart = 0;
		_introSkip.begin();
		_initialInitPresentation = true;
		_dosInitialFrameCoverage = Common::Rect();
		_dosInitialFrameHasCoverage = false;
		_dosInitialFramePresented = false;
		setInputContext(kEmmanuelleInputIntro);
		const ModuleLoopResult result = runModuleLoop(
			*this, _variables, ARRAYSIZE(_variables));
		debugC(1, kDebugGameFlow, "Emmanuelle: runtime stopped: %s",
			   loopResultDescription(result));
		_active.restoreAll();
		clearModuleObjects();
	}
	hideCursor();
	_introSkip.finish();
	_mainMenuActionThisPoll = false;
	_openMainMenuAfterSuspend = false;
	setInputContext(kEmmanuelleInputNone);
	removeCursor();
	_cursorOwned = false;
}

} // End of namespace Emmanuelle
} // End of namespace Gob
