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

#ifndef GOB_PREGOB_ONCEUPON_ENDING_H
#define GOB_PREGOB_ONCEUPON_ENDING_H

#include "common/ptr.h"
#include "common/scummsys.h"

#include "gob/pregob/onceupon/scene.h"

namespace Gob {
class GobEngine;
class ANIFile;
class Surface;

class GCTFile;

namespace OnceUpon {

class Assets;

/** Final-scene animations shared by Abracadabra and Baba Yaga.
 * The section finishes the text pages before starting these animations.
 */
class Ending : public Section {
public:
	Ending(OnceUpon &game);
	~Ending() override;
	void begin();
	Outcome step() override;

private:
	void prepare();
	void drawInterface();
	void adoptBackground(const Surface &source);
	void stepAnimation();

	static const uint kObjectCount = 4;
	static const uint16 kAnimations[kObjectCount];
	GobEngine *_vm;
	Assets &_assets;
	OnceUpon &_game;
	Common::SharedPtr<const ANIFile> _ani;
	uint16 _frames[kObjectCount];
	Surface *_background, *_frame;
	Common::ScopedPtr<GCTFile> _text;
	Common::ScopedPtr<Surface> _captionBackground;
	enum Phase { kPhaseText, kPhasePlaying };
	Phase _phase;
	uint32 _captionStart;
	void drawPage();
};

} // End of namespace OnceUpon
} // End of namespace Gob

#endif // GOB_PREGOB_ONCEUPON_ENDING_H
