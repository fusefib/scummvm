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

#ifndef GOB_PREGOB_ONCEUPON_ENCOUNTERS_H
#define GOB_PREGOB_ONCEUPON_ENCOUNTERS_H

#include "common/ptr.h"
#include "common/scummsys.h"

#include "gob/sound/sounddesc.h"
#include "gob/pregob/onceupon/scene.h"

namespace Gob {

class ANIFile;
class GobEngine;
class Surface;

class GCTFile;

namespace OnceUpon {

class Assets;
/** Presentation, surfaces and input for the Chapter 3 and Chapter 5 encounters. */
class Encounter : public Section {
public:
	enum EventType { kText, kFlash, kShake, kWait, kDelay, kPostFightHold, kFrame, kComplete };
	struct Event {
		EventType type;
		int16 value, selection;
		Event(EventType t, int16 v = 0, int16 s = -1) : type(t), value(v), selection(s) {}
	};

	Outcome step() override;

protected:
	struct Hotspot {
		int16 animation, left, top, right, bottom, visible, initialCounted;
	};

	Encounter(OnceUpon &game, int16 animation, bool boss);
	~Encounter() override;
	void loadInterface(const char *animation, const char *artwork);
	bool animationEnded() const;
	void readInput(int16 x, int16 y, bool pressed, uint32 heldKeys);
	void copyScore();
	void playCue(uint index);
	virtual void prepare() = 0;
	virtual Event next(int16 x, int16 y, bool pressed, uint32 heldKeys) = 0;
	virtual void filterFightTaps() {}

	GobEngine *_vm;
	OnceUpon &_game;
	Common::SharedPtr<const ANIFile> _ani;
	Surface *_background, *_frame;
	int16 _animation, _animationFrame, _x, _y;
	bool _pressed;

private:
	friend class OnceUpon;

	enum Phase {
		kExteriorDelay, kExteriorHold, kPlaying, kCaption, kTimedWait, kPostVictoryHold, kScoreDelay
	};

	void drawExterior(Surface &dest);
	void beginExterior();
	void beginFight();
	void captureBackground();
	void loadCues();
	void drawCaption(uint item, int selection = -1);
	void drawPage();
	void shake(int16 first, int16 second);

	Assets &_assets;
	SoundDesc _cues[6];
	const bool _boss;
	Phase _phase;
	Common::ScopedPtr<GCTFile> _text;
	Common::ScopedPtr<Surface> _captionBackground;
	uint32 _captionStart, _waitStart, _waitDuration;
	uint16 _textItem;
#ifdef USE_TTS
	int16 _queuedText, _queuedSelection;
#endif
	bool _captionPressed, _needsFadeIn;
};

/**
 * The chapter 3 fight and objective search shared by Abracadabra and Baba Yaga.
 * The early parents' castle-layout search is a separate section.
 */
class CastleEncounter : public Encounter {
public:

	CastleEncounter(OnceUpon &game);

private:
	void prepare() override;
	Event next(int16 x, int16 y, bool pressed, uint32 heldKeys = 0) override;
	bool searching() const { return _searching; }
	bool acceptsFightTap() const { return _stage == kFightInput && !_spellActive && !_won; }

	enum Stage {
		kFightInput, kVictoryChoice, kVictoryResume, kVictoryHold, kEnterSearch,
		kFrameEffects, kFlashEffects, kSpell,
		kEnemyAttack, kEnemyAnimation, kFightFrame,
		kSearchInput, kSearchSample, kSearchFrame,
		kFailureWait, kSearchWait, kFinalDelay, kFinalScore, kDone
	};
	static const Hotspot kHotspots[15];

	bool _counted[15];
	Stage _stage;
	int16 _cursorFrame, _spell, _spellFrame;
	int16 _cursorX, _cursorY;
	uint16 _attackClock, _damage, _hits, _objects;
	bool _spellActive, _won, _alternate, _searching, _success;

	void filterFightTaps() override;
	void drawFrame(int16 animation, int16 frame, bool restore = false);
	void publish();
	void renderFight();
	void renderSearch();
};

/** Chapter 5 fight and objective search shared by Abracadabra and Baba Yaga. */
class BossEncounter : public Encounter {
public:

	BossEncounter(OnceUpon &game);

private:
	void prepare() override;
	Event next(int16 x, int16 y, bool pressed, uint32 heldKeys = 0) override;

	enum Stage {
		kFightInput, kVictoryRestore, kVictoryText, kEnterSearch,
		kFrameEffects, kFlashEffects, kSpell, kHit, kEnemyAttack,
		kEnemyAnimation, kFightFrame, kSearchInput,
		kSearchToggle, kFinalSample, kSearchFrame,
		kFailureWait, kSearchWait, kFinalScore, kDone
	};
	static const Hotspot kHotspots[16];
	bool _counted[16];
	Stage _stage;
	int16 _cursorFrame, _spell, _spellFrame, _missFrame;
	int16 _cursorX, _cursorY, _missX, _missY;
	uint16 _attackClock, _damage, _hits, _objects;
	bool _spellActive, _toggled, _success;

	void drawFrame(Surface &dest, int16 animation, int16 frame, bool restore = false);
	void publish(bool fight);
	void renderFight();
	void renderSearch();
};

} // End of namespace OnceUpon
} // End of namespace Gob
#endif // GOB_PREGOB_ONCEUPON_ENCOUNTERS_H
