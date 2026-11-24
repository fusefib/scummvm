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

#include "common/list.h"
#include "common/system.h"
#include "common/util.h"

#include "gob/anifile.h"
#include "gob/draw.h"
#include "gob/gob.h"
#include "gob/global.h"
#include "gob/surface.h"
#include "gob/util.h"
#include "gob/video.h"
#include "gob/sound/sound.h"
#include "gob/pregob/gctfile.h"
#include "gob/pregob/onceupon/assets.h"
#include "gob/pregob/onceupon/encounters.h"
#include "gob/pregob/onceupon/onceupon.h"

namespace Gob {
namespace OnceUpon {

Encounter::Encounter(OnceUpon &game, int16 animation, bool boss) :
	_vm(game._vm), _game(game), _ani(nullptr), _background(nullptr), _frame(nullptr), _animation(animation),
	_animationFrame(0), _x(160), _y(100), _pressed(false), _assets(*game._assets), _boss(boss),
	_phase(kExteriorDelay), _captionStart(0), _waitStart(0), _waitDuration(0), _textItem(0),
#ifdef USE_TTS
	_queuedText(-1), _queuedSelection(-1),
#endif
	_captionPressed(false), _needsFadeIn(false) {
}

Encounter::~Encounter() {
	for (uint i = 0; i < ARRAYSIZE(_cues); ++i)
		_vm->_sound->sampleFree(&_cues[i]);
	delete _background;
	delete _frame;
}

void Encounter::loadInterface(const char *animation, const char *artwork) {
	_ani = _assets.animation(animation);
	_background = new Surface(320, 151, 1);
	_frame = new Surface(320, 151, 1);
	const Assets::Image image = _assets.image(artwork);
	_vm->_draw->_backSurface->blit(*image);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 0, 0, 319, 199);
}

void Encounter::captureBackground() {
	_background->blit(*_vm->_draw->_backSurface, 0, 49, 319, 199, 0, 0);
	_frame->blit(*_background);
}

bool Encounter::animationEnded() const {
	return _animation >= 0 && _animationFrame >= _ani->getAnimationInfo(_animation).frameCount;
}

void Encounter::loadCues() {
	const char *const cues[ARRAYSIZE(_cues)] = {
		"mechant.snd", "eclair.snd", "gagne.snd", "diamant.snd", "fuite.snd", "mormon3.snd"
	};
	for (uint i = 0; i < ARRAYSIZE(cues); ++i)
		Assets::loadSample(_vm, _cues[i], cues[i]);
}

void Encounter::playCue(uint index) {
	SoundDesc &sample = _cues[index];
	if (!sample.empty())
		_game.replaceSample(sample, 1);
}

void Encounter::readInput(int16 x, int16 y, bool pressed, uint32 heldKeys) {
	KeyboardInput::moveActionPointer(x, y, heldKeys);
	KeyboardInput::clampActionPointer(x, y);
	_x = x;
	_y = y;
	_pressed = pressed;
	if (heldKeys & 0x000F)
		_vm->_util->setMousePos(_x, _y);
}

void Encounter::copyScore() {
	_frame->blit(*_vm->_draw->_backSurface, 0, 190, 319, 199, 0, 141);
	_background->blit(*_vm->_draw->_backSurface, 0, 190, 319, 199, 0, 141);
}

// Chapter 3 castle encounter

const CastleEncounter::Hotspot CastleEncounter::kHotspots[15] = {
	{ 4,  19, 110,  49, 182, 0, 0}, { 5,  76, 167,  88, 186, 0, 0},
	{ 6,  63, 173,  77, 186, 0, 0}, { 7, 260, 100, 305, 185, 0, 0},
	{11,  54, 143,  79, 159, 0, 0}, {18,  16,  49,  46,  78, 0, 0},
	{ 2, 204, 137, 227, 164, 0, 0}, { 0, 149, 177, 180, 194, 0, 0},
	{26, 134, 132, 200, 152, 0, 0}, {27,  52, 120,  67, 136, 0, 1},
	{28,  69, 116,  81, 135, 0, 1}, {29, 200, 119, 216, 136, 0, 1},
	{30, 217, 119, 231, 139, 0, 1}, {31, 237,  51, 292,  82, 0, 1},
	{32, 180,  97, 201, 125, 0, 1}
};

CastleEncounter::CastleEncounter(OnceUpon &game) :
	Encounter(game, 16, false), _stage(kFightInput),
	_cursorFrame(0), _spell(0), _spellFrame(0), _cursorX(160), _cursorY(100),
	_attackClock(0), _damage(0), _hits(0), _objects(0), _spellActive(false),
	_won(false), _alternate(false), _searching(false), _success(false) {
	// Decorative targets animate but do not count toward the five-object search.
	for (uint i = 0; i < ARRAYSIZE(_counted); ++i)
		_counted[i] = kHotspots[i].initialCounted != 0;
}

void CastleEncounter::prepare() {
	// Abracadabra: Gruzzo and the torch; Baba Yaga: Skeleton and the sword.
	loadInterface(_vm->getGameType() == kGameTypeAbracadabra ? "argh.ani" : "castle.ani", "castle.cmp");
}

void CastleEncounter::drawFrame(int16 animation, int16 frame, bool restore) {
	if (animation < 0)
		return;
	const ANIFile::Animation &info = _ani->getAnimationInfo(animation);
	if (frame >= info.frameCount)
		return;
	int16 x = animation == 25 ? _cursorX : info.x;
	int16 y = (animation == 25 ? _cursorY : info.y) - 49;
	if (!restore) {
		_ani->draw(*_frame, animation, frame, x, y);
	} else {
		const ANIFile::FrameArea &area = info.frameAreas[frame];
		_frame->blit(*_background, x + area.left, y + area.top,
			x + area.right, y + area.bottom, x + area.left, y + area.top);
	}
}

void CastleEncounter::publish() {
	if (_vm->getPlatform() == Common::kPlatformAmiga) {
		_vm->_draw->_backSurface->blit(*_frame, 16, 0, 319, 142, 16, 49);
		_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 16, 49, 319, 191);
	} else {
		_vm->_draw->_backSurface->blit(*_frame, 15, 0, 319, 191, 15, 49);
		_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 15, 49, 319, 199);
	}
}

void CastleEncounter::renderFight() {
	drawFrame(_animation, _animationFrame);
	if (_spellActive)
		drawFrame(_spell, _spellFrame);
	_cursorX = _x;
	_cursorY = _y;
	if (_animation < 19 || _animation > 24)
		_game.showActionCursor(*_ani, 25, _cursorFrame, _cursorX, _cursorY);
	else
		_game.removeSelectionCursor();
	publish();
	if (_spellActive)
		drawFrame(_spell, _spellFrame, true);
	drawFrame(_animation, _animationFrame, true);
}

void CastleEncounter::renderSearch() {
	drawFrame(_animation, _animationFrame);
	if (!_cursorFrame) {
		_cursorX = _x;
		_cursorY = _y;
	}
	const bool liveCursor = !_cursorFrame && !_pressed;
	if (liveCursor)
		_game.showActionCursor(*_ani, 25, 0, _cursorX, _cursorY);
	else {
		_game.removeSelectionCursor();
		drawFrame(25, _cursorFrame);
	}
	publish();
	if (!liveCursor)
		drawFrame(25, _cursorFrame, true);
	drawFrame(_animation, _animationFrame, true);
}

CastleEncounter::Event CastleEncounter::next(int16 x, int16 y, bool pressed, uint32 heldKeys) {
	while (true) {
		switch (_stage) {
		case kFightInput:
			if (_animation < 19 || _animation > 24)
				readInput(x, y, pressed, heldKeys);
			renderFight();
			_alternate = !_alternate;
			if ((_animation != 16 && _animation != 17) || !_alternate)
				++_animationFrame;
			if (_cursorFrame && ++_cursorFrame >= _ani->getAnimationInfo(25).frameCount)
				_cursorFrame = 0;
			if ((_animation == 16 || _animation == 17) && animationEnded()) {
				_animation = _animation == 16 ? 17 : 16;
				_animationFrame = 0;
			}
			if (_animation >= 13 && _animation <= 15 && animationEnded()) {
				_animation = _animation == 14 ? 16 : 17;
				_animationFrame = 0;
				++_damage;
				_game.changeScore(-100);
				copyScore();
			} else if (_animation == 12 && animationEnded()) {
				_animation = 17;
				_animationFrame = 0;
			}
			if (_damage == 10) {
				_success = false;
				_stage = kFailureWait;
				return Event(kText, 10);
			}
			_stage = kFrameEffects;
			if (_animation == 19 && animationEnded()) {
				_stage = kVictoryChoice;
				return Event(kText, 6);
			}
			if ((_animation == 20 || _animation == 21) && animationEnded()) {
				_animation = _animation == 20 ? 23 : 22;
				_animationFrame = 0;
				playCue(2);
				return Event(kText, 7);
			}
			if ((_animation == 22 || _animation == 23) && animationEnded()) {
				_stage = kVictoryResume;
				return Event(kWait, 700);
			}
			if (_animation == 24 && animationEnded())
				_stage = kVictoryHold;
			break;
		case kVictoryChoice:
			_animation = 20 + _vm->_util->getRandom(2);
			_animationFrame = 0;
			_stage = kFrameEffects;
			break;
		case kVictoryResume:
			// Native animation24 keeps the preceding animation's completed frame.
			_animation = 24;
			_stage = animationEnded() ? kVictoryHold : kFrameEffects;
			break;
		case kVictoryHold:
			// The originals hold the last fight frame before revealing the search.
			_stage = kEnterSearch;
			return Event(kPostFightHold, 2000);
		case kEnterSearch:
			_vm->_draw->_backSurface->blit(*_background, 0, 0, 319, 199, 0, 49);
			_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 0, 49, 319, 199);
			_frame->blit(*_background);
			_game.changeScore(1500);
			// The search reuses these surfaces after the second score award.
			copyScore();
			_cursorFrame = 0;
			_animation = -1;
			_searching = true;
			_game._clock.setFrameRate(20);
			_stage = kSearchInput;
			break;
		case kFrameEffects:
			_stage = kFlashEffects;
			if (_vm->getPlatform() == Common::kPlatformDOS && _animation >= 13 && _animation <= 15 &&
				(_animationFrame == 3 || _animationFrame == 8)) {
				const int16 first = _vm->_util->getRandom(8);
				const int16 second = _vm->_util->getRandom(8);
				return Event(kShake, first, second);
			}
			break;
		case kFlashEffects:
			_stage = kSpell;
			if (_animation >= 13 && _animation <= 15 && _animationFrame == 6)
				return Event(kFlash, 1, 0);
			if (_animation == 12 && _animationFrame == 5)
				playCue(4);
			break;
		case kSpell:
			_stage = kEnemyAttack;
			if (_spellActive && _animation != 19) {
				if (++_spellFrame >= _ani->getAnimationInfo(_spell).frameCount) {
					_spellActive = false;
					_animation = 12;
					_animationFrame = 0;
					if (++_hits == 10) {
						playCue(5);
						_game.changeScore(1500);
						copyScore();
						_animation = 19;
						_animationFrame = 0;
						_won = true;
						return Event(kText, 5);
					}
					return Event(kText, 3, _vm->_util->getRandom(4));
				}
				break;
			}
			if ((_animation < 19 || _animation > 24) && _pressed) {
				if (_x >= 115 && _x <= 182 && _y >= 74 && _y <= 169 && !_won) {
					_cursorFrame = 1;
					_spellActive = true;
					_spell = 8 + _vm->_util->getRandom(3);
					// Show the whole spell; the originals start on its final pose.
					_spellFrame = 0;
					return Event(kFlash, 1, 0);
				}
				return Event(kText, 4, _vm->_util->getRandom(3));
			}
			break;
		case kEnemyAttack:
			_stage = kFightFrame;
			if (++_attackClock > 49 && (_animation == 16 || _animation == 17)) {
				playCue(0);
				_stage = kEnemyAnimation;
				return Event(kText, 2, _vm->_util->getRandom(3));
			}
			break;
		case kEnemyAnimation:
			_attackClock = 0;
			_animation = 13 + _vm->_util->getRandom(3);
			_animationFrame = 0;
			_stage = kFightFrame;
			break;
		case kFightFrame:
			_stage = kFightInput;
			return Event(kFrame);
		case kSearchInput:
			readInput(x, y, pressed, heldKeys);
			renderSearch();
			if (_cursorFrame)
				++_cursorFrame;
			if (_animation >= 0 && ++_animationFrame >= _ani->getAnimationInfo(_animation).frameCount) {
				if (_animation == 24) {
					_stage = kSearchWait;
					return Event(kText, 9);
				}
				_animation = -1;
			}
			if (_cursorFrame >= _ani->getAnimationInfo(25).frameCount)
				_cursorFrame = 0;
			_stage = kSearchFrame;
			if (_animation < 0 && _pressed) {
				for (uint i = 0; i < 15; ++i) {
					const Hotspot &item = kHotspots[i];
					if (_x <= item.left || _x >= item.right || _y <= item.top || _y >= item.bottom)
						continue;
					_animation = item.animation;
					_animationFrame = 0;
					if (!_counted[i])
						++_objects;
					_stage = kSearchSample;
					if (_objects < 5 || _counted[i]) {
						_counted[i] = true;
						return Event(kText, 8, _vm->_util->getRandom(6));
					}
					_animation = 24;
					return Event(kText, 3, 0);
				}
			}
			break;
		case kSearchSample:
			_stage = kSearchFrame;
			playCue(_animation == 24 ? 2 : 3);
			break;
		case kSearchFrame:
			if (_pressed)
				_cursorFrame = 1;
			_stage = kSearchInput;
			return Event(kFrame);
		case kFailureWait:
			_stage = kDone;
			return Event(kWait, 700);
		case kSearchWait:
			_stage = kFinalDelay;
			return Event(kWait, 700);
		case kFinalDelay:
			_stage = kFinalScore;
			return Event(kDelay, 4000);
		case kFinalScore:
			_success = true;
			_stage = kDone;
			_game.changeScore(1500);
			break;
		case kDone:
			return Event(kComplete, _success ? 1 : 0);
		}
	}
}

// Chapter 5 boss encounter

const BossEncounter::Hotspot BossEncounter::kHotspots[16] = {
	{ 3,  95, 165, 115, 187, 0, 0}, { 4, 188, 126, 224, 170, 1, 0},
	{ 0, 174,  78, 207, 107, 1, 0}, { 1,  20,  52,  43,  70, 1, 0},
	{ 2,  18, 110,  52, 183, 0, 0}, { 5,  61, 152,  90, 184, 0, 0},
	{ 6, 123, 123, 155, 149, 0, 0}, { 7, 263, 141, 304, 186, 0, 0},
	{ 8, 223,  53, 253,  73, 0, 0}, { 9, 222,  93, 242, 117, 0, 0},
	{10, 123, 123, 155, 149, 0, 0}, {11, 128, 166, 154, 186, 0, 0},
	{27, 156,  74, 177,  91, 0, 1}, {26,  65, 105,  77, 123, 0, 1},
	{25,  54,  74,  75,  94, 0, 1}, {24, 154, 162, 191, 185, 0, 1}
};

BossEncounter::BossEncounter(OnceUpon &game) :
	Encounter(game, 12, true), _stage(kFightInput),
	_cursorFrame(0), _spell(0), _spellFrame(0), _missFrame(0),
	_cursorX(160), _cursorY(100), _missX(0), _missY(0),
	_attackClock(0), _damage(0), _hits(0), _objects(0), _spellActive(false),
	_toggled(false), _success(false) {
	for (uint i = 0; i < ARRAYSIZE(_counted); ++i)
		_counted[i] = kHotspots[i].initialCounted != 0;
}

void BossEncounter::prepare() {
	// Satanicus guards the bracelet; Baba Yaga guards the glove.
	const bool abra = _vm->getGameType() == kGameTypeAbracadabra;
	loadInterface(abra ? "ogre.ani" : "sorciere.ani", abra ? "ogr.cmp" : "sorciere.cmp");
}

void BossEncounter::drawFrame(Surface &dest, int16 animation, int16 frame, bool restore) {
	if (animation < 0 || frame < 0)
		return;
	const ANIFile::Animation &info = _ani->getAnimationInfo(animation);
	if (frame >= info.frameCount)
		return;
	const int16 x = animation == 22 ? _cursorX : animation == 21 ? _missX : info.x;
	const int16 y = (animation == 22 ? _cursorY : animation == 21 ? _missY : info.y) - 49;
	if (!restore) {
		_ani->draw(dest, animation, frame, x, y);
	} else {
		const ANIFile::FrameArea &area = info.frameAreas[frame];
		dest.blit(*_background, x + area.left, y + area.top,
			x + area.right, y + area.bottom, x + area.left, y + area.top);
	}
}

void BossEncounter::publish(bool fight) {
	const int16 left = fight && _vm->getPlatform() == Common::kPlatformAmiga ? 16 : 15;
	_vm->_draw->_backSurface->blit(*_frame, left, 0, 319, 191, left, 49);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, left, 49, 319, 199);
}

void BossEncounter::renderFight() {
	for (uint i = 0; i < 16; ++i)
		if (kHotspots[i].visible)
			drawFrame(*_frame, kHotspots[i].animation, 0);
	drawFrame(*_frame, _animation, _animationFrame);
	if (_spellActive)
		drawFrame(*_frame, _spell, _spellFrame);
	_cursorX = _x;
	_cursorY = _y;
	_game.showActionCursor(*_ani, 22, _cursorFrame, _cursorX, _cursorY);
	if (_missFrame)
		drawFrame(*_frame, 21, _missFrame);
	publish(true);
	if (_missFrame)
		drawFrame(*_frame, 21, _missFrame, true);
	if (_spellActive)
		drawFrame(*_frame, _spell, _spellFrame, true);
	drawFrame(*_frame, _animation, _animationFrame, true);
	for (int i = 15; i >= 0; --i)
		if (kHotspots[i].visible)
			drawFrame(*_frame, kHotspots[i].animation, 0, true);
}

void BossEncounter::renderSearch() {
	const bool amiga = _vm->getPlatform() == Common::kPlatformAmiga;
	if (!amiga)
		drawFrame(*_frame, _animation, _animationFrame);
	for (uint i = 0; i < 16; ++i)
		if (kHotspots[i].visible && kHotspots[i].animation != _animation)
			drawFrame(*_frame, kHotspots[i].animation, 0);
	if (amiga)
		drawFrame(*_frame, _animation, _animationFrame);
	if (_animation == 6 || _animation == 10)
		drawFrame(*_background, _animation, _animationFrame);
	if (!_cursorFrame) {
		_cursorX = _x;
		_cursorY = _y;
	}
	const bool liveCursor = !_cursorFrame && !_pressed;
	if (liveCursor)
		_game.showActionCursor(*_ani, 22, 0, _cursorX, _cursorY);
	else {
		_game.removeSelectionCursor();
		drawFrame(*_frame, 22, _cursorFrame);
	}
	publish(false);
	if (!liveCursor)
		drawFrame(*_frame, 22, _cursorFrame, true);
	for (int i = 15; i >= 0; --i)
		if (kHotspots[i].visible && kHotspots[i].animation != _animation)
			drawFrame(*_frame, kHotspots[i].animation, 0, true);
	drawFrame(*_frame, _animation, _animationFrame, true);
}

BossEncounter::Event BossEncounter::next(int16 x, int16 y, bool pressed, uint32 heldKeys) {
	while (true) {
		switch (_stage) {
		case kFightInput:
			readInput(x, y, pressed, heldKeys);
			renderFight();
			if (_missFrame && ++_missFrame >= _ani->getAnimationInfo(21).frameCount)
				_missFrame = 0;
			++_animationFrame;
			if (_cursorFrame && ++_cursorFrame >= _ani->getAnimationInfo(22).frameCount)
				_cursorFrame = 0;
			if ((_animation == 12 || _animation == 14 || _animation == 19) && animationEnded()) {
				_animation = _animation == 12 ? 14 : 12;
				_animationFrame = 0;
			}
			if (_animation == 13 && animationEnded()) {
				_animation = 12;
				_animationFrame = 0;
				++_damage;
				_game.changeScore(-500);
				copyScore();
			}
			if (_damage == 5) {
				_stage = kFailureWait;
				return Event(kText, 8);
			}
			_stage = kFrameEffects;
			if (_animation == 18 && animationEnded()) {
				_stage = kVictoryRestore;
				return Event(kFlash, 2, 1);
			}
			break;
		case kVictoryRestore:
			_vm->_draw->_backSurface->blit(*_background, 0, 0, 319, 150, 0, 49);
			_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 0, 49, 319, 199);
			_frame->blit(*_background);
			_stage = kVictoryText;
			return Event(kText, 5);
		case kVictoryText:
			_cursorFrame = 0;
			_animation = -1;
			_spellActive = _toggled = false;
			_stage = kEnterSearch;
			return Event(kWait, 700);
		case kEnterSearch:
			_game._clock.setFrameRate(20);
			_stage = kSearchInput;
			break;
		case kFrameEffects:
			_stage = kFlashEffects;
			if (_vm->getPlatform() == Common::kPlatformDOS && _animation == 13 &&
				(_animationFrame == 3 || _animationFrame == 8)) {
				const int16 first = _vm->_util->getRandom(8);
				const int16 second = _vm->_util->getRandom(8);
				return Event(kShake, first, second);
			}
			break;
		case kFlashEffects:
			_stage = kSpell;
			if (_animation == 13 && _animationFrame == 13)
				return Event(kFlash, 1, 0);
			break;
		case kSpell:
			_stage = kEnemyAttack;
			if (_spellActive && _animation != 18) {
				if (!_spellFrame) {
					playCue(1);
				}
				if (++_spellFrame >= _ani->getAnimationInfo(_spell).frameCount) {
					_spellActive = false;
					_animation = 19;
					_animationFrame = 0;
					++_hits;
					playCue(4);
					_stage = kHit;
					return Event(kFlash, 1, 0);
				}
				break;
			}
			if (_pressed) {
				_cursorFrame = 1;
				const int16 spell = _x >= 115 && _x <= 174 && _y >= 53 && _y <= 97 ? 15 :
					_x >= 115 && _x <= 146 && _y >= 99 && _y <= 172 ? 16 :
					_x >= 148 && _x <= 188 && _y >= 99 && _y <= 172 ? 17 : -1;
				if (spell >= 0) {
					_spell = spell;
					_spellActive = true;
					_spellFrame = 0;
				} else {
					if (!_missFrame) {
						_missFrame = 1;
						_missX = _x;
						_missY = _y;
					}
					return Event(kText, 4, _vm->_util->getRandom(3));
				}
			}
			break;
		case kHit:
			_stage = kEnemyAttack;
			if (_hits != 15)
				return Event(kText, 3, _vm->_util->getRandom(4));
			playCue(5);
			_game.changeScore(2000);
			copyScore();
			_animation = 18;
			_animationFrame = 0;
			break;
		case kEnemyAttack:
			_stage = kFightFrame;
			if (++_attackClock > 49 && _animation == 12) {
				playCue(0);
				_stage = kEnemyAnimation;
				return Event(kText, 2, _vm->_util->getRandom(3));
			}
			break;
		case kEnemyAnimation:
			_attackClock = 0;
			_animation = 13;
			_animationFrame = 0;
			_stage = kFightFrame;
			break;
		case kFightFrame:
			_stage = kFightInput;
			return Event(kFrame);
		case kSearchInput:
			readInput(x, y, pressed, heldKeys);
			renderSearch();
			if (_cursorFrame > 0)
				++_cursorFrame;
			if (_animation >= 0 && ++_animationFrame >= _ani->getAnimationInfo(_animation).frameCount) {
				if (_animation == 23) {
					_stage = kSearchWait;
					return Event(kText, 6);
				}
				_animation = -1;
				_cursorFrame = 0;
			}
			if (_cursorFrame >= _ani->getAnimationInfo(22).frameCount)
				_cursorFrame = 0;
			_stage = kSearchFrame;
			if (_animation < 0 && _pressed)
				for (uint i = 0; i < 16; ++i) {
					const Hotspot &item = kHotspots[i];
					if (_x <= item.left || _x >= item.right || _y <= item.top || _y >= item.bottom)
						continue;
					_cursorFrame = -1;
					_animation = item.animation;
					_animationFrame = 0;
					if (!_counted[i])
						++_objects;
					if (_objects < 5 || _counted[i]) {
						_counted[i] = true;
						playCue(3);
						_stage = kSearchToggle;
						return Event(kText, 7, _vm->_util->getRandom(5));
					}
					_animation = 23;
					_stage = kFinalSample;
					return Event(kText, 3, 0);
				}
			break;
		case kSearchToggle:
			if (_animation == 6 || _animation == 10) {
				_counted[6] = _counted[10] = true;
				_animation = _toggled ? 10 : 6;
				_toggled = !_toggled;
			}
			_stage = kSearchFrame;
			break;
		case kFinalSample:
			_stage = kSearchFrame;
			playCue(2);
			break;
		case kSearchFrame:
			if (_pressed && _cursorFrame >= 0)
				_cursorFrame = 1;
			_stage = kSearchInput;
			return Event(kFrame);
		case kFailureWait:
			_stage = kDone;
			return Event(kWait, 700);
		case kSearchWait:
			_stage = kFinalScore;
			return Event(kWait, 700);
		case kFinalScore:
			_success = true;
			_stage = kDone;
			_game.changeScore(3000);
			break;
		case kDone:
			return Event(kComplete, _success ? 1 : 0);
		}
	}
}

// Encounter scene driver

const OnceUpon::EncounterSpec &OnceUpon::encounterSpec(bool boss) {
	static const EncounterSpec specs[] = {
		{kSceneCastle, 5, 16, 8, 4, "mechant1.gc"},
		{kSceneBoss, 7, 12, 12, 8, "sorciere.gc"}
	};
	return specs[boss ? 1 : 0];
}

void OnceUpon::beginEncounter(bool boss) {
	const EncounterSpec &spec = encounterSpec(boss);
	beginScene(spec.kind);
	Encounter *encounter = boss ? static_cast<Encounter *>(new BossEncounter(*this)) :
		new CastleEncounter(*this);
	_scene.reset(encounter);
	encounter->beginExterior();
}

void Encounter::drawExterior(Surface &dest) {
	const bool baba = _vm->getGameType() == kGameTypeBabaYaga;
	Assets::Image base = _assets.image(baba ? "maison3.cmp" : "maison1.cmp");
	dest.blit(*base);
	if (!_boss && baba) {
		Assets::Image overlay = _assets.image("maison1.cmp", 320, 72);
		dest.blit(*overlay, 0, 0, 135, 71, 100, 44);
		dest.blit(*overlay, 136, 0, 271, 71, 100, 116);
	} else if (_boss && !baba) {
		Assets::Image overlay = _assets.image("maison2.cmp", 320, 105);
		dest.blit(*overlay, 0, 0, 140, 103, 99, 83);
	}
}

void Encounter::beginExterior() {
	_game.fadeOut();
	_game.hideCursor();
	_game.setGamePalette(OnceUpon::encounterSpec(_boss).exteriorPalette);
	drawExterior(*_vm->_draw->_backSurface);
	_vm->_draw->forceBlit();
	_game.fadeIn();
	_waitStart = _game._clock.now();
	_waitDuration = 4000;
	_phase = kExteriorDelay;
	_game._clock.setFrameRate(50);
	_game._activations.clear();
	_game._freshInput = false;
	_game._acceptActivations = false;
	_vm->_util->clearKeyBuf();
}

void Encounter::beginFight() {
	_game.endReadingPage();
	const OnceUpon::EncounterSpec &spec = OnceUpon::encounterSpec(_boss);
	assert(!_ani);
	_game.fadeOut();
	_game.setGamePalette(spec.fightPalette);
	prepare();
	_game.drawScore();
	captureBackground();
	_captionBackground.reset(_game.captureSceneTextBackground(*_vm->_draw->_backSurface));
	_text.reset(_game.loadStoryGCT(_game.getLocFile(spec.text)));
	// English Abracadabra has two empty rows after the bracelet instruction.
	// Put the existing terminal wait on the last readable page.
	if (_boss && _vm->getGameType() == kGameTypeAbracadabra && _vm->_global->_language == kLanguageBritish)
		_text->trimEmptyTail(5, 2);
	_text->setText(1, _game._progress.name);
	loadCues();
	_game._clock.setFrameRate(spec.frameRate);
	_vm->_util->setMousePos(160, 100);
	_vm->_util->processInput();
	_game.discardActivationInput();
	_game._acceptActivations = true;
	_needsFadeIn = true;
	_captionPressed = false;
	drawCaption(0);
}

void Encounter::drawCaption(uint item, int selection) {
	_textItem = item;
	_text->reset();
	if (selection >= 0)
		_text->selectLine(item, selection);
	drawPage();
}

void Encounter::drawPage() {
	const bool terminal = _boss ? _textItem == 5 || _textItem == 6 || _textItem == 8 :
		_textItem == 7 || _textItem == 9 || _textItem == 10;
	_phase = _game.drawScenePage(*_text, *_captionBackground, _textItem, _captionStart, terminal) ? kPlaying : kCaption;
	if (_phase == kCaption)
		_game.holdActionCursor();
}

void CastleEncounter::filterFightTaps() {
	if (searching() || _game._activations.empty())
		return;
	int16 x, y;
	MouseButtons buttons;
	_vm->_util->getMouseState(&x, &y, &buttons);
	// The native fight samples live buttons. Keep one released tap at its
	// press coordinates when the next fight update can start a cast.
	bool keepTap = acceptsFightTap() && !(buttons & kMouseButtonsLeft);
	for (Common::List<OnceUpon::Activation>::iterator tap = _game._activations.begin(); tap != _game._activations.end();) {
		if (tap->buttons == kMouseButtonsLeft) {
			if (!keepTap) {
				tap = _game._activations.erase(tap);
				continue;
			}
			keepTap = false;
		}
		++tap;
	}
}

Section::Outcome Encounter::step() {
	if (_phase == kScoreDelay) {
		if (_game._clock.now() - _waitStart < _waitDuration) {
			_game.endFrame();
			return kSectionContinue;
		}
		_game.discardActivationInput();
		_phase = kPlaying;
	}
	if (_phase == kExteriorDelay) {
		if (_game._clock.now() - _waitStart >= _waitDuration) {
			_game._acceptActivations = true;
			_phase = kExteriorHold;
			_waitStart = _game._clock.now();
			// The four-second reveal already showed the art. Keep the
			// following skippable hold short on this text-free screen.
			_waitDuration = 2000;
			_game.beginReadingPage(OnceUpon::kReadingAny);
		}
		_game.endFrame();
		return kSectionContinue;
	}
	if (_phase == kPlaying)
		filterFightTaps();
	const bool reading = _game._readingKeys != OnceUpon::kNoReading;
	OnceUpon::SceneInput input = _game.readSceneInput(reading ? OnceUpon::kReadingInput : OnceUpon::kSceneInput);

	if (input.key == kKeyEscape || input.buttons == kMouseButtonsRight) {
		bool resumePage = false;
		const Outcome outcome = _game.sceneMenu(&resumePage);
		if (outcome != kSectionContinue)
			return outcome;
		if (_phase == kExteriorHold && resumePage)
			beginFight();
		return kSectionContinue;
	}
	if (_phase == kPostVictoryHold) {
		if (_game._clock.now() - _waitStart < _waitDuration) {
			_game.endFrame();
			return kSectionContinue;
		}
		_vm->_util->clearKeyBuf();
		_game._activations.clear();
		if (_vm->shouldQuit())
			return kSectionQuit;
		_phase = kPlaying;
		input = _game.readSceneInput();
	}
	const bool accepted = reading ? input.activation : _game.acceptCaption(input, _captionPressed);
#ifdef USE_TTS
	const bool caption = _phase == kCaption;
	const bool playing = _phase == kPlaying;
	if (accepted && ((playing && input.key == kKeySpace) || (caption && _queuedText >= 0)))
		_game.cancelNarration();
	if ((playing || caption) && _queuedText >= 0 &&
		!_game._narration.speakingOrStarting(g_system->getMillis())) {
		const int16 item = _queuedText;
		_queuedText = -1;
		drawCaption(item, _queuedSelection);
		if (caption || _phase == kCaption) {
			_game.endFrame();
			return kSectionContinue;
		}
	}
#endif
	if (_phase == kCaption) {
		if (
#ifdef USE_TTS
			_queuedText < 0 &&
#endif
			_game.captionReady(accepted, _captionStart))
			drawPage();
		_game.endFrame();
		return kSectionContinue;
	}
	if (_phase == kExteriorHold || _phase == kTimedWait) {
		const bool acknowledged = input.activation;
		if (acknowledged)
			_game.takeReadingAdvance();
#ifdef USE_TTS
		if (acknowledged)
			_game.cancelNarration();
#endif
		if (acknowledged || (_game._clock.now() - _waitStart >= _waitDuration
#ifdef USE_TTS
			&& !_game._narration.update(g_system->getMillis())
#endif
			)) {
			if (_phase == kExteriorHold)
				beginFight();
			else
				_phase = kPlaying;
			_game.endReadingPage();
		}
		_game.endFrame();
		return kSectionContinue;
	}
	bool pressed = input.activation || (input.buttons & kMouseButtonsLeft) || (input.keys & 0x0010) || input.key == kKeyReturn;
	const uint32 keys = input.activation ? input.keys & ~0xFU : input.keys;
	for (;;) {
		const Event event = next(input.x, input.y, pressed, keys);
		switch (event.type) {
		case Encounter::kText:
#ifdef USE_TTS
			if (_game._narration.speakingOrStarting(g_system->getMillis())) {
				_queuedText = event.value;
				_queuedSelection = event.selection;
				// Combat and search comments can be replaced during gameplay.
				if ((event.value >= 2 && event.value <= 4) || event.value == (_boss ? 7 : 8))
					break;
				_game.holdActionCursor();
				_phase = kCaption;
				_game.endFrame();
				return kSectionContinue;
			}
			_queuedText = -1;
#endif
			drawCaption(event.value, event.selection);
			if (_phase == kCaption) {
				_game.endFrame();
				return kSectionContinue;
			}
			break;
		case Encounter::kFlash:
			for (int16 i = 0; i < event.value; ++i) {
				_game.showTemporaryPalette(13);
				_vm->_video->retrace();
				_vm->_util->delay(50);
				_game._palette.showTarget();
				_vm->_video->retrace();
				if (event.selection)
					_vm->_util->delay(50);
			}
			break;
		case Encounter::kShake:
			_game.holdActionCursor();
			shake(event.value, event.selection);
			break;
		case Encounter::kWait:
			_game.holdActionCursor();
			_waitStart = _game._clock.now();
			_waitDuration = event.value * 20;
			_phase = kTimedWait;
			_game.endFrame();
			return kSectionContinue;
		case Encounter::kPostFightHold:
			_vm->_draw->blitInvalidated();
			_vm->_video->retrace();
			_waitStart = _game._clock.now();
			_waitDuration = event.value;
			_phase = kPostVictoryHold;
			_game.endFrame();
			return kSectionContinue;
		case Encounter::kDelay:
			assert(!_boss);
			_game.holdActionCursor();
			_waitStart = _game._clock.now();
			_waitDuration = event.value;
			_phase = kScoreDelay;
			_game.endFrame();
			return kSectionContinue;
		case Encounter::kFrame:
			if (_needsFadeIn) {
				_vm->_draw->forceBlit();
				_game.fadeIn();
				_needsFadeIn = false;
			}
			_game.endFrame();
			return kSectionContinue;
		case Encounter::kComplete:
			return event.value ? kSectionComplete : kSectionRetry;
		}
	}
}

void Encounter::shake(int16 first, int16 second) {
	if (_vm->getPlatform() != Common::kPlatformDOS)
		return;
	// Both DOS encounters write four retrace-separated display starts.
	// Keep the game's host PAL cadence, independent of encounter FPS.
	const int16 offsets[] = {first, 0, second, 0};
	_vm->_draw->blitInvalidated();
	for (uint i = 0; i < ARRAYSIZE(offsets); ++i) {
		_vm->_util->delay(20);
		_vm->_util->processInput();
		if (_vm->shouldQuit()) {
			g_system->setShakePos(0, 0);
			_vm->_video->retrace();
			return;
		}
		g_system->setShakePos(0, -offsets[i]);
		_vm->_video->retrace();
	}
}

} // End of namespace OnceUpon
} // End of namespace Gob
