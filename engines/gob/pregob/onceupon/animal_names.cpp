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

#include "gob/draw.h"
#include "gob/gob.h"
#include "gob/surface.h"
#include "gob/util.h"
#include "gob/video.h"

#include "gob/pregob/txtfile.h"
#include "gob/pregob/onceupon/onceupon.h"
#include "gob/sound/sound.h"

namespace Gob {
namespace OnceUpon {

const OnceUpon::MenuButton OnceUpon::kAbracadabraAnimalsButtons = {
	true, 131, 127, 183, 164, 193, 0, 243, 35, 132, 128, 0
};


const OnceUpon::MenuButton OnceUpon::kAbracadabraAnimalButtons[] = {
	{false,  37,  89,  95, 127,  37,  89,  95, 127, 131, 25, 0},
	{false, 114,  65, 172, 111, 114,  65, 172, 111, 131, 25, 1},
	{false, 186,  72, 227,  96, 186,  72, 227,  96, 139, 25, 2},
	{false, 249,  87, 282, 112, 249,  87, 282, 112, 143, 25, 3},
	{false, 180, 102, 234, 138, 180, 102, 234, 138, 133, 25, 4},
	{false, 197, 145, 242, 173, 197, 145, 242, 173, 137, 25, 5},
	{false, 113, 151, 171, 176, 113, 151, 171, 176, 131, 25, 6},
	{false, 114, 122, 151, 150, 114, 122, 151, 150, 141, 25, 7},
	{false,  36, 136,  94, 176,  36, 136,  94, 176, 131, 25, 8},
	{false, 243, 123, 295, 155, 243, 123, 295, 155, 136, 25, 9}
};


const char *OnceUpon::kAbracadabraAnimalNames[] = {
	"loup",
	"drag",
	"arai",
	"crap",
	"crab",
	"mous",
	"saut",
	"guep",
	"rhin",
	"scor"
};


const OnceUpon::MenuButton OnceUpon::kBabaYagaAnimalsButtons = {
	true, 131, 127, 183, 164, 193, 0, 245, 37, 131, 127, 0
};


const OnceUpon::MenuButton OnceUpon::kBabaYagaAnimalButtons[] = {
	{false,  34,  84,  92, 127,  34,  84,  92, 127, 131, 25, 0},
	{false, 114,  65, 172, 111, 114,  65, 172, 111, 131, 25, 1},
	{false, 186,  72, 227,  96, 186,  72, 227,  96, 139, 25, 2},
	{false, 249,  87, 282, 112, 249,  87, 282, 112, 143, 25, 3},
	{false, 180,  97, 234, 138, 180,  97, 234, 138, 133, 25, 4},
	{false, 197, 145, 242, 173, 197, 145, 242, 173, 137, 25, 5},
	{false, 113, 156, 171, 176, 113, 156, 171, 176, 131, 25, 6},
	{false, 114, 127, 151, 150, 114, 127, 151, 150, 141, 25, 7},
	{false,  36, 136,  94, 176,  36, 136,  94, 176, 131, 25, 8},
	{false, 245, 123, 293, 155, 245, 123, 293, 155, 136, 25, 9}
};


const char *OnceUpon::kBabaYagaAnimalNames[] = {
	"vaut",
	"drag",
	"arai",
	"gren",
	"fauc",
	"abei",
	"serp",
	"tort",
	"sang",
	"rena"
};


const OnceUpon::MenuButton OnceUpon::kAnimalNamesBack = {
	true, 19, 13, 50, 46, 36, 0, 67, 34, 19, 13, 1
};


const OnceUpon::MenuButton OnceUpon::kLanguageButtons[] = {
	{true,  43,  80,  93, 115,   0, 55,  50, 90,  43,  80, 0},
	{true, 132,  80, 182, 115,  53, 55, 103, 90, 132,  80, 1},
	{true, 234,  80, 284, 115, 106, 55, 156, 90, 234,  80, 2},
	{true,  43, 138,  93, 173, 159, 55, 209, 90,  43, 138, 3},
	{true, 132, 138, 182, 173, 212, 55, 262, 90, 132, 138, 4},
	{true, 234, 138, 284, 173, 265, 55, 315, 90, 234, 138, 2}
};


const OnceUpon::MenuButton &OnceUpon::animalNamesButton() const {
	return _vm->getGameType() == kGameTypeAbracadabra ? kAbracadabraAnimalsButtons : kBabaYagaAnimalsButtons;
}

enum AnimalNamesState {
	kANStateChoose, // We're in the animal chooser
	kANStateNames,  // We're in the language chooser
	kANStateFinish  // We're finished
};

void OnceUpon::handleAnimalNames() {
	const bool abra = _vm->getGameType() == kGameTypeAbracadabra;
	const MenuButton *buttons = abra ? kAbracadabraAnimalButtons : kBabaYagaAnimalButtons;
	const char * const *names = abra ? kAbracadabraAnimalNames : kBabaYagaAnimalNames;
	const uint count = ARRAYSIZE(kAbracadabraAnimalButtons);
	beginMenuInput();
	fadeOut();
	clearScreen();

	bool cursorVisible = isCursorVisible();

	// Set the cursor
	addCursor();
	setGameCursor();

	anSetupChooser();

	int8 _animal = -1;

	AnimalNamesState state = kANStateChoose;
	while (!_vm->shouldQuit() && (state != kANStateFinish)) {
		showCursor();
		fadeIn();

		endFrame();

		// Check user input

		int16 mouseX, mouseY;
		MouseButtons mouseButtons;

		int16 key = checkMenuInput(mouseX, mouseY, mouseButtons);
		if (key == kKeyEscape) {
			if (state == kANStateNames) {
				anSetupChooser();
				state = kANStateChoose;
			} else {
				state = kANStateFinish;
			}
			continue;
		}

		// If we moused over an animal button, draw a border around it
		int animal = checkButton(buttons, count, mouseX, mouseY);
		if ((state == kANStateChoose) && (animal != _animal)) {
			// Erase the old border
			if (_animal >= 0)
				drawButtonBorder(buttons[_animal], 15);

			_animal = animal;

			// Draw the new border
			if (_animal >= 0)
				drawButtonBorder(buttons[_animal], 10);
		}

		if (mouseButtons != kMouseButtonsLeft)
			continue;

		playSound(kSoundClick);

		// We clicked on a language button, play the animal name
		int language = checkButton(kLanguageButtons, ARRAYSIZE(kLanguageButtons), mouseX, mouseY);
		if ((state == kANStateNames) && (language >= 0))
			anPlayAnimalName(names[_animal], language);

		// We clicked on an animal
		if ((state == kANStateChoose) && (_animal >= 0)) {
			anSetupNames(buttons[_animal]);

			state = kANStateNames;
		}

		// If we clicked on the back button, go back
		if (checkButton(&kAnimalNamesBack, 1, mouseX, mouseY) != -1) {
			if        (state == kANStateNames) {
				anSetupChooser();

				state = kANStateChoose;
			} else if (state == kANStateChoose)
				state = kANStateFinish;
		}
	}

	fadeOut();

	// Restore the cursor
	if (!cursorVisible)
		hideCursor();
	removeCursor();
	endMenuInput();
}

void OnceUpon::anSetupChooser() {
	fadeOut();
	setGamePalette(18);

	_vm->_video->drawPackedSprite("dico.cmp", *_vm->_draw->_backSurface);

	// Draw the back button
	Surface menu(320, 34, 1);
	_vm->_video->drawPackedSprite("icon.cmp", menu);
	drawButton(*_vm->_draw->_backSurface, menu, kAnimalNamesBack);

	// "Choose an animal"
	TXTFile *choose = loadTXT(getLocFile("choisi.tx"), TXTFile::kFormatStringPosition);
	choose->draw(*_vm->_draw->_backSurface, &_plettre, 1, 14);
	delete choose;

	_vm->_draw->forceBlit();
}

void OnceUpon::anSetupNames(const MenuButton &animal) {
	fadeOut();
	setGamePalette(19);

	Surface background(320, 200, 1);

	_vm->_video->drawPackedSprite("dico.cmp", background);

	// Draw the background and clear what we don't need
	_vm->_draw->_backSurface->blit(background);
	_vm->_draw->_backSurface->fillRect(19, 19, 302, 186, 15);

	// Draw the back button
	Surface menu(320, 34, 1);
	_vm->_video->drawPackedSprite("icon.cmp", menu);
	drawButton(*_vm->_draw->_backSurface, menu, kAnimalNamesBack);

	// Draw the animal
	drawButton(*_vm->_draw->_backSurface, background, animal);

	// Draw the language buttons
	Surface elements(320, 200, 1);
	_vm->_video->drawPackedSprite("elemenu.cmp", elements);
	drawButtons(*_vm->_draw->_backSurface, elements, kLanguageButtons, ARRAYSIZE(kLanguageButtons));

	// Draw the language names
	struct LanguageLabel {
		const char *text;
		int16 x, y;
	};
	static const LanguageLabel labels[] = {
		{"Fran\207ais", 43, 70}, {"Deutsch", 136, 70}, {"English", 238, 70},
		{"Italiano", 43, 128}, {"Espa\244ol", 136, 128}, {"English", 238, 128}
	};
	for (uint i = 0; i < ARRAYSIZE(labels); ++i) {
		_plettre->drawString(labels[i].text, labels[i].x, labels[i].y, 10, 15, true, *_vm->_draw->_backSurface);
	}

	_vm->_draw->forceBlit();
}

void OnceUpon::anPlayAnimalName(const Common::String &animal, uint language) {
	// Sound file to play
	Common::String soundFile = animal + "_" + kLanguageSuffixLong[language] + ".snd";

	// Get the name of the animal
	TXTFile *names = loadTXT(animal + ".anm", TXTFile::kFormatString);
	Common::String name = names->getLines()[language].text;
	delete names;

	// It should be centered on the screen
	const int nameX = 160 - (name.size() * _plettre->getCharWidth()) / 2;

	// Backup the screen surface
	Surface backup(162, 23, 1);
	backup.blit(*_vm->_draw->_backSurface, 78, 123, 239, 145, 0, 0);

	// Draw the name border
	Surface nameBorder(162, 23, 1);
	_vm->_video->drawPackedSprite("mot.cmp", nameBorder);
	_vm->_draw->_backSurface->blit(nameBorder, 0, 0, 161, 22, 78, 123);

	// Print the animal name
	_plettre->drawString(name, nameX, 129, 10, 0, true, *_vm->_draw->_backSurface);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 78, 123, 239, 145);

	stopSound();
	SoundDesc sound;
	if (_vm->_sound->sampleLoad(&sound, SOUND_SND, soundFile.c_str())) {
		_vm->_sound->blasterPlay(&sound, 0, 0);
		_vm->_util->forceMouseUp();
		while (!_vm->shouldQuit() && _vm->_sound->blasterPlayingSound()) {
			endFrame();
			if (hasInput())
				break;
		}
		_vm->_util->forceMouseUp();
		// The mixer borrows this stack descriptor; release only this cue.
		_vm->_sound->blasterStop(0, &sound);
	}

	// Restore the screen
	_vm->_draw->_backSurface->blit(backup, 0, 0, 161, 22, 78, 123);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 78, 123, 239, 145);
}


} // namespace OnceUpon
} // namespace Gob
