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

#include "common/str.h"
#include "common/stream.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/tokenizer.h"
#include "common/util.h"

#include "gob/anifile.h"
#include "gob/dataio.h"
#include "gob/decfile.h"
#include "gob/draw.h"
#include "gob/gob.h"
#include "gob/global.h"
#include "gob/surface.h"
#include "gob/util.h"
#include "gob/video.h"

#include "gob/sound/sound.h"

#include "gob/pregob/gctfile.h"
#include "gob/pregob/onceupon/forest.h"
#include "gob/pregob/onceupon/assets.h"
#include "gob/pregob/onceupon/charactercreation.h"
#include "gob/pregob/onceupon/onceupon.h"

namespace Gob {
namespace OnceUpon {

const int16 Forest::kDecorationAnimations[2][6][5] = {
	{{2, 3, 4, -1, -1}, {0, 1, 2, 5, -1}, {2, 3, -1, -1, -1},
	 {0, 1, 3, 4, 5}, {0, 2, -1, -1, -1}, {2, 3, 4, 7, -1}},
	{{0, 4, 5, 6, -1}, {0, 1, 2, 5, -1}, {0, 1, 2, 3, 4},
	 {0, 1, 6, 3, 4}, {0, 5, 6, 7, 8}, {0, 1, 2, 5, -1}}
};

const int16 Forest::kDecorationRates[2][6][5] = {
	{{30, 50, 50, -1, -1}, {1, 50, 50, 50, -1}, {1, 50, -1, -1, -1},
	 {1, 50, 50, 50, 50}, {1, 1, -1, -1, -1}, {50, 50, 50, 1, -1}},
	{{1, 50, 50, 50, -1}, {1, 50, 50, 50, -1}, {50, 50, 50, 50, 50},
	 {50, 50, 50, 50, 50}, {1, 50, 50, 50, 50}, {50, 50, 50, 50, -1}}
};

// Return legs reuse the original DEC scenes in reverse order. Their LE
// tables and checkpoint offsets remain distinct from the outward passages.
const Forest::RouteDescription Forest::kRoutes[4] = {
	{"foret1.dec", "animaux2.ani", 'G',  1, 0, 0},
	{"foret2.dec", "animaux1.ani", 'M',  1, 1, 1},
	{"foret2.dec", "animaux1.ani", 'R', -1, 1, 2},
	{"foret1.dec", "animaux2.ani", 'F', -1, 0, 2}
};

static void alignActorArea(Common::Rect &area) {
	area.left &= ~7;
	area.right = ((area.right - 1) | 7) + 1;
}

const int16 Forest::kSceneScore[3] = {1000, 1500, 3000};
const int16 Forest::kEnemyScore[3] = {100, 200, 500};
const int16 Forest::kDamageScore[3] = {-500, -1000, -2000};

Forest::Forest(OnceUpon &game) : _vm(game._vm),
	_assets(*game._assets), _game(game), _playerANI(nullptr), _enemyANI(nullptr),
	_decorationANI(nullptr), _effectANI(nullptr), _smokeANI(nullptr), _dec(nullptr),
	_energyIcons(nullptr), _background(nullptr), _frame(nullptr), _route(kFirstOutward), _difficulty(0),
	_sceneIndex(0), _energy(180), _comment(0), _energyBlocks(0), _resume(kFrameStart),
	_transition(kNextScene), _finished(false), _phase(kPhasePlaying),
	_textItem(0), _captionStart(0), _waitStart(0) {
	memset(_enemies, 0, sizeof(_enemies));
	memset(_activated, 0, sizeof(_activated));
}

Forest::~Forest() {
	for (uint i = 0; i < ARRAYSIZE(_sounds); ++i)
		_vm->_sound->sampleFree(&_sounds[i]);
	delete _background;
	delete _frame;
}

void Forest::prepare(uint initialVariant, uint difficulty, byte hair, byte jacket, byte trousers) {
	assert(initialVariant < 3 && difficulty < 3);
	_route = (Route)initialVariant;
	_difficulty = difficulty;
	_sceneIndex = returning() ? 5 : 0;
	_energy = 180;
	const uint height = _vm->getPlatform() == Common::kPlatformAmiga ? 160 : 200;
	_background = new Surface(320, height, 1);
	_frame = new Surface(320, height, 1);
	_weaponsArtwork = _assets.forestWeapons();
	_energyIcons = _assets.forestEnergyIcons();
	_background->blit(*_weaponsArtwork);
	_playerANI = _assets.characterAnimation(hair, jacket, trousers);
	_actors[kPlayer].bind(*_playerANI);
	_actors[kPlayer].place(50, 140, 8);
	_actors[kPlayer].bind(*_playerANI, true);
	_actors[kPlayer]._state.status = ForestActor::kActive;
	const bool abra = _vm->getGameType() == kGameTypeAbracadabra;
	const char *effect = initialVariant == 0 ? (abra ? "epouvant.ani" : "transfor.ani") :
		initialVariant == 1 ? "flamme.ani" : (abra ? "pluie.ani" : "explo.ani");
	_effectANI = _assets.animation(effect);
	_actors[kEffect].bind(*_effectANI);
	_actors[kEffect].place(150, 140, 0);
	if (initialVariant) {
		_smokeANI = _assets.animation("fumee.ani");
		_actors[kSmoke].bind(*_smokeANI);
		_actors[kSmoke].place(150, 140, 0);
	}
	loadRoute();
}

void Forest::prepareInterface(Surface &dest, const Surface &portrait, const Surface &frame, const Surface &atlas) const {
	dest.blit(frame);
	dest.blit(portrait, 0, 0, 40, 30, 134, 157);
	if (_vm->getGameType() == kGameTypeAbracadabra) {
		const int16 weapons[3][5] = {{92, 162, 25, 217, 159}, {6, 72, 21, 220, 162}, {176, 220, 31, 225, 155}};
		const int16 *weapon = weapons[routeDescription().weaponTier];
		dest.blit(atlas, weapon[0], 0, weapon[1], weapon[2], weapon[3], weapon[4]);
	} else {
		const int16 weapons[3][3] = {{164, 220, 24}, {0, 88, 26}, {224, 288, 23}};
		const int16 *weapon = weapons[routeDescription().weaponTier];
		dest.blit(atlas, weapon[0], 0, weapon[1], weapon[2], 210, 158);
	}
}

void Forest::drawInterface(const Surface &portrait) {
	const Assets::Image border = _assets.image("cadre.cmp");
	// Only construction needs these templates. Runtime drawing uses the
	// composed surfaces and the small energy-icon strip.
	prepareInterface(*_vm->_draw->_backSurface, portrait, *border, *_weaponsArtwork);
	_weaponsArtwork.reset();
	drawEnergy(true);
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 0, 0, 319, 199);
}

void Forest::drawEnergy(bool redrawAll) {
	const uint blocks = (_energy + 4) / 5;
	const uint first = redrawAll ? 0 : MIN(blocks, _energyBlocks);
	const uint last = redrawAll ? 36 : MAX(blocks, _energyBlocks);
	for (uint i = first; i < last; ++i) {
		int16 x = 24 + (i % 9) * 8, y = 156 + (i / 9) * 8;
		const int16 sourceY = i < blocks ? 8 : 0;
		_vm->_draw->_backSurface->blit(*_energyIcons, 0, sourceY, 6, sourceY + 6, x, y);
		_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, x, y, x + 6, y + 6);
	}
	_energyBlocks = blocks;
}

void Forest::loadRoute() {
	const RouteDescription &route = routeDescription();
	_enemyANI = _assets.animation(route.enemies);
	_dec = _assets.terrain(route.terrain, true);
	for (uint i = kEnemy; i < kEnemy + 4; ++i) {
		_actors[i].bind(*_enemyANI);
		_actors[i].place(2000, 140, 0);
		_actors[i]._state.status = ForestActor::kHidden;
	}
	loadEnemies();
}

bool Forest::readEnemyRow(Common::SeekableReadStream &stream, int16 values[6]) {
	while (!stream.eos() && !stream.err()) {
		const Common::String line = stream.readLine();
		Common::StringTokenizer tokens(line, " \t\r\n\x1a");
		if (tokens.empty())
			continue;
		Common::String token = tokens.nextToken();
		if (token[0] != '-' && token[0] != '+' && (token[0] < '0' || token[0] > '9'))
			continue;
		for (uint field = 0; field < 6; ++field) {
			if (field) {
				if (tokens.empty())
					return false;
				token = tokens.nextToken();
			}
			const bool negative = token[0] == '-';
			uint digit = negative || token[0] == '+' ? 1 : 0;
			if (digit == token.size())
				return false;
			int32 value = 0;
			for (; digit < token.size(); ++digit) {
				if (token[digit] < '0' || token[digit] > '9')
					return false;
				value = value * 10 + token[digit] - '0';
				if (value > (negative ? 32768 : 32767))
					return false;
			}
			values[field] = negative ? -value : value;
		}
		return tokens.empty() && !stream.err();
	}
	return false;
}

void Forest::loadEnemies() {
	const Common::String name = Common::String::format("foret%c.le%u", routeDescription().enemyPrefix, _difficulty + 1);
	Common::ScopedPtr<Common::SeekableReadStream> stream(_vm->_dataIO->getFile(name));
	if (!stream)
		error("Once Upon a Time: missing forest enemy table %s", name.c_str());
	// Six rooms use four records each. Some shipped files contain unused rows.
	for (uint scene = 0; scene < 6; ++scene)
		for (uint i = 0; i < 4; ++i) {
			int16 values[6];
			if (!readEnemyRow(*stream, values))
				error("Once Upon a Time: invalid row %u in forest enemy table %s", scene * 4 + i + 1, name.c_str());
			Enemy &enemy = _enemies[scene][i];
			enemy.lethal = values[0];
			enemy.animation = values[1];
			enemy.triggerX = values[2];
			enemy.y = values[3];
			enemy.x = values[4];
			enemy.destructible = values[5];
		}
}

void Forest::resetEnemies() {
	for (uint i = 0; i < 4; ++i) {
		_activated[i] = false;
		_actors[kEnemy + i]._state.status = ForestActor::kHidden;
	}
}

void Forest::prepareDecorations() {
	_actors[kEffect]._state.status = ForestActor::kHidden;
	if (_vm->getPlatform() == Common::kPlatformAmiga)
		return;
	const uint kind = routeDescription().terrainFamily;
	loadDecorations(_sceneIndex, kind);
	for (uint i = 0; i < 5; ++i) {
		ForestActor &actor = _actors[i];
		actor._state.status = ForestActor::kHidden;
		actor._state.animation = kDecorationAnimations[kind][_sceneIndex][i];
		if (actor._state.animation < 0)
			continue;
		actor.place(150, 140, actor._state.animation);
		const ANIFile::Animation &info = _decorationANI->getAnimationInfo(actor._state.animation);
		actor._state.x = info.x;
		actor._state.y = info.y;
		actor._state.delay = 3;
		_decorationANI->draw(*_frame, actor._state.animation, 0, info.x, info.y);
	}
}

void Forest::loadDecorations(uint scene, uint kind) {
	Common::String name = Common::String::format("foret%u%c.ani", scene + 1, kind ? 'M' : 'G');
	_decorationANI = _assets.animation(name);
	for (uint i = 0; i < 5; ++i)
		_actors[i].bind(*_decorationANI);
}

bool Forest::usesBabaDosWipe() const {
	return _vm->getGameType() == kGameTypeBabaYaga && _vm->getPlatform() == Common::kPlatformDOS;
}

void Forest::drawBabaDosWipePass(uint pass) {
	assert(usesBabaDosWipe() && pass <= 8);
	static const byte offsets[] = {0, 4, 2, 6, 1, 7, 3, 5};
	Surface &screen = *_vm->_draw->_backSurface;
	if (pass < ARRAYSIZE(offsets)) {
		// Native stripes exclude the right/bottom endpoints; the final copy includes them.
		for (int16 x = 16 + offsets[pass]; x < 304; x += 8)
			screen.blit(*_frame, x, 49, x, 148, x, 49);
		for (int16 y = 49 + offsets[pass]; y < 148; y += 8)
			screen.blit(*_frame, 16, y, 304, y, 16, y);
	} else {
		screen.blit(*_frame, 16, 49, 304, 148, 16, 49);
	}
	_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 16, 49, 304, 148);
}

void Forest::enterScene(bool resetPlayer, bool decorations, bool publish) {
	ForestActor::State &player = _actors[kPlayer]._state;
	if (resetPlayer) {
		_comment = 0;
		player.x = player.lastDrawnArea.left = !returning() ? 20 : 270;
		player.lastDrawnArea.right = player.x + 1;
		player.direction = !returning() ? 4 : 0;
		_actors[kPlayer].setAnimation(9);
	}
	_dec->drawBackdrop(*_background, 16, 49);
	_dec->drawParts(*_background, _sceneIndex);
	_frame->blit(*_background, 16, 49, 304, 148, 16, 49);
	if (publish) {
		_vm->_draw->_backSurface->blit(*_frame, 16, 49, 304, 148, 16, 49);
		_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, 16, 49, 304, 148);
	}
	if (decorations)
		prepareDecorations();
	resetEnemies();
}

void Forest::begin() {
	enterScene(true, true);
}

void Forest::stepDecorations() {
	if (_vm->getPlatform() == Common::kPlatformAmiga)
		return;
	const uint kind = routeDescription().terrainFamily;
	for (uint i = 0; i < 5; ++i) {
		ForestActor &actor = _actors[i];
		if (actor._state.animation < 0)
			continue;
		if (actor.lastFrame())
			actor._state.status = ForestActor::kErase;
		if (_vm->_util->getRandom(kDecorationRates[kind][_sceneIndex][i]) == 0)
			actor._state.status = ForestActor::kActive;
	}
}

bool Forest::attacking() const {
	const ForestActor::State &player = _actors[kPlayer]._state;
	return player.frame == 4 && (player.animation == 10 || player.animation == 11 ||
		player.animation == 13 || player.animation == 14 || player.animation == 17);
}

void Forest::startProjectile() {
	if (_route == kFirstOutward || !attacking() || _actors[kEffect]._state.status == ForestActor::kActive)
		return;
	const ForestActor::State &player = _actors[kPlayer]._state;
	int16 x = player.direction == 4 ? player.lastDrawnArea.right + 29 : player.lastDrawnArea.left - 30;
	int16 y = returning() && _vm->getGameType() == kGameTypeAbracadabra ? 80 : 130;
	_actors[kEffect].place(x, y, 0);
	_actors[kEffect]._state.status = ForestActor::kActive;
}

void Forest::activateEnemies() {
	const ForestActor::State &player = _actors[kPlayer]._state;
	for (uint i = 0; i < 4; ++i) {
		ForestActor &actor = _actors[kEnemy + i];
		const Enemy &enemy = _enemies[_sceneIndex][i];
		if (actor._state.status == ForestActor::kHidden && !_activated[i] && enemy.animation != -1 &&
			(!returning() ? enemy.triggerX < player.lastDrawnArea.right - 1 : player.lastDrawnArea.left < enemy.triggerX)) {
			actor.place(enemy.x, enemy.y, enemy.animation);
			actor._state.status = ForestActor::kActive;
			_activated[i] = true;
		}
		if (actor._state.status == ForestActor::kActive && actor._state.lastDrawnArea.right <= 0)
			actor._state.status = ForestActor::kErase;
	}
}

void Forest::crossScene() {
	ForestActor::State &player = _actors[kPlayer]._state;
	enterScene(false, true, !usesBabaDosWipe());
	player.frame = 0;
	if (!returning()) {
		player.x = player.animation == 3 ? player.x - 272 : 20;
		player.lastDrawnArea.left = player.x;
		if (player.animation != 3)
			_actors[kPlayer].setAnimation(9);
	} else {
		player.x = player.animation == 1 ? player.x + 264 : 270;
		player.lastDrawnArea.left = player.x;
		player.lastDrawnArea.right = player.x + 1;
		if (player.animation != 1)
			_actors[kPlayer].setAnimation(8);
	}
}

void Forest::render() {
	Common::Rect restoreAreas[kActorCount], drawnAreas[kActorCount];
	bool redraw[kActorCount];
	for (uint i = 0; i < kActorCount; ++i) {
		const ForestActor &actor = _actors[i];
		const ForestActor::State &state = actor._state;
		redraw[i] = state.status != ForestActor::kHidden && state.redraw;
		if (state.status == ForestActor::kHidden)
			continue;

		restoreAreas[i] = actor.frameArea();
		Common::Rect &drawn = drawnAreas[i];
		drawn = state.lastDrawnArea;
		if (state.status == ForestActor::kActive) {
			drawn = restoreAreas[i];
			alignActorArea(drawn);
		}
		Common::Rect &restore = restoreAreas[i];
		restore.extend(state.lastDrawnArea);
		alignActorArea(restore);
	}

	for (uint i = 0; i < kActorCount; ++i) {
		if (_actors[i]._state.status == ForestActor::kHidden || redraw[i])
			continue;
		for (uint j = 0; j < kActorCount; ++j) {
			if (i != j && _actors[j]._state.status != ForestActor::kHidden && restoreAreas[i].intersects(restoreAreas[j])) {
				redraw[i] = redraw[j] = true;
				break;
			}
		}
	}

	// Restore every selected rectangle before drawing any actor. Combining
	// these passes would erase earlier actors at native overlap boundaries.
	for (uint i = 0; i < kActorCount; ++i) {
		if (!redraw[i])
			continue;
		const Common::Rect &area = restoreAreas[i];
		_frame->blit(*_background, area.left, area.top, area.right - 1, area.bottom - 1, area.left, area.top);
	}
	for (uint i = 0; i < kActorCount; ++i) {
		const ForestActor &actor = _actors[i];
		const ForestActor::State &state = actor._state;
		if (state.status != ForestActor::kActive || !redraw[i])
			continue;
		actor._ani->draw(*_frame, actor.mappedAnimation(), state.frame, state.x, state.y);
		const Common::Rect &drawn = drawnAreas[i];
		_dec->drawForeground(*_frame, _sceneIndex, drawn, drawn.bottom - 1);
	}

	for (uint i = 0; i < kActorCount; ++i) {
		ForestActor::State &state = _actors[i]._state;
		if (state.status == ForestActor::kHidden)
			continue;
		state.redraw = redraw[i];
		if (!redraw[i])
			continue;
		if (state.status == ForestActor::kActive)
			state.lastDrawnArea = drawnAreas[i];
		else {
			restoreAreas[i] = drawnAreas[i];
			state.status = ForestActor::kHidden;
		}
	}

	// Erased actors still publish their old bounds on this frame.
	for (uint i = 0; i < kActorCount; ++i) {
		if (!redraw[i])
			continue;
		Common::Rect area = restoreAreas[i];
		if (i >= kEnemy && i < kEffect) {
			// Abracadabra's falcon in Chapter 2, room 2 (Intermediate,
			// all platforms) flies below the forest as it exits right.
			area.clip(Common::Rect(16, 49, 305, 149));
			if (area.isEmpty())
				continue;
		}
		int16 left = CLIP<int16>(area.left, 16, 304);
		int16 right = MAX<int16>(left, CLIP<int16>(area.right - 1, 16, 304));
		int16 top = MAX<int16>(area.top, 49);
		int16 bottom = MAX<int16>(top, MIN<int16>(area.bottom - 1, 200));
		_vm->_draw->_backSurface->blit(*_frame, left, top, right, bottom, left, top);
		_vm->_draw->dirtiedRect(_vm->_draw->_backSurface, left, top, right, bottom);
	}
}

void OnceUpon::beginForest(uint variant) {
	beginScene(kSceneForest);
	Forest *forest = new Forest(*this);
	_scene.reset(forest);
	forest->enter(variant);
}

void Forest::loadSounds() {
	const char *attack = _route == kFirstOutward ? "mag.snd" : "eclair.snd";
	if (returning() && _vm->getGameType() == kGameTypeAbracadabra)
		attack = _vm->getPlatform() == Common::kPlatformAmiga ? "mag.snd" : "pluie.snd";
	const char *sounds[4] = {attack, "down.snd", "saute.snd", "enclume.snd"};
	for (uint i = 0; i < ARRAYSIZE(_sounds); ++i) {
		_vm->_sound->sampleFree(&_sounds[i]);
		Assets::loadSample(_vm, _sounds[i], sounds[i]);
	}
}

void Forest::loadText(bool firstForest) {
	_text.reset(_game.loadStoryGCT(_game.getLocFile(firstForest ? "foretg.gc" : "foretm.gc")));
	_text->setText(1, _game._progress.name);
}

void Forest::drawCaption(uint item, int selection) {
	_textItem = item;
	_text->reset();
	if (selection >= 0)
		_text->selectLine(item, selection);
	drawPage();
}

void Forest::drawPage() {
	// The return-leg handoff shares item 7 but continues without a result hold.
	const bool terminal = _resume == kCrossing && _transition == kEnd;
	_phase = _game.drawScenePage(*_text, *_captionBackground, _textItem, _captionStart, terminal) ?
		kPhasePlaying : kPhaseText;
}

void Forest::enter(uint route) {
	_game.fadeOut();
	_game.hideCursor();
	_game.setGamePalette(4);
	ColorMap colors;
	if (_game._progress.hair == 7)
		colors.replace(12, 7);
	colors.replace(2, 10);
	colors.replace(7, 15);
	const Assets::Image heads = _assets.image("elchoix.cmp", 320, 200, colors);
	prepare(route, _game._progress.difficulty, _game._progress.hair == 7 ? 15 : 12, _game._progress.jacket, _game._progress.trousers);
	Surface portrait(41, 31, 1);
	CharacterCreation::drawPortrait(*heads, _game._progress.head, portrait);
	drawInterface(portrait);
	_captionBackground.reset(_game.captureSceneTextBackground(*_vm->_draw->_backSurface));
	loadText(routeDescription().terrainFamily == 0);
	loadSounds();
	begin();

	_game._clock.setFrameRate(12);
	_game.discardActivationInput();
	drawCaption(0);
	_game.drawScore();
	_vm->_draw->forceBlit();
	_game.fadeIn();
#ifdef USE_TTS
	_game.publishCaptionNarration();
#endif
	_game._acceptActivations = true;
}

void Forest::showBabaDosWipe() {

	// The patterned DOS path has nine retrace waits. Approximate its timing
	// with 50 ms host slots; native startup benchmarking may skip the pattern.
	for (uint slot = 0; slot < 9 && !_vm->shouldQuit() && !_game._quit; ++slot) {
		const uint32 started = _game._clock.now();

		if (slot)
			drawBabaDosWipePass(slot - 1);
		_vm->_draw->blitInvalidated();
		_vm->_video->retrace();
#ifdef USE_TTS
		_game.publishCaptionNarration();
#endif
		for (;;) {
			_vm->_util->processInput();
			if (_vm->shouldQuit() || _game._quit)
				break;
#ifdef USE_TTS
			_game._narration.update(g_system->getMillis());
#endif

			const uint32 elapsed = _game._clock.now() - started;
			if (elapsed >= 50)
				break;
			_vm->_util->delay(MAX<uint32>(_vm->_global->_speedFactor, MIN<uint32>(10, 50 - elapsed)));
		}
	}
	if (!_vm->shouldQuit() && !_game._quit) {
		drawBabaDosWipePass(8);
		_vm->_draw->blitInvalidated();
		_vm->_video->retrace();
	}

}

Section::Outcome Forest::step() {
	if (_phase == kPhaseComplete)
		return kSectionComplete;
	const bool reading = _game._readingKeys != OnceUpon::kNoReading;
	const OnceUpon::SceneInput input = _game.readSceneInput(reading ? OnceUpon::kReadingInput : OnceUpon::kSceneInput);
	if (input.key == kKeyEscape || input.buttons == kMouseButtonsRight)
		return _game.sceneMenu();
	const bool accepted = input.activation;
	if (_phase == kPhaseText) {
		if (_game.captionReady(accepted, _captionStart))
			drawPage();
		_game.endFrame();
		return kSectionContinue;
	}
	if (_phase == kPhaseWait) {
		// Use the ST PAL cadence for the native 700-retrace hold on every port.
		const uint32 terminalWait = 700 * 20;
		const bool acknowledged = input.activation;
		if (acknowledged)
			_game.takeReadingAdvance();
#ifdef USE_TTS
		if (acknowledged)
			_game.cancelNarration();
#endif
		if (acknowledged || (_game._clock.now() - _waitStart >= terminalWait
#ifdef USE_TTS
			&& !_game._narration.update(g_system->getMillis())
#endif
			)) {
			_phase = kPhasePlaying;
			_game.endReadingPage();
		}
		_game.endFrame();
		return kSectionContinue;
	}

	ForestActor &actor = _actors[kPlayer];
	ForestActor::State &player = actor._state;
	for (;;) {
		switch (_resume) {
		case kFrameStart:
			stepDecorations();
			_resume = kAfterComment;
			if (++_comment == 50) {
				_comment = 0;
				drawCaption(2, _vm->_util->getRandom(4));
				if (_phase == kPhaseText) {
					_game.endFrame();
					return kSectionContinue;
				}
			}
			// fall through
		case kAfterComment: {
			if (_actors[kEffect].lastFrame()) _actors[kEffect]._state.status = ForestActor::kErase;
			if (_smokeANI && _actors[kSmoke].lastFrame()) _actors[kSmoke]._state.status = ForestActor::kErase;

			// Cues use the displayed pose, before input advances the player.
			if ((player.frame == 1 || player.frame == 6) && (player.animation == 0 || player.animation == 4)) {
				_game.beep(50, 10);
				_vm->_util->delay(10);
			} else {
				int16 cue = -1, repeats = 1;
				if (player.frame == 1) {
					if (player.animation == 18 || player.animation == 19) cue = 1;
					if (player.animation == 1 || player.animation == 3) cue = 2;
					if (player.animation == 50 || player.animation == 51 || player.animation == 34) cue = 3;
				} else if (attacking()) {
					cue = 0;
					repeats = _vm->getGameType() == kGameTypeAbracadabra && _route == kReturnSecond ? 2 : 1;
				}
				if (cue >= 0 && !_sounds[cue].empty())
					_game.replaceSample(_sounds[cue], repeats);
			}

			startProjectile();
			for (uint i = 0; i < 4; ++i) {
				ForestActor &enemy = _actors[kEnemy + i];
				const bool hit = _route == kFirstOutward ? attacking() && actor.wandReaches(enemy) : _actors[kEffect].overlaps(enemy);
				if (!hit || !_enemies[_sceneIndex][i].destructible)
					continue;
				enemy._state.status = ForestActor::kErase;
				ForestActor &effect = _actors[_route == kFirstOutward ? kEffect : kSmoke];
				if (effect._state.status != ForestActor::kActive)
					effect.place(enemy._state.lastDrawnArea.left, 140, 0);
				effect._state.status = ForestActor::kActive;
				_game.changeScore(kEnemyScore[routeDescription().weaponTier]);
			}
			activateEnemies();
			if ((player.animation == 18 || player.animation == 19) && player.frame == 4 && (input.keys & 0x60002))
				--player.frame;
			if ((!returning() && player.lastDrawnArea.right > 331 && player.direction == 4) ||
				(returning() && player.lastDrawnArea.right <= 10 && player.direction == 0)) {
				_transition = !returning() ? (_sceneIndex == 5 ? kEnd : kNextScene) :
					_sceneIndex ? kNextScene : _route == kReturnSecond ? kFirstForest : kEnd;
				_game.changeScore(kSceneScore[routeDescription().weaponTier]);
				_resume = kCrossing;
				if (_transition == kNextScene)
					drawCaption(3, _vm->_util->getRandom(3));
				else
					drawCaption(!returning() ? 6 : 7);
				if (_phase == kPhaseText) {
					_game.endFrame();
					return kSectionContinue;
				}
				continue;
			} else if (!returning() && player.lastDrawnArea.left < 16 && player.direction == 0) {
				player.x = 30;
				actor.setAnimation(34);
			} else if (returning() && player.lastDrawnArea.left > 280 && player.direction == 4) {
				player.x = player.lastDrawnArea.left = 270;
				actor.setAnimation(30);
			}
		}
			// fall through
		case kDamage: {
			// Every overlap uses the animation's eligibility at the start of the scan.
			const bool damageEligible = player.animation == 0 || player.animation == 1 || player.animation == 3 || player.animation == 4 ||
				player.animation == 8 || player.animation == 9 || player.animation == 18 || player.animation == 19;
			for (uint i = 0; i < 4; ++i) {
				if (damageEligible && _enemies[_sceneIndex][i].lethal && actor.overlaps(_actors[kEnemy + i])) {
					player.x = player.lastDrawnArea.left;
					actor.setAnimation(player.direction == 0 ? 50 : 51);
					_game.changeScore(kDamageScore[routeDescription().weaponTier]);
				}
			}
			if ((player.animation == 50 || player.animation == 51) && actor.lastFrame()) {
				_resume = kLoseEnergy;
				drawCaption(4, _vm->_util->getRandom(4));
				if (_phase == kPhaseText) {
					_game.endFrame();
					return kSectionContinue;
				}
				continue;
			}
		}
			// fall through
		case kAdvance:
			actor.stepPlayer(input.keys);
			for (uint i = 0; i < kActorCount; ++i)
				_actors[i].advance();
			render();
			_resume = kFrameStart;
			if (_finished)
				_phase = kPhaseComplete;
			_game.endFrame();
			return _finished ? kSectionComplete : kSectionContinue;

		case kCrossing:
			if (_transition == kEnd) {
				_finished = true;
				_resume = kDamage;
				_waitStart = _game._clock.now();
				_phase = kPhaseWait;
				_game.endFrame();
				return kSectionContinue;
			}
			if (_transition == kFirstForest) {
				_route = kReturnFirst;
				_sceneIndex = 5;
				loadRoute();
				loadText(true);
				if (_vm->getPlatform() == Common::kPlatformAmiga)
					loadSounds();
			} else {
				_sceneIndex += routeDescription().sceneStep;
			}
			crossScene();
			_resume = kDamage;
			break;

		case kLoseEnergy:
			_energy -= 5;
			drawEnergy();
			_resume = kRetry;
			if (!_energy) {
				// Both heart updates are published with the refill caption.
				_sceneIndex = returning() ? 5 : 0;
				_energy = 180;
				drawEnergy();
				drawCaption(5);
				if (_phase == kPhaseText) {
					_game.endFrame();
					return kSectionContinue;
				}
			}
			// fall through
		case kRetry:
			enterScene(true, _energy == 180, !usesBabaDosWipe());
			_resume = kAdvance;
			break;
		}
		// A wipe yields so
		// the driver reads fresh input before damage or animation updates.

		if (usesBabaDosWipe()) {
			showBabaDosWipe();
			return (_vm->shouldQuit() || _game._quit) ? kSectionQuit : kSectionContinue;
		}
	}
}


} // End of namespace OnceUpon
} // End of namespace Gob
