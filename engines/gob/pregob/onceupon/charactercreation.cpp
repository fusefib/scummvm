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

#include "common/system.h"
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
#include "gob/pregob/onceupon/charactercreation.h"
#include "gob/pregob/onceupon/assets.h"
#include "gob/pregob/onceupon/character_preview.h"
#include "gob/pregob/onceupon/onceupon.h"

namespace Gob {
namespace OnceUpon {

const CharacterCreation::Head CharacterCreation::kHeads[4] = {
	{106, 146, 152, 180,  29, 4,  68, 31, 40, 51},
	{155, 146, 201, 180,  83, 4, 113, 31, 45, 51},
	{204, 146, 250, 180, 132, 4, 162, 31, 45, 51},
	{253, 146, 299, 180, 182, 4, 211, 31, 46, 51}
};

const int16 CharacterCreation::kColorAreas[3][2][4] = {
	{{106, 56, 122, 68}, {106, 75, 122, 87}},
	{{106, 91, 122, 103}, {106, 110, 122, 122}},
	{{106, 141, 122, 155}, {106, 160, 122, 173}}
};

CharacterCreation::CharacterCreation(OnceUpon &game) : _vm(game._vm),
	_assets(*game._assets), _game(game), _phase(kPhaseNone), _waitStart(0), _activityStart(0), _font(nullptr), _ani(nullptr),
	_child(nullptr), _choices(nullptr), _heads(nullptr), _background(nullptr), _frame(nullptr),
	_progress(kHead), _head(-1), _hair(12), _jacket(10), _trousers(9), _revealFrame(0),
	_previousPress(false), _awaitNameRelease(false), _acceptedName(false) {
}

CharacterCreation::~CharacterCreation() {
	delete _child;
	delete _background;
	delete _frame;
}

void CharacterCreation::prepare(const Font &font, byte hair, byte jacket, byte trousers) {
	_hair = hair;
	_jacket = jacket;
	_trousers = trousers;
	_font = &font;
	setAppearanceAssets(_hair, _jacket, _trousers);
	if (_ani->getAnimationCount() < 15)
		error("Once Upon a Time: invalid character animations");
	_child = new CharGenChild(*_ani);
	_background = new Surface(320, 200, 1);
	_frame = new Surface(320, 200, 1);
	Assets::Image border = _assets.image("cadre.cmp");
	composeInitialInterface(*border, *_background);
	_frame->blit(*_background);
}

void CharacterCreation::setAppearanceAssets(byte hair, byte jacket, byte trousers) {
	// Both scenes render selected pale hair as animation color 15.
	Assets::Animation animation = _assets.characterAnimation(hair == 7 ? 15 : hair, jacket, trousers);
	if (_child && !_child->rebind(*animation))
		error("Once Upon a Time: incompatible character appearance");
	_ani = animation;
	ColorMap paper;
	paper.replace(12, hair);
	paper.replace(10, jacket);
	paper.replace(9, trousers);
	_choices = _assets.image("choix.cmp", 320, 200, paper);
	ColorMap head;
	head.replace(12, hair);
	_heads = _assets.image("elchoix.cmp", 320, 35, head);
}

void CharacterCreation::composeInitialInterface(const Surface &frame, Surface &dest) const {
	dest.blit(frame);
	dest.fillRect(16, 50, 303, 187, 5);
	dest.blit(*_choices, 0, 38, 159, 121, 140, 54);
	dest.blit(*_choices, 0, 0, 193, 34, 106, 146);
	dest.blit(*_choices, 200, 0, 264, 136, 32, 51);
}

void CharacterCreation::drawInterface() {
	_vm->_draw->_backSurface->blit(*_background);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 0, 0, 319, 199);
}

void CharacterCreation::drawPointer(Surface &cursor) const {
	cursor.blit(*_heads, 0, 0, 15, 15, 0, 0);
}

void CharacterCreation::syncCaption() {
	_frame->blit(*_background, 17, 18, 303, 43, 17, 18);
	_vm->_draw->_backSurface->blit(*_background, 17, 18, 303, 43, 17, 18);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 17, 18, 303, 43);
}

void CharacterCreation::drawPortrait(const Surface &heads, uint head, Surface &dest) {
	assert(head < ARRAYSIZE(kHeads));
	const Head &item = kHeads[head];
	dest.blit(heads, item.sourceLeft, item.sourceTop, item.sourceRight, item.sourceBottom, 0, 0);
}

void CharacterCreation::render() {
	int16 left, top, right, bottom;
	_child->draw(*_frame, left, top, right, bottom);
	_vm->_draw->_backSurface->blit(*_frame, 16, 50, 304, 187, 16, 50);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 16, 50, 304, 187);
	_child->clear(*_frame, left, top, right, bottom);
}

bool CharacterCreation::chooseAppearance(int16 x, int16 y, bool pressed, bool timedOut) {
	if (_progress == kHead) {
		for (uint i = 0; i < ARRAYSIZE(kHeads); ++i) {
			const Head &item = kHeads[i];
			if (timedOut || (pressed && item.left < x && x < item.right && item.top < y && y < item.bottom)) {
				selectHead(i);
				return true;
			}
		}
		return false;
	}

	const uint part = _progress - kHair;
	// Both titles put the alternative jacket color in the upper box.
	const uint secondaryChoice = part == 1 ? 0 : 1;
	const bool secondary = pressed && colorHit(part, secondaryChoice, x, y);
	if (!timedOut && !secondary && !(pressed && colorHit(part, 1 - secondaryChoice, x, y)))
		return false;
	selectColor(part, secondary);
	return true;
}

void CharacterCreation::selectHead(int16 head) {
	_head = head;
	const Head &item = kHeads[head];
	_background->blit(*_heads, item.sourceLeft, item.sourceTop, item.sourceRight, item.sourceBottom, item.x, item.y, 0);
	_background->fillRect(106, 146, 299, 180, 5);
	_background->blit(*_choices, 271, 1, 289, 34, 105, 55);
	_frame->blit(*_background);
	_progress = kHair;
}

void CharacterCreation::redrawPaperDoll() {
	assert(_head >= 0 && _head < 4);
	_background->blit(*_choices, 200, 0, 264, 136, 32, 51);
	const Head &item = kHeads[_head];
	_background->blit(*_heads, item.sourceLeft, item.sourceTop, item.sourceRight, item.sourceBottom, item.x, item.y, 0);
	_child->setAnimation(0);
	_child->setPosition(260, 110);
}

void CharacterCreation::selectColor(uint part, bool secondary) {
	if (secondary) {
		if (part == 0) {
			_hair = 7;
		} else if (part == 1) {
			_jacket = 14;
		} else {
			_trousers = 8;
		}
		setAppearanceAssets(_hair, _jacket, _trousers);
		redrawPaperDoll();
	}
	const int16 tops[] = {55, 90, 140};
	const int16 bottoms[] = {90, 125, 175};
	_background->fillRect(105, tops[part], 123, bottoms[part], 5);
	if (part == 0)
		_background->blit(*_choices, 271, 39, 289, 72, 105, 90);
	else if (part == 1)
		_background->blit(*_choices, 271, 76, 289, 110, 105, 140);
	_frame->blit(*_background);
	_progress = part == 0 ? kJacket : part == 1 ? kTrousers : kNameReveal;
}

bool CharacterCreation::colorHit(uint part, uint choice, int16 x, int16 y) const {
	const int16 *area = kColorAreas[part][choice];
	return area[0] <= x && x <= area[2] && area[1] <= y && y <= area[3];
}

void CharacterCreation::drawNameCursor(uint8 color) {
	int16 x = 150 + _name.size() * 6;
	_background->fillRect(x, 152, x + 6, 163, color);
	_frame->fillRect(x, 152, x + 6, 163, color);
}

void CharacterCreation::prepareName() {
	_name.clear();
	_background->blit(*_choices, 0, 38, 161, 47, 140, 145);
	_background->blit(*_choices, 0, 104, 161, 122, 140, 155);
	_frame->blit(*_background);
	drawNameCursor(10);
	_vm->_draw->_backSurface->blit(*_choices, 0, 38, 161, 47, 140, 145);
	_vm->_draw->_backSurface->blit(*_choices, 0, 104, 161, 122, 140, 155);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 140, 145, 301, 173);
	_revealFrame = 0;
}

void CharacterCreation::revealName() {
	int16 x = 273 - _revealFrame * 3;
	_vm->_draw->_backSurface->blit(*_frame, 273, 144, 303, 175, x, 144);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, x, 144, x + 30, 175);
	_background->blit(*_frame, 273, 144, 303, 175, x, 144);
	++_revealFrame;
}

bool CharacterCreation::editName(int16 key) {
	if (key == kKeyBackspace && !_name.empty()) {
		drawNameCursor(1);
		_name.deleteLastChar();
		drawNameCursor(10);
		return true;
	}
	if (key == kKeySpace)
		key = ' ';
	if (key < ' ' || key > 0xFF || _name.size() >= 14)
		return false;
	drawNameCursor(1);
	Common::String letter;
	letter += (char)key;
	_font->drawString(letter, 150 + _name.size() * 6, 152, 10, 0, true, *_background);
	_font->drawString(letter, 150 + _name.size() * 6, 152, 10, 0, true, *_frame);
	_name += (char)key;
	drawNameCursor(10);
	return true;
}

void CharacterCreation::prepareNameConfirmation(bool timedOut) {
	if (timedOut)
		_name.clear();
	if (_name.empty())
		_name = "Nemo";
	if (_name[0] >= 'a' && _name[0] <= 'z')
		_name.setChar(_name[0] - 'a' + 'A', 0);
	_acceptedName = true;
	_background->fillRect(139, 144, 301, 174, 5);
	_frame->blit(*_background);
	_progress = kConfirmName;
}

void OnceUpon::beginCharacterCreation() {
	finishCharacterCreation();
	beginScene(kSceneCharacter);
	CharacterCreation *character = new CharacterCreation(*this);
	_scene.reset(character);
	character->enter();
}

void OnceUpon::finishCharacterCreation() {
	if (_sceneKind != kSceneCharacter)
		return;
	CharacterCreation *character = static_cast<CharacterCreation *>(_scene.get());
	// Both games retain the previous valid head until a new one is chosen.
	if (character->head() >= 0)
		_progress.head = character->head();
	_progress.hair = character->hair();
	_progress.jacket = character->jacket();
	_progress.trousers = character->trousers();
	if (character->hasAcceptedName())
		_progress.name = character->name();
	stopSound();
	finishScene();
}

void CharacterCreation::enter() {
	_game.fadeOut();
	_game.hideCursor();
	_game.showWait(1);
	prepare(*_game._plettre);
	_text.reset(_game.loadStoryGCT(_game.getLocFile("choix.gc")));
	_prompt.reset(_game.loadTXT(_game.getLocFile("estu.tx"), TXTFile::kFormatStringPositionColor));
	_game.fadeOut();
	drawInterface();
	render();
	drawCaption(5);
	_phase = kPhasePlaying;
	Surface cursor(16, 16, 1);
	drawPointer(cursor);
	_game.showSelectionCursor(cursor);
	_vm->_draw->forceBlit();
	_game.fadeIn();
#ifdef USE_TTS
	_game.publishCaptionNarration();
#endif
	_game._clock.setFrameRate(12);
	_vm->_util->processInput();
	_game.discardActivationInput();
	_game._acceptActivations = true;
	_activityStart = _game._clock.now();
}

void CharacterCreation::drawCaption(uint item) {
	_text->reset();
	if (item != 11)
		_activityStart = _game._clock.now();

	Surface &background = captionBackground();
	int16 left, top, right, bottom;
	_text->clear(background, left, top, right, bottom);
#ifdef USE_TTS
	_game._captionNarrationPending = _text->draw(background, item, *_game._plettre, 10, left, top, right, bottom, &_game._captionWords);
#else
	_text->draw(background, item, *_game._plettre, 10, left, top, right, bottom);
#endif
	syncCaption();
	if (item == 11)
		_game.beginReadingPage(OnceUpon::kReadingAny);
	if (_game._pendingRestore)
		return;
	_vm->_draw->blitInvalidated();
	if (item == 11)
		_game.publishReadingPage();
	else
		_vm->_video->retrace();

	if (item == 10) {
		// Name entry still waits for the selection press to be released.
		_vm->_util->clearKeyBuf();
		_game._activations.clear();
	}
}

Section::Outcome CharacterCreation::stepNameReveal() {
	if (_revealFrame < 18) {
		revealName();
		_vm->_draw->blitInvalidated();
		_vm->_video->retrace();
		_vm->_util->delay(20);
		_game._yield = OnceUpon::kInputYield;
		_vm->_util->processInput();
		_game._yield = OnceUpon::kNotYielded;
		return _game._pendingRestore ? kSectionRestore : kSectionContinue;
	}

	_frame->blit(*_background);
	_progress = CharacterCreation::kName;
	_awaitNameRelease = true;
	drawCaption(10);
	_phase = kPhaseInput;
	return kSectionContinue;
}

bool CharacterCreation::acceptsKeyRepeat(const Common::KeyState &key) const {
	return _phase == kPhaseInput && progress() == kName &&
		(key.keycode == Common::KEYCODE_BACKSPACE || (key.ascii >= ' ' && key.ascii <= 0xFF));
}

Section::Outcome CharacterCreation::step() {
	// Names and localized Yes/No answers use literal keys, including repeats.
	_game._keyboard.setContext(progress() >= kNameReveal && progress() <= kConfirmName ? kTextInput : kPointerInput);
	const bool revealing = progress() == CharacterCreation::kNameReveal;
	if (_phase == kPhasePlaying && !revealing) {
		_phase = kPhaseInput;
		return kSectionContinue;
	}

	const bool firstNameInput = awaitingNameRelease();
	OnceUpon::SceneInput input = _game.readSceneInput(_phase == kPhaseWait ? OnceUpon::kReadingInput :
		OnceUpon::kNameConfirmation | (firstNameInput ? OnceUpon::kWaitNameMouseRelease : 0) |
		(revealing ? OnceUpon::kFlushCharacters : 0));
	if (input.key == kKeyEscape || (input.keys & 0x0040) || input.buttons == kMouseButtonsRight)
		return _game.sceneMenu();

	if (_phase == kPhaseWait) {
		const bool acknowledged = input.activation;
		if (acknowledged || (_game._clock.now() - _waitStart >= 3000
#ifdef USE_TTS
			&& !_game._narration.update(g_system->getMillis())
#endif
			)) {
#ifdef USE_TTS
			_game.cancelNarration();
#endif
			return kSectionComplete;
		}
		_game.endFrame();
		return kSectionContinue;
	}

	if (revealing)
		return stepNameReveal();

	if (input.awaitingRelease) {
		_game.waitInputDelay(10);
		if (_game._pendingRestore)
			return kSectionRestore;
		_game._yield = OnceUpon::kInputYield;
		_vm->_util->processInput();
		_game._yield = OnceUpon::kNotYielded;
		return _game._pendingRestore ? kSectionRestore : kSectionContinue;
	}

	const bool timedOut = _game._clock.now() - _activityStart > 15000;
	if (progress() < CharacterCreation::kNameReveal && !input.activation && (input.keys & 15)) {
		KeyboardInput::moveActionPointer(input.x, input.y, input.keys);
		KeyboardInput::clampActionPointer(input.x, input.y);
		_vm->_util->setMousePos(input.x, input.y);
	}
	if (_game._pendingRestore)
		return kSectionRestore;
	_phase = kPhasePlaying;
	_awaitNameRelease = false;

	const bool mousePressed = input.buttons & kMouseButtonsLeft;
	const bool pressed = mousePressed || input.activation || (input.keys & 0x0010) || input.key == kKeyReturn;
	// Handle the first name input immediately after release, before the
	// normal child animation and click cue resume.
	if (!firstNameInput) {
		render();
		_child->advance();
		const CharGenChild::Sound sound = _child->shouldPlaySound();
		if (sound == CharGenChild::kSoundWalk) {
			_game.beep(50, 10);
		} else if (sound == CharGenChild::kSoundJump) {
			_game.stopSound();
			_game.playSound(OnceUpon::kSoundJump);
		}
		if (_game._pendingRestore)
			return kSectionRestore;

		const bool click = input.activation || (pressed && !_previousPress);
		_previousPress = pressed;
		if (click) {
#ifdef USE_TTS
			_game._narration.cancel();
#endif
			_game.stopSound();
			_game.playSound(OnceUpon::kSoundClick);
			if (_game._pendingRestore)
				return kSectionRestore;
		}
	}

	if (progress() < CharacterCreation::kNameReveal &&
			chooseAppearance(input.x, input.y, pressed, timedOut)) {
		if (progress() == CharacterCreation::kNameReveal) {
			_game.removeSelectionCursor();
#ifdef USE_TTS
			_game.cancelNarration();
#endif
			int16 left, top, right, bottom;
			_text->clear(captionBackground(), left, top, right, bottom);
			syncCaption();
			if (_game._pendingRestore)
				return kSectionRestore;
			prepareName();
			_game._keyboard.setContext(kTextInput);
			return stepNameReveal();
		}
		// CHOIX items 5 to 8 label the four appearance choices.
		drawCaption(5 + progress());
	}

	if (progress() == CharacterCreation::kName) {
		if (timedOut || input.key == kKeyReturn || mousePressed || input.activation) {
			prepareNameConfirmation(timedOut);
			_game._progress.name = name();
			Surface &background = captionBackground();
			background.fillRect(17, 18, 303, 43, 9);
#ifdef USE_TTS
			_prompt->draw(background, &_game._plettre, 1, -1, &_game._captionWords);
			_game._captionNarrationPending = true;
#else
			_prompt->draw(background, &_game._plettre, 1, -1);
#endif
			syncCaption();
			_vm->_draw->blitInvalidated();
			_vm->_video->retrace();
#ifdef USE_TTS
			_game.publishCaptionNarration();
#endif
			if (_game._pendingRestore)
				return kSectionRestore;
		} else if (editName(input.key)) {
#ifdef USE_TTS
			_game._narration.cancel();
#endif
			_activityStart = _game._clock.now();
		}
	}

	if (progress() == CharacterCreation::kConfirmName) {
		if ((input.confirmation & 15) || timedOut || input.key == 'y' || input.key == 'Y' ||
				input.key == 'j' || input.key == 'J' || input.key == 's' || input.key == 'S' ||
				input.key == 'o' || input.key == 'O') {
			_progress = CharacterCreation::kFinished;
			_text->setText(9, _game._progress.name);
			drawCaption(11);
			_waitStart = _game._clock.now();
			_phase = kPhaseWait;
#ifdef USE_TTS
			_game.publishCaptionNarration();
#endif
			return kSectionContinue;
		}
		if ((input.confirmation & 16) || input.key == 'n' || input.key == 'N')
			return kSectionRetry;
	}

	_game.endFrame();
	return kSectionContinue;
}

} // End of namespace OnceUpon
} // End of namespace Gob
