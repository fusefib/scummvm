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

#include "common/memstream.h"
#include "common/stream.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/util.h"

#include "gob/dataio.h"
#include "gob/draw.h"
#include "gob/global.h"
#include "gob/gob.h"
#include "gob/surface.h"
#include "gob/util.h"

#include "gob/pregob/txtfile.h"
#include "gob/pregob/onceupon/assets.h"
#include "gob/pregob/onceupon/onceupon.h"
#include "gob/pregob/onceupon/storytext.h"

namespace Gob {
namespace OnceUpon {

StoryText::StoryText(OnceUpon &game) : _game(game),
	_phase(kPhaseNone), _chapter(-1), _page(0), _waitStart(0) {
}

StoryText::~StoryText() {
	clear();
}

void StoryText::clear() {
	for (uint i = 0; i < _pages.size(); ++i)
		delete _pages[i];
	_pages.clear();
}

void StoryText::addPage() {
	Common::MemoryReadStream empty(nullptr, 0);
	_pages.push_back(new TXTFile(empty, TXTFile::kFormatString));
}

bool StoryText::readToken(Common::SeekableReadStream &source, Common::String &value) {
	value.clear();
	while (source.pos() < source.size()) {
		const byte c = source.readByte();
		if (c == ' ' || c == '\r' || c == '\n' || c == '\t' || c == 0x1A) {
			if (!value.empty())
				return !source.err();
		} else {
			value += c;
		}
	}
	return !value.empty() && !source.err();
}

bool StoryText::readNumber(Common::SeekableReadStream &source, int &value) {
	Common::String token;
	if (!readToken(source, token))
		return false;
	const bool negative = token[0] == '-';
	value = 0;
	if (negative && token.size() == 1)
		return false;
	for (uint i = negative ? 1 : 0; i < token.size(); ++i) {
		if (token[i] < '0' || token[i] > '9')
			return false;
		value = value * 10 + token[i] - '0';
		if (value > 32767 + (negative ? 1 : 0))
			return false;
	}
	if (negative)
		value = -value;
	return true;
}

bool StoryText::load(Common::SeekableReadStream &source, bool repairAbracadabraChapter4) {
	clear();
	if (repairAbracadabraChapter4) {
		Common::String corrected;
		while (source.pos() < source.size() && !source.err())
			corrected += source.readByte();
		if (source.err())
			return false;
		// The English and Italian chapter 4 files omit the blank text token.
		// Insert its space marker without changing the four numeric fields.
		const char *const emptyRows[] = {"\r\n 100 141 2 2\r\n", "\r\n 100 132 2 2\r\n"};
		for (uint i = 0; i < ARRAYSIZE(emptyRows); ++i) {
			const uint32 at = corrected.find(emptyRows[i]);
			if (at != Common::String::npos)
				corrected.insertChar('#', at + 2);
		}
		Common::MemoryReadStream stream((const byte *)corrected.c_str(), corrected.size());
		return load(stream);
	}
	int remaining;
	if (!readNumber(source, remaining) || remaining < 0)
		return false;
	addPage();
	while (remaining > 0) {
		Common::String token;
		if (!readToken(source, token))
			break;
		// Native AP markers separate pages without consuming a counted row.
		if (token.size() >= 2 && token[0] == 'A' && token[1] == 'P') {
			addPage();
			continue;
		}
		TXTFile::Line line;
		for (uint i = 0; i < token.size(); ++i) {
			if (token[i] == '#')
				line.text += ' ';
			else if ((byte)token[i] >= ' ')
				line.text += token[i];
		}
		int font;
		if (!readNumber(source, line.x) || !readNumber(source, line.y) ||
			!readNumber(source, line.color) || !readNumber(source, font))
			break;
		// Both native helpers select GLETTRE only for font 1.
		line.font = font == 1 ? 1 : 0;
		_pages.back()->getLines().push_back(line);
		--remaining;
	}
	if (remaining != 0) {
		clear();
		return false;
	}
	// The counted rows exclude trailing DOS end-of-file markers.
	return !source.err();
}

void OnceUpon::beginStoryText(int chapter) {
	beginScene(kSceneStory);
	StoryText *story = new StoryText(*this);
	_scene.reset(story);
	story->begin(chapter);
}

void StoryText::begin(int chapter) {
	GobEngine *const vm = _game._vm;
	_chapter = chapter;
	_page = 0;
	if (chapter >= 0)
		_game._freshInput = false;
	const Common::String filename = _game.getLocFile(chapter < 0 ? "gene.tx" : Common::String::format("gene%d.tx", chapter));
	Common::SeekableReadStream *source = vm->_dataIO->getFile(filename);
	if (!source)
		error("StoryText::begin(): Failed to open \"%s\"", filename.c_str());
	const bool repairEmptyRow = vm->getGameType() == kGameTypeAbracadabra && chapter == 4;
	const bool loaded = load(*source, repairEmptyRow);
	delete source;
	if (!loaded)
		error("StoryText::begin(): Invalid text in \"%s\"", filename.c_str());
	for (uint i = 0; i < pageCount(); ++i)
		_game.fixTXTStrings(page(i));
	// Baba's chapter follows the sword reward, not the earlier dagger.
	if (chapter == 4 && _game._vm->getGameType() == kGameTypeBabaYaga &&
		_game._vm->_global->_language == kLanguageBritish) {
		TXTFile::LineArray &lines = page(0).getLines();
		if (lines.size() > 1 && lines[1].text == "Dagger in hand, the")
			lines[1].text = "Sword in hand, the";
	}
	_canvas.reset(new Surface(320, 200, 1));
	if (chapter >= 0) {
		const Assets::Image artwork = _game._assets->image("parch.cmp");
		_canvas->blit(*artwork);
	}
	drawPage();
}

void StoryText::drawPage() {
	GobEngine *const vm = _game._vm;
	_game.fadeOut();
	_game.setGamePalette(11);
	vm->_draw->_backSurface->blit(*_canvas);
	const Font *fonts[2] = {_game._plettre, _game._glettre};
	TXTFile &page = this->page(_page);
	page.draw(*vm->_draw->_backSurface, fonts, ARRAYSIZE(fonts), -1);
	_game.beginReadingPage(OnceUpon::kReadingAny);
	vm->_draw->forceBlit();
	_game.publishReadingPage();
	_game.fadeIn();
	_waitStart = _game._clock.now();
	_phase = kPhaseDelay;
	_game._acceptActivations = false;
	_game._activations.clear();
}

Section::Outcome StoryText::step() {
	if (_phase == kPhaseComplete)
		return kSectionComplete;
	if (_phase == kPhaseDelay) {
		_game.collectReadingInput();
		if (_game._clock.now() - _waitStart >= 500) {
			_phase = kPhaseWait;
		} else {
			_game.endFrame();
			return kSectionContinue;
		}
	}
	const OnceUpon::SceneInput input = _game.readSceneInput(OnceUpon::kReadingInput);
	const bool menu = input.key == kKeyEscape;
	const bool accepted = input.activation || menu;
	if (accepted)
		_game.takeReadingAdvance();
	// A skip or menu request can arrive as one part finishes, before the
	// next part has been submitted.
	if (!accepted && (
		_game._clock.now() - _waitStart < 14000)) {
		_game.endFrame();
		return kSectionContinue;
	}
	if (menu && _chapter > 0) {
		// Resume acknowledges this page; chooser and GMM returns do not.
		bool resumePage = false;
		const Outcome outcome = _game.sceneMenu(&resumePage);
		if (outcome != kSectionContinue)
			return outcome;
		if (!resumePage)
			return kSectionContinue;
	}
	if ((uint)_page + 1 < pageCount()) {
		++_page;
		drawPage();
		return kSectionContinue;
	}
	_phase = kPhaseComplete;
	return kSectionComplete;
}

} // End of namespace OnceUpon
} // End of namespace Gob
