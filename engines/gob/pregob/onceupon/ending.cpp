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

#include "gob/anifile.h"
#include "gob/draw.h"
#include "gob/global.h"
#include "gob/gob.h"
#include "gob/surface.h"
#include "gob/util.h"

#include "gob/pregob/gctfile.h"
#include "gob/pregob/onceupon/assets.h"
#include "gob/pregob/onceupon/ending.h"
#include "gob/pregob/onceupon/onceupon.h"

namespace Gob {
namespace OnceUpon {

const uint16 Ending::kAnimations[kObjectCount] = {0, 6, 9, 11};

Ending::Ending(OnceUpon &game) : _vm(game._vm),
	_assets(*game._assets), _game(game), _ani(nullptr), _background(nullptr), _frame(nullptr),
	_phase(kPhaseText), _captionStart(0) {
	for (uint i = 0; i < kObjectCount; ++i)
		_frames[i] = 0;
}

Ending::~Ending() {
	delete _background;
	delete _frame;
}

void Ending::prepare() {
	_ani = _assets.animation("fin.ani");
	if (_ani->getAnimationCount() <= kAnimations[kObjectCount - 1])
		error("Once Upon a Time: invalid ending animations");
	_background = new Surface(320, 200, 1);
	_frame = new Surface(320, 200, 1);
	_assets.drawFramedBackground(*_background, true);
	_frame->blit(*_background);
}

void Ending::drawInterface() {
	_vm->_draw->_backSurface->blit(*_background);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 0, 0, 319, 199);
}

void Ending::adoptBackground(const Surface &source) {
	_background->blit(source);
	_frame->blit(source);
}

void Ending::stepAnimation() {
	for (uint i = 0; i < kObjectCount; ++i) {
		const ANIFile::Animation &info = _ani->getAnimationInfo(kAnimations[i]);
		_ani->draw(*_frame, kAnimations[i], _frames[i], info.x, info.y);
	}
	_vm->_draw->_backSurface->blit(*_frame, 16, 50, 304, 187, 16, 50);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 16, 50, 304, 187);
	for (int i = kObjectCount - 1; i >= 0; --i) {
		const ANIFile::Animation &info = _ani->getAnimationInfo(kAnimations[i]);
		const ANIFile::FrameArea &area = info.frameAreas[_frames[i]];
		_frame->blit(*_background, info.x + area.left, info.y + area.top,
			info.x + area.right, info.y + area.bottom, info.x + area.left, info.y + area.top);
	}
	// These four counters wrap without applying the ANI cycle deltas.
	for (uint i = 0; i < kObjectCount; ++i) {
		if (++_frames[i] >= _ani->getAnimationInfo(kAnimations[i]).frameCount)
			_frames[i] = 0;
	}
}

void OnceUpon::beginEnding() {
	beginScene(kSceneEnding);
	Ending *ending = new Ending(*this);
	_scene.reset(ending);
	ending->begin();
}

void Ending::begin() {
	_game.fadeOut();
	_game.hideCursor();
	_game.setGamePalette(9);
	prepare();
	drawInterface();
	_game.drawScore();
	adoptBackground(*_vm->_draw->_backSurface);
	_captionBackground.reset(_game.captureSceneTextBackground(*_vm->_draw->_backSurface));
	_text.reset(_game.loadStoryGCT(_game.getLocFile("final.gc")));
	// Both FINAL.GCI files expose alternate-gender notes in the printed
	// proposal. Use the feminine wording for the caption and its TTS.
	if (_vm->_global->_language == kLanguageItalian)
		_text->setText(0, 3, "la sua unica figlia in sposa!");
	_text->setText(1, _game._progress.name);
	_game._clock.setFrameRate(12);
	drawPage();
	_vm->_draw->forceBlit();
	_game.fadeIn();
#ifdef USE_TTS
	_game.publishCaptionNarration();
#endif
	_game._acceptActivations = true;
}

void Ending::drawPage() {
	_phase = _game.drawScenePage(*_text, *_captionBackground, 0, _captionStart) ? kPhasePlaying : kPhaseText;
}

Section::Outcome Ending::step() {
	const OnceUpon::SceneInput input = _game.readSceneInput(_phase == kPhasePlaying ? OnceUpon::kFlushCharacters : OnceUpon::kReadingInput);

	if (input.key == kKeyEscape || (input.keys & 0x0040) || input.buttons == kMouseButtonsRight) {
		return _game.sceneMenu();
	}
	if (_phase == kPhaseText) {
		if (_game.captionReady(input.activation, _captionStart))
			drawPage();
#ifdef USE_TTS
		_game.publishCaptionNarration();
#endif
		if (_phase == kPhaseText)
			_game.endFrame();
		return kSectionContinue;
	}
	stepAnimation();
	_game.endFrame();
	return kSectionContinue;
}

} // End of namespace OnceUpon
} // End of namespace Gob
