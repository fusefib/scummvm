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

#ifndef GOB_PREGOB_ONCEUPON_FOREST_H
#define GOB_PREGOB_ONCEUPON_FOREST_H

#include "common/ptr.h"

#include "gob/sound/sounddesc.h"
#include "gob/pregob/onceupon/scene.h"
#include "gob/pregob/onceupon/forest_actor.h"

namespace Common {
class SeekableReadStream;
}

namespace Gob {

class GobEngine;
class DECFile;
class Surface;

class GCTFile;

namespace OnceUpon {

class Assets;
class OnceUpon;

/** Abracadabra and Baba Yaga forests, including the final return through both. */
class Forest : public Section {
public:
	Forest(OnceUpon &game);
	~Forest() override;
	void enter(uint variant, uint room, uint energy);
	Outcome step() override;
	void pollControls();
	uint room() const;
	uint energy() const { return _energy; }

private:
	// The return passage has twelve rooms: six in each route leg.
	void prepare(uint initialVariant, uint difficulty, byte hair, byte jacket, byte trousers,
		uint room = 0, uint energy = 180);
	void drawInterface(const Surface &portrait);
	void begin();
	void render();
	void drawActor(Surface &surface, const ForestActor &actor, const Common::Rect &area) const;
	bool usesBabaDosWipe() const;
	void drawBabaDosWipePass(uint pass);
	enum Route { kFirstOutward, kSecondOutward, kReturnSecond, kReturnFirst };
	enum ResumePoint {
		kFrameStart, kAfterComment, kCrossing, kDamage, kLoseEnergy, kRetry, kAdvance
	};
	enum Transition { kNextScene, kFirstForest, kEnd };
	enum { kPlayer = 5, kEnemy = 6, kEffect = 10, kSmoke = 11, kActorCount = 12 };
	struct Enemy {
		int16 lethal, animation, triggerX, y, x, destructible;
	};

	struct RouteDescription {
		const char *terrain, *enemies;
		char enemyPrefix;
		int16 sceneStep;
		uint terrainFamily, weaponTier, checkpointBase;
	};
	static const RouteDescription kRoutes[4];
	const RouteDescription &routeDescription() const { return kRoutes[_route]; }
	bool returning() const { return routeDescription().sceneStep < 0; }

	static const int16 kDecorationAnimations[2][6][5];
	static const int16 kDecorationRates[2][6][5];
	static const int16 kSceneScore[3], kEnemyScore[3], kDamageScore[3];

	GobEngine *_vm;
	Assets &_assets;
	OnceUpon &_game;
	Common::SharedPtr<const ANIFile> _playerANI, _enemyANI, _decorationANI, _effectANI, _smokeANI;
	Common::SharedPtr<const DECFile> _dec;
	Common::SharedPtr<const Surface> _weaponsArtwork, _energyIcons;
	Surface *_background, *_frame;
	Common::ScopedPtr<Surface> _preview;
	ForestActor _actors[kActorCount];
	Enemy _enemies[6][4];
	bool _activated[4];
	Route _route;
	uint _difficulty, _sceneIndex, _energy, _comment;
	uint _energyBlocks;
	ResumePoint _resume;
	Transition _transition;
	bool _finished;
	SoundDesc _sounds[4];
	Common::ScopedPtr<GCTFile> _text;
	Common::ScopedPtr<Surface> _captionBackground;
	enum Phase { kPhasePlaying, kPhaseText, kPhaseWait, kPhaseComplete };
	Phase _phase;
	uint16 _textItem;
	uint32 _captionStart, _waitStart;

	void loadSounds();
	void loadText(bool firstForest);
	void drawCaption(uint item, int selection = -1);
	void drawPage();
	void showBabaDosWipe();

	void loadRoute();
	void loadEnemies();
	void prepareDecorations();
	void loadDecorations(uint scene, uint kind);
	void stepDecorations();
	void resetEnemies();
	void activateEnemies();
	void enterScene(bool resetPlayer, bool decorations, bool publish = true);
	void crossScene();
	void startProjectile();
	bool attacking() const;
	static bool readEnemyRow(Common::SeekableReadStream &stream, int16 values[6]);
	void prepareInterface(Surface &dest, const Surface &portrait, const Surface &frame, const Surface &atlas) const;
	void drawEnergy(bool redrawAll = false);
};

} // End of namespace OnceUpon
} // End of namespace Gob

#endif // GOB_PREGOB_ONCEUPON_FOREST_H
