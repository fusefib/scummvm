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

#ifndef GOB_PREGOB_ONCEUPON_TITLE_H
#define GOB_PREGOB_ONCEUPON_TITLE_H

#include "gob/sound/sounddesc.h"

#include "gob/pregob/seqfile.h"
#include "gob/pregob/onceupon/scene.h"

namespace Gob {

namespace OnceUpon {

class Assets;
class RuntimeClock;
class Palette;

/** Shared title sequence for Abracadabra and Baba Yaga. */
class Title : public SEQFile, public Section {
public:
	Title(OnceUpon &game);
	~Title() override;

	void begin();
	Outcome step() override;
	bool activateMovie(MouseButtons buttons) override;
	void end();
	void activate(MouseButtons buttons);

protected:
	void changeFrameRate(int16 rate) override;
	void waitFrame() override;
	void handleFrameEvent() override;
	void handleInput(int16 key, int16 mouseX, int16 mouseY, MouseButtons mouseButtons) override;

private:
	OnceUpon &_game;
	enum Phase { kPhaseNone, kPhasePlaying };
	Phase _phase;
	RuntimeClock &_clock;
	Palette &_palette;
	SoundDesc _samples[3];
	bool _musicStarted;
	bool _activated;

	void playMusic();        ///< Play the title music.
	void playMusicDOS();     ///< Play the title music of the DOS      version.
	void playMusicAmiga();   ///< Play the title music of the Amiga    version.
	void playMusicAtariST(); ///< Play the title music of the Atari ST version.
	void stopMusic();        ///< Stop the title music.
};

} // End of namespace OnceUpon

} // End of namespace Gob

#endif // GOB_PREGOB_ONCEUPON_TITLE_H
