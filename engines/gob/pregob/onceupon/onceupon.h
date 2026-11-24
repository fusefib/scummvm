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

#ifndef GOB_PREGOB_ONCEUPON_ONCEUPON_H
#define GOB_PREGOB_ONCEUPON_ONCEUPON_H

#include "common/system.h"
#include "common/str.h"
#include "common/list.h"
#include "common/ptr.h"

#include "gob/detection/detection.h"
#include "gob/sound/sounddesc.h"
#include "gob/pregob/pregob.h"
#include "gob/pregob/onceupon/input.h"
#ifdef USE_TTS
#include "gob/pregob/onceupon/tts.h"
#endif
#include "gob/pregob/onceupon/scene.h"
#include "gob/pregob/onceupon/runtime_clock.h"
#include "gob/pregob/onceupon/palette.h"

namespace Gob {

class Surface;
class Font;
class ANIFile;

class GCTFile;

namespace OnceUpon {

class Parents;
class DwellingSearch;
class Forest;
class CastleEncounter;
class BossEncounter;
class HouseChoice;
class CharacterCreation;
class Ending;
class StoryText;
class Title;
class Assets;

class OnceUpon : public PreGob {
public:
	/** Number of languages we support. */
	static const uint kLanguageCount = 5;


	OnceUpon(GobEngine *vm);
	~OnceUpon() override;
	void run() override;
	void handleMouseDown(int16 x, int16 y, MouseButtons buttons) override;
	bool handleKeyEvent(const Common::Event &event) override;
	void pauseChanged(bool paused, uint32 duration) override;
	bool canSaveGameState(Common::U32String *msg = nullptr) const override;
	bool canLoadGameState(Common::U32String *msg = nullptr) const override;
	Common::Error saveGameStream(Common::WriteStream &stream, bool isAutosave = false) const override;
	Common::Error loadGameStream(Common::SeekableReadStream &stream) override;


private:
	/** A description of a menu button. */
	struct MenuButton {
		bool needDraw; ///< Does the button need drawing?

		int16 left;   ///< Left   coordinate of the button.
		int16 top;    ///< Top    coordinate of the button.
		int16 right;  ///< Right  coordinate of the button.
		int16 bottom; ///< Bottom coordinate of the button.

		int16 srcLeft;   ///< Left  coordinate of the button's sprite.
		int16 srcTop;    ///< Top   coordinate of the button's sprite.
		int16 srcRight;  ///< Right coordinate of the button's sprite.
		int16 srcBottom; ///< Bottom coordinate of the button's sprite.

		int16 dstX; ///< Destination X coordinate of the button's sprite.
		int16 dstY; ///< Destination Y coordinate of the button's sprite.

		uint id; ///< The button's ID.
	};

	void init();
	void deinit();

	bool doCopyProtection();
	void doStartMenu();
	const MenuButton &animalNamesButton() const;

	void runStory();


	friend class DwellingSearch;
	friend class Forest;
	friend class Encounter;
	friend class CastleEncounter;
	friend class BossEncounter;
	friend class StoryText;
	friend class Ending;
	friend class HouseChoice;
	friend class CharacterCreation;
	friend class Title;
	friend class Parents;
	/** All actions a user can request in a menu. */
	enum MenuAction {
		kMenuActionNone = 0, ///< No action.
		kMenuActionAnimals , ///< Do the animal names.
		kMenuActionPlay    , ///< Play the game.
		kMenuActionRestart , ///< Restart the section.
		kMenuActionMainMenu, ///< Go to the main menu.
		kMenuActionQuit      ///< Quit the game.
	};

	/** Difficulty levels. */
	enum Difficulty {
		kDifficultyBeginner     = 0,
		kDifficultyIntermediate = 1,
		kDifficultyAdvanced     = 2,
		kDifficultyCount
	};

	/** The different sounds common in the game. */
	enum Sound {
		kSoundClick = 0,
		kSoundCourier  ,
		kSoundJump     ,
		kSoundReward   ,
		kSoundCount
	};

	/** A complete screen backup. */
	struct ScreenBackup {
		Surface *screen; ///< Screen contents.
		Palette::State palette; ///< Target and visible color roles.

		bool changedCursor; ///< Did we change the cursor?
		bool cursorVisible; ///< Was the cursor visible?

		ScreenBackup();
		~ScreenBackup();
	};


	/** The number of game sections. */
	static const int kSectionCount = 15;
	static const uint32 kGCTDelay = 12000;

	static const MenuButton kMainMenuDifficultyButton[]; ///< Difficulty buttons.
	static const MenuButton kSectionButtons[];           ///< Section buttons.

	static const MenuButton kIngameButtons[];            ///< Ingame menu buttons.

	static const MenuButton kAbracadabraAnimalsButtons;
	static const MenuButton kAbracadabraAnimalButtons[10];
	static const char *kAbracadabraAnimalNames[10];
	static const MenuButton kBabaYagaAnimalsButtons;
	static const MenuButton kBabaYagaAnimalButtons[10];
	static const char *kBabaYagaAnimalNames[10];
	static const MenuButton kAnimalNamesBack;   ///< "Back" button in the animal names screens.
	static const MenuButton kLanguageButtons[]; ///< Language buttons in the animal names screen.

	/** All general game sounds we know about. */
	static const char *kSound[kSoundCount];

	/** Did we open the game archives? */
	bool _openedArchives;

	// Fonts
	Font *_jeudak;
	Font *_lettre;
	Font *_plettre;
	Font *_glettre;

	Palette _palette;

	bool _quit; ///< Did the user request a normal game quit?

	/** Each dwelling keeps its animation frames and orientation between visits. */
	struct DwellingHotspots {
		int16 cabin[13], castle[11];
		bool cabinMirrored, castleMirrored;
		DwellingHotspots();
		void enter(bool inCabin, bool mirror);
	};

	Common::ScopedPtr<Assets> _assets;
	RuntimeClock _clock;
	DwellingHotspots _dwellingHotspots;
	Progress _progress;
	Common::ScopedPtr<Section> _scene;
	SceneKind _sceneKind;
	int16 _sceneFrameRate;


	struct Activation {
		int16 x, y;
		MouseButtons buttons;
	};

	struct SceneInput {
		int16 x, y, key;
		MouseButtons buttons;
		uint32 keys;
		uint8 confirmation;
		bool activation, awaitingRelease;
	};
	enum InputPolicy {
		kSceneInput = 0,
		kNameConfirmation = 1,
		kWaitNameMouseRelease = 2,
		kFlushCharacters = 4,
		kActivationButtons = 8,
		kReadingInput = 16
	};
	SceneInput readSceneInput(uint policy = kSceneInput);
	bool acceptCaption(const SceneInput &input, bool &pressed);
	// An advance belongs to the page visible at its down edge, not its successor.
	enum ReadingKeys { kNoReading, kReadingSpace, kReadingControls, kReadingAny };
	ReadingKeys _readingKeys;
	bool _readingVisible, _readingOpening, _readingAdvance;
	int16 _readingMenu;
	void beginReadingPage(ReadingKeys keys);
	void publishReadingPage();
	void collectReadingInput(int16 key = 0);
	void queueReadingAdvance();
	bool takeReadingAdvance();
	void expireReadingInput();
	void endReadingPage();

	enum YieldPoint { kNotYielded, kFrameYield, kMovieYield, kInputYield, kNativeEffectYield };
	YieldPoint _yield;
	enum IntroStage { kIntroLoading, kIntroQuote, kIntroTitle, kIntroGameTitle, kIntroReady, kIntroDone };
	IntroStage _introStage;
	// A committed entry, independent of the live scene's partial progress.
	struct Checkpoint {
		Progress progress;
		uint32 randomSeed;
		byte room, energy;
		bool valid;
		Checkpoint() : randomSeed(0), room(0), energy(0), valid(false) {}
		bool intro() const { return valid && progress.section == 0; }
		bool forest() const {
			return progress.section == 4 || progress.section == 8 || progress.section == 12;
		}
	};
	Checkpoint _checkpoint;
	Checkpoint *_pendingRestore;
	bool _initialized;
	SoundDesc _commonSounds[kSoundCount];
	void playSample(SoundDesc &sample, int16 repeats, int16 frequency = 0);
	void replaceSample(SoundDesc &sample, int16 repeats, int16 frequency = 0);
	void playSound(uint sound, int16 frequency = 0, int16 repeats = 0);
	bool _paused;
	bool _selectionCursor;
	const ANIFile *_actionCursorANI;
	uint16 _actionCursorAnimation, _actionCursorFrame;
	int16 _actionCursorX, _actionCursorY;
	bool _acceptActivations;
	bool _freshInput;
	KeyboardInput _keyboard;
	Common::List<Activation> _activations;
	bool _menuInputActive, _menuIgnoreEscape;
	uint _menuEscapePending;
	MouseButtons _menuHeld, _menuIgnoredButtons;
#ifdef USE_TTS
	Narration _narration;
	// Publication requests are transient, never continuation state.
	Common::String _captionWords;
	bool _captionNarrationPending;
	Common::String _menuWords;
	bool _menuNarrationPending;
#endif


	// -- General helpers --

#ifdef USE_TTS
	void narrateText(const Common::String &text);
#endif
	void resetProgress();
	void captureChapterCheckpoint();
	void captureRoomCheckpoint(uint room, uint energy);
	bool validCheckpoint(const Checkpoint &checkpoint) const;
	void commitRestore();
	void discardRestore();
	void beginScene(SceneKind kind);
	void finishScene();
	void beginStorySection();
	SectionOutcome stepScene();
	void leaveScene(SectionOutcome outcome);
	void completeScene(SectionOutcome outcome);
	bool atSaveBoundary() const;
#ifdef USE_TTS
	void cancelNarration();
	void publishCaptionNarration();
	void publishMenuNarration();
	void waitMenuNarration();
#endif
	void endFrame();
	bool captionReady(bool accepted, uint32 captionStart);
	GCTFile *loadStoryGCT(const Common::String &name) const;
	void setGamePalette(uint palette); ///< Select the next game palette.
	void showTemporaryPalette(uint palette); ///< Publish a temporary color pulse.
	void setPalette(const byte *palette, uint16 size);
	void fadeOut();
	void fadeIn();
	void setGameCursor();              ///< Set the default game cursor.
	void showSelectionCursor(Surface &cursor);
	void showActionCursor(const ANIFile &ani, uint16 animation, uint16 frame, int16 x, int16 y);
	void holdActionCursor();
	void removeSelectionCursor();

	/** Reveal alternating rows downwards, then the remaining rows upwards. */
	void drawLineByLine(const Surface &src, int16 left, int16 top, int16 right, int16 bottom,
	                    int16 x, int16 y) const;

	/** Backup the screen contents. */
	void backupScreen(ScreenBackup &backup, bool setDefaultCursor = false);
	/** Restore the screen contents with a previously made backup. */
	void restoreScreen(ScreenBackup &backup, bool keepBlack = false);
	void finishMenu(ScreenBackup &backup, bool fade);
	void waitInputDelay(uint16 milliseconds);

	Common::String fixString(const Common::String &str) const; ///< Fix a string if necessary.
	void fixTXTStrings(TXTFile &txt) const override;                    ///< Fix all strings in a TXT.


	// -- Copy protection helpers --

	/** Set up the copy protection. */
	int8 cpSetup(const uint8 colors[7], const uint8 shapes[7 * 20],
	             const uint8 obfuscate[4], const Surface sprites[2]);
	/** Display the "You are wrong" screen. */
	void cpWrong();


	// -- Show different game screens --

	void showWait(uint palette = 0xFFFF); ///< Show the wait / loading screen.
	void beginStoryText(int chapter);
	void beginTitle();
	void showByeBye();                    ///< Show the "bye bye" screen.

	/** Handle the "listen to animal names" part. */
	void handleAnimalNames();


	// -- Menu helpers --

	MenuAction handleStartMenu(const MenuButton *animalsButton); ///< Handle the start  menu.
	MenuAction handleMainMenu();                                 ///< Handle the main   menu.
	MenuAction handleIngameMenu();                               ///< Handle the ingame menu.
	void beginMenuInput();
	void endMenuInput();
	int16 checkMenuInput(int16 &x, int16 &y, MouseButtons &buttons);

	void drawStartMenu(const MenuButton *animalsButton); ///< Draw the start  menu.
	void drawMainMenu();                                 ///< Draw the main   menu.
	void drawIngameMenu();                               ///< Draw the ingame menu.

	/** Draw the difficulty label. */
	void drawMenuDifficulty();

	/** Clear the ingame menu in an animated way. */
	void clearIngameMenu(const Surface &background);

	/** Handle the whole ingame menu. */
	MenuAction doIngameMenu();
	SectionOutcome sceneMenu(bool *resumePage = nullptr);


	// -- Menu button helpers --

	/** Find the button under these coordinates. */
	int checkButton(const MenuButton *buttons, uint count, int16 x, int16 y, int failValue = -1) const;

	/** Draw a menu button. */
	void drawButton (Surface &dest, const Surface &src, const MenuButton &button, int transp = -1) const;
	/** Draw multiple menu buttons. */
	void drawButtons(Surface &dest, const Surface &src, const MenuButton *buttons, uint count, int transp = -1) const;

	/** Draw a border around a button. */
	void drawButtonBorder(const MenuButton &button, uint8 color);


	// -- Animal names helpers --

	/** Set up the animal chooser. */
	void anSetupChooser();
	/** Set up the language chooser for one animal. */
	void anSetupNames(const MenuButton &animal);
	/** Play / Display the name of an animal in one language. */
	void anPlayAnimalName(const Common::String &animal, uint language);


	// -- Game sections --


	void beginHouseChoice();
	void finishHouseChoice();
	void beginParents();
	void discardActivationInput();
	void drawScore(uint8 color = 10);
	void drawScore(Surface &dest, uint32 score, uint8 color = 10) const;
	void changeScore(int16 amount);
	void beginForest(uint variant, uint room = 0, uint energy = 180);
	Surface *captureSceneTextBackground(const Surface &source) const;
	bool drawScenePage(GCTFile &text, const Surface &background, uint16 item, uint32 &captionStart, bool terminal = false);
	struct EncounterSpec {
		SceneKind kind;
		int16 exteriorPalette, fightPalette, frameRate, retrySection;
		const char *text;
	};
	static const EncounterSpec &encounterSpec(bool boss);
	void beginEncounter(bool boss);
	void beginEnding();

	void beginCharacterCreation();
	void finishCharacterCreation();
};

} // End of namespace OnceUpon

} // End of namespace Gob

#endif // GOB_PREGOB_ONCEUPON_ONCEUPON_H
