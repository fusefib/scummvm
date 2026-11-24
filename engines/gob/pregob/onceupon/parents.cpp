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
 *
 *
 * This file is dual-licensed.
 * In addition to the GPLv3 license mentioned above, this code is also
 * licensed under LGPL 2.1. See LICENSES/COPYING.LGPL file for the
 * full text of the license.
 *
 */

#include "common/system.h"

#include "gob/gob.h"
#include "gob/global.h"
#include "gob/util.h"
#include "gob/draw.h"
#include "gob/surface.h"
#include "gob/video.h"

#include "gob/sound/sound.h"

#include "gob/pregob/gctfile.h"

#include "gob/pregob/onceupon/parents.h"
#include "gob/pregob/onceupon/runtime_clock.h"
#include "gob/pregob/onceupon/palette.h"
#include "gob/pregob/onceupon/assets.h"
#include "gob/pregob/onceupon/dwelling.h"
#include "gob/pregob/onceupon/onceupon.h"

namespace Gob {

namespace OnceUpon {

const char *Parents::Movie::kSound[kSoundCount] = {
	"rire.snd", // kSoundCackle
	"tonn.snd"  // kSoundThunder
};

// Both games rewind at these gates until 100 movie frames have elapsed.
// SEQFile increments the written frame before drawing the next frame.
const uint16 Parents::Movie::kHoldFrames[kHoldCount][2] = {
	{ 77,  72}, {109, 105}, {145, 141}, {454, 446},
	{464, 456}, {474, 466}, {484, 476}
};

// The cabin and castle movies have distinct cues in both titles.
// Thunder follows each flash by 15 movie frames.
const uint16 Parents::Movie::kLightningFrames[2][9] = {
	{188, 228, 237, 257, 275, 426, 0xFFFF, 0xFFFF, 0xFFFF},
	{209, 216, 237, 248, 272, 277, 403, 408, 417}
};

Parents::Movie::Movie(OnceUpon &game, const Common::String &seq, GCTFile &caption) :
	SEQFile(game._vm, seq, game._assets.get()), _game(game), _clock(game._clock), _palette(game._palette),
	_house(game._progress.house), _font(game._plettre),
	_paletteSize(kPaletteSize), _normalPalette(kGamePalettes[_house == 1 || _house == 2 ? 14 : 15]),
	_brightPalette(kGamePalettes[13]), _gct(&caption),
	_frameCounter(0), _finished(false),
	_textItem(0), _textStart(0),
	_sequenceUpdating(false), _textFramePending(false), _waitingText(false) {

	// Load sounds
	for (int i = 0; i < kSoundCount; i++)
		Assets::loadSample(_vm, _sounds[i], kSound[i]);
}

Parents::Movie::~Movie() {
	endPlay();
	_game.endReadingPage();
	// The mixer borrows these samples until playback is stopped.
	for (uint i = 0; i < kSoundCount; ++i)
		_vm->_sound->blasterStop(0, &_sounds[i]);
}

void Parents::Movie::changeFrameRate(int16 rate) {
	_clock.setFrameRate(rate);
}

void Parents::Movie::waitFrame() {
	_game.publishReadingPage();
	if (isSuspended() || _game._pendingRestore || _vm->shouldQuit())
		return;
	_clock.waitFrame();
}

void Parents::Movie::begin() {
#ifdef USE_TTS
	discardNarration();
#endif
	_frameCounter = 0;
	_sequenceUpdating = _textFramePending = false;
	_finished = false;
	_waitingText = false;
	_game.endReadingPage();
	// The native parents movie sets 12 fps after reading the SEQ header.
	beginPlay(12);
}

bool Parents::Movie::stepPlay() {
	if (isSuspended())
		return false;
	if (_finished)
		return false;
	if (!_waitingText || hasPublishedFrame()) {
		_sequenceUpdating = true;
		const bool playing = SEQFile::stepPlay();
		_sequenceUpdating = false;
		if (isSuspended())
			return false;
#ifdef USE_TTS
		if (isAborted() || _vm->shouldQuit())
			discardNarration();
		else
			publishNarration();
		_game._narration.update(g_system->getMillis());
#endif
		return playing && !_finished;
	}
	if (_vm->shouldQuit() || isAborted())
		return false;

	// Native showTextItem finishes intermediate pages before the next SEQ frame.
	_vm->_util->processInput();
	if (isSuspended())
		return false;
	int16 x, y;
	MouseButtons buttons;
	_vm->_util->getMouseState(&x, &y, &buttons);
	const int16 key = _vm->_util->checkKey();
	handleInput(key, x, y, buttons);
	if (isAborted())
		return false;
	const bool accepted = _game.takeReadingAdvance();
#ifdef USE_TTS
	const bool active = _game._narration.update(g_system->getMillis());
	if (accepted)
		_game._narration.cancel();
#endif
	if (accepted || (_clock.now() - _textStart > OnceUpon::kGCTDelay
#ifdef USE_TTS
		&& !active
#endif
		))
		drawGCT(_textItem);
	_vm->_draw->blitInvalidated();
	_textFramePending = true;
	waitFrame();
	if (isSuspended())
		return false;
	_textFramePending = false;
#ifdef USE_TTS
	publishNarration();
#endif
	return true;
}

void Parents::Movie::resumePlay() {
	SEQFile::resumePlay();
	_game.expireReadingInput();
}

void Parents::Movie::activate(MouseButtons buttons) {
	if (buttons == kMouseButtonsRight)
		_game._readingMenu = -1;
}

void Parents::Movie::requireFreshInput() {
#ifdef USE_TTS
	discardNarration();
#endif
	_game.expireReadingInput();
}

#ifdef USE_TTS
void Parents::Movie::discardNarration() {
	_game._captionWords.clear();
	_game._captionNarrationPending = false;
}

void Parents::Movie::publishNarration() {
	if (_game._captionNarrationPending && !_game._readingAdvance)
		_game._narration.publishPage(_game._captionWords, _vm->_global->_language, g_system->getMillis());
	discardNarration();
}

#endif
void Parents::Movie::handleFrameEvent() {
	++_frameCounter;
	const uint16 frame = getFrame();
	const uint layout = (_house == 1 || _house == 2) ? 0 : 1;
	for (uint i = 0; i < ARRAYSIZE(kLightningFrames[0]); ++i) {
		const uint16 lightningFrame = kLightningFrames[layout][i];
		if (lightningFrame == 0xFFFF)
			break;
		if (frame == lightningFrame)
			lightningEffect();
		if (frame == lightningFrame + 15)
			playSound(kSoundThunder);
	}
	if (frame == (layout ? 332 : 340))
		playSound(kSoundCackle);
	switch (frame) {
	case 0:
		// On frame 0, fade in
		_vm->_draw->forceBlit();
		_palette.fadeToTarget();
		break;

	case 4:
		drawGCT(0);
		break;

	case 55:
		drawGCT(3, true);
		break;

	case 79:
		_gct->selectLine(_house + 5, _vm->_rnd.getRandomNumber(2));
		drawGCT(_house + 5, true);
		break;

	case 110:
		_gct->selectLine(_house + 9, _vm->_rnd.getRandomNumber(1));
		drawGCT(_house + 9, true);
		break;

	case 146:
		drawGCT(17, true);
		break;

	case 198:
		drawGCT(13);
		break;

	case 445:
		drawGCT(14, true);
		break;

	case 455:
		drawGCT(18, true);
		break;

	case 465:
		drawGCT(19, true);
		break;

	case 475:
		drawGCT(20, true);
		break;

	default:
		break;
	}
}

void Parents::Movie::handleInput(int16 key, int16 mouseX, int16 mouseY, MouseButtons mouseButtons) {
	// SEQ forces its mouse sample up; reading uses down edges, never that level.
	_vm->_util->forceMouseButtonsSync();
	_game.collectReadingInput(key);
	if (_game._readingMenu) {
		_game._readingMenu = 0;
		abortPlay();
		return;
	}

	if (_waitingText)
		return;
	for (uint i = 0; i < kHoldCount; ++i) {
		if (getFrame() != kHoldFrames[i][0])
			continue;
#ifdef USE_TTS
		const bool active = _game._narration.update(g_system->getMillis());
#endif
		if (!_game.takeReadingAdvance() && (_frameCounter < 100
#ifdef USE_TTS
			|| active
#endif
			))
			seekFrame(kHoldFrames[i][1]);
		return;
	}
	if (getFrame() > 496) {
#ifdef USE_TTS
		const bool active = _game._narration.update(g_system->getMillis());
#endif
		if (_game.takeReadingAdvance() || (_frameCounter > 100
#ifdef USE_TTS
			&& !active
#endif
			))
			_finished = true;
	}
}

void Parents::Movie::playSound(Sound sound) {
	_vm->_sound->blasterStop(0);
	_vm->_sound->blasterPlay(&_sounds[sound], 0, 0);
}

void Parents::Movie::lightningEffect() {
	const uint16 delay = _vm->getPlatform() == Common::kPlatformDOS ? 5 : 25;
	for (int i = 0; (i < 5) && !_vm->shouldQuit(); i++) {

		setPalette(_brightPalette, _paletteSize);
		_vm->_util->delay(delay);

		setPalette(_normalPalette, _paletteSize);
		_vm->_util->delay(delay);
	}
}

void Parents::Movie::setPalette(const byte *palette, uint size) {
	_palette.showTemporary(palette, size);
	_vm->_video->retrace();
}

void Parents::Movie::drawGCT(uint item, bool resetCounter) {
	int16 left, top, right, bottom;
	if (_gct->clear(*_vm->_draw->_backSurface, left, top, right, bottom))
		_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, left, top, right, bottom);
#ifdef USE_TTS
	const bool drawn = _gct->draw(*_vm->_draw->_backSurface, item, *_font, 10, left, top, right, bottom, &_game._captionWords);
	_game._captionNarrationPending = drawn;
#else
	const bool drawn = _gct->draw(*_vm->_draw->_backSurface, item, *_font, 10, left, top, right, bottom);
#endif
	if (drawn) {
		_game.beginReadingPage(_gct->finished() ? OnceUpon::kReadingAny : OnceUpon::kReadingSpace);
		_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, left, top, right, bottom);
	}

	if (resetCounter)
		_frameCounter = 0;
	_textItem = item;
	_textStart = _clock.now();
	_waitingText = !_gct->finished();
}

Parents::Parents(OnceUpon &game) : _game(game), _phase(kMovie), _cabin(false) {
}

Parents::~Parents() {
	_movie.reset();
	_dwelling.reset();
	if (_phase == kSearch)
		_game._clock.setFrameRate(15);
}

void OnceUpon::beginParents() {
	beginScene(kSceneParents);
	Parents *parents = new Parents(*this);
	_scene.reset(parents);
	parents->enter();
}

void Parents::enter() {
	_game.fadeOut();
	_game.hideCursor();
	_cabin = _game._progress.house == 1 || _game._progress.house == 2;
	_game._dwellingHotspots.enter(_cabin, _game._vm->getGameType() == kGameTypeAbracadabra);
	const uint palette = _cabin ? 14 : 15;
	_game.setGamePalette(palette);
	_game.clearScreen();
	_text.reset(_game.loadStoryGCT(_game.getLocFile("mefait.gc")));
	_text->setText(1, _game._progress.name);
	_text->selectLine(2, _game._progress.house);
	_text->selectLine(4, _game._progress.house);
	_movie.reset(new Movie(_game, _cabin ? "parents.seq" : "parents2.seq", *_text));
	_movie->begin();
}

Section::Outcome Parents::step() {
	if (_phase == kSearch)
		return _dwelling->step();
	const Outcome outcome = _movie->step();
	if (_game._pendingRestore)
		return kSectionRestore;
	if (outcome != kSectionComplete)
		return outcome;

	// The SEQ update has unwound. Release its captures, art and borrowed
	// samples before constructing the dwelling presentation.
	_movie.reset();
	// GCTFile::reset() preserves the old background capture. Discard it
	// before the search caption captures its own background.
	_text->setArea(17, 18, 303, 41);
	_text->reset();
	_dwelling.reset(new DwellingSearch(_game, _cabin, *_text));
	_phase = kSearch;
	_dwelling->begin();
	return kSectionContinue;
}

bool Parents::atSaveBoundary() const {
	return _movie && _movie->atSaveBoundary();
}

void Parents::suspendMovie() {
	if (_movie)
		_movie->suspendPlay();
}

bool Parents::activateMovie(MouseButtons buttons) {
	if (!_movie)
		return false;
	_movie->activate(buttons);
	return true;
}

void Parents::resumeAfterPause() {
	if (_movie)
		_movie->requireFreshInput();
}

Section::Outcome Parents::Movie::step() {
	_game._yield = OnceUpon::kMovieYield;
	const bool playing = stepPlay();
	_game._yield = OnceUpon::kNotYielded;
	if (_game._pendingRestore)
		return kSectionRestore;
	if (playing)
		return kSectionContinue;
	if (_vm->shouldQuit())
		return kSectionQuit;
	if (isAborted()) {
		const Outcome outcome = _game.sceneMenu();
		if (outcome != kSectionContinue)
			return outcome;
		resumePlay();
		return kSectionContinue;
	}
	endPlay();
	// The chapter switches to its dwelling after this update returns.
	_game.fadeOut();
	return kSectionComplete;
}


} // End of namespace OnceUpon

} // End of namespace Gob
