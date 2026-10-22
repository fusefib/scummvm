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

#include "gob/pregob/emmanuelle/dos.h"
#include "gob/pregob/emmanuelle/oldixx.h"
#include "gob/pregob/emmanuelle/oldtot.h"
#include "gob/pregob/emmanuelle/runtime.h"

namespace Gob {
namespace Emmanuelle {

namespace {

OldTotPlatformTag detectPlatform(const GobEngine *vm) {

	return kOldTotPlatformDOS;
}

} // End of anonymous namespace

PreGob_Emmanuelle::PreGob_Emmanuelle(GobEngine *vm,
									 bool optionalAtariAudio) :
	PreGob(vm), _renderer(vm), _active(vm, 20), _moduleFileStem(),
	_randomState(vm->_rnd.getRandomNumber(0xFFFFFFFFU)),
	_platformTag(detectPlatform(vm)),
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
	_runtimeState.videoMode = dosVideoModeForRenderMode(
		Common::parseRenderMode(ConfMan.get("render_mode")));
	_runtimeState.savedVideoMode = _runtimeState.videoMode;
	_enhancedDosFights =
		ConfMan.getBool("emmanuelle_enhanced_fights");

}

PreGob_Emmanuelle::~PreGob_Emmanuelle() {
	_introSkip.finish();
	setInputContext(kEmmanuelleInputNone);

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
	const char *fileName = _platformTag == kOldTotPlatformAmiga
								   ? "RECUP.TOA"
								   : "RECUP.TOT";
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
	const char *bankName = _platformTag == kOldTotPlatformAmiga
							   ? "INIT.IA1"
							   : "INIT.I01";
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

	const char *totExtension = _platformTag == kOldTotPlatformAmiga
								   ? ".TOA"
								   : ".TOT";
	const char *ixxExtension = _platformTag == kOldTotPlatformAmiga
								   ? ".IA0"
								   : ".I00";
	Common::String reason;
	_tot = loadScriptResource(_moduleFileStem + totExtension, reason);
	if (!_tot) {
		debugC(1, kDebugFileIO, "Emmanuelle: unable to load %s%s: %s",
			stem, totExtension, reason.c_str());
		return false;
	}

	if (moduleNeedsLocalGraphicsFile(moduleId, usesLocalGraphics,
									 false)) {
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
	if (_vm->shouldQuit())
		return false;
	bool paletteApplied = false;
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

bool PreGob_Emmanuelle::applyInitialModuleSelectorPolicy(
		ModuleId moduleId, uint16 &selector) {
	return false;
}

uint16 PreGob_Emmanuelle::randomBounded(uint16 upperExclusive) {

	return dosRandomBounded(_randomState, upperExclusive);
}

void PreGob_Emmanuelle::schedulerTiming(byte period, bool timingVariant,
										uint16 &reload, uint16 &initialCountdown) {

	getDosSchedulerTiming(period, timingVariant, reload, initialCountdown);
}

bool PreGob_Emmanuelle::skipIntermediateSequenceNodes() const {
	return _platformTag != kOldTotPlatformDOS && _skipIntermediateSequenceNodes;
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

void PreGob_Emmanuelle::dispatchScheduled(uint16 target) {
	if (_executor)
		_executor->dispatchScheduled(target);
}

bool PreGob_Emmanuelle::handleNativeFF0B(
	byte command, OldTotRuntimeState &runtimeState) {

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

} // End of namespace Emmanuelle
} // End of namespace Gob
