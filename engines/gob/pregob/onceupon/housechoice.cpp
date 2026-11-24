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

#include "common/textconsole.h"
#include "common/util.h"

#include "gob/anifile.h"
#include "gob/draw.h"
#include "gob/gob.h"
#include "gob/surface.h"
#include "gob/util.h"
#include "gob/video.h"
#include "gob/pregob/gctfile.h"
#include "gob/pregob/txtfile.h"
#include "gob/pregob/onceupon/housechoice.h"
#include "gob/pregob/onceupon/assets.h"
#include "gob/pregob/onceupon/onceupon.h"

namespace Gob {
namespace OnceUpon {

const int16 HouseChoice::kBackgroundAnimations[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 17, 16, 15};

const HouseChoice::Drop HouseChoice::kDrops[4] = {
	{ 61, 123,  96, 149,  65, 14, 1, 150, 1},
	{ 16,  80,  87, 125,  30, 14, 1, 130, 0},
	{199, 118, 226, 137, 210, 14, 1, 136, 2},
	{229,  91, 304, 188, 200, 11, 0, 220, 3}
};

HouseChoice::HouseChoice(OnceUpon &game) : _vm(game._vm),
	_assets(*game._assets), _game(game), _phase(kPhaseNone), _ani(nullptr), _background(nullptr), _frame(nullptr),
	_flightAnimation(9), _flightFrame(0), _bundleAnimation(-1), _bundleFrame(0),
	_selection(-1), _soundCounter(20), _far(false), _dropped(false),
	_previousPress(false), _animateScenery(true) {
	memset(_positions, 0, sizeof(_positions));
	memset(_backgroundFrames, 0, sizeof(_backgroundFrames));
}

HouseChoice::~HouseChoice() {
	delete _background;
	delete _frame;
}

void HouseChoice::prepare(bool animateScenery) {
	_ani = _assets.animation("present.ani");
	if (_ani->getAnimationCount() != 19)
		error("Once Upon a Time: invalid house animations");
	_background = new Surface(320, 200, 1);
	_frame = new Surface(320, 200, 1);
	_assets.drawFramedBackground(*_background, false);
	_frame->blit(*_background);
	for (uint i = 0; i < ARRAYSIZE(_positions); ++i) {
		const ANIFile::Animation &info = _ani->getAnimationInfo(i);
		_positions[i][0] = info.x;
		_positions[i][1] = info.y;
	}
	_positions[9][0] = -80;
	// Only DOS has the native startup-controlled reduced-scenery path.
	_animateScenery = _vm->getPlatform() != Common::kPlatformDOS || animateScenery;
}

void HouseChoice::drawInterface() {
	_vm->_draw->_backSurface->blit(*_background);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 0, 0, 319, 199);
}

void HouseChoice::syncCaption(int16 left, int16 top, int16 right, int16 bottom) {
	_frame->blit(*_background, left, top, right, bottom, left, top);
}

void HouseChoice::drawPointer(Surface &cursor) const {
	_ani->draw(cursor, 18, 0, 0, 0);
}

int16 HouseChoice::selectedHouse() const {
	return _selection < 0 ? -1 : kDrops[_selection].house;
}

Common::Rect HouseChoice::frameBounds(int16 animation, int16 frame) const {
	const ANIFile::FrameArea &area = _ani->getAnimationInfo(animation).frameAreas[frame];
	return Common::Rect(_positions[animation][0] + area.left, _positions[animation][1] + area.top,
		_positions[animation][0] + area.right + 1, _positions[animation][1] + area.bottom + 1);
}

void HouseChoice::drawFrame(int16 animation, int16 frame, bool restore) {
	if (animation < 0)
		return;
	if (restore) {
		const Common::Rect area = frameBounds(animation, frame);
		_frame->blit(*_background, area.left, area.top, area.right - 1, area.bottom - 1, area.left, area.top);
	} else {
		_ani->draw(*_frame, animation, frame, _positions[animation][0], _positions[animation][1]);
	}
}

void HouseChoice::render() {
	if (_animateScenery)
		for (uint i = 0; i < ARRAYSIZE(kBackgroundAnimations); ++i)
			drawFrame(kBackgroundAnimations[i], _backgroundFrames[i]);
	drawFrame(_bundleAnimation, _bundleFrame);
	drawFrame(_flightAnimation, _flightFrame);
	_vm->_draw->_backSurface->blit(*_frame, 16, 12, 304, 187, 16, 12);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 16, 12, 304, 187);
	drawFrame(_flightAnimation, _flightFrame, true);
	drawFrame(_bundleAnimation, _bundleFrame, true);
	if (_animateScenery)
		for (int i = ARRAYSIZE(kBackgroundAnimations) - 1; i >= 0; --i)
			drawFrame(kBackgroundAnimations[i], _backgroundFrames[i], true);
}

bool HouseChoice::bundleLanded() const {
	if (_bundleAnimation < 0)
		return false;
	assert(_selection >= 0 && _selection < ARRAYSIZE(kDrops));
	return frameBounds(_bundleAnimation, _bundleFrame).bottom - 1 > kDrops[_selection].landingY;
}

void HouseChoice::advance() {
	if ((_flightAnimation == 9 || _flightAnimation == 10) && _positions[_flightAnimation][0] > 329) {
		_far = true;
		_flightFrame = 0;
		_flightAnimation += 3;
		_positions[_flightAnimation][0] = 330;
	}
	if ((_flightAnimation == 12 || _flightAnimation == 13) && _positions[_flightAnimation][0] < -30) {
		_far = false;
		_flightFrame = 0;
		_flightAnimation -= 3;
		_positions[_flightAnimation][0] = -50;
	}
	for (uint i = 0; i < ARRAYSIZE(kBackgroundAnimations); ++i)
		if (++_backgroundFrames[i] >= _ani->getAnimationInfo(kBackgroundAnimations[i]).frameCount)
			_backgroundFrames[i] = 0;
	const ANIFile::Animation &flight = _ani->getAnimationInfo(_flightAnimation);
	if (++_flightFrame >= flight.frameCount) {
		_flightFrame = 0;
		_positions[_flightAnimation][0] += flight.deltaX;
		_positions[_flightAnimation][1] += flight.deltaY;
	}
	if (_bundleAnimation >= 0) {
		const ANIFile::Animation &bundle = _ani->getAnimationInfo(_bundleAnimation);
		if (++_bundleFrame >= bundle.frameCount) {
			_bundleFrame = 0;
			_positions[_bundleAnimation][0] += bundle.deltaX;
			_positions[_bundleAnimation][1] += bundle.deltaY;
		}
	}
}

void HouseChoice::dropBundle() {
	if (_selection < 0 || _dropped)
		return;
	const Drop &drop = kDrops[_selection];
	const Common::Rect area = frameBounds(_flightAnimation, _flightFrame);
	if (drop.dropWhileFar != _far || area.left < drop.dropX - 5 || area.left > drop.dropX + 5)
		return;
	_positions[_flightAnimation + 1][0] = _positions[_flightAnimation][0];
	_bundleAnimation = drop.bundleAnimation;
	_positions[_bundleAnimation][0] = _far ? area.left : area.right - 1;
	_positions[_bundleAnimation][1] = area.bottom - 1;
	++_flightAnimation;
	_dropped = true;
}

bool HouseChoice::selectHouse(int16 x, int16 y) {
	if (_selection >= 0)
		return false;
	for (uint i = 0; i < ARRAYSIZE(kDrops); ++i) {
		const Drop &drop = kDrops[i];
		if (drop.left < x && x < drop.right && drop.top < y && y < drop.bottom) {
			_selection = i;
			return true;
		}
	}
	return false;
}

void OnceUpon::beginHouseChoice() {
	finishHouseChoice();
	beginScene(kSceneHouse);
	HouseChoice *house = new HouseChoice(*this);
	_scene.reset(house);
	house->enter();
}

void OnceUpon::finishHouseChoice() {
	if (_sceneKind != kSceneHouse)
		return;
	const HouseChoice *choice = static_cast<HouseChoice *>(_scene.get());
	const int16 house = choice->selectedHouse();
	if (house >= 0)
		_progress.house = house;
	stopSound();
	finishScene();
}

void HouseChoice::enter() {
	_game.fadeOut();
	_game.hideCursor();
	_game.setGamePalette(0);
	prepare();
	drawInterface();
	_prompt.reset(_game.loadTXT(_game.getLocFile("ouva.tx"), TXTFile::kFormatStringPositionColor));
	int16 left = 0, top = 0, right = 0, bottom = 0;
#ifdef USE_TTS
	const bool drawn = _prompt->draw(captionBackground(), left, top, right, bottom, &_game._plettre, 1, -1, &_game._captionWords);
#else
	const bool drawn = _prompt->draw(captionBackground(), left, top, right, bottom, &_game._plettre, 1, -1);
#endif
	if (drawn)
		syncCaption(left, top, right, bottom);
#ifdef USE_TTS
	_game._captionNarrationPending = true;
#endif
	_text.reset(_game.loadStoryGCT(_game.getLocFile("choix.gc")));
	render();
	Surface cursor(16, 16, 1);
	drawPointer(cursor);
	_game.showSelectionCursor(cursor);
	_vm->_draw->forceBlit();
	_game.fadeIn();
	_game._clock.setFrameRate(12);
	_vm->_util->processInput();
	_game.discardActivationInput();
	_game._acceptActivations = true;
	_phase = kPhasePlaying;
}

void HouseChoice::drawSelectionCaption() {
	Surface &background = captionBackground();
	int16 left = 17, top = 18, right = 303, bottom = 41;
	int16 promptLeft = 0, promptTop = 0, promptRight = 0, promptBottom = 0;
	const int sky = _vm->getGameType() == kGameTypeBabaYaga ? 2 : 0;
	const bool erased = _prompt->draw(background,
		promptLeft, promptTop, promptRight, promptBottom, &_game._plettre, 1, sky);
#ifdef USE_TTS
	_game._captionNarrationPending = _text->draw(background, 2, *_game._plettre, 10, left, top, right, bottom, &_game._captionWords);
#else
	_text->draw(background, 2, *_game._plettre, 10, left, top, right, bottom);
#endif
	// Baba Yaga's French and German prompts extend below the GCT caption.
	if (erased) {
		left = MIN(left, promptLeft);
		top = MIN(top, promptTop);
		right = MAX(right, promptRight);
		bottom = MAX(bottom, promptBottom);
	}
	syncCaption(left, top, right, bottom);
	_vm->_draw->_backSurface->blit(background, left, top, right, bottom, left, top);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, left, top, right, bottom);
	_vm->_draw->blitInvalidated();
	_vm->_video->retrace();
}

Section::Outcome HouseChoice::step() {
	if (_phase == kPhasePlaying) {
		if (++_soundCounter == 50) {
			_soundCounter = _vm->_util->getRandom(30);
			_game.playSound(OnceUpon::kSoundCourier);
		}
		_phase = kPhaseInput;
		return kSectionContinue;
	}

	OnceUpon::SceneInput input = _game.readSceneInput();
	if (input.key == kKeyEscape || input.buttons == kMouseButtonsRight)
		return _game.sceneMenu();

	assert(_phase == kPhaseInput);
	if (!input.activation && (input.keys & 15)) {
		KeyboardInput::moveActionPointer(input.x, input.y, input.keys);
		KeyboardInput::clampActionPointer(input.x, input.y);
		_vm->_util->setMousePos(input.x, input.y);
	}
	const bool pressed = input.activation || (input.buttons & kMouseButtonsLeft) ||
		(input.keys & 0x0010) || input.key == kKeyReturn;
	const bool click = input.activation || (pressed && !_previousPress);
	_previousPress = pressed;
	if (click) {
#ifdef USE_TTS
		_game._narration.cancel();
#endif
		_game.playSound(OnceUpon::kSoundClick);
	}

	render();
	if (bundleLanded()) {
		_vm->_draw->blitInvalidated();
		_vm->_video->retrace();
		return kSectionComplete;
	}
	advance();
	if (pressed && selectHouse(input.x, input.y)) {
		const int16 selection = selectedHouse();
		_game.removeSelectionCursor();
		_text->selectLine(3, selection);
		_text->selectLine(4, selection);
		// Every shipped house caption fits one page. Publish it before the drop.
		drawSelectionCaption();
	}

	dropBundle();
	_phase = kPhasePlaying;
	_game.endFrame();
	return kSectionContinue;
}

} // End of namespace OnceUpon
} // End of namespace Gob
