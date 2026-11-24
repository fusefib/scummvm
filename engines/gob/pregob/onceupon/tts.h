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

#ifndef GOB_PREGOB_ONCEUPON_TTS_H
#define GOB_PREGOB_ONCEUPON_TTS_H

#include "common/scummsys.h"

#ifdef USE_TTS
#include "common/str-array.h"

namespace Common {
class TextToSpeechManager;
}

namespace Gob {
namespace OnceUpon {

/** Optional narration owned by either Once Upon game, never by its saves. */
class Narration {
public:
	Narration();
	~Narration();
	static bool isEnabled();

	/** Submit only after publishing a new visible page or selection. */
	void publishNew(const Common::String &text, uint language, uint32 now);
	void publishPage(const Common::String &rows, uint language, uint32 now);
	void publishStoryPage(const Common::StringArray &parts, uint language, uint32 now);
	/** Poll ordinary captions without extending their visibility for startup. */
	bool update(uint32 now);
	/** Include a submitted request still in its bounded startup window. */
	bool speakingOrStarting(uint32 now);
	/** Service story parts after handling input. Keep the page through the handoff. */
	bool updateStoryPage(uint32 now);
	void cancel();

private:
	enum State {
		kIdle,
		kWaiting,
		kSubmitted,
		kSpeaking
	};
	Common::TextToSpeechManager *_manager;
	Common::String _text;
	Common::StringArray _storyParts;
	uint _storyPart;
	State _state;
	uint _language;
	uint32 _stateSince;

	bool service(uint32 now);
};

} // End of namespace OnceUpon
} // End of namespace Gob

#endif
#endif // GOB_PREGOB_ONCEUPON_TTS_H
