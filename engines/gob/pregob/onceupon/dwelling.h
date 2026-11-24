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

#ifndef GOB_PREGOB_ONCEUPON_DWELLING_H
#define GOB_PREGOB_ONCEUPON_DWELLING_H

#include "common/ptr.h"
#include "common/scummsys.h"

#include "gob/pregob/onceupon/scene.h"

namespace Gob {

class ANIFile;
class GobEngine;
class Surface;

class GCTFile;

namespace OnceUpon {

class Assets;
class OnceUpon;

/** Early parents search shared by Abracadabra and Baba Yaga, in either dwelling. */
class DwellingSearch {
public:
	DwellingSearch(OnceUpon &game, bool cabin, GCTFile &text);
	~DwellingSearch();
	void begin();
	SectionOutcome step();

private:
	struct Hotspot {
		int16 animation;
		int16 left, top, right, bottom;
		bool repeat, background;
	};

	struct State {
		bool inCabin;
		uint16 activations;
		int16 rewardFrame;
		int16 x, y;
		bool pressed;
	};

	enum Event {
		kNone,
		kComment,
		kReward,
		kComplete
	};

	/** Prepare the search resources after the parents sequence. */
	void prepare(Surface &dest);
	void setPosition(int16 x, int16 y);
	/** Draw the current poses before advance() changes their frames. */
	void draw(Surface &dest);
	Event advance(bool down, bool activation = false);

	static const Hotspot kCabin[13];
	static const Hotspot kCastle[11];

	GobEngine *_vm;
	Assets &_assets;
	OnceUpon &_game;
	State _state;
	const Hotspot *_hotspots;
	int16 *_frames;
	bool _mirrored;
	bool _fireTick;
	Common::SharedPtr<const ANIFile> _ani;
	Surface *_background;
	GCTFile *_text; // Borrowed from Parents for this visit.
	enum Phase { kPhaseNone, kPhaseSearch, kPhaseResult, kPhaseRewardRead, kPhaseComplete };
	Phase _phase;
	uint16 _textItem;
	uint32 _captionStart;

	void drawCaption(uint item);
	void drawPage();

	uint hotspotCount() const;
	void drawFrame(Surface &surface, uint animation, uint frame) const;
};

} // End of namespace OnceUpon
} // End of namespace Gob

#endif // GOB_PREGOB_ONCEUPON_DWELLING_H
