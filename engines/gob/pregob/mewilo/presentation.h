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
 */

#ifndef GOB_PREGOB_MEWILO_PRESENTATION_H
#define GOB_PREGOB_MEWILO_PRESENTATION_H

#include "common/language.h"
#include "common/rect.h"

#include "gob/pregob/mewilo/platform.h"

namespace Gob {
namespace Mewilo {

class MewiloTotData;

enum MewiloTimeAnchor {
	kMewiloElapsedWait, ///< Budget begins on entering the delay instruction.
	kMewiloVisibleHold  ///< Minimum exposure of the specified output region.
};

struct MewiloTimingPlan {
	MewiloTimeAnchor anchor;
	uint32 milliseconds;
	uint32 minimumDelay; ///< Elapsed delay also applies under a visibility budget.
	uint32 minimumExposure; ///< Concurrent minimum exposure of the selected frame.
	Common::Rect region;
	Common::Rect exposureRegion; ///< Optional separate image floor under dialogue.
	const char *rule;
	// True identifies a recovered nominal base conversion, not hardware
	// calibration or the duration of a reading hold.
	bool calibrated;
	bool animation;
	bool reading;
	uint16 textNode; ///< Reading floor uses the displayed language when nonzero.
	MewiloTimingPlan();
};

uint32 mewiloScaleTime(uint32 milliseconds, uint speedFactor);
uint32 mewiloAnimationDelayMillis(uint32 frameMillis, byte operand);
uint32 mewiloReadingMillis(const byte *text, uint length);
// Story cues share a frame pace; portrait poses have a half-frame minimum.
// Reviewed short text gets a reading hold. Other waits retain native timing.
MewiloTimingPlan mewiloTimingPlan(const MewiloTotData &tot, const char *module,
	Common::Language language, MewiloPlatform platform, uint16 site, byte operand,
	uint32 animationFrameMillis);

/** Mewilo-only presentation clock, not an interpreter or a frame scheduler.
 *
 * Direct pixel comparisons detect changes, not just repeated dirty calls. A
 * regional exposure survives unrelated score/cursor drawing. Eight-pixel tiles
 * make overlapping changes conservative. No pixels, pointers, deadlines or
 * presentation policy are serialized; saves occur outside timed operations.
 */
class MewiloPresentation {
public:
	MewiloPresentation();
	void reset();
	void dirty(const Common::Rect &region);

	bool pending() const { return !_dirty.isEmpty(); }

	// Called AFTER the back buffer has actually been submitted to the backend.
	bool submitted(const byte *pixels, uint pitch, uint32 activeTime, Common::Rect &changed);
	uint32 visibleAge(const Common::Rect &region, uint32 activeTime) const;

	uint32 frame() const { return _frame; }

	uint32 waitRemaining(
		const MewiloTimingPlan &plan, uint speed, uint32 waitStart, uint32 activeTime) const;
	static uint32 remaining(uint32 budget, uint32 elapsed);

private:
	enum { kTileSize = 8, kColumns = 40, kRows = 25, kTiles = kColumns * kRows };

	struct Tile {
		uint32 since;
		bool known;
	};

	byte _pixels[320 * 200]; ///< Last submitted image, not another render buffer.
	Tile _tiles[kTiles];
	Common::Rect _dirty;
	uint32 _frame;
};

} // End of namespace Mewilo
} // End of namespace Gob

#endif // GOB_PREGOB_MEWILO_PRESENTATION_H
