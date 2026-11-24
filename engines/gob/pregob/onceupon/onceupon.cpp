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

#include "gob/pregob/onceupon/onceupon.h"
#include "gob/pregob/onceupon/assets.h"


#include "gob/gob.h"
#include "gob/global.h"
#include "gob/util.h"
#include "gob/dataio.h"
#include "gob/surface.h"
#include "gob/draw.h"
#include "gob/video.h"
#include "gob/anifile.h"
#include "gob/sound/sound.h"

#include "gob/pregob/txtfile.h"
#include "gob/pregob/gctfile.h"

#include "gob/pregob/onceupon/text_corrections.h"
#include "gob/pregob/onceupon/palette.h"

namespace Gob {

namespace OnceUpon {

const OnceUpon::MenuButton OnceUpon::kMainMenuDifficultyButton[] = {
	{false,  29, 18,  77, 57, 0, 0, 0, 0, 0, 0, (int)kDifficultyBeginner},
	{false, 133, 18, 181, 57, 0, 0, 0, 0, 0, 0, (int)kDifficultyIntermediate},
	{false, 241, 18, 289, 57, 0, 0, 0, 0, 0, 0, (int)kDifficultyAdvanced},
};

const OnceUpon::MenuButton OnceUpon::kSectionButtons[] = {
	{false,  27, 121,  91, 179,   0, 0,   0,  0,   0,   0,  0},
	{ true,  95, 121, 159, 179,   4, 1,  56, 49, 100, 126,  2},
	{ true, 163, 121, 227, 179,  64, 1, 120, 49, 168, 126,  6},
	{ true, 231, 121, 295, 179, 128, 1, 184, 49, 236, 126, 10}
};

const OnceUpon::MenuButton OnceUpon::kIngameButtons[] = {
	{true, 108, 83, 139, 116,   0,   0,  31,  34, 108,  83, 0},
	{true, 144, 83, 175, 116,  36,   0,  67,  34, 144,  83, 1},
	{true, 180, 83, 211, 116,  72,   0, 103,  34, 180,  83, 2}
};

const char *OnceUpon::kSound[kSoundCount] = {
	"diamant.snd", // kSoundClick
	"cigogne.snd", // kSoundCourier
	"saute.snd",  // kSoundJump
	"gagne.snd"   // kSoundReward
};

void OnceUpon::beginScene(SceneKind kind) {
	if (_sceneKind != kSceneNone)
		finishScene();
	_sceneKind = kind;
	_sceneFrameRate = _vm->_util->getFrameRate();
}

void OnceUpon::finishScene() {
	if (_sceneKind == kSceneNone)
		return;
	endReadingPage();
	removeSelectionCursor();
	_acceptActivations = false;
	_activations.clear();
	const SceneKind kind = _sceneKind;
	const int16 oldRate = _sceneFrameRate;
	_scene.reset();
	_sceneKind = kSceneNone;
	if (kind == kSceneForest || kind == kSceneCastle || kind == kSceneBoss ||
		kind == kSceneHouse || kind == kSceneCharacter || kind == kSceneEnding)
		_clock.setFrameRate(oldRate);
}

OnceUpon::ScreenBackup::ScreenBackup() : changedCursor(false), cursorVisible(false) {
	screen = new Surface(320, 200, 1);
}

OnceUpon::ScreenBackup::~ScreenBackup() {
	delete screen;
}

OnceUpon::OnceUpon(GobEngine *vm) : PreGob(vm), _openedArchives(false),
	_jeudak(nullptr), _lettre(nullptr), _plettre(nullptr), _glettre(nullptr), _palette(vm),
	_assets(new Assets(vm)), _clock(vm), _sceneKind(kSceneNone), _sceneFrameRate(15),
	_readingKeys(kNoReading), _readingVisible(false), _readingOpening(false), _readingAdvance(false), _readingMenu(0),
	_introStage(kIntroLoading),
	_paused(false),
	_selectionCursor(false), _actionCursorANI(nullptr),
	_actionCursorAnimation(0), _actionCursorFrame(0), _actionCursorX(0), _actionCursorY(0),
	_acceptActivations(false), _freshInput(true),
	_menuInputActive(false), _menuIgnoreEscape(false), _menuEscapePending(0),
	_menuHeld(kMouseButtonsNone), _menuIgnoredButtons(kMouseButtonsNone)
{
}

void OnceUpon::playSample(SoundDesc &sample, int16 repeats, int16 frequency) {
	// Keep the current cue unless the caller requests replacement.
	if (sample.empty() || _vm->_sound->blasterPlayingSound())
		return;
	_vm->_sound->blasterPlay(&sample, repeats, frequency);
}

void OnceUpon::replaceSample(SoundDesc &sample, int16 repeats, int16 frequency) {
	stopSound();
	// Only the Atari ST helper inserts a fixed gap before the replacement cue.
	if (_vm->getPlatform() == Common::kPlatformAtariST)
		_vm->_util->delay(50);
	playSample(sample, repeats, frequency);
}

void OnceUpon::playSound(uint sound, int16 frequency, int16 repeats) {
	if (sound < kSoundCount)
		playSample(_commonSounds[sound], repeats, frequency);
}

OnceUpon::~OnceUpon() {
	deinit();
}

void OnceUpon::init() {
	deinit();
	_dwellingHotspots = DwellingHotspots();

	// Open data files

	bool hasSTK1 = _vm->_dataIO->openArchive("stk1.stk", true);
	bool hasSTK2 = _vm->_dataIO->openArchive("stk2.stk", true);
	bool hasSTK3 = _vm->_dataIO->openArchive("stk3.stk", true);

	if (!hasSTK1 || !hasSTK2 || !hasSTK3)
		error("OnceUpon::OnceUpon(): Failed to open archives");

	_openedArchives = true;

	// Open fonts

	_jeudak  = _vm->_draw->loadFont("jeudak.let");
	_lettre  = _vm->_draw->loadFont("lettre.let");
	_plettre = _vm->_draw->loadFont("plettre.let");
	_glettre = _vm->_draw->loadFont("glettre.let");

	if (!_jeudak || !_lettre || !_plettre || !_glettre)
		error("OnceUpon::OnceUpon(): Failed to fonts (%d, %d, %d, %d)",
		      _jeudak != nullptr, _lettre != nullptr, _plettre != nullptr, _glettre != nullptr);

	// Verify the language

	if (_vm->_global->_language == kLanguageAmerican)
		_vm->_global->_language = kLanguageBritish;

	if ((_vm->_global->_language >= kLanguageCount))
		error("We do not support the language \"%s\".\n"
		      "If you are certain that your game copy includes this language,\n"
		      "please contact the ScummVM team with details about this version.\n"
		      "Thanks", _vm->getLangDesc(_vm->_global->_language));

	// Load all our sounds and init the screen

	for (uint i = 0; i < kSoundCount; ++i)
		Assets::loadSample(_vm, _commonSounds[i], kSound[i]);
	initScreen();

	// Match the palette state to the black screen cleared by PreGob.
	_palette.reset(true);

	// No quit requested at start
	_quit = false;

	resetProgress();
	_introStage = kIntroLoading;
	_keyboard.begin();
}

void OnceUpon::resetProgress() {
	_progress = Progress();
	_progress.difficulty = kDifficultyCount;
	_progress.name = "Nemo";
}

void OnceUpon::deinit() {

	_menuInputActive = false;
	_activations.clear();
	if (_vm->_sound)
		stopSound();
	finishScene();
	_assets->invalidate();

	// Free sounds
	for (uint i = 0; i < kSoundCount; ++i)
		_commonSounds[i].free();

	// Free fonts

	delete _jeudak;
	delete _lettre;
	delete _plettre;
	delete _glettre;

	_jeudak  = nullptr;
	_lettre  = nullptr;
	_plettre = nullptr;
	_glettre = nullptr;

	// Close archives

	if (_openedArchives) {
		_vm->_dataIO->closeArchive(true);
		_vm->_dataIO->closeArchive(true);
		_vm->_dataIO->closeArchive(true);
	}

	_openedArchives = false;
	_keyboard.end();
}

void OnceUpon::setGamePalette(uint palette) {
	if (palette < kPaletteCount)
		_palette.selectTarget(kGamePalettes[palette], kPaletteSize);
}

void OnceUpon::showTemporaryPalette(uint palette) {
	if (palette < kPaletteCount)
		_palette.showTemporary(kGamePalettes[palette], kPaletteSize);
}

void OnceUpon::setPalette(const byte *palette, uint16 size) {
	_palette.selectTarget(palette, size);
}

void OnceUpon::fadeOut() {
	_palette.fadeToBlack();
}

void OnceUpon::fadeIn() {
	if (!_palette.holdingBlack() || _vm->shouldQuit())
		return;
	_vm->_draw->blitInvalidated();
	_palette.fadeToTarget();
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 0, 0, 319, 199);
}

void OnceUpon::setGameCursor() {
	Surface cursor(320, 16, 1);

	// Set the default game cursor
	_vm->_video->drawPackedSprite("icon.cmp", cursor);
	setCursor(cursor, 105, 0, 120, 15, 0, 0);
}

void OnceUpon::showSelectionCursor(Surface &cursor) {
	addCursor();
	setCursor(cursor, 0, 0);
	_vm->_util->setMousePos(160, 100);
	showCursor();
	_selectionCursor = true;
}

void OnceUpon::removeSelectionCursor() {
	if (!_selectionCursor)
		return;
	hideCursor();
	removeCursor();
	_selectionCursor = false;
	_actionCursorANI = nullptr;
	_clock.limitActionCursor(false);
}

void OnceUpon::showActionCursor(const ANIFile &ani, uint16 animation, uint16 frame, int16 x, int16 y) {
	if (_actionCursorANI != &ani || _actionCursorAnimation != animation || _actionCursorFrame != frame) {
		const ANIFile::FrameArea &area = ani.getAnimationInfo(animation).frameAreas[frame];
		const int16 left = MIN<int16>(area.left, 0), top = MIN<int16>(area.top, 0);
		Surface cursor(MAX<int16>(area.right, 0) - left + 1, MAX<int16>(area.bottom, 0) - top + 1, 1);
		ani.draw(cursor, animation, frame, -left, -top);
		if (!_selectionCursor) {
			addCursor();
			_selectionCursor = true;
		}
		setCursor(cursor, -left, -top);
		showCursor();
		_clock.limitActionCursor(true);
		_actionCursorANI = &ani;
		_actionCursorAnimation = animation;
		_actionCursorFrame = frame;
	}
	_actionCursorX = x;
	_actionCursorY = y;
}

void OnceUpon::holdActionCursor() {
	if (!_actionCursorANI)
		return;
	const ANIFile::FrameArea &area = _actionCursorANI->getAnimationInfo(_actionCursorAnimation).frameAreas[_actionCursorFrame];
	_actionCursorANI->draw(*_vm->_draw->_backSurface, _actionCursorAnimation, _actionCursorFrame, _actionCursorX, _actionCursorY);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, _actionCursorX + area.left, _actionCursorY + area.top,
		_actionCursorX + area.right, _actionCursorY + area.bottom);
	removeSelectionCursor();
}

void OnceUpon::drawLineByLine(const Surface &src, int16 left, int16 top, int16 right, int16 bottom,
							  int16 x, int16 y) const {

	if (_vm->shouldQuit())
		return;

	const int16 width  = right  - left + 1;
	const int16 height = bottom - top  + 1;

	if ((width <= 0) || (height <= 0))
		return;

	// Draw the even lines downwards
	for (int16 i = 0; i < height; i += 2) {
		if (_vm->shouldQuit())
			return;

		_vm->_draw->_backSurface->blit(src, left, top + i, right, top + i, x, y + i);

		// Draw normalizes reversed bounds, so the bottom must follow this row.
		_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, x, y + i, x + width - 1, y + i);
		_vm->_draw->blitInvalidated();

		_vm->_util->longDelay(1);
	}

	// Draw the odd lines upwards
	for (int16 i = (height & 1) ? height : (height - 1); i >= 0; i -= 2) {
		if (_vm->shouldQuit())
			return;

		_vm->_draw->_backSurface->blit(src, left, top + i, right, top + i, x, y + i);

		_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, x, y + i, x + width - 1, y + i);
		_vm->_draw->blitInvalidated();

		_vm->_util->longDelay(1);
	}
}

void OnceUpon::backupScreen(ScreenBackup &backup, bool setDefaultCursor) {
	// Backup the screen and palette
	backup.screen->blit(*_vm->_draw->_backSurface);
	backup.palette = _palette.snapshot();

	// Backup the cursor

	backup.cursorVisible = isCursorVisible();

	backup.changedCursor = false;
	if (setDefaultCursor) {
		backup.changedCursor = true;

		addCursor();
		setGameCursor();
	}
}

void OnceUpon::restoreScreen(ScreenBackup &backup, bool keepBlack) {
	// Cursor-stack cleanup is still required when quitting.
	if (!_vm->shouldQuit()) {
		_vm->_draw->_backSurface->blit(*backup.screen);
		_vm->_draw->forceBlit();
		if (keepBlack)
			_palette.selectTarget(backup.palette.target, 256);
		else
			_palette.adopt(backup.palette);
	}
	if (!backup.cursorVisible)
		hideCursor();
	if (backup.changedCursor)
		removeCursor();
	backup.changedCursor = false;
}

void OnceUpon::finishMenu(ScreenBackup &backup, bool fade) {
	if (fade)
		fadeOut();
	else
		clearIngameMenu(*backup.screen);

	restoreScreen(backup, fade);
	endMenuInput();
}

void OnceUpon::waitInputDelay(uint16 milliseconds) {
	// Keep the scaled timing and retrace order of Util::longDelay.
	const uint32 deadline = _vm->_util->getTimeKey() + milliseconds;
	do {
		_vm->_video->waitRetrace();

		_vm->_util->processInput();

		_vm->_util->delay(15);
	} while (!_vm->shouldQuit() && _vm->_util->getTimeKey() < deadline);
}

void OnceUpon::endFrame() {
	_vm->_draw->blitInvalidated();
	publishReadingPage();

	const bool waited = _clock.waitFrame();
	if (waited)
		_vm->_util->processInput();

	if (!waited)
		return;
}

bool OnceUpon::captionReady(bool accepted, uint32 captionStart) {
	if (accepted) {
		takeReadingAdvance();
		return true;
	}
	return _clock.now() - captionStart > kGCTDelay
		;
}

GCTFile *OnceUpon::loadStoryGCT(const Common::String &name) const {
	GCTFile *gct = loadGCT(name);
	const GameType game = _vm->getGameType();
	for (uint i = 0; i < ARRAYSIZE(kBrokenGCTStrings); ++i) {
		const BrokenGCTString &fix = kBrokenGCTStrings[i];
		if ((fix.game == kGameTypeNone || fix.game == game) && name.equalsIgnoreCase(fix.resource))
			gct->replaceTextChunk(fix.item, fix.line, fix.wrong, fix.correct);
	}
	gct->enableNativeLayout();
	gct->setArea(17, 18, 303, 41);
	return gct;
}

void OnceUpon::fixTXTStrings(TXTFile &txt) const {
	TXTFile::LineArray &lines = txt.getLines();
	for (uint i = 0; i < lines.size(); i++)
		lines[i].text = fixString(lines[i].text);
}

Common::String OnceUpon::fixString(const Common::String &str) const {
	const BrokenStringLanguage &broken = kBrokenStrings[_vm->_global->_language];

	for (uint i = 0; i < broken.count; i++) {
		if (str == broken.strings[i].wrong)
			return broken.strings[i].correct;
	}

	return str;
}

void OnceUpon::showWait(uint palette) {
	// Show the loading floppy

	fadeOut();
	clearScreen();
	setGamePalette(palette);

	Surface wait(320, 43, 1);

	_vm->_video->drawPackedSprite("wait.cmp", wait);
	_vm->_draw->_backSurface->blit(wait, 0, 0, 72, 33, 122, 84);

	_vm->_draw->forceBlit();

	fadeIn();
}

void OnceUpon::showByeBye() {
	fadeOut();
	hideCursor();
	clearScreen();
	setGamePalette(1);

	_plettre->drawString("Bye Bye....", 140, 80, 2, 0, true, *_vm->_draw->_backSurface);
	_vm->_draw->forceBlit();

	fadeIn();

	_vm->_util->longDelay(1000);

	fadeOut();
}

void OnceUpon::doStartMenu() {
	const MenuButton *animalsButton = &animalNamesButton();
	clearScreen();

	while (!_vm->shouldQuit() && !_quit) {
		MenuAction action = handleStartMenu(animalsButton);
		if (action == kMenuActionPlay)
			break;
		if (action == kMenuActionQuit) {
			_quit = true;
			break;
		}

		// If we pressed the "listen to animal names" button, handle that screen
		if (action == kMenuActionAnimals)
			handleAnimalNames();
	}
}

OnceUpon::MenuAction OnceUpon::handleStartMenu(const MenuButton *animalsButton) {
	beginMenuInput();
	ScreenBackup screenBackup;
	backupScreen(screenBackup, true);

	fadeOut();
	setGamePalette(17);
	drawStartMenu(animalsButton);
	showCursor();
	fadeIn();

	MenuAction action = kMenuActionNone;
	while (!_vm->shouldQuit() && (action == kMenuActionNone)) {
		endFrame();

		// Check user input

		int16 mouseX, mouseY;
		MouseButtons mouseButtons;

		int16 key = checkMenuInput(mouseX, mouseY, mouseButtons);
		if (key == kKeyEscape) {
			action = kMenuActionQuit;
			break;
		}

		if (mouseButtons != kMouseButtonsLeft)
			continue;

		playSound(kSoundClick);

		// If we clicked on a difficulty button, show the selected difficulty and start the game
		int diff = checkButton(kMainMenuDifficultyButton, ARRAYSIZE(kMainMenuDifficultyButton), mouseX, mouseY);
		if (diff >= 0) {
			_progress.difficulty = (Difficulty)diff;
			action      = kMenuActionPlay;

			drawStartMenu(animalsButton);
			waitInputDelay(1000);
		}

		if (animalsButton && (checkButton(animalsButton, 1, mouseX, mouseY) != -1))
			action = kMenuActionAnimals;

	}

	finishMenu(screenBackup, true);

	return action;
}

OnceUpon::MenuAction OnceUpon::handleMainMenu() {
	beginMenuInput();
	_progress.score = 0;
	ScreenBackup screenBackup;
	backupScreen(screenBackup, true);

	fadeOut();
	setGamePalette(17);
	drawMainMenu();
	showCursor();
	fadeIn();

	MenuAction action = kMenuActionNone;
	while (!_vm->shouldQuit() && (action == kMenuActionNone)) {
		endFrame();

		// Check user input

		int16 mouseX, mouseY;
		MouseButtons mouseButtons;

		int16 key = checkMenuInput(mouseX, mouseY, mouseButtons);
		if (key == kKeyEscape) {
			action = kMenuActionQuit;
			break;
		}

		if (mouseButtons != kMouseButtonsLeft)
			continue;

		playSound(kSoundClick);

		// If we clicked on a difficulty button, change the current difficulty level
		int diff = checkButton(kMainMenuDifficultyButton, ARRAYSIZE(kMainMenuDifficultyButton), mouseX, mouseY);
		if ((diff >= 0) && (diff != (int)_progress.difficulty)) {
			_progress.difficulty = (Difficulty)diff;

			drawMainMenu();
		}

		// If we clicked on a section button, restart the game from this section
		int section = checkButton(kSectionButtons, ARRAYSIZE(kSectionButtons), mouseX, mouseY);
		if ((section >= 0) && (section <= _progress.maximumSection)) {
			_progress.section = section;

			action   = kMenuActionRestart;
		}

	}

	finishMenu(screenBackup, true);

	return action;
}

OnceUpon::MenuAction OnceUpon::handleIngameMenu() {
	beginMenuInput();
	ScreenBackup screenBackup;
	backupScreen(screenBackup, true);

	drawIngameMenu();
	showCursor();

	MenuAction action = kMenuActionNone;
	while (!_vm->shouldQuit() && (action == kMenuActionNone)) {
		endFrame();

		// Check user input

		int16 mouseX, mouseY;
		MouseButtons mouseButtons;

		int16 key = checkMenuInput(mouseX, mouseY, mouseButtons);
		if ((key == kKeyEscape) || (mouseButtons == kMouseButtonsRight))
			// ESC or right mouse button -> Dismiss the menu
			action = kMenuActionPlay;

		if (mouseButtons != kMouseButtonsLeft)
			continue;

		int button = checkButton(kIngameButtons, ARRAYSIZE(kIngameButtons), mouseX, mouseY);
		if      (button == 0)
			action = kMenuActionQuit;
		else if (button == 1)
			action = kMenuActionMainMenu;
		else if (button == 2)
			action = kMenuActionPlay;

	}

	finishMenu(screenBackup, false);

	return action;
}

void OnceUpon::beginMenuInput() {
	expireReadingInput();
	_vm->_util->clearKeyBuf();
	_activations.clear();
	_vm->_util->forceMouseButtonsSync();
	int16 x, y;
	_vm->_util->getMouseState(&x, &y, &_menuHeld);
	_menuIgnoredButtons = _menuHeld;
	_menuIgnoreEscape = (_keyboard.heldKeys() & 0x0040) != 0;
	_menuEscapePending = 0;
	_menuInputActive = true;
}

void OnceUpon::endMenuInput() {
	_menuInputActive = false;
	_menuEscapePending = 0;
	discardActivationInput();
	publishReadingPage();
}

int16 OnceUpon::checkMenuInput(int16 &x, int16 &y, MouseButtons &buttons) {
	_vm->_util->forceMouseButtonsSync();
	MouseButtons held;
	_vm->_util->getMouseState(&x, &y, &held);
	_menuIgnoredButtons = (MouseButtons)((uint32)_menuIgnoredButtons & (uint32)held);
	buttons = (MouseButtons)((uint32)held & ~(uint32)_menuHeld & ~(uint32)_menuIgnoredButtons);
	_menuHeld = held;
	if (!_activations.empty()) {
		const Activation press = _activations.front();
		_activations.pop_front();
		x = press.x;
		y = press.y;
		buttons = press.buttons;
	}
	int16 key = _vm->_util->checkKey();
	if (_menuEscapePending) {
		--_menuEscapePending;
		return kKeyEscape;
	}
	return key;
}

void OnceUpon::drawStartMenu(const MenuButton *animalsButton) {
	// Draw the background
	_vm->_video->drawPackedSprite("menu2.cmp", *_vm->_draw->_backSurface);

	// Draw the "Listen to animal names" button
	if (animalsButton) {
		Surface elements(320, 38, 1);
		_vm->_video->drawPackedSprite("elemenu.cmp", elements);
		_vm->_draw->_backSurface->fillRect(animalsButton->left , animalsButton->top,
		                                   animalsButton->right, animalsButton->bottom, 0);
		drawButton(*_vm->_draw->_backSurface, elements, *animalsButton);
	}

	// Highlight the current difficulty
	drawMenuDifficulty();

	_vm->_draw->forceBlit();
}

void OnceUpon::drawMainMenu() {
	// Draw the background
	_vm->_video->drawPackedSprite("menu.cmp", *_vm->_draw->_backSurface);

	// Highlight the current difficulty
	drawMenuDifficulty();

	// Draw the section buttons
	Surface elements(320, 200, 1);
	_vm->_video->drawPackedSprite("elemenu.cmp", elements);

	for (uint i = 0; i < ARRAYSIZE(kSectionButtons); i++) {
		const MenuButton &button = kSectionButtons[i];

		if (!button.needDraw)
			continue;

		if (_progress.maximumSection >= (int)button.id)
			drawButton(*_vm->_draw->_backSurface, elements, button);
	}

	_vm->_draw->forceBlit();
}

void OnceUpon::drawIngameMenu() {
	Surface menu(320, 34, 1);
	_vm->_video->drawPackedSprite("icon.cmp", menu);

	// Draw the menu in a special way, button by button
	for (uint i = 0; i < ARRAYSIZE(kIngameButtons); i++) {
		const MenuButton &button = kIngameButtons[i];

		drawLineByLine(menu, button.srcLeft, button.srcTop, button.srcRight, button.srcBottom,
		               button.dstX, button.dstY);
	}

	_vm->_draw->forceBlit();
	_vm->_video->retrace();

}

void OnceUpon::drawMenuDifficulty() {
	if (_progress.difficulty == kDifficultyCount)
		return;

	TXTFile *difficulties = loadTXT(getLocFile("diffic.tx"), TXTFile::kFormatStringPositionColor);

	// Center the difficulty name in the caption panel (x=92..222).
	TXTFile::Line &line = difficulties->getLines()[_progress.difficulty];
	line.text.trim();
	line.x = 157 - (line.text.size() * _plettre->getCharWidth()) / 2;
	difficulties->draw((uint) _progress.difficulty, *_vm->_draw->_backSurface, &_plettre, 1, -1);

	// Draw a border around the current difficulty
	drawButtonBorder(kMainMenuDifficultyButton[_progress.difficulty], difficulties->getLines()[_progress.difficulty].color);

	delete difficulties;
}

void OnceUpon::clearIngameMenu(const Surface &background) {
	if (_vm->shouldQuit())
		return;

	// Find the area encompassing the whole ingame menu

	int16 left   = 0x7FFF;
	int16 top    = 0x7FFF;
	int16 right  = 0x0000;
	int16 bottom = 0x0000;

	for (uint i = 0; i < ARRAYSIZE(kIngameButtons); i++) {
		const MenuButton &button = kIngameButtons[i];

		if (!button.needDraw)
			continue;

		left   = MIN<int16>(left  , button.dstX);
		top    = MIN<int16>(top   , button.dstY);
		right  = MAX<int16>(right , button.dstX + (button.srcRight  - button.srcLeft + 1) - 1);
		bottom = MAX<int16>(bottom, button.dstY + (button.srcBottom - button.srcTop  + 1) - 1);
	}

	if ((left > right) || (top > bottom))
		return;

	// Clear it line by line
	drawLineByLine(background, left, top, right, bottom, left, top);
}

OnceUpon::MenuAction OnceUpon::doIngameMenu() {
	_clock.pause();
	// Show the ingame menu
	MenuAction action = handleIngameMenu();

	if ((action == kMenuActionQuit) || _vm->shouldQuit()) {

		// User pressed the quit button, or quit ScummVM
		_quit  = true;
		action = kMenuActionQuit;

	} else if (action == kMenuActionPlay) {

		// User pressed the return to game button
		action = kMenuActionPlay;

	} else if (action == kMenuActionMainMenu) {

		// User pressed the return to main menu button
		action = handleMainMenu();
	}
	if (action == kMenuActionQuit)
		_quit = true;

	_clock.resume();
	return action;
}

SectionOutcome OnceUpon::sceneMenu(bool *resumePage) {
	_acceptActivations = false;
	const MenuAction action = doIngameMenu();
	if (resumePage)
		*resumePage = action == kMenuActionPlay;
	discardActivationInput();
	_acceptActivations = true;
	if (action == kMenuActionQuit)
		return kSectionQuit;
	return action == kMenuActionRestart ? kSectionRestart : kSectionContinue;
}

int OnceUpon::checkButton(const MenuButton *buttons, uint count, int16 x, int16 y, int failValue) const {
	// Look through all buttons, and return the ID of the button we're in

	for (uint i = 0; i < count; i++) {
		const MenuButton &button = buttons[i];

		if ((x >= button.left) && (x <= button.right) && (y >= button.top) && (y <= button.bottom))
			return (int)button.id;
	}

	// We're in none of these buttons, return the fail value
	return failValue;
}

void OnceUpon::drawButton(Surface &dest, const Surface &src, const MenuButton &button, int transp) const {
	dest.blit(src, button.srcLeft, button.srcTop, button.srcRight, button.srcBottom, button.dstX, button.dstY, transp);
}

void OnceUpon::drawButtons(Surface &dest, const Surface &src, const MenuButton *buttons, uint count, int transp) const {
	for (uint i = 0; i < count; i++) {
		const MenuButton &button = buttons[i];

		if (!button.needDraw)
			continue;

		drawButton(dest, src, button, transp);
	}
}

void OnceUpon::drawButtonBorder(const MenuButton &button, uint8 color) {
	_vm->_draw->_backSurface->drawRect(button.left, button.top, button.right, button.bottom, color);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, button.left, button.top, button.right, button.bottom);
}

void OnceUpon::run() {
	init();
	{
		const bool correct = doCopyProtection();
		if (_vm->shouldQuit() || !correct)
			return;
	}
	runStory();
}

void OnceUpon::runStory() {
	while (!_vm->shouldQuit() && !_quit) {
		if (_sceneKind == kSceneNone) {
			if (_introStage != kIntroDone) {
				switch (_introStage) {
				case kIntroLoading:

					showWait(10);
					break;
				case kIntroQuote: beginStoryText(-1); break;
				case kIntroTitle: beginTitle(); break;
				case kIntroGameTitle: beginStoryText(0); break;
				case kIntroReady: showWait(17); break;
				case kIntroDone: break;
				}
				if (_sceneKind == kSceneNone)
					_introStage = (IntroStage)(_introStage + 1);
				continue;
			}
			if (_progress.difficulty == kDifficultyCount) {
				doStartMenu();
				continue;
			}
			_progress.maximumSection = MAX(_progress.maximumSection, _progress.section);
			beginStorySection();
			continue;
		}
		const SectionOutcome outcome = stepScene();
		if (outcome != kSectionContinue)
			completeScene(outcome);
	}
	if (!_vm->shouldQuit())
		showByeBye();
}

void OnceUpon::beginStorySection() {
	struct StoryEntry { SceneKind kind; int16 argument; };
	static const StoryEntry sections[kSectionCount] = {
		{kSceneHouse, 0}, {kSceneStory, 1}, {kSceneParents, 0},
		{kSceneStory, 2}, {kSceneForest, 0}, {kSceneStory, 3},
		{kSceneCastle, 0}, {kSceneStory, 4}, {kSceneForest, 1},
		{kSceneStory, 5}, {kSceneBoss, 0}, {kSceneStory, 6},
		{kSceneForest, 2}, {kSceneStory, 7}, {kSceneEnding, 0}
	};
	if (_progress.section < 0 || _progress.section >= kSectionCount) {
		_quit = true;
		return;
	}
	const StoryEntry &section = sections[_progress.section];

	switch (section.kind) {
	case kSceneHouse: beginHouseChoice(); break;
	case kSceneStory: beginStoryText(section.argument); break;
	case kSceneParents: beginParents(); break;
	case kSceneForest: beginForest(section.argument); break;
	case kSceneCastle: beginEncounter(false); break;
	case kSceneBoss: beginEncounter(true); break;
	case kSceneEnding: beginEnding(); break;
	default: break;
	}
}

SectionOutcome OnceUpon::stepScene() {
	if (!_scene || _vm->shouldQuit() || _quit)
		return kSectionQuit;
	const SectionOutcome outcome = _scene->step();
	return outcome;
}

void OnceUpon::leaveScene() {
	const SceneKind kind = _sceneKind;
	// Title music continues through its normal exit fade.
	if (kind == kSceneTitle)
		fadeOut();
	switch (kind) {
	case kSceneHouse: finishHouseChoice(); break;
	case kSceneCharacter: finishCharacterCreation(); break;
	default: finishScene(); break;
	}
	if (kind == kSceneStory || kind == kSceneHouse ||
		kind == kSceneCharacter || kind == kSceneEnding)
		fadeOut();
	if (kind == kSceneHouse || kind == kSceneCharacter)
		hideCursor();
}

void OnceUpon::completeScene(SectionOutcome outcome) {
	const SceneKind kind = _sceneKind;
	leaveScene();
	if (outcome == kSectionQuit) {
		_quit = true;
		return;
	}
	if (_introStage != kIntroDone) {
		if (outcome == kSectionComplete)
			_introStage = (IntroStage)(_introStage + 1);
		return;
	}
	if ((kind == kSceneHouse && outcome == kSectionComplete) ||
		(kind == kSceneCharacter && outcome == kSectionRetry)) {
		beginCharacterCreation();
		return;
	}
	if (outcome == kSectionRetry && (kind == kSceneCastle || kind == kSceneBoss))
		_progress.section = encounterSpec(kind == kSceneBoss).retrySection;
	else if (outcome == kSectionComplete && kind != kSceneEnding)
		_progress.section = MIN(_progress.section + 1, kSectionCount - 1);
}

OnceUpon::SceneInput OnceUpon::readSceneInput(uint policy) {
	SceneInput input;
	_vm->_util->getMouseState(&input.x, &input.y, &input.buttons);
	input.keys = _keyboard.heldKeys();
	input.confirmation = policy & kNameConfirmation ? _keyboard.confirmationKeys() : 0;
	input.awaitingRelease = (policy & kWaitNameMouseRelease) && (input.buttons & kMouseButtonsLeft);
	input.key = input.awaitingRelease ? 0 : _vm->_util->checkKey();
	if (policy & kReadingInput) {
		if (input.buttons == kMouseButtonsNone && input.keys == 0)
			_freshInput = false;
		collectReadingInput(input.key);
		input.key = _readingMenu ? kKeyEscape : 0;
		_readingMenu = 0;
		input.activation = _readingAdvance;
		input.buttons = kMouseButtonsNone;
		input.keys = input.confirmation = 0;
		_activations.clear();
		return input;
	}
	if (policy & kFlushCharacters) {
		// Reveal and the completed ending discard ordinary buffered characters.
		int16 next;
		while (_vm->_util->checkKey(next))
			if (next == kKeyEscape)
				input.key = next;
	}
	input.activation = false;
	if (_freshInput) {
		if (input.buttons == kMouseButtonsNone && input.keys == 0 && input.confirmation == 0)
			_freshInput = false;
		_activations.clear();
		input.buttons = kMouseButtonsNone;
		input.keys = input.confirmation = 0;
		input.key = 0;
	} else if (!_activations.empty()) {
		const Activation &press = _activations.front();
		input.x = press.x;
		input.y = press.y;
		input.activation = press.buttons == kMouseButtonsLeft;
		if (press.buttons == kMouseButtonsRight)
			input.key = kKeyEscape;
		if (policy & kActivationButtons)
			input.buttons = press.buttons;
		_activations.pop_front();
	}
	return input;
}

void OnceUpon::queueReadingAdvance() {
	if (!_readingVisible || _readingAdvance || _paused || _menuInputActive)
		return;
	_readingAdvance = true;
}

void OnceUpon::collectReadingInput(int16 key) {
	if (_keyboard.takeFocusLost())
		expireReadingInput();
	// Preserve menu requests queued by an outgoing action scene.
	for (Common::List<Activation>::const_iterator press = _activations.begin(); press != _activations.end(); ++press)
		if (press->buttons == kMouseButtonsRight && _readingMenu != kKeyEscape)
			_readingMenu = -1;
	_activations.clear();
	do {
		if (key == kKeyEscape)
			_readingMenu = kKeyEscape;
		else if (_readingKeys == kReadingAny ? key != 0 :
			key == kKeySpace || (_readingKeys == kReadingControls && key == kKeyReturn))
			queueReadingAdvance();
	} while (_vm->_util->checkKey(key));
}

void OnceUpon::beginReadingPage(ReadingKeys keys) {
	// Poll before replacing the visible page; old taps cannot pay for new text.
	_vm->_util->processInput();
	collectReadingInput();
	_readingKeys = keys;
	_readingVisible = false;
	_readingOpening = keys != kNoReading;
	_readingAdvance = false;
	_activations.clear();
}

void OnceUpon::publishReadingPage() {
	if (!_readingOpening || _paused || _menuInputActive)
		return;
	_vm->_util->processInput();
	collectReadingInput();
	if (_vm->shouldQuit() || _paused || _menuInputActive)
		return;
	_readingOpening = false;
	_readingVisible = true;
	// The clock polls before retracing. Publish first so a short new-page tap
	// received in that wait belongs to the pixels the player has already seen.
	_vm->_video->retrace();
}

bool OnceUpon::takeReadingAdvance() {
	const bool advance = _readingAdvance;
	_readingAdvance = false;
	return advance;
}

void OnceUpon::expireReadingInput() {
	_readingVisible = false;
	_readingOpening = _readingKeys != kNoReading;
	_readingAdvance = false;
	_readingMenu = 0;
}

void OnceUpon::endReadingPage() {
	_readingKeys = kNoReading;
	expireReadingInput();
}

bool OnceUpon::acceptCaption(const SceneInput &input, bool &pressed) {
	const bool down = (input.buttons & kMouseButtonsLeft) || (input.keys & 0x0030);
	const bool accepted = input.activation || (down && !pressed) ||
		((input.key == kKeySpace || input.key == kKeyReturn) && !pressed);
	pressed = down;
	return accepted;
}

void OnceUpon::discardActivationInput() {
	expireReadingInput();
	_activations.clear();
	_freshInput = true;
	_vm->_util->clearKeyBuf();
}

void OnceUpon::pauseChanged(bool paused, uint32 duration) {
	(void)duration; // RuntimeClock tracks the pause interval itself.
	if (paused == _paused)
		return;
	_paused = paused;
	expireReadingInput();
	if (paused) {
		_clock.pause();
	} else {
		_clock.resume();
		if (_scene)
			_scene->resumeAfterPause();
		discardActivationInput();
	}
}

bool OnceUpon::handleKeyEvent(const Common::Event &event) {
	if (!_menuInputActive || event.kbd.keycode != Common::KEYCODE_ESCAPE) {
		if (event.type == Common::EVENT_KEYDOWN && event.kbdRepeat) {
			return _menuInputActive || !_scene || !_scene->acceptsKeyRepeat(event.kbd);
		}
		return false;
	}
	if (event.type == Common::EVENT_KEYUP) {
		_menuIgnoreEscape = false;
		return false;
	}
	if (event.type != Common::EVENT_KEYDOWN)
		return false;
	if (!event.kbdRepeat && !_menuIgnoreEscape && _menuEscapePending < KEYBUFSIZE - 1)
		++_menuEscapePending;
	return true;
}

void OnceUpon::handleMouseDown(int16 x, int16 y, MouseButtons buttons) {
	if (_menuInputActive) {
		MouseButtons held;
		int16 mouseX, mouseY;
		_vm->_util->getMouseState(&mouseX, &mouseY, &held);
		_menuIgnoredButtons = (MouseButtons)((uint32)_menuIgnoredButtons & (uint32)held);
		if ((held & buttons) || (_menuIgnoredButtons & buttons) || _activations.size() >= 16)
			return;
		Activation activation;
		activation.x = x + _vm->_video->_scrollOffsetX - _vm->_video->_screenDeltaX;
		activation.y = y + _vm->_video->_scrollOffsetY - _vm->_video->_screenDeltaY;
		activation.buttons = buttons;
		_activations.push_back(activation);
		return;
	}
	if (_readingKeys != kNoReading) {
		if (_keyboard.takeFocusLost())
			expireReadingInput();
		MouseButtons held;
		int16 mouseX, mouseY;
		_vm->_util->getMouseState(&mouseX, &mouseY, &held);
		if (!(held & buttons)) {
			if (buttons == kMouseButtonsLeft) {
				// Both buttons together are not a menu request; Escape is independent.
				if (_readingMenu == -1)
					_readingMenu = 0;
				queueReadingAdvance();
			} else if (buttons == kMouseButtonsRight && !(held & kMouseButtonsLeft) && _readingMenu != kKeyEscape) {
				_readingMenu = -1;
			}
		}
		return;
	}
	if (_scene && _scene->activateMovie(buttons))
		return;
	if (!_acceptActivations || _freshInput || _activations.size() >= 16)
		return;
	MouseButtons held;
	int16 mouseX, mouseY;
	_vm->_util->getMouseState(&mouseX, &mouseY, &held);
	if (held & buttons)
		return;
	Activation activation;
	activation.x = x + _vm->_video->_scrollOffsetX - _vm->_video->_screenDeltaX;
	activation.y = y + _vm->_video->_scrollOffsetY - _vm->_video->_screenDeltaY;
	activation.buttons = buttons;
	_activations.push_back(activation);
}

void OnceUpon::drawScore(uint8 color) {
	drawScore(*_vm->_draw->_backSurface, _progress.score, color);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 119, 190, 185, 199);
}

void OnceUpon::drawScore(Surface &dest, uint32 value, uint8 color) const {
	Common::String score = Common::String::format("%u", value);
	dest.fillRect(119, 190, 185, 197, 9);
	int16 x = 153 - 4 * score.size();
	// Baba Yaga's 12-pixel font extends past the screen. The native
	// renderer clips its rows; drawString would reject the whole glyph.
	for (uint i = 0; i < score.size(); ++i) {
		_plettre->drawLetter(dest, score[i], x, 190, color, 0, true);
		x += _plettre->getCharWidth(score[i]);
	}
}

void OnceUpon::changeScore(int16 amount) {
	int32 delta = (amount / 100) * ((int)_progress.difficulty + 1);
	uint32 score = _progress.score + delta;
	_progress.score = (int32)score < 0 ? 0 : score;
	drawScore(amount > 0 ? 12 : amount < 0 ? 14 : 10);
	_vm->_draw->blitInvalidated();
	_vm->_video->retrace();
	if (amount != 0) {
		_vm->_util->delay(100);
		drawScore();
		_vm->_draw->blitInvalidated();
		_vm->_video->retrace();
	}
}

Surface *OnceUpon::captureSceneTextBackground(const Surface &source) const {
	Surface *background = new Surface(287, 24, 1);
	background->blit(source, 17, 18, 303, 41, 0, 0);
	return background;
}

bool OnceUpon::drawScenePage(GCTFile &text, const Surface &background, uint16 item, uint32 &captionStart, bool terminal) {
	_vm->_draw->_backSurface->blit(background, 0, 0, 286, 23, 17, 18);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 17, 18, 303, 41);
	int16 left, top, right, bottom;
	const bool drawn = text.draw(*_vm->_draw->_backSurface, item, *_plettre, 10, left, top, right, bottom);
	if (drawn)
		_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, left, top, right, bottom);
	if (drawn && (!text.finished() || terminal || _readingKeys != kNoReading))
		beginReadingPage(!text.finished() ? kReadingControls : terminal ? kReadingAny : kNoReading);
	if (_vm->shouldQuit())
		return text.finished();
	_vm->_draw->blitInvalidated();
	if (_readingOpening)
		publishReadingPage();
	else
		_vm->_video->retrace();
	captionStart = _clock.now();
	return text.finished();
}

} // End of namespace OnceUpon

} // End of namespace Gob
