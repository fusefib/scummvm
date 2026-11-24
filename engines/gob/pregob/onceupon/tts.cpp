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
 *
 *
 * This file is dual-licensed.
 * In addition to the GPLv3 license mentioned above, this code is also
 * licensed under LGPL 2.1. See LICENSES/COPYING.LGPL file for the
 * full text of the license.
 *
 */

#include "gob/pregob/onceupon/tts.h"

#ifdef USE_TTS
#include "common/config-manager.h"
#include "common/system.h"
#include "common/text-to-speech.h"

namespace Gob {
namespace OnceUpon {

Narration::Narration() : _manager(nullptr), _storyPart(0), _state(kIdle), _language(0), _stateSince(0) {
}

Narration::~Narration() {
	cancel();
}

bool Narration::isEnabled() {
	return ConfMan.hasKey("tts_enabled") && ConfMan.getBool("tts_enabled");
}

void Narration::publishNew(const Common::String &text, uint language, uint32 now) {
	cancel();
	if (text.empty() || language >= 5 || !g_system || !isEnabled())
		return;
	_manager = g_system->getTextToSpeechManager();
	if (!_manager)
		return;
	_text = text;
	_language = language;
	_state = kWaiting;
	update(now);
}

void Narration::publishPage(const Common::String &rows, uint language, uint32 now) {
	if (!isEnabled()) {
		cancel();
		return;
	}
	Common::String words;
	uint breaks = 0;
	bool space = false;
	for (uint i = 0; i < rows.size(); ++i) {
		const byte c = rows[i];
		if (c <= ' ') {
			space = true;
			if (c == '\n')
				++breaks;
			continue;
		}
		if (space && !words.empty())
			words += breaks > 1 ? "\n\n" : " ";
		words += c;
		space = false;
		breaks = 0;
	}
	publishNew(words, language, now);
}

void Narration::publishStoryPage(const Common::StringArray &parts, uint language, uint32 now) {
	if (parts.empty())
		return;
	publishNew(parts.front(), language, now);
	if (_state != kIdle)
		_storyParts = parts;
}

bool Narration::update(uint32 now) {
	// StoryText::step() services grouped speech after handling page input.
	// The ordinary endFrame() poll must not start the next story part first.
	return _storyParts.empty() && service(now);
}

bool Narration::updateStoryPage(uint32 now) {
	// A later part starts only after observed speech. Keep that card visible
	// during its bounded startup window, even past the unattended dwell.
	return service(now) || _storyPart != 0;
}

bool Narration::service(uint32 now) {
	if (_state == kIdle)
		return false;
	if (!g_system || g_system->getTextToSpeechManager() != _manager || !isEnabled()) {
		cancel();
		return false;
	}
	const uint32 startTimeout = 1000;
	if (_state == kSpeaking) {
		if (_manager->isSpeaking())
			return true;
		_state = kIdle;
		if (_storyPart + 1 >= _storyParts.size()) {
			cancel();
			return false;
		}
		_text = _storyParts[++_storyPart];
		_state = kWaiting;
		_stateSince = now;
	}
	if (_state == kWaiting) {
		// Leave another owner's speech and language settings alone.
		if (_manager->isSpeaking() || _manager->isPaused()) {
			// An occupied backend also ends a stalled handoff.
			if (_storyPart != 0 && now - _stateSince >= startTimeout)
				cancel();
			return false;
		}
		const char *const languages[] = {"fr", "de", "en", "es", "it"};
		if (_manager->getLanguage() != languages[_language])
			_manager->setLanguage(languages[_language]);
		_manager->enable(true);
		const uint voices = _manager->getVoicesArray().size();
		int voice = ConfMan.hasKey("tts_voice") ? ConfMan.getInt("tts_voice") : _manager->getDefaultVoice();
		if (voice < 0 || (uint)voice >= voices)
			voice = _manager->getDefaultVoice();
		if (voice >= 0 && (uint)voice < voices)
			_manager->setVoice(voice);
		_state = kSubmitted;
		_stateSince = now;
		// Backends disagree on the Boolean result. Observe activity instead.
		_manager->say(_text, Common::TextToSpeechManager::DROP, Common::kDos850);
	}
	if (_manager->isSpeaking()) {
		_state = kSpeaking;
		return true;
	}
	if (now - _stateSince >= startTimeout) {
		// Cancel a rejected or never-starting request before it can speak later.
		cancel();
	}
	return false;
}

bool Narration::speakingOrStarting(uint32 now) {
	return update(now) || _state == kSubmitted;
}

void Narration::cancel() {
	if ((_state == kSubmitted || _state == kSpeaking) && g_system &&
		g_system->getTextToSpeechManager() == _manager)
		_manager->stop();
	_manager = nullptr;
	_text.clear();
	_storyParts.clear();
	_storyPart = 0;
	_state = kIdle;
}

} // End of namespace OnceUpon
} // End of namespace Gob
#endif
