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
 */

#ifndef WIZARDRY_VM_CONSOLE_H
#define WIZARDRY_VM_CONSOLE_H

#include "common/stream.h"
#include "wizardry/data/huffman.h"
#include "wizardry/vm/host.h"
#include "wizardry/vm/random.h"

namespace Wizardry {

/** Native console services for the DOS interpreter profiles.
 * The host selects compatibility polling or explicit native-service replay.
 * All state belongs to this instance and is included in the state stream.
 */
class PCodeConsole {
public:
	enum { kCols = 40,
		   kRows = 25,
		   kCells = 1000,
		   kVramSize = 0x4000,
		   kCharsetSize = 5120,
		   kLargePortraitSize = 3072,
		   kTextCols = 80,
		   kTextCells = 2000 };
	explicit PCodeConsole(MachineHost &host, NativeRandom::Algorithm algorithm = NativeRandom::kFourWord,
						  bool classicCache = false);
	PCodeConsole(MachineHost &host, const PCodeConsole &source);
	bool call(bool write, uint16 buffer, uint16 selector, uint16 z, uint16 w);
	bool graphicsCharacter(byte character);
	void teletypeCharacter(byte character);
	bool textOutput() const { return _state.textOutput; }
	byte textCell(uint index) const { return index < kTextCells ? _state.text[index] : 0; }
	byte textColumn() const { return _state.textColumn; }
	byte textRow() const { return _state.textRow; }
	void renderText(byte *pixels, uint pitch, const byte *font) const;
	const byte *vram() const { return _state.vram; }
	const byte *keyboardVram() const;
	void drawTransferChoice(byte profile = 3);
	uint16 visibleCell(uint index) const;
	uint16 visibleAt(uint index) const { return visibleCell(index); }
	void getRngState(uint16 state[4]) const;
	void setRngState(const uint16 state[4]);
	NativeRandom::Algorithm rngAlgorithm() const { return _rngAlgorithm; }
	byte portraitPolicy() const { return _portraitPolicy; }
	bool classicPortraitCache() const { return _classicCache; }
	bool graphicsOutput() const { return _state.initialized; }
	byte errorPrefix() const { return _state.errorPrefix; }
	uint16 stateVersion() const { return 8; }
	bool validateClassicState(Common::String &error) const;
	bool saveState(Common::WriteStream &stream) const;
	bool loadState(Common::ReadStream &stream, Common::String &error);

private:
	struct State {
		uint16 rng[4];
		uint32 idle;
		byte priority[kCells];
		uint16 cell[kCells];
		byte vram[kVramSize];
		byte charset[kCharsetSize];
		uint16 monsterBlock;
		uint16 monsterPicture[4];
		byte monsterRecord[4][512];
		uint16 head;
		bool initialized;
		byte largePortrait[2][kLargePortraitSize];
		byte portraitOrientation; // Logical slot containing physical CS:71A2.
		byte errorPrefix;         // Original CS:2570/2690/2770, retained by WR1 and WR15.
		byte text[kTextCells];
		byte textColumn, textRow;
		bool textOutput;
	};
	struct Window {
		uint16 address, next;
		byte x, y, width, height, priority;
	};
	MachineHost &_host;
	State _state;
	mutable Common::Array<byte> _textVram;
	HuffmanDecoder _huffman;
	NativeRandom::Algorithm _rngAlgorithm;
	byte _portraitPolicy;
	bool _classicCache;
	bool _failed;

	bool fail(const char *message);
	void resetText();
	bool readWindow(uint16 address, Window &window);
	uint16 windowCell(const Window &window, uint index) const;
	void advanceRandom(uint16 buffer);
	void cacheList(uint16 buffer, uint16 key);
	void init(uint16 buffer);
	void putTile(uint cell, const byte *rows);
	void blank(uint cell);
	void draw(uint cell, uint16 word);
	void storeCell(uint cell, uint16 word);
	void drawBorder(const Window &window, bool layoutMode);
	void borderCell(const Window &window, uint cell, uint16 glyph, bool layoutMode);
	void interior(const Window &window);
	void flush();
	void layout(uint16 buffer);
	void present(uint16 buffer);
	void putChar(uint16 address, uint16 character);
	void putString(uint16 address, uint16 source);
	void drawLine(uint16 buffer, uint16 address);
	void clearAttributes(uint16 address);
	void portraits(uint16 buffer, uint16 address);
	void classicPortraits(uint16 buffer, uint16 address);
	void largePortraits(uint16 buffer, uint16 address);
	void backupPortrait(uint16 buffer, uint16 address);
	void swapLargePortraits();
	void syncPortraitCache();
	void title(uint16 block);
	void appendString(uint16 destination, uint16 source);
	void stringPosition(uint16 buffer, uint16 pattern, uint16 text);
	void loadHuffman(uint16 table);
	void decodeHuffman(uint16 source, uint16 destination);
};

} // namespace Wizardry
#endif
