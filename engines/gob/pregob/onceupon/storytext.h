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

#ifndef GOB_PREGOB_ONCEUPON_STORYTEXT_H
#define GOB_PREGOB_ONCEUPON_STORYTEXT_H

#include "common/array.h"
#include "common/ptr.h"
#include "common/str.h"

#include "gob/pregob/onceupon/scene.h"

namespace Common {
class SeekableReadStream;
}

namespace Gob {
class TXTFile;
class Surface;

namespace OnceUpon {

/** Formatted quote, game-title and chapter pages used by both titles. */
class StoryText : public Section {
public:
	explicit StoryText(OnceUpon &game);
	~StoryText() override;
	void begin(int chapter);
	Outcome step() override;
	bool load(Common::SeekableReadStream &source, bool repairAbracadabraChapter4 = false);
	void clear();
	uint pageCount() const { return _pages.size(); }
	TXTFile &page(uint index) { return *_pages[index]; }

private:
	OnceUpon &_game;
	Common::Array<TXTFile *> _pages;
	Common::ScopedPtr<Surface> _canvas;
	enum Phase { kPhaseNone, kPhaseDelay, kPhaseWait, kPhaseComplete };
	Phase _phase;
	int16 _chapter;
	uint16 _page;
	uint32 _waitStart;
	void drawPage();
	void addPage();
	static bool readToken(Common::SeekableReadStream &source, Common::String &value);
	static bool readNumber(Common::SeekableReadStream &source, int &value);
};

} // End of namespace OnceUpon
} // End of namespace Gob

#endif // GOB_PREGOB_ONCEUPON_STORYTEXT_H
