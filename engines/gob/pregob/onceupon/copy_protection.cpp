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

#include "gob/anifile.h"
#include "gob/aniobject.h"
#include "gob/draw.h"
#include "gob/global.h"
#include "gob/gob.h"
#include "gob/surface.h"
#include "gob/util.h"
#include "gob/video.h"

#include "gob/pregob/onceupon/onceupon.h"

namespace Gob {
namespace OnceUpon {

static const uint kCopyProtectionHelpStringCount = 3;

static const char *const kCopyProtectionHelpStrings[OnceUpon::kLanguageCount][kCopyProtectionHelpStringCount] = {
	{ // French
		"Consulte le livret des animaux, rep\212re la",
		"page correspondant \205 la couleur de l\'\202cran",
		"et clique le symbole associ\202 \205 l\'animal affich\202.",
	},
	{ // German
		"Suche im Tieralbum die Seite, die der Farbe auf",
		"dem Bildschirm entspricht und klicke auf das",
		"Tiersymbol.",
	},
	{ // English
		"Consult the book of animals, find the page",
		"corresponding to the colour of screen and click",
		"the symbol associated with the animal displayed.",
	},
	{ // Spanish
		"Consulta el libro de los animales, localiza la ",
		"p\240gina que corresponde al color de la pantalla.",
		"Cliquea el s\241mbolo asociado al animal que aparece.",
	},
	{ // Italian
		"Guarda il libretto degli animali, trova la",
		"pagina che corrisponde al colore dello schermo,",
		"clicca il simbolo associato all\'animale presentato",
	}
};

static const char *const kCopyProtectionWrongStrings[OnceUpon::kLanguageCount] = {
	"Tu t\'es tromp\202, dommage...", // French
	"Schade, du hast dich geirrt."  , // German
	"You are wrong, what a pity!"   , // English
	"Te equivocas, l\240stima..."   , // Spanish
	"Sei Sbagliato, peccato..."       // Italian
};

static const uint kCopyProtectionShapeCount = 5;

static const int16 kCopyProtectionShapeCoords[kCopyProtectionShapeCount][6] = {
	{  0,  51,  26,  75,  60, 154},
	{ 28,  51,  58,  81,  96, 151},
	{ 60,  51,  94,  79, 136, 152},
	{ 96,  51, 136,  71, 180, 155},
	{140,  51, 170,  77, 228, 153}
};

static const uint8 kCopyProtectionColors[7] = {
	14, 11, 13,  1,  7, 12,  2
};


static const uint8 kAbracadabraCopyProtectionShapes[7 * 20] = {
	3, 4, 3, 0, 1, 2, 0, 2, 2, 0, 2, 4, 0, 3, 4, 1, 1, 4, 1, 3,
	0, 2, 0, 4, 2, 4, 4, 2, 3, 0, 1, 1, 1, 1, 3, 0, 4, 2, 3, 4,
	0, 0, 1, 2, 1, 1, 2, 4, 3, 1, 4, 2, 4, 4, 2, 4, 1, 2, 3, 3,
	1, 0, 2, 3, 4, 2, 3, 2, 2, 0, 0, 0, 4, 2, 3, 4, 4, 0, 4, 1,
	4, 2, 1, 1, 1, 1, 4, 3, 4, 2, 3, 0, 0, 3, 0, 2, 3, 0, 2, 4,
	4, 2, 4, 3, 0, 4, 0, 2, 3, 1, 4, 1, 3, 1, 0, 0, 2, 1, 3, 2,
	3, 1, 0, 3, 1, 3, 4, 2, 4, 4, 3, 2, 0, 2, 0, 1, 2, 0, 1, 4
};


static const uint8 kAbracadabraCopyProtectionObfuscate[4] = {
	1, 0, 2, 3
};


static const uint8 kBabaYagaCopyProtectionShapes[7 * 20] = {
	0, 0, 1, 2, 1, 1, 2, 4, 3, 1, 4, 2, 4, 4, 2, 4, 1, 2, 3, 3,
	3, 1, 0, 3, 1, 3, 4, 2, 4, 4, 3, 2, 0, 2, 0, 1, 2, 0, 1, 4,
	1, 0, 2, 3, 4, 2, 3, 2, 2, 0, 0, 0, 4, 2, 3, 4, 4, 0, 4, 1,
	0, 2, 0, 4, 2, 4, 4, 2, 3, 0, 1, 1, 1, 1, 3, 0, 4, 2, 3, 4,
	3, 4, 3, 0, 1, 2, 0, 2, 2, 0, 2, 4, 0, 3, 4, 1, 1, 4, 1, 3,
	4, 2, 1, 1, 1, 1, 4, 3, 4, 2, 3, 0, 0, 3, 0, 2, 3, 0, 2, 4,
	4, 2, 4, 3, 0, 4, 0, 2, 3, 1, 4, 1, 3, 1, 0, 0, 2, 1, 3, 2
};


static const uint8 kBabaYagaCopyProtectionObfuscate[4] = {
	0, 1, 2, 3
};


enum ClownAnimation {
	kClownAnimationStand = 0,
	kClownAnimationCheer = 1,
	kClownAnimationCry   = 2
};

static const PreGob::AnimProperties kClownAnimations[] = {
	{ 1, 0, ANIObject::kModeContinuous, true, false, false, 0, 0},
	{ 0, 0, ANIObject::kModeOnce      , true, false, false, 0, 0},
	{ 6, 0, ANIObject::kModeOnce      , true, false, false, 0, 0}
};

enum CopyProtectionState {
	kCPStateSetup,     // Set up the screen
	kCPStateWaitUser,  // Waiting for the user to pick a shape
	kCPStateWaitClown, // Waiting for the clown animation to finish
	kCPStateFinish     // Finishing
};

static int8 findProtectionShape(int16 x, int16 y) {
	// Look through all shapes and check if the coordinates are inside one of them
	for (uint i = 0; i < kCopyProtectionShapeCount; i++) {
		const int16 * const coords = kCopyProtectionShapeCoords[i];

		const int16 left   = coords[4];
		const int16 top    = coords[5];
		const int16 right  = coords[4] + (coords[2] - coords[0] + 1) - 1;
		const int16 bottom = coords[5] + (coords[3] - coords[1] + 1) - 1;

		if ((x >= left) && (x <= right) && (y >= top) && (y <= bottom))
			return i;
	}

	return -1;
}

bool OnceUpon::doCopyProtection() {
	const bool abra = _vm->getGameType() == kGameTypeAbracadabra;
	const uint8 *colors = kCopyProtectionColors;
	const uint8 *shapes = abra ? kAbracadabraCopyProtectionShapes : kBabaYagaCopyProtectionShapes;
	const uint8 *obfuscate = abra ? kAbracadabraCopyProtectionObfuscate : kBabaYagaCopyProtectionObfuscate;
	fadeOut();
	setPalette(kCopyProtectionPalette, kPaletteSize);

	// Load the copy protection sprites
	Surface sprites[2] = {Surface(320, 200, 1), Surface(320, 200, 1)};

	_vm->_video->drawPackedSprite("grille1.cmp", sprites[0]);
	_vm->_video->drawPackedSprite("grille2.cmp", sprites[1]);

	// Load the clown animation
	ANIFile ani  (_vm, "grille.ani", 320);
	ANIList anims;

	loadAnims(anims, ani, 1, &kClownAnimations[kClownAnimationStand]);

	// Set the copy protection cursor
	setCursor(sprites[1], 5, 110, 20, 134, 3, 0);

	// We start with 2 tries left, not having a correct answer and the copy protection not set up yet
	CopyProtectionState state = kCPStateSetup;

	uint8 triesLeft   =  2;
	int8  animalShape = -1;
	bool  hasCorrect  = false;

	while (!_vm->shouldQuit() && (state != kCPStateFinish)) {
		clearAnim(anims);

		// Set up the screen
		if (state == kCPStateSetup) {
			animalShape = cpSetup(colors, shapes, obfuscate, sprites);

			setAnim(*anims[0], kClownAnimations[kClownAnimationStand]);
			state = kCPStateWaitUser;
		}

		drawAnim(anims);

		// If we're waiting for the clown and he finished, evaluate if we're finished
		if (!anims[0]->isVisible() && (state == kCPStateWaitClown))
			state = (hasCorrect || (--triesLeft == 0)) ? kCPStateFinish : kCPStateSetup;

		showCursor();
		fadeIn();

		endFrame();

		int16 mouseX, mouseY;
		MouseButtons mouseButtons;

		checkInput(mouseX, mouseY, mouseButtons);

		if (state == kCPStateWaitUser) {
			int8 guessedShape = -1;
			if (mouseButtons == kMouseButtonsLeft)
				guessedShape = findProtectionShape(mouseX, mouseY);

			if (guessedShape >= 0) {
				hasCorrect  = guessedShape == animalShape;
				animalShape = -1;

				setAnim(*anims[0], kClownAnimations[hasCorrect ? kClownAnimationCheer : kClownAnimationCry]);
				state = kCPStateWaitClown;
			}
		}
	}

	freeAnims(anims);

	fadeOut();
	hideCursor();
	clearScreen();

	// Display the "You are wrong" screen
	if (!hasCorrect)
		cpWrong();

	// The story driver adopts a load requested on either protection screen.
	return hasCorrect;
}

int8 OnceUpon::cpSetup(const uint8 colors[7], const uint8 shapes[7 * 20], const uint8 obfuscate[4],
					   const Surface sprites[2]) {

	fadeOut();
	hideCursor();

	// Get a random animal and animal color
	int8 animalColor = _vm->_util->getRandom(7);
	while ((colors[animalColor] == 1) || (colors[animalColor] == 7) || (colors[animalColor] == 11))
		animalColor = _vm->_util->getRandom(7);

	int8 animal = _vm->_util->getRandom(20);

	int8 animalShape = shapes[animalColor * 20 + animal];
	if (animal < 4)
		animal = obfuscate[animal];

	// Get the position of the animal sprite
	int16 animalLeft = (animal % 4) * 80;
	int16 animalTop  = (animal / 4) * 50;

	uint8 sprite = 0;
	if (animalTop >= 200) {
		animalTop -= 200;
		sprite = 1;
	}

	int16 animalRight  = animalLeft + 80 - 1;
	int16 animalBottom = animalTop  + 50 - 1;

	// Fill with the animal color
	_vm->_draw->_backSurface->fill(colors[animalColor]);

	// The printed help rows form one TTS instruction. Publish it once
	// after the fade, alongside the first visible clown frame.
	for (uint i = 0; i < kCopyProtectionHelpStringCount; i++) {
		const char * const helpString = kCopyProtectionHelpStrings[_vm->_global->_language][i];

		const int x = 160 - (strlen(helpString) * _plettre->getCharWidth()) / 2;
		const int y = i * 10 + 5;

		_plettre->drawString(helpString, x, y, 8, 0, true, *_vm->_draw->_backSurface);
	}

	// White rectangle with black border
	_vm->_draw->_backSurface->fillRect( 93, 43, 226, 134, 15);
	_vm->_draw->_backSurface->drawRect( 92, 42, 227, 135,  0);

	// Draw the animal in the animal color
	_vm->_draw->_backSurface->fillRect(120, 63, 199, 112, colors[animalColor]);
	_vm->_draw->_backSurface->blit(sprites[sprite], animalLeft, animalTop, animalRight, animalBottom, 120, 63, 0);

	// Draw the shapes
	for (uint i = 0; i < kCopyProtectionShapeCount; i++) {
		const int16 * const coords = kCopyProtectionShapeCoords[i];

		_vm->_draw->_backSurface->blit(sprites[1], coords[0], coords[1], coords[2], coords[3], coords[4], coords[5], 0);
	}

	_vm->_draw->forceBlit();

	return animalShape;
}

void OnceUpon::cpWrong() {
	if (_vm->shouldQuit())
		return;

	// Speak the centered failure message once the caption is visible.
	const char * const wrongString = kCopyProtectionWrongStrings[_vm->_global->_language];
	const int          wrongX      = 160 - (strlen(wrongString) * _plettre->getCharWidth()) / 2;

	_vm->_draw->_backSurface->clear();
	_plettre->drawString(wrongString, wrongX, 100, 15, 0, true, *_vm->_draw->_backSurface);

	_vm->_draw->forceBlit();

	fadeIn();

	do {
		endFrame();
	} while (!_vm->shouldQuit() && !hasInput());

	if (_vm->shouldQuit())
		return;

	fadeOut();
	clearScreen();
}


} // namespace OnceUpon
} // namespace Gob
