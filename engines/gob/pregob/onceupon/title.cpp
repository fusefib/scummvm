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

#include "gob/gob.h"
#include "gob/draw.h"
#include "gob/util.h"

#include "gob/sound/sound.h"

#include "gob/pregob/onceupon/title.h"
#include "gob/pregob/onceupon/runtime_clock.h"
#include "gob/pregob/onceupon/palette.h"
#include "gob/pregob/onceupon/assets.h"
#include "gob/pregob/onceupon/onceupon.h"

namespace Gob {

namespace OnceUpon {

Title::Title(OnceUpon &game) :
	SEQFile(game._vm, "ville.seq", game._assets.get()), _game(game), _phase(kPhaseNone), _clock(game._clock), _palette(game._palette), _musicStarted(false), _activated(false) {
	if (_vm->getPlatform() == Common::kPlatformAtariST) {
		static const char * const files[] = {"baba1.snd", "baba2.snd", "baba3.snd"};
		for (uint i = 0; i < ARRAYSIZE(files); ++i)
			_vm->_sound->sampleLoad(&_samples[i], SOUND_SND, files[i]);
	}
}

Title::~Title() {
	end();
}

void Title::changeFrameRate(int16 rate) {
	_clock.setFrameRate(rate);
}

void Title::waitFrame() {
	_clock.waitFrame();
}

void Title::begin() {
	end();
	_activated = false;
	// The native title player overrides the SEQ header's 10 fps.
	beginPlay(12);
	_phase = kPhasePlaying;
}

void Title::end() {
	endPlay();
	stopMusic();
}

void Title::activate(MouseButtons buttons) {
	if (buttons != kMouseButtonsNone)
		_activated = true;
}

void Title::handleInput(int16 key, int16 mouseX, int16 mouseY, MouseButtons mouseButtons) {
	if (_activated)
		mouseButtons = kMouseButtonsLeft;
	_activated = false;
	SEQFile::handleInput(key, mouseX, mouseY, mouseButtons);
}

void Title::handleFrameEvent() {
	// Start the music and fade in after the first title frame is drawn.
	if (getFrame() == 0 && !_musicStarted) {
		playMusic();

		_vm->_draw->forceBlit();
		_palette.fadeToTarget();
	}
}

void Title::playMusic() {
	_musicStarted = true;
	// Look at what platform this is and play the appropriate music type

	if      (_vm->getPlatform() == Common::kPlatformDOS)
		playMusicDOS();
	else if (_vm->getPlatform() == Common::kPlatformAmiga)
		playMusicAmiga();
	else if (_vm->getPlatform() == Common::kPlatformAtariST)
		playMusicAtariST();
}

void Title::playMusicDOS() {
	// Play an AdLib track

	_vm->_sound->adlibLoadTBR("babayaga.tbr");
	_vm->_sound->adlibLoadMDY("babayaga.mdy");
	_vm->_sound->adlibSetRepeating(-1);
	_vm->_sound->adlibPlay();
}

void Title::playMusicAmiga() {
	// Play a Protracker track

	_vm->_sound->protrackerPlay("mod.babayaga");
}

void Title::playMusicAtariST() {
	// Both ST titles repeat four native sample/count pairs, each once.
	static const int16 titleMusic[] = {0, 1, 0, 2, -1};
	_vm->_sound->blasterPlayComposition(titleMusic, 0, _samples, ARRAYSIZE(_samples));
	_vm->_sound->blasterRepeatComposition(-1);
}

void Title::stopMusic() {
	if (!_musicStarted)
		return;
	_musicStarted = false;
	if (_vm->getPlatform() == Common::kPlatformDOS) {
		_vm->_sound->adlibSetRepeating(0);
		_vm->_sound->adlibStop();
	} else if (_vm->getPlatform() == Common::kPlatformAmiga) {
		_vm->_sound->protrackerStop();
	} else if (_vm->getPlatform() == Common::kPlatformAtariST) {
		_vm->_sound->blasterRepeatComposition(0);
		_vm->_sound->blasterStopComposition();
		for (uint i = 0; i < ARRAYSIZE(_samples); ++i)
			_vm->_sound->blasterStop(0, &_samples[i]);
	}
}

void OnceUpon::beginTitle() {
	beginScene(kSceneTitle);
	fadeOut();
	setGamePalette(10);
	Title *title = new Title(*this);
	_scene.reset(title);
	_vm->_util->clearKeyBuf();
	if (!_pendingRestore)
		title->begin();
}

Section::Outcome Title::step() {
	if (_phase != kPhasePlaying)
		return kSectionQuit;
	_game._yield = OnceUpon::kMovieYield;
	const bool playing = stepPlay();
	_game._yield = OnceUpon::kNotYielded;
	return _game._pendingRestore ? kSectionRestore :
		playing ? kSectionContinue : kSectionComplete;
}

bool Title::activateMovie(MouseButtons buttons) {
	if (_phase != kPhasePlaying)
		return false;
	activate(buttons);
	return true;
}

} // End of namespace OnceUpon

} // End of namespace Gob
