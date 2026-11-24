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

#include "common/util.h"

#include "gob/anifile.h"
#include "gob/draw.h"
#include "gob/gob.h"
#include "gob/surface.h"
#include "gob/util.h"
#include "gob/video.h"
#include "gob/pregob/gctfile.h"
#include "gob/pregob/onceupon/dwelling.h"
#include "gob/pregob/onceupon/assets.h"
#include "gob/pregob/onceupon/onceupon.h"

namespace Gob {
namespace OnceUpon {

const DwellingSearch::Hotspot DwellingSearch::kCabin[13] = {
	{ 7, 134,  98, 178, 135, true, false},
	{15,  87, 161, 167, 185, false, false},
	{ 4,   0,   0,   0,   0, false, true},
	{ 5,  10, 136,  92, 185, false, false},
	{ 9, 177, 113, 211, 151, false, false},
	{11, 194, 115, 225, 151, false, false},
	{13, 165, 153, 220, 184, false, false},
	{16, 112, 135, 186, 158, false, false},
	{17,  18, 108,  64, 148, false, false},
	{18,  93, 115, 131, 151, false, false},
	{19, 204, 101, 237, 131, false, false},
	{35, 259,  80, 314, 176, false, false},
	{ 3,   0,   0,   0,   0, false, true}
};

const DwellingSearch::Hotspot DwellingSearch::kCastle[11] = {
	{ 4, 180,  47, 210,  83, false, false},
	{ 5,  10, 149,  65, 197, false, false},
	{ 7,   0,   0,   0,   0, true, false},
	{ 9, 228, 150, 307, 195, false, false},
	{11, 108, 106, 177, 138, false, false},
	{13, 102, 138, 141, 181, false, false},
	{15, 170,  69, 187,  99, false, false},
	{16, 263,  65, 303, 119, false, false},
	{17, 251, 108, 284, 144, false, false},
	{18, 138, 155, 161, 196, false, false},
	{19, 154, 124, 220, 173, false, false}
};

OnceUpon::DwellingHotspots::DwellingHotspots() : cabinMirrored(false), castleMirrored(false) {
	for (uint i = 0; i < ARRAYSIZE(cabin); ++i)
		cabin[i] = -1;
	for (uint i = 0; i < ARRAYSIZE(castle); ++i)
		castle[i] = -1;
	cabin[0] = castle[2] = 0;
}

void OnceUpon::DwellingHotspots::enter(bool inCabin, bool mirror) {
	// Abracadabra reverses the chosen layout before every movie, including
	// abandoned visits. Reflect the click instead of rewriting fixed bounds.
	if (mirror) {
		bool &mirrored = inCabin ? cabinMirrored : castleMirrored;
		mirrored = !mirrored;
	}
}

DwellingSearch::DwellingSearch(OnceUpon &game, bool cabin, GCTFile &text) : _vm(game._vm),
	_assets(*game._assets), _game(game), _hotspots(cabin ? kCabin : kCastle),
	_frames(cabin ? game._dwellingHotspots.cabin : game._dwellingHotspots.castle),
	_mirrored(cabin ? game._dwellingHotspots.cabinMirrored : game._dwellingHotspots.castleMirrored),
	_ani(nullptr), _background(nullptr), _text(&text),
	_phase(kPhaseNone), _textItem(0), _captionStart(0) {
	_state.inCabin = cabin;
	_state.activations = 0;
	_state.x = 160;
	_state.y = 100;
	_state.pressed = false;
	_state.rewardFrame = -1;
}

DwellingSearch::~DwellingSearch() {
	delete _background;
}

uint DwellingSearch::hotspotCount() const {
	return _state.inCabin ? ARRAYSIZE(kCabin) : ARRAYSIZE(kCastle);
}

void DwellingSearch::prepare(Surface &dest) {
	const char *name = _state.inCabin ? "cabane" : "chateau";
	_ani = _assets.animation(Common::String(name) + ".ani");
	_background = new Surface(320, 200, 1);
	const Common::String backgroundFile = Common::String(name) + ".cmp";
	Assets::Image artwork = _assets.image(backgroundFile);
	_background->blit(*artwork);

	for (uint i = 0; i < hotspotCount(); ++i) {
		if (_hotspots[i].background)
			drawFrame(*_background, _hotspots[i].animation, 0);
	}
	dest.blit(*_background);
}

void DwellingSearch::setPosition(int16 x, int16 y) {
	KeyboardInput::clampActionPointer(x, y);
	_state.x = x;
	_state.y = y;
}

void DwellingSearch::drawFrame(Surface &surface, uint animation, uint frame) const {
	const ANIFile::Animation &info = _ani->getAnimationInfo(animation);
	_ani->draw(surface, animation, frame, info.x, info.y);
}

void DwellingSearch::draw(Surface &dest) {
	assert(_ani && _background);
	// Reachable object and reward poses fit within x16..319/y49..191.
	// Restore that area and draw directly on the destination.
	dest.blit(*_background, 16, 49, 319, 191, 16, 49);
	for (uint i = 0; i < hotspotCount(); ++i) {
		if (_frames[i] < 0)
			continue;
		drawFrame(dest, _hotspots[i].animation, _frames[i]);
	}
	if (_state.rewardFrame >= 0) {
		drawFrame(dest, 28, _state.rewardFrame);
	}
	_game.showActionCursor(*_ani, 34, 0, _state.x, _state.y);
}

DwellingSearch::Event DwellingSearch::advance(bool down, bool activation) {
	assert(_ani);
	if (_state.rewardFrame >= 0 && ++_state.rewardFrame >= _ani->getAnimationInfo(28).frameCount)
		return kComplete;

	for (uint i = 0; i < hotspotCount(); ++i) {
		if (_frames[i] >= 0 && ++_frames[i] >= _ani->getAnimationInfo(_hotspots[i].animation).frameCount)
			_frames[i] = _hotspots[i].repeat ? 0 : -1;
	}

	bool activate = activation || (down && !_state.pressed);
	_state.pressed = down;
	if (!activate)
		return kNone;
	const int16 x = _mirrored ? 320 - _state.x : _state.x;
	for (uint i = 0; i < hotspotCount(); ++i) {
		const Hotspot &spot = _hotspots[i];
		if (_frames[i] != -1 || x <= spot.left || x >= spot.right ||
		    _state.y <= spot.top || _state.y >= spot.bottom)
			continue;
		_frames[i] = 0;
		++_state.activations;
		if (_state.activations < (_state.inCabin ? 4 : 7))
			return kComment;
		_state.rewardFrame = 0;
		return kReward;
	}
	return kNone;
}

void DwellingSearch::drawCaption(uint item) {
	_textItem = item;
	_text->reset();
	drawPage();
}

void DwellingSearch::drawPage() {
	int16 left, top, right, bottom;
	if (_text->clear(*_vm->_draw->_backSurface, left, top, right, bottom))
		_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, left, top, right, bottom);
#ifdef USE_TTS
	const bool drawn = _text->draw(*_vm->_draw->_backSurface, _textItem, *_game._plettre, 10, left, top, right, bottom, &_game._captionWords);
	_game._captionNarrationPending = drawn;
#else
	const bool drawn = _text->draw(*_vm->_draw->_backSurface, _textItem, *_game._plettre, 10, left, top, right, bottom);
#endif
	if (drawn) {
		_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, left, top, right, bottom);
		if (_textItem == 15)
			_game.beginReadingPage(OnceUpon::kReadingControls);
	}
	_captionStart = _game._clock.now();
}

void DwellingSearch::begin() {
#ifdef USE_TTS
	_game.cancelNarration();
#endif
	_game.fadeOut();
	_game.setGamePalette((_game._progress.house == 1 || _game._progress.house == 2) ? 14 : 15);
	prepare(*_vm->_draw->_backSurface);
	drawCaption(21);
	_game.drawScore();
	_vm->_draw->forceBlit();
	_game.fadeIn();
#ifdef USE_TTS
	_game.publishCaptionNarration();
#endif
	_game._clock.setFrameRate(20);
	_vm->_util->setMousePos(160, 100);
	_vm->_util->processInput();
	_game.discardActivationInput();
	_game._acceptActivations = true;
	_phase = kPhaseSearch;
}

SectionOutcome DwellingSearch::step() {
	if (_phase == kPhaseComplete)
		return kSectionComplete;
	OnceUpon::SceneInput input = _game.readSceneInput(
		_phase == kPhaseResult ? OnceUpon::kReadingInput : OnceUpon::kSceneInput);

	if (input.key == kKeyEscape || input.buttons == kMouseButtonsRight) {
		return _game.sceneMenu();
	}
	bool down = (input.buttons & kMouseButtonsLeft) || (input.keys & 0x0010);
	if (_phase == kPhaseResult) {
		if (_game.captionReady(input.activation, _captionStart))
			drawPage();
		if (_text->finished()) {
			_vm->_draw->blitInvalidated();
			_game.publishReadingPage();
#ifdef USE_TTS
			_game.publishCaptionNarration();
			if (_game._narration.update(g_system->getMillis())) {
				_game.endFrame();
				return kSectionContinue;
			}
#endif
			_phase = kPhaseComplete;
			return kSectionComplete;
		}
		_game.endFrame();
		return kSectionContinue;
	}
	const uint32 direction = input.activation ? 0 : input.keys & 0x000F;
	KeyboardInput::moveActionPointer(input.x, input.y, direction);
	setPosition(input.x, input.y);
	if (direction)
		_vm->_util->setMousePos(_state.x, _state.y);
	draw(*_vm->_draw->_backSurface);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 16, 49, 319, 191);
	_game.drawScore();
	_vm->_draw->blitInvalidated();
	DwellingSearch::Event event = advance(down, input.activation);
	if (event == DwellingSearch::kComment) {
		_text->selectLine(16, _vm->_util->getRandom(4));
		drawCaption(16);
		_game.replaceSample(_game._commonSounds[OnceUpon::kSoundClick], 0);
	} else if (event == DwellingSearch::kReward) {
		_game.replaceSample(_game._commonSounds[OnceUpon::kSoundReward], 0);
	} else if (event == DwellingSearch::kComplete) {
		_game.holdActionCursor();
		_phase = kPhaseResult;
		_game._activations.clear();
		_game.changeScore(1000);
		// A load during the score flash must not publish the old result caption.
		drawCaption(15);
		_vm->_draw->blitInvalidated();
		_game.publishReadingPage();
		return kSectionContinue;
	}
	_game.endFrame();
	return kSectionContinue;
}

} // End of namespace OnceUpon
} // End of namespace Gob
