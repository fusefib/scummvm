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

#include "backends/keymapper/keymapper.h"
#include "common/config-manager.h"
#include "common/events.h"
#include "common/language.h"
#include "common/text-to-speech.h"
#include "graphics/cursorman.h"
#include "gob/gob.h"
#include "gob/global.h"
#include "gob/sound/sound.h"
#include "gob/util.h"
#include "gob/pregob/emmanuelle/amiga.h"
#include "gob/pregob/emmanuelle/atari.h"
#include "gob/pregob/emmanuelle/dos.h"
#include "gob/pregob/emmanuelle/text.h"

namespace Gob {
namespace Emmanuelle {

static Common::TextToSpeechManager *getNarrationManager() {
#ifdef USE_TTS
	return g_system->getTextToSpeechManager();
#else
	return nullptr;
#endif
}

bool PreGob_Emmanuelle::narrationEnabled() const {
	Common::TextToSpeechManager *manager = getNarrationManager();
	return manager && ConfMan.hasKey("tts_enabled") && ConfMan.getBool("tts_enabled") &&
		(manager->isReady() || manager->isSpeaking() || manager->isPaused());
}

bool PreGob_Emmanuelle::narrationPlaying() {
	if (!_narrationOwned)
		return false;
	if (!narrationEnabled()) {
		clearNarration();
		return false;
	}
	Common::TextToSpeechManager *manager = getNarrationManager();
	if (!manager->isSpeaking() && !manager->isPaused())
		continueNarration();
	return _narrationOwned;
}

void PreGob_Emmanuelle::setNarrationLanguage() {
	Common::TextToSpeechManager *manager = getNarrationManager();
	const char *language = Common::getLanguageCode(_vm->_language);
	if (manager && language && manager->getLanguage() != Common::String(language, 2))
		manager->setLanguage(language);
}

void PreGob_Emmanuelle::clearNarration(const Common::Rect *removed) {
	if (removed) {
		for (uint i = 0; i < _pendingNarration.size();) {
			if (removed->intersects(_pendingNarration[i].rect))
				_pendingNarration.remove_at(i);
			else
				++i;
		}
		// The gaps between spoken text boxes do not belong to the utterance.
		bool overlaps = false;
		for (uint i = 0; i < _narration.size(); ++i)
			overlaps |= removed->intersects(_narration[i].rect);
		if (!overlaps)
			return;
	} else {
		_pendingNarration.clear();
	}
	Common::TextToSpeechManager *manager = getNarrationManager();
	if (_narrationOwned && manager)
		manager->stop();
	if (_narratorVoice >= 0 && manager)
		manager->setVoice(_narratorVoice);
	_narratorVoice = -1;
	_narration.clear();
	_narrationNext = 0;
	_narrationOwned = false;
	_narrationCanContinue = false;
	if (_narrationTimerHeld)
		_schedulerClockRunning = false;
	_narrationTimerHeld = false;
}

uint16 PreGob_Emmanuelle::narrationSpeaker(const OldTotTextPlan &plan) const {
	if (!_scriptVM || !_executor || _currentModule == kModuleInit)
		return kNarrator;
	uint16 owner = _narrationCallback;
	if ((!owner && !_executor->sequenceOwner(owner)) || uint32(owner) + 2 > _tot->size())
		return kNarrator;
	if (_narrationCallback && _tot->byteAt(owner) != 0x1C && _tot->byteAt(owner) != 0x1E)
		return kNarrator;
	bool playerTopic = false, playerReply = false, displayedTopic = false;
	bool mario = false, silvinho = false, reveal = false, boatOwner = false, encounterReply = false;
	uint32 cursor = uint32(owner) + 2;
	for (uint i = 0; i < _tot->byteAt(owner + 1); ++i) {
		if (cursor + 2 > _tot->size())
			return kNarrator;
		if (_tot->byteAt(cursor) != 0xFF) {
			cursor += 2;
			continue;
		}
		OldTotInline instruction;
		if (cursor > 0xFFFF || !_tot->decodeInline(cursor, instruction))
			return kNarrator;
		// Topics copy a score from v80..89 to v90. Intimacy choices set v97.
		// Inspect the complete caller: these assignments can follow its text.
		playerTopic |= instruction.subOpcode == 6 && instruction.words[0] == 90 &&
			instruction.words[1] >= 80 && instruction.words[1] <= 89;
		playerReply |= instruction.subOpcode == 5 && instruction.words[0] == 97 &&
			instruction.words[1] >= 1 && instruction.words[1] <= 5;
		displayedTopic |= instruction.subOpcode == 5 && instruction.words[0] == 93 &&
			instruction.words[1] >= 1 && instruction.words[1] <= 10;
		mario |= instruction.subOpcode == 5 && instruction.words[0] == 32 && instruction.words[1] == 1;
		silvinho |= instruction.subOpcode == 5 && instruction.words[0] == 50 && instruction.words[1] == 1;
		reveal |= instruction.subOpcode == 5 && instruction.words[0] == 57 && instruction.words[1] == 2;
		boatOwner |= instruction.subOpcode == 5 &&
			((instruction.words[0] == 47 && instruction.words[1] == 1) ||
			 (instruction.words[0] == 31 && instruction.words[1] == 0));
		encounterReply |= (instruction.subOpcode == 7 && instruction.words[0] == 92) ||
			(instruction.subOpcode == 5 && instruction.words[0] == 13 && instruction.words[1] != 0) ||
			(_currentModule == kModuleLoadGar && instruction.subOpcode == 5 && instruction.words[0] == 19);
		cursor += instruction.size;
	}
	if (playerTopic || playerReply || (_narrationCallback && displayedTopic && plan.x == 40 && plan.y == 5))
		return kPlayer;
	if (_currentModule == kModuleLoadAero && plan.x == 184 && plan.y == 146 && plan.width == 128)
		return kPlayer;
	if (_currentModule == kModuleLoadHot && mario)
		return kMario;
	if (_currentModule == kModuleLoadHot && silvinho)
		return kSilvinho;
	// The airport referral calls her a female friend. Her beach introduction
	// sets v47; her later aphrodisiac dialogue consumes v31, unlike the contact
	// who gives the player that bottle. Travel labels retain the narrator.
	if (_currentModule == kModuleLoadPla && boatOwner && plan.x == 40 && plan.y == 5)
		return kBoatOwner;
	// The helicopter conversation has no character ID. Hotel view 5 owns
	// Samantha's replies; its scheduler displays the player's three topics.
	if (_currentModule == kModuleLoadHot && _scriptVM->readVariable(17) == 5 &&
		plan.x == 40 && plan.y == 5 && plan.width == 256 && plan.height == 32)
		return displayedTopic ? kPlayer : kSamantha;
	const uint16 character = _scriptVM->readVariable(13);
	if (_currentModule == kModuleLoadAero && character == 18 && reveal)
		return plan.y == 140 ? kRevealedCompanion : kPlayer;
	// Wide captions also serve other people in the same room. Only the
	// encounter's speaker, score or rebuff callers establish an active reply.
	if (plan.x == 40 && plan.y == 5 && !encounterReply)
		return kNarrator;
	// These are the romantic interactions, not every use of v13. Clerks,
	// prices, travel labels and unrelated retained IDs keep the narrator.
	if (!((plan.x == 40 && plan.y == 5 && plan.width == 256 && plan.height == 32) ||
		(plan.x == 128 && plan.y == 111 && plan.width == 72) ||
		(plan.x == 104 && plan.y == 19 && plan.width == 128)))
		return kNarrator;
	static const struct {
		ModuleId module;
		byte character;
	} companions[] = {
		{kModuleLoadAero, 1}, {kModuleLoadAero, 2}, // Rio and Iguacu airports
		{kModuleLoadAero, 3}, {kModuleLoadAero, 4}, // Salvador and Manaus airports
		{kModuleLoadAero, 13}, {kModuleLoadAero, 18}, {kModuleLoadAero, 19}, // Carnival
		{kModuleLoadCas, 17},
		{kModuleLoadGar, 5}, {kModuleLoadGar, 14}, {kModuleLoadGar, 20},
		{kModuleLoadHot, 14}, {kModuleLoadHot, 15}, {kModuleLoadHot, 16},
		{kModuleLoadPla, 8}, {kModuleLoadPla, 12}
	};
	for (uint i = 0; i < ARRAYSIZE(companions); ++i) {
		if (companions[i].module == _currentModule && companions[i].character == character) {
			// The carnival encounter presents as female until the explicit reveal.
			if (_currentModule == kModuleLoadAero && character == 18 && _scriptVM->readVariable(57) == 2)
				return kRevealedCompanion;
			return kFirstCompanion + _currentModule * 32 + character;
		}
	}
	return kNarrator;
}

static Common::U32String speechFromTextRows(const OldTotTextPlan &plan, int first, int end) {
	Common::String text;
	int previousRow = -1;
	for (uint i = 0; i < plan.glyphs.size(); ++i) {
		const OldTotTextGlyph &glyph = plan.glyphs[i];
		if (glyph.cellY < first || glyph.cellY >= end)
			continue;
		if (previousRow != glyph.cellY && !text.empty())
			text += ' ';
		text += (char)glyph.character;
		previousRow = glyph.cellY;
	}
	text.trim();
	return Common::U32String(text, Common::kDos850);
}

void PreGob_Emmanuelle::stageNarration(const OldTotTextPlan &plan,
		const Common::U32String &text) {
	// The narrow cells at the left are cash and meter redraws. Numeric
	// choices and the casino's number overlay must still be read.
	if (plan.x < 40 && plan.width <= 24)
		return;
	const Common::Rect rect(plan.x, plan.y, plan.x + plan.width, plan.y + plan.height);
	if (text.empty()) {
		clearNarration(&rect);
		return;
	}
	Common::U32String spoken = text;
	if (_currentModule == kModuleInit) {
		// Both questions share the visible prefix on the instructions' last
		// row. The second redraw contains only its changing letter and number.
		if (plan.x == 0 && plan.y == 0 && plan.height == 200)
			_protectionQuestion = speechFromTextRows(plan, 128, 136);
		else if (plan.y == 160 && _pendingNarration.empty() && !_protectionQuestion.empty()) {
			spoken = _protectionQuestion;
			spoken += '\n';
			spoken += text;
		}
	}
	const uint16 speaker = narrationSpeaker(plan);
	if (speaker == kMario && plan.x == 40 && plan.y == 52 && plan.height == 96) {
		// Mario explains the rules below a separate, visible title. Keep that
		// heading with the narrator rather than reading it in his voice.
		const Common::U32String heading = speechFromTextRows(plan, 52, 60);
		if (!heading.empty() && spoken.substr(0, heading.size()) == heading) {
			NarrationText title;
			title.rect = Common::Rect(plan.x, plan.y, plan.x + plan.width, plan.y + 8);
			title.text = heading;
			title.speaker = kNarrator;
			title.voice = -1;
			uint index = 0;
			while (index < _pendingNarration.size() && _pendingNarration[index].rect != title.rect)
				++index;
			if (index == _pendingNarration.size())
				_pendingNarration.push_back(title);
			else
				_pendingNarration[index] = title;
			uint start = heading.size();
			while (start < spoken.size() && spoken[start] == ' ')
				++start;
			spoken = spoken.substr(start);
		}
	}
	for (uint i = 0; i < _pendingNarration.size(); ++i) {
		if (_pendingNarration[i].rect == rect) {
			_pendingNarration[i].text = spoken;
			_pendingNarration[i].speaker = speaker;
			return;
		}
	}
	NarrationText part;
	part.rect = rect;
	part.text = spoken;
	part.speaker = speaker;
	part.voice = -1;
	_pendingNarration.push_back(part);
}

void PreGob_Emmanuelle::speakPendingNarration() {
	if (_pendingNarration.empty())
		return;
	if (!narrationEnabled() || shouldAbortExecution() || _introSkip.fastForwarding()) {
		clearNarration();
		return;
	}
	bool same = _pendingNarration.size() == _narration.size();
	for (uint i = 0; same && i < _pendingNarration.size(); ++i)
		same = _pendingNarration[i].text == _narration[i].text &&
			_pendingNarration[i].speaker == _narration[i].speaker;
	if (same) {
		for (uint i = 0; i < _pendingNarration.size(); ++i)
			_narration[i].rect = _pendingNarration[i].rect;
		_pendingNarration.clear();
		return;
	}
	Common::Array<NarrationText> pending;
	pending.swap(_pendingNarration);
	clearNarration();
	_narration.swap(pending);
	Common::TextToSpeechManager *manager = getNarrationManager();
	_narratorVoice = manager->getVoiceIndex();
	int male = _narratorVoice, female = _narratorVoice;
	// Resolve against this language's current voices for each batch. An
	// unknown-gender voice must not replace the user's selected narrator.
#ifdef USE_TTS
	const Common::Array<int> unknownVoices = manager->getVoiceIndicesByGender(Common::TTSVoice::UNKNOWN_GENDER);
	const Common::Array<int> maleVoices = manager->getVoiceIndicesByGender(Common::TTSVoice::MALE);
	const Common::Array<int> femaleVoices = manager->getVoiceIndicesByGender(Common::TTSVoice::FEMALE);
	if (!maleVoices.empty() && (unknownVoices.empty() || maleVoices[0] != unknownVoices[0]))
		male = maleVoices[0];
	if (!femaleVoices.empty() && (unknownVoices.empty() || femaleVoices[0] != unknownVoices[0]))
		female = femaleVoices[0];
#endif
	for (uint i = 0; i < _narration.size(); ++i)
		_narration[i].voice = _narration[i].speaker >= kFirstCompanion ? female :
			(_narration[i].speaker == kNarrator ? _narratorVoice : male);
	continueNarration();
	if (_narrationOwned && _scriptVM && _scriptVM->schedulerEnabled()) {
		_schedulerDebt = 0;
		_schedulerMilliseconds = 0;
		_narrationTimerHeld = true;
	}
}

void PreGob_Emmanuelle::continueNarration() {
	Common::TextToSpeechManager *manager = getNarrationManager();
	_narrationOwned = false;
	while (manager && _narrationNext < _narration.size()) {
		const int voice = _narration[_narrationNext].voice;
		Common::U32String text;
		do {
			const Common::U32String &part = _narration[_narrationNext++].text;
			if (part.empty()) continue;
			if (!text.empty()) text += '\n';
			text += part;
		} while (_narrationNext < _narration.size() && _narration[_narrationNext].voice == voice);
		if (text.empty()) continue;
		if (voice >= 0 && manager->getVoiceIndex() != voice)
			manager->setVoice(voice);
		// Suppression above includes the speaker, which backend text-only
		// suppression cannot distinguish when two people say the same words.
		manager->say(text, Common::TextToSpeechManager::INTERRUPT);
		// Backends disagree on say()'s return value. Advance rejected groups
		// without waiting; only an accepted utterance can hold the whole batch.
		_narrationOwned = manager->isSpeaking() || manager->isPaused();
		if (_narrationOwned)
			return;
	}
	if (manager && _narratorVoice >= 0)
		manager->setVoice(_narratorVoice);
	_narratorVoice = -1;
}

void PreGob_Emmanuelle::setInputContext(EmmanuelleInputContext context) {
	Common::Keymapper *keymapper = _vm->getEventManager()->getKeymapper();
	if (!keymapper)
		return;
	const EmmanuelleInputContext effectiveContext =
		_introSkip.available() ? kEmmanuelleInputIntro : context;
	for (int value = kEmmanuelleInputIntro;
		 value <= kEmmanuelleInputFight; ++value) {
		const EmmanuelleInputContext candidate =
			(EmmanuelleInputContext)value;
		Common::Keymap *keymap = keymapper->getKeymap(
			emmanuelleInputKeymapId(candidate));
		if (keymap)
			keymap->setEnabled(candidate == effectiveContext);
	}
	_inputContext = effectiveContext;
}

void PreGob_Emmanuelle::handleCustomAction(
		Common::CustomEventType action) {
	if (action == (Common::CustomEventType)kEmmanuelleActionOpenMainMenu) {
		EmmanuelleSaveBoundaryState state = saveBoundaryState();
		// Finish any pending publication before handing the request to the menu.
		// chooseInputRegion checks the actual boundary after presentation.
		state.paletteReady = true;
		_mainMenuActionThisPoll = state.canHandoffMenu();
		if (_diagnosticTraceEnabled && !_mainMenuActionThisPoll)
			debugC(2, kDebugSaveLoad,
				   "EMMA_TRACE event=menu_request result=ignored module=%s "
				   "active_input=%u node=0x%04X suspended=%u blocker=%s",
				   _moduleFileStem.c_str(), state.activeInput ? 1U : 0U,
				   state.activeInputNode, state.suspendedWait ? 1U : 0U,
				   emmanuelleSaveBoundaryBlockerName(
					   state.blocker()));
		return;
	}
	if (action == (Common::CustomEventType)kEmmanuelleActionSkipIntro)
		_introSkip.request();
}

void PreGob_Emmanuelle::handleMouseDown(int16 x, int16 y,
		MouseButtons buttons) {
	// Fights retain their native held-button handling.
	if (_inputContext != kEmmanuelleInputFight)
		_pendingInput.noteMouse(x, y, (byte)buttons);
}

void PreGob_Emmanuelle::processGameEvents() {
	_vm->_util->processInput();
	if (_platformTag == kOldTotPlatformAmiga &&
		_audioKind == kOldTotAudioAmigaSample && _audioActive &&
		!_vm->_sound->blasterPlayingSound())
		setAudioState(kOldTotAudioAmigaSample, _audioSlot, 0,
			_audioRepeatCount, false, false);

	int16 key = kKeyNone;
	while (_vm->_util->checkKey(key)) {
		if (_narrationCanContinue && key == kKeyReturn) {
			clearNarration();
			continue;
		}
		if (_inputContext == kEmmanuelleInputFight &&
			_platformTag == kOldTotPlatformDOS)
			_pendingInput.noteLatestKey(key);
		else
			_pendingInput.noteKey(key);
	}

	int16 mouseX = 0;
	int16 mouseY = 0;
	MouseButtons mouseButtons = kMouseButtonsNone;
	_vm->_util->getMouseState(&mouseX, &mouseY, &mouseButtons);
	_pendingInput.noteMouse(mouseX, mouseY, (byte)mouseButtons);
	if (mouseButtons != kMouseButtonsNone)
		_vm->_util->forceMouseUp();
	if (_introSkip.activate(_vm->shouldQuit()))
		beginIntroFastForward();
	if (shouldAbortExecution())
		clearNarration();
}

int16 PreGob_Emmanuelle::takePendingInput(
	int16 &mouseX, int16 &mouseY, MouseButtons &mouseButtons) {
	byte buttons = kMouseButtonsNone;
	const int16 key = _pendingInput.take(mouseX, mouseY, buttons);
	mouseButtons = (MouseButtons)buttons;
	return key;
}

int16 PreGob_Emmanuelle::pollGameInput(int16 &mouseX, int16 &mouseY,
										 MouseButtons &mouseButtons) {
	if (shouldAbortExecution())
		return kKeyNone;
	serviceAtariAudio();
	const bool paced = _platformTag == kOldTotPlatformAtariST &&
		servicePresentation();
	if (!paced)
		presentFrame(true);
	if (_hostFailed)
		return kKeyNone;
	processGameEvents();
	return takePendingInput(mouseX, mouseY, mouseButtons);
}

int16 PreGob_Emmanuelle::pollInterpreterInput(
	int16 &mouseX, int16 &mouseY, MouseButtons &mouseButtons, bool inFight) {
	const bool atari = _platformTag == kOldTotPlatformAtariST;
	if (shouldAbortExecution())
		return kKeyNone;
	processGameEvents();
	if (shouldAbortExecution()) {
		_schedulerDebt = 0;
		return kKeyNone;
	}

	serviceAtariAudio();
	serviceColorCycle();
	serviceInterpreterTimer();

	const bool paced = servicePresentation();
	// Amiga fights perform their own frame pacing.
	if ((_platformTag != kOldTotPlatformAmiga || !inFight) &&
		!paced && !shouldAbortExecution() && !_introSkip.fastForwarding())
		presentFrame(true);
	if (atari && !shouldAbortExecution())
		fadeAtariFromBlack();
	if (shouldAbortExecution()) {
		_schedulerDebt = 0;
		return kKeyNone;
	}

	return takePendingInput(mouseX, mouseY, mouseButtons);
}

void PreGob_Emmanuelle::serviceInterpreterTimer() {
	// Scheduled children can synchronously enter another input routine.
	if (_servicingTimer)
		return;
	if (!_scriptVM || !_scriptVM->schedulerEnabled() || shouldAbortExecution()) {
		_schedulerDebt = 0;
		_schedulerClockRunning = false;
		return;
	}

	const uint32 now = g_system->getMillis();
	if (narrationPlaying()) {
		_schedulerServiceTime = now;
		_schedulerClockRunning = true;
		_narrationTimerHeld = true;
		return;
	}
	if (_narrationTimerHeld) {
		_schedulerServiceTime = now;
		_schedulerClockRunning = true;
		_narrationTimerHeld = false;
	}
	const uint64 elapsed = _schedulerClockRunning
		? (uint64)(uint32)(now - _schedulerServiceTime) * _vm->_global->_speedFactor
		: 1000 / kHostFrameRate;
	_schedulerServiceTime = now;
	_schedulerClockRunning = true;
	const uint64 milliseconds = elapsed + _schedulerMilliseconds;
	_schedulerMilliseconds = milliseconds % (1000 / kHostFrameRate);
	const uint64 quanta = _schedulerDebt + accumulateTimerQuanta(
		_platformTag, _schedulerAccumulator, milliseconds / (1000 / kHostFrameRate));
	// Saturate only at the saved field's representable limit.
	_schedulerDebt = quanta > 0xFFFFFFFFU ? 0xFFFFFFFFU : (uint32)quanta;
	_servicingTimer = true;
	if (narrationEnabled()) {
		// Publish each spoken option before considering another overdue target.
		// Silent callbacks retain the normal eight-callback service budget.
		// Rejection is known only after publication, so rejected text can still
		// spread overdue callbacks across additional bounded service passes.
		for (uint i = 0; i < 8 && _schedulerDebt; ++i) {
			_schedulerDebt = _scriptVM->advanceTimer(_schedulerDebt, 1);
			if (!_pendingNarration.empty() || narrationPlaying())
				break;
		}
	} else {
		_schedulerDebt = _scriptVM->advanceTimer(_schedulerDebt, 8);
	}
	_servicingTimer = false;
}

void PreGob_Emmanuelle::pauseChanged(bool paused, uint32 duration) {
	if (!paused && _schedulerClockRunning)
		_schedulerServiceTime += duration;
}

bool PreGob_Emmanuelle::inputIdle(int16 key,
										MouseButtons mouseButtons) const {
	return key == kKeyNone && mouseButtons == kMouseButtonsNone;
}

void PreGob_Emmanuelle::discardPendingInput() {
	_vm->_util->clearKeyBuf();
	_vm->_util->forceMouseUp();
	// clearKeyBuf also pumps events, including mouse presses.
	_pendingInput.clear();
}

void PreGob_Emmanuelle::applyInputBoundary(
	const InputBoundary &boundary) {
	if (boundary.nativeDelayUnits) {
		const uint16 delayMilliseconds =
			_platformTag == kOldTotPlatformAmiga
				? amigaNativeDelayMilliseconds(boundary.nativeDelayUnits)
				: boundary.nativeDelayUnits;
		delay(delayMilliseconds);
	}
	if (boundary.showCursor && _scriptVM && _scriptVM->schedulerEnabled()) {
		// Count the choice's publication, but not the preceding entry delay.
		// The first logical service tick matches the native Amiga input pass.
		_schedulerServiceTime = g_system->getMillis();
		_schedulerMilliseconds += 1000 / kHostFrameRate;
		_schedulerClockRunning = true;
	}
	if (boundary.discardStale) {
		// Activate the choice only after its interface has been published.
		if (!servicePresentation())
			presentFrame(true);
		if (shouldAbortExecution())
			return;
		discardPendingInput();
	}
	if (_platformTag == kOldTotPlatformAtariST && boundary.discardStale) {
		int16 initialX = 0;
		int16 initialY = 0;
		_vm->_util->getMouseState(&initialX, &initialY, nullptr);
		initialX = CLIP<int16>(initialX, 0, 304);
		initialY = CLIP<int16>(initialY, 0, 184);
		_vm->_util->setMousePos(initialX, initialY);
	}
	if (boundary.showCursor)
		showCursor();
}

int16 PreGob_Emmanuelle::chooseInputRegion(uint16 nodeOffset, byte count,
											bool initialEntry, bool reentering) {
	if (!_tot || uint32(nodeOffset) + 6U + 4U * uint32(count) >
							 _tot->size()) {
		failHostOperation("input regions", nodeOffset);
		return -1;
	}
	// INIT's first choice is copy protection. End intro skipping before
	// accepting input so the skip action cannot select an answer.
	if (_introSkip.available())
		finishIntro();

	setInputContext(kEmmanuelleInputChoice);
	_mainMenuActionThisPoll = false;
	if (initialEntry || reentering)
		_schedulerClockRunning = false;
	applyInputBoundary(choiceInputBoundary(
		_platformTag, initialEntry, reentering));
	int16 mouseX = 0;
	int16 mouseY = 0;
	MouseButtons mouseButtons = kMouseButtonsNone;
	// Atari traite_souris_ stops an active sample on every input pass.
	if (_platformTag == kOldTotPlatformAtariST &&
		_vm->_sound && _vm->_sound->blasterPlayingSound())
		stopSampleAudio();
	const int16 key = pollInterpreterInput(mouseX, mouseY, mouseButtons);
	const bool menuAction = _mainMenuActionThisPoll;
	_mainMenuActionThisPoll = false;
	if (shouldAbortExecution()) {
		if (menuAction && _diagnosticTraceEnabled)
			debugC(2, kDebugSaveLoad,
				   "EMMA_TRACE event=menu_request result=ignored reason=quit "
				   "module=%s",
				   _moduleFileStem.c_str());
		_openMainMenuAfterSuspend = false;
		setInputContext(kEmmanuelleInputNone);
		return -3;
	}
	const EmmanuelleSaveBoundaryState boundary = saveBoundaryState();
	if (menuAction && boundary.canHandoffMenu() &&
		boundary.activeInputNode == nodeOffset) {
		_openMainMenuAfterSuspend = true;
		if (_diagnosticTraceEnabled)
			debugC(2, kDebugSaveLoad,
				   "EMMA_TRACE event=menu_request result=accepted module=%s "
				   "node=0x%04X",
				   _moduleFileStem.c_str(), nodeOffset);
		setInputContext(kEmmanuelleInputNone);
		return -3;
	}
	if (menuAction && _diagnosticTraceEnabled)
		debugC(2, kDebugSaveLoad,
			   "EMMA_TRACE event=menu_request result=ignored module=%s "
			   "node=0x%04X blocker=%s",
			   _moduleFileStem.c_str(), nodeOffset,
			   emmanuelleSaveBoundaryBlockerName(boundary.blocker()));
	const int16 cursorMinimum = minimumCursorOrigin(_platformTag);
	int16 cursorX = CLIP<int16>(mouseX, cursorMinimum, 316);
	int16 cursorY = CLIP<int16>(mouseY, cursorMinimum, 196);
	if (cursorX != mouseX || cursorY != mouseY)
		_vm->_util->setMousePos(cursorX, cursorY);

	if (key == kKeyEscape || mouseButtons == kMouseButtonsRight) {
		clearNarration();
		_schedulerClockRunning = false;
		if (_diagnosticTraceEnabled)
			debugC(3, kDebugGameFlow,
				"EMMA_TRACE event=input_cancel module=%s node=0x%04X",
				_moduleFileStem.c_str(), nodeOffset);
		hideCursor();
		setInputContext(kEmmanuelleInputNone);
		return -2;
	}
	const byte cursorStep = keyboardCursorStep(_platformTag);
	if (key == kKeyLeft) {
		cursorX = cursorX >= cursorMinimum + cursorStep
			? cursorX - cursorStep : cursorMinimum;
		_vm->_util->setMousePos(cursorX, cursorY);
		setInputContext(kEmmanuelleInputNone);
		return -3;
	}
	if (key == kKeyRight) {
		cursorX = cursorX <= 316 - cursorStep
			? cursorX + cursorStep : 316;
		_vm->_util->setMousePos(cursorX, cursorY);
		setInputContext(kEmmanuelleInputNone);
		return -3;
	}
	if (key == kKeyUp) {
		cursorY = cursorY >= cursorMinimum + cursorStep
			? cursorY - cursorStep : cursorMinimum;
		_vm->_util->setMousePos(cursorX, cursorY);
		setInputContext(kEmmanuelleInputNone);
		return -3;
	}
	if (key == kKeyDown) {
		cursorY = cursorY <= 196 - cursorStep
			? cursorY + cursorStep : 196;
		_vm->_util->setMousePos(cursorX, cursorY);
		setInputContext(kEmmanuelleInputNone);
		return -3;
	}

	const bool selected = key == kKeyReturn ||
						  mouseButtons == kMouseButtonsLeft;
	if (!selected) {
		setInputContext(kEmmanuelleInputNone);
		return -3;
	}
	clearNarration();
	hideCursor();

	_schedulerClockRunning = false;
	const int16 region = hitTestOldTotInputRegions(
		*_tot, nodeOffset, count, cursorX, cursorY,
		_alternateInputGeometry ? 0 : 1,
		_alternateInputGeometry ? 6 : 8);
	if (_diagnosticTraceEnabled)
		debugC(3, kDebugGameFlow,
			   "EMMA_TRACE event=input_region module=%s node=0x%04X count=%u x=%d y=%d region=%d key=%d buttons=%u",
			   _moduleFileStem.c_str(), nodeOffset, (uint)count, (int)cursorX,
			   (int)cursorY, (int)region, (int)key, (uint)mouseButtons);
	setInputContext(kEmmanuelleInputNone);
	return region;
}

void PreGob_Emmanuelle::waitReturn() {
	setInputContext(kEmmanuelleInputContinue);
	int16 mouseX = 0;
	int16 mouseY = 0;
	MouseButtons mouseButtons = kMouseButtonsNone;
	int16 key = kKeyNone;
	do {
		key = pollInterpreterInput(mouseX, mouseY, mouseButtons);
	} while (!shouldAbortExecution() && !_introSkip.fastForwarding() &&
			 !inputIdle(key, mouseButtons));
	do {
		key = pollInterpreterInput(mouseX, mouseY, mouseButtons);
	} while (!shouldAbortExecution() && !_introSkip.fastForwarding() &&
			 key != kKeyReturn);
	clearNarration();
	do {
		key = pollInterpreterInput(mouseX, mouseY, mouseButtons);
	} while (!shouldAbortExecution() && !_introSkip.fastForwarding() &&
			 !inputIdle(key, mouseButtons));
	setInputContext(kEmmanuelleInputNone);
}

void PreGob_Emmanuelle::waitInputRelease() {
	int16 mouseX = 0;
	int16 mouseY = 0;
	MouseButtons mouseButtons = kMouseButtonsNone;
	int16 key = kKeyNone;
	delay(200);
	do {
		key = pollInterpreterInput(mouseX, mouseY, mouseButtons);
		if (!inputIdle(key, mouseButtons))
			delay(40);
	} while (!shouldAbortExecution() && !_introSkip.fastForwarding() &&
			 !inputIdle(key, mouseButtons));
}

void PreGob_Emmanuelle::waitOpcode03(byte arg, uint16) {
	_schedulerClockRunning = false;
	if (shouldAbortExecution())
		return;
	if (_introSkip.fastForwarding()) {
		servicePresentation();
		processGameEvents();
		return;
	}
	if (arg == 0) {
		if (_platformTag == kOldTotPlatformAmiga ||
			_platformTag == kOldTotPlatformDOS)
			waitReturn();
		else
			waitAtariContinuation();
		return;
	}
	if (arg == 0xFF) {
		if (_platformTag == kOldTotPlatformAmiga)
			waitAmigaInputRelease();
		else if (_platformTag == kOldTotPlatformDOS)
			waitInputRelease();
		else
			waitAtariInputRelease();
		return;
	}
	// Gameplay waits count presentation toward an elapsed target.
	// INIT retains its native requested sleep.
	const uint32 presentationStart = _currentModule != kModuleInit ? g_system->getMillis() : 0;
	if (!servicePresentation())
		presentFrame(true);
	if (shouldAbortExecution())
		return;
	if (_platformTag == kOldTotPlatformAtariST)
		fadeAtariFromBlack();
	noteInitialTomahawkVisible();
	serviceWaitColorCycle();
	const bool spokenCaption = narrationPlaying();
	const EmmanuelleInputContext previousContext = _inputContext;
	if (spokenCaption) {
		// A press collected while the caption was being drawn predates it.
		discardPendingInput();
		setInputContext(kEmmanuelleInputContinue);
		_narrationCanContinue = true;
	}
	if (_currentModule != kModuleInit) {
		waitForMinimumDuration(presentationStart, 100 + (arg / 5) * _animationSpeedFactor * 10);
	} else {
		uint16 milliseconds = opcode03DelayUnits(arg);
		if (_platformTag == kOldTotPlatformDOS)
			milliseconds = dosOpcode03DelayMilliseconds(arg);
		else if (_platformTag == kOldTotPlatformAmiga)
			milliseconds = amigaNativeDelayMilliseconds(milliseconds);
		delay(milliseconds);
	}
	while (!shouldAbortExecution() && !_introSkip.fastForwarding() && narrationPlaying())
		delay(20);
	_narrationCanContinue = false;
	if (spokenCaption)
		setInputContext(previousContext);
}

void PreGob_Emmanuelle::delay(uint16 milliseconds) {
	// Script sleeping and blocking audio do not advance an earlier input wait.
	_schedulerClockRunning = false;
	while (milliseconds && !shouldAbortExecution()) {
		if (_introSkip.fastForwarding()) {
			processGameEvents();
			return;
		}
		const uint16 slice = milliseconds < 20 ? milliseconds : 20;
		_vm->_util->delay(slice);
		processGameEvents();
		serviceAtariAudio();
		milliseconds -= slice;
	}
}

void PreGob_Emmanuelle::waitPresentationFrames(byte count) {
	for (byte i = 0; i < count && !shouldAbortExecution(); ++i) {
		if (!_introSkip.fastForwarding())
			_vm->_util->waitEndFrame(false);
		processGameEvents();
	}
}

} // End of namespace Emmanuelle
} // End of namespace Gob
