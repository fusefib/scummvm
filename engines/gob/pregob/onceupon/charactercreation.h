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

#ifndef GOB_PREGOB_ONCEUPON_CHARACTERCREATION_H
#define GOB_PREGOB_ONCEUPON_CHARACTERCREATION_H

#include "common/ptr.h"
#include "common/str.h"

#include "gob/pregob/onceupon/scene.h"


namespace Gob {
class GobEngine;
class GCTFile;
class TXTFile;
class ANIFile;
class Surface;
class Font;

namespace OnceUpon {

class Assets;
class CharGenChild;
class OnceUpon;

/** Shared character and name creation for Abracadabra and Baba Yaga. */
class CharacterCreation : public Section {
public:
	enum Progress { kHead, kHair, kJacket, kTrousers, kNameReveal, kName, kConfirmName, kFinished };
	CharacterCreation(OnceUpon &game);
	~CharacterCreation() override;
	void enter();
	Outcome step() override;
	bool acceptsKeyRepeat(const Common::KeyState &key) const override;
	static void drawPortrait(const Surface &heads, uint head, Surface &dest);
	Progress progress() const { return _progress; }
	int16 head() const { return _head; }
	uint8 hair() const { return _hair; }
	uint8 jacket() const { return _jacket; }
	uint8 trousers() const { return _trousers; }
	const Common::String &name() const { return _name; }
	bool hasAcceptedName() const { return _acceptedName; }
	bool awaitingNameRelease() const { return _awaitNameRelease; }

private:
	friend class OnceUpon;

	void prepare(const Font &font, byte hair = 12, byte jacket = 10, byte trousers = 9);
	/** Bind immutable appearance art without advancing state or painting. */
	void setAppearanceAssets(byte hair, byte jacket, byte trousers);
	/** Compose the immutable initial panel without changing live images or state. */
	void composeInitialInterface(const Surface &frame, Surface &dest) const;
	void drawInterface();
	void drawPointer(Surface &cursor) const;
	Surface &captionBackground() { return *_background; }
	void syncCaption();
	void drawCaption(uint item);
	Outcome stepNameReveal();

	struct Head {
		int16 left, top, right, bottom, sourceLeft, sourceTop, sourceRight, sourceBottom, x, y;
	};
	static const Head kHeads[4];
	static const int16 kColorAreas[3][2][4];

	GobEngine *_vm;
	Assets &_assets;
	OnceUpon &_game;
	Common::ScopedPtr<GCTFile> _text;
	Common::ScopedPtr<TXTFile> _prompt;
	enum Phase { kPhaseNone, kPhasePlaying, kPhaseInput, kPhaseWait };
	Phase _phase;
	uint32 _waitStart, _activityStart;
	const Font *_font;
	Common::SharedPtr<const ANIFile> _ani;
	CharGenChild *_child;
	Common::SharedPtr<const Surface> _choices, _heads;
	Surface *_background, *_frame;
	Progress _progress;
	Common::String _name;
	int16 _head;
	uint8 _hair, _jacket, _trousers, _revealFrame;
	bool _previousPress, _awaitNameRelease, _acceptedName;

	void render();
	bool chooseAppearance(int16 x, int16 y, bool pressed, bool timedOut);
	void selectHead(int16 head);
	void selectColor(uint part, bool secondary);
	void redrawPaperDoll();
	void prepareName();
	void revealName();
	void drawNameCursor(uint8 color);
	bool editName(int16 key);
	void prepareNameConfirmation(bool timedOut);
	bool colorHit(uint part, uint choice, int16 x, int16 y) const;
};

} // End of namespace OnceUpon
} // End of namespace Gob

#endif // GOB_PREGOB_ONCEUPON_CHARACTERCREATION_H
