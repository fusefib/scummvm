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
 */

#ifndef GOB_PREGOB_MEWILO_EXECUTOR_H
#define GOB_PREGOB_MEWILO_EXECUTOR_H

#include "common/array.h"
#include "gob/pregob/mewilo/tot.h"

namespace Gob {
namespace Mewilo {

static const uint kMewiloVariableCount = 52;

struct MewiloVar {
	int16 word0;
	int16 word1;

	MewiloVar() : word0(0), word1(0) {}
};

enum MewiloInputKey {
	kMewiloKeyNone = 0,
	kMewiloKeyEscape = 0x100,
	kMewiloKeyReturn,
	kMewiloKeyBackspace,
	kMewiloKeyDelete
};

enum MewiloInputFeedback {
	kMewiloInputEscapeWithoutCallback,
	kMewiloInputFieldFull,
	kMewiloInputInvalidNumber,
	kMewiloInputEmptyDelete
};

struct MewiloInputEvent {
	int16 x;
	int16 y;
	uint16 key; ///< printable internal byte or MewiloInputKey
	uint8 mouseButtons;

	MewiloInputEvent() : x(0), y(0), key(kMewiloKeyNone), mouseButtons(0) {}
};

struct MewiloTextField {
	byte xCell;
	byte y;
	byte maxChars;
	byte serializedHeight;
	byte lineAdvance;
	byte colorBase;
	byte colorInput;
	byte colorAlt;
	byte reserved;
	byte answerLength;
	uint16 answerOffset;
};

class MewiloExecHost {
public:
	virtual ~MewiloExecHost() {}
	virtual bool shouldQuit() const = 0;
	virtual bool hasError() const { return false; }
	virtual void serviceEvents() {}
	virtual void drawTextNode(const MewiloTotData &tot, uint16 offset) = 0;
	virtual void drawInlineImage(const MewiloTotData &tot, uint16 offset) = 0;
	virtual void drawExternalImage(const MewiloTotData &tot, uint16 offset) = 0;
	virtual void saveRect(byte xCell, byte y, byte widthCell, byte height) = 0;
	virtual void restoreRect() = 0;
	virtual void waitOrDelay(byte argument) = 0;
	virtual void waitOrDelayAt(const MewiloTotData &, uint16, byte argument) { waitOrDelay(argument); }
	virtual bool waitContinueInput() = 0;
	virtual bool waitInputRelease() = 0;
	virtual void fillScreen(byte color) = 0;
	virtual void specialCommand(byte command, const MewiloVar *variables, uint count) = 0;
	virtual uint16 randomModulo(uint16 count) = 0;
	virtual void choiceFeedback(byte value) = 0;
	virtual void inputFeedback(MewiloInputFeedback) {}
	virtual bool waitInput(MewiloInputEvent &event) = 0;
	virtual bool editTextField(const MewiloTextField &field, Common::Array<byte> &text) = 0;
	virtual void showRejectMessage(const byte *text, uint length) = 0;
	virtual bool translatedAnswer(const MewiloTotData &, uint16, byte,
		const Common::Array<byte> &, bool &handled) const {
		handled = false;
		return false;
	}
	virtual void showRejectMessageAt(const MewiloTotData &, uint16, const byte *text, uint length) { showRejectMessage(text, length); }
};

// Continuations contain script offsets, never native pointers or C++ frames.
struct MewiloExecFrame {
	enum Kind {
		kTarget,
		kSequence,
		kMouse,
		kNumbered,
		kContinue
	};

	byte kind;
	uint16 offset;
	uint16 index;

	MewiloExecFrame(byte k = kTarget, uint16 o = 0, uint16 i = 0) : kind(k), offset(o), index(i) {}
};

struct MewiloExecState {
	MewiloVar variables[kMewiloVariableCount];
	Common::Array<MewiloExecFrame> frames;
};

class MewiloExecutor {
public:
	explicit MewiloExecutor(MewiloExecHost &host);
	void reset();
	void detachModule();
	bool executeRoot(const MewiloTotData &tot);
	bool resume();
	bool atSaveBoundary() const { return _atInput && !_modalDepth; }
	bool exportState(MewiloExecState &out) const;
	static bool validateState(const MewiloTotData &tot, const MewiloExecState &state);
	bool importState(const MewiloTotData &tot, const MewiloExecState &state);
	void adoptState(MewiloExecutor &prepared);

	MewiloVar &variable(uint index) { return _variables[index]; }
	const MewiloVar &variable(uint index) const { return _variables[index]; }

private:
	bool serviceHost();
	bool runFrames(uint baseDepth);
	bool pushFrame(byte kind, uint16 offset, uint16 index = 0);
	bool executeInlineFF(uint16 offset);
	bool conditionTrue(uint16 offset) const;
	bool executeTarget(uint16 offset, uint depth);
	bool stepInput(const MewiloExecFrame &frame);
	bool executeFreeText(uint16 offset, uint depth, uint16 &tailTarget);
	bool answerMatches(const MewiloTextField &field, const Common::Array<byte> &input) const;
	bool parseTextField(uint32 offset, MewiloTextField &field, uint32 &nextOffset) const;

	MewiloExecHost &_host;
	const MewiloTotData *_tot;
	uint _dispatchesUntilService;
	Common::Array<MewiloExecFrame> _frames;
	uint _modalDepth;
	bool _atInput;
	MewiloVar _variables[kMewiloVariableCount];
};

} // End of namespace Mewilo
} // End of namespace Gob

#endif // GOB_PREGOB_MEWILO_EXECUTOR_H
