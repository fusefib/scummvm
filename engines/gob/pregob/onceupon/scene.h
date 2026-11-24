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

#ifndef GOB_PREGOB_ONCEUPON_SCENE_H
#define GOB_PREGOB_ONCEUPON_SCENE_H

#include "common/str.h"

#include "gob/util.h"

namespace Gob {
namespace OnceUpon {

class OnceUpon;

enum SectionOutcome {
	kSectionContinue,
	kSectionComplete,
	kSectionRetry,
	kSectionRestart,
	kSectionQuit,
	kSectionRestore
};

/** An active story controller. The story loop adopts loads after step returns. */
class Section {
public:
	typedef SectionOutcome Outcome;
	virtual ~Section() {}
	virtual Outcome step() = 0;
	virtual bool atSaveBoundary() const { return false; }
	virtual bool acceptsKeyRepeat(const Common::KeyState &) const { return false; }
	virtual void resumeAfterPause() {}
	virtual void suspendMovie() {}
	virtual bool activateMovie(MouseButtons) { return false; }
};

// Story choices shared by the live game and its committed entry checkpoint.
struct Progress {
	int16 difficulty, section, maximumSection;
	uint32 score;
	Common::String name;
	byte house, head, hair, jacket, trousers;
	Progress() : difficulty(0), section(0), maximumSection(0), score(0),
		house(0), head(0), hair(0), jacket(0), trousers(0) {}
};

enum SceneKind {
	kSceneNone, kSceneTitle, kSceneParents, kSceneForest,
	kSceneCastle, kSceneBoss, kSceneHouse, kSceneCharacter, kSceneEnding, kSceneStory
};

} // namespace OnceUpon
} // namespace Gob
#endif
