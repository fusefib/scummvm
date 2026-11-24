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

#ifndef GOB_PREGOB_ONCEUPON_PARENTS_H
#define GOB_PREGOB_ONCEUPON_PARENTS_H

#include "common/ptr.h"
#include "common/str.h"

#include "gob/sound/sounddesc.h"

#include "gob/pregob/seqfile.h"
#include "gob/pregob/onceupon/scene.h"

namespace Gob {

class Font;
class GCTFile;

namespace OnceUpon {

class DwellingSearch;
class RuntimeClock;
class Palette;

/** Parents chapter shared by both titles: movie, dwelling search and reward. */
class Parents : public Section {
public:
	explicit Parents(OnceUpon &game);
	~Parents() override;
	void enter();
	Outcome step() override;
	bool atSaveBoundary() const override;
	void suspendMovie() override;
	bool activateMovie(MouseButtons buttons) override;

private:
	enum Phase { kMovie, kSearch };
	class Movie : public SEQFile {
	public:
		typedef Section::Outcome Outcome;
		// Borrow the chapter's caption file; movie sounds and captures stay here.
		Movie(OnceUpon &game, const Common::String &seq, GCTFile &caption);
		~Movie() override;

		bool atSaveBoundary() const { return hasPublishedFrame() || (!_sequenceUpdating && (_waitingText || _textFramePending)); }

		void begin();
		Outcome step();
		bool stepPlay();

	protected:
		void changeFrameRate(int16 rate) override;
		void waitFrame() override;
		void handleFrameEvent() override;
		void handleInput(int16 key, int16 mouseX, int16 mouseY, MouseButtons mouseButtons) override;

	private:
		OnceUpon &_game;
		RuntimeClock &_clock;
		Palette &_palette;
		static const uint kHoldCount = 7;
		static const uint16 kHoldFrames[kHoldCount][2];
		static const uint16 kLightningFrames[2][9];

		enum Sound {
			kSoundCackle  = 0,
			kSoundThunder    ,
			kSoundCount
		};

		static const char *kSound[kSoundCount];


		uint8 _house;

		const Font *_font;

		uint _paletteSize;
		const byte *_normalPalette;
		const byte *_brightPalette;

		SoundDesc _sounds[kSoundCount];

		GCTFile *_gct;

		uint _frameCounter;
		bool _finished;
		uint _textItem;
		uint32 _textStart;
		bool _sequenceUpdating, _textFramePending;
		bool _waitingText;


		void lightningEffect();

		void playSound(Sound sound);
		void setPalette(const byte *palette, uint size);

		void drawGCT(uint item, bool resetCounter = false);
		void readMovieInput(int16 key);
	};

	OnceUpon &_game;
	Common::ScopedPtr<GCTFile> _text;
	Common::ScopedPtr<Movie> _movie;
	Common::ScopedPtr<DwellingSearch> _dwelling;
	Phase _phase;
	bool _cabin;
};

} // End of namespace OnceUpon

} // End of namespace Gob

#endif // GOB_PREGOB_ONCEUPON_PARENTS_H
