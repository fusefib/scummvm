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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "common/system.h"
#include "common/memstream.h"
#include "common/config-manager.h"
#include "common/textconsole.h"

#include "gob/dataio.h"
#include "gob/gob.h"
#include "gob/util.h"
#include "gob/sound/soundmixer.h"
#include "gob/pregob/mewilo/mewilo.h"

namespace Gob {
namespace Mewilo {

static const uint32 kExternalScratchSize = 32000;

PreGob_Mewilo::PreGob_Mewilo(GobEngine *vm, bool thomsonTO, bool thomsonFD) : PreGob(vm),
	_profile(getMewiloPlatformProfile(mewiloPlatformFromCommon(vm->getPlatform(), thomsonTO))),
	_useThomsonDisk(thomsonFD), _audioBackend(_profile.audioBackend), _sampleAudioSelected(false), _executor(*this),
	_tot(nullptr), _i00(nullptr), _music(nullptr), _pendingKey(0),
	_acceptMousePresses(false), _pauseStart(0), _pausedMillis(0), _pauseDepth(0), _hostFailed(false),
	_normalizeTiming(ConfMan.hasKey("mewilo_presentation_timing") && ConfMan.getBool("mewilo_presentation_timing")),
	_scriptPatchPolicy(!ConfMan.hasKey("mewilo_script_fixes") || ConfMan.getBool("mewilo_script_fixes") ? kMewiloPatchGermanText | kMewiloPatchHenProtection : 0),
	_saveReady(false), _startupLoad(false), _pendingRestore(nullptr), _effectToneUntil(0), _outputMode(kMewiloOriginal) {
	const Common::String output = ConfMan.hasKey("mewilo_output") ? ConfMan.get("mewilo_output") : Common::String();
	if (output == "corrected") _outputMode = kMewiloCorrectedOriginal;
	else if (output == "english") _outputMode = kMewiloEnglish;
	memset(_fontMd5, 0, sizeof(_fontMd5));
	_effectToneUntil = activeMillis();
	// Freeze the optional enhancement independently of the known corrections.
	if (ConfMan.hasKey("mewilo_require_cece_answer") && ConfMan.getBool("mewilo_require_cece_answer"))
		_scriptPatchPolicy |= kMewiloPatchRequireCeceAnswer;
	_music = new SoundMixer(*g_system->getMixer(), Audio::Mixer::kMusicSoundType);
	if (_profile.persistentI00Scratch) {
		_i00Scratch.resize(kExternalScratchSize);
		memset(_i00Scratch.data(), 0, _i00Scratch.size());
	}
}

PreGob_Mewilo::~PreGob_Mewilo() {
	discardRestore();
	stopMewiloAudio();
	delete _music;
	unloadModule();
}

bool PreGob_Mewilo::shouldQuit() const {
	return _vm->shouldQuit();
}

void PreGob_Mewilo::run() {
	initScreen();
	if (!initResources())
		return;
	byte palette[16 * 3];
	buildMewiloDisplayPalette(_profile.platform, palette);
	// Gob's palette fade accepts six-bit staging values, not RGB888.
	for (uint i = 0; i < sizeof(palette); ++i)
		palette[i] >>= 2;
	setPalette(palette, 16);
	if (!loadReplacements())
		return;
	if (!loadFont()) {
		warning("Mewilo: unable to load LETTRES.BIN");
		return;
	}
	_saveReady = buildReleaseIdentity(_releaseIdentity);
	if (!_saveReady) {
		warning("Mewilo: incomplete or changed game resources; startup aborted");
		return;
	}
	Common::String initialModule = _vm->_startTot;
	if (initialModule.hasSuffixIgnoreCase(".TOT"))
		initialModule = initialModule.substr(0, initialModule.size() - 4);
	if (initialModule.empty())
		initialModule = _profile.modulePrefix;
	addCursor();
	installCrossCursor();
	showCursor();
	fadeIn();
	_executor.reset();
	bool ok = true;
	if (ConfMan.hasKey("save_slot") && ConfMan.getInt("save_slot") >= 0) {
		_startupLoad = true;
		ok = _vm->loadGameState(ConfMan.getInt("save_slot")).getCode() == Common::kNoError;
		_startupLoad = false;
	} else {
		ok = executeModule(initialModule);
	}
	while (!shouldQuit()) {
		// The requesting input callback has now unwound. No old module pointer
		// or C++ execution frame can survive the prepared-state commit.
		if (_pendingRestore) {
			commitRestore();
			ok = _executor.resume();
			continue;
		}
		if (!ok) {
			warning("Mewilo: module execution failed");
			break;
		}
		serviceEvents();
		if (shouldQuit())
			break;
		const uint16 selector = (uint16)_executor.variable(0x33).word0;
		if (selector == 254)
			break;
		if (selector == 255) {
			resetForRestart();
			if (!shouldQuit())
				ok = executeModule(initialModule);
		} else {
			const Common::String module = selectorToModule(selector);
			if (module.empty()) {
				warning("Mewilo: invalid module selector %u", selector);
				break;
			}
			ok = executeModule(module);
		}
	}
	stopMewiloAudio();
	removeCursor();
}

void PreGob_Mewilo::resetForRestart() {
	_hostFailed = false;
	discardRestore();
	_presentation.reset();
	stopMewiloAudio();
	_sample.free();
	_musicSample.free();
	unloadModule();
	_savedRects.clear();
	_pendingKey = 0;
	_mousePresses.clear();
	// Ordinary module retirement keeps scratch. A new game clears its pixels.
	if (!_i00Scratch.empty())
		memset(_i00Scratch.data(), 0, _i00Scratch.size());
	_executor.reset();
	_vm->_util->clearKeyBuf();
	_vm->_util->forceMouseUp();
	clearScreen();
}

bool PreGob_Mewilo::initResources() {
	if (!_useThomsonDisk) {
		selectSampleAudio();
		return true;
	}
	if (_profile.platform != kMewiloPlatformThomsonTO) return false;
	if (!_thomsonDisk.empty()) return true; // one immutable media snapshot per session
	Common::SeekableReadStream *disk = _vm->_dataIO->getFile("mewilo.fd");
	Common::String reason;
	const bool ok = disk && _thomsonDisk.load(*disk, reason);
	delete disk;
	if (!ok) warning("Mewilo: cannot open mewilo.fd (%s)", reason.c_str());
	return ok;
}

Common::SeekableReadStream *PreGob_Mewilo::openResource(const Common::String &name) const {
	// Disk sessions resolve exclusively from the owned media snapshot.
	if (_useThomsonDisk)
		return _thomsonDisk.openFile(name);
	Common::SeekableReadStream *stream = _vm->_dataIO->getFile(name);
	return _profile.platform == kMewiloPlatformAmstradCPC ? normalizeMewiloCpcResource(stream) : stream;
}

bool PreGob_Mewilo::loadFont() {
	Common::SeekableReadStream *stream = openResource("LETTRES.BIN");
	const bool ok = stream && _font.load(*stream, _profile.platform == kMewiloPlatformThomsonTO);
	delete stream;
	return ok && _font.computeMD5(_fontMd5) && _replacements.applyGlyphs(_font);
}

PreGob_Mewilo::PreparedModule::PreparedModule() : tot(nullptr), i00(nullptr) {
	memset(bankMd5, 0, sizeof(bankMd5));
}

PreGob_Mewilo::PreparedModule::~PreparedModule() {
	delete tot;
	delete i00;
}

bool PreGob_Mewilo::prepareModule(const Common::String &stem, PreparedModule &out) const {
	Common::String upper = stem;
	upper.toUppercase();
	Common::SeekableReadStream *stream = openResource(upper + ".TOT");
	if (!stream)
		return false;
	// Semantic corrections prepare a private copy of normalized resident bytes.
	// Presentation recipes use local guards separately from these fingerprints.
	Common::Array<byte> script;
	const bool ok = prepareMewiloScript(*stream, upper.c_str(), _profile.platform,
		_vm->_language, _scriptPatchPolicy, script, out.scriptPatchReport);
	delete stream;
	if (!ok)
		return false;
	Common::MemoryReadStream effective(script.data(), script.size());
	out.tot = new MewiloTotData(effective, _profile.platform);
	MewiloGraphStats stats;
	if (!out.tot->validateReachableGraph(stats)) {
		warning("Mewilo: invalid %s.TOT graph", upper.c_str());
		return false;
	}
	if (_replacements.size()) {
		const int index = mewiloModuleIndex(upper, _profile.modulePrefix);
		Common::String reason;
		if (index < 0 || !_replacements.validateModule(*out.tot, index, reason)) {
			warning("Mewilo: incompatible output replacements for %s (%s)", upper.c_str(), reason.c_str());
			return false;
		}
	}
	stream = openResource(upper + ".I00");
	if (stream) {
		out.i00 = new MewiloI00Bank(*stream, _profile.platform == kMewiloPlatformAmstradCPC);
		delete stream;
		if (!out.i00->valid()) {
			delete out.i00;
			out.i00 = nullptr;
		}
	}
	if (out.i00 && !out.i00->computeMD5(out.bankMd5))
		return false;
	if (stats.requiredI00Records && (!out.i00 || out.i00->recordCount() < stats.requiredI00Records)) {
		warning("Mewilo: %s.TOT requires unavailable I00 records", upper.c_str());
		return false;
	}
	// The optional Atari bank is loaded by native code but never drawn.
	stream = _profile.loadOptionalI01 ? openResource(upper + ".I01") : nullptr;
	if (stream) {
		const int64 size = stream->size();
		if (size > 0 && size <= 0x7FFFFFFF) {
			out.i01.resize((uint32)size);
			stream->seek(0);
			if (stream->read(out.i01.data(), out.i01.size()) != out.i01.size()) {
				delete stream;
				return false;
			}
		}
		delete stream;
	}
	out.stem = upper;
	return true;
}

void PreGob_Mewilo::unloadModule() {
	_presentation.reset();
	_executor.detachModule();
	_acceptMousePresses = false;
	_mousePresses.clear();
	delete _tot;
	_tot = nullptr;
	delete _i00;
	_i00 = nullptr;
	_i01.clear();
	_moduleStem.clear();
	_scriptPatchReport = MewiloScriptPatchReport();
}

void PreGob_Mewilo::commitModule(PreparedModule &prepared) {
	unloadModule();
	_tot = prepared.tot;
	prepared.tot = nullptr;
	_i00 = prepared.i00;
	prepared.i00 = nullptr;
	_i01.swap(prepared.i01);
	_moduleStem = prepared.stem;
	_scriptPatchReport = prepared.scriptPatchReport;
}

bool PreGob_Mewilo::loadModule(const Common::String &stem) {
	PreparedModule prepared;
	if (!prepareModule(stem, prepared))
		return false;
	if (_saveReady) {
		const int index = mewiloModuleIndex(prepared.stem, _profile.modulePrefix);
		if (index < 0 || memcmp(prepared.bankMd5, _releaseIdentity.banks[index], 16))
			return false;
		const MewiloScriptPatchReport &expected = _releaseIdentity.scripts[index];
		const MewiloScriptPatchReport &actual = prepared.scriptPatchReport;
		if (strcmp(expected.sourceMd5, actual.sourceMd5) || strcmp(expected.effectiveMd5, actual.effectiveMd5) ||
			expected.sourceSize != actual.sourceSize || expected.effectiveSize != actual.effectiveSize)
			return false;
	}
	commitModule(prepared);
	return true;
}

bool PreGob_Mewilo::executeModule(const Common::String &stem) {
	if (!loadModule(stem))
		return false;
	return _executor.executeRoot(*_tot);
}

Common::String PreGob_Mewilo::selectorToModule(uint16 selector) const {
	int module = -1;
	switch (selector) {
	case 0: case 1: case 2: case 3: module = 0; break;
	case 4: module = 1; break;
	case 5: module = 2; break;
	case 6: module = 4; break;
	case 7: module = 7; break;
	case 8: module = 8; break;
	case 9: module = 5; break;
	case 10: module = 3; break;
	default: return Common::String();
	}
	return Common::String::format("%s%d", _profile.modulePrefix, module);
}

} // End of namespace Mewilo
} // End of namespace Gob
