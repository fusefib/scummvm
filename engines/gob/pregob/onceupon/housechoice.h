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

#ifndef GOB_PREGOB_ONCEUPON_HOUSECHOICE_H
#define GOB_PREGOB_ONCEUPON_HOUSECHOICE_H

#include "common/ptr.h"
#include "common/rect.h"

#include "gob/pregob/onceupon/scene.h"

namespace Gob {
class GobEngine;
class GCTFile;
class TXTFile;
class ANIFile;
class Surface;

namespace OnceUpon {

class Assets;
class OnceUpon;

/** Initial house choice shared by Abracadabra and Baba Yaga, before character
 * creation and the separate parents search. Abracadabra has a dragon courier;
 * Baba Yaga has a stork.
 */
class HouseChoice : public Section {
public:
	HouseChoice(OnceUpon &game);
	~HouseChoice() override;
	void enter();
	Outcome step() override;
	int16 selectedHouse() const;

private:
	friend class OnceUpon;

	void prepare(bool animateScenery = true);
	void drawInterface();
	Surface &captionBackground() { return *_background; }
	void syncCaption(int16 left, int16 top, int16 right, int16 bottom);
	void drawPointer(Surface &cursor) const;
	void drawSelectionCaption();

	struct Drop {
		int16 left, top, right, bottom, dropX, bundleAnimation, dropWhileFar, landingY, house;
	};
	static const int16 kBackgroundAnimations[12];
	static const Drop kDrops[4];

	GobEngine *_vm;
	Assets &_assets;
	OnceUpon &_game;
	Common::ScopedPtr<GCTFile> _text;
	Common::ScopedPtr<TXTFile> _prompt;
	enum Phase { kPhaseNone, kPhasePlaying, kPhaseInput };
	Phase _phase;
	Common::SharedPtr<const ANIFile> _ani;
	Surface *_background, *_frame;
	int16 _positions[18][2], _backgroundFrames[12];
	int16 _flightAnimation, _flightFrame, _bundleAnimation, _bundleFrame;
	int16 _selection, _soundCounter;
	bool _far, _dropped, _previousPress, _animateScenery;

	Common::Rect frameBounds(int16 animation, int16 frame) const;
	void drawFrame(int16 animation, int16 frame, bool restore = false);
	void render();
	bool bundleLanded() const;
	void advance();
	bool selectHouse(int16 x, int16 y);
	void dropBundle();
};

} // End of namespace OnceUpon
} // End of namespace Gob

#endif // GOB_PREGOB_ONCEUPON_HOUSECHOICE_H
