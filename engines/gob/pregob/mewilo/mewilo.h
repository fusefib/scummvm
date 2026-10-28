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

#ifndef GOB_PREGOB_MEWILO_MEWILO_H
#define GOB_PREGOB_MEWILO_MEWILO_H

#include "gob/pregob/pregob.h"
#include "gob/pregob/mewilo/executor.h"
#include "gob/pregob/mewilo/font.h"
#include "gob/pregob/mewilo/graphics.h"
#include "gob/sound/sounddesc.h"
#include "gob/surface.h"

namespace Gob {

class SoundMixer;

namespace Mewilo {

class PreGob_Mewilo : public PreGob, public MewiloExecHost {
public:
	explicit PreGob_Mewilo(GobEngine *vm);
	~PreGob_Mewilo() override;

	void run() override;
	void pauseChanged(bool paused) override;
	void mousePressed(int16 x, int16 y, MouseButtons buttons) override;
	bool shouldQuit() const override;
	bool hasError() const override { return _hostFailed; }
	void serviceEvents() override;
	void drawTextNode(const MewiloTotData &tot, uint16 nodeOffset) override;
	void drawInlineImage(const MewiloTotData &tot, uint16 nodeOffset) override;
	void drawExternalImage(const MewiloTotData &tot, uint16 nodeOffset) override;
	void saveRect(byte xCell, byte y, byte widthCell, byte height) override;
	void restoreRect() override;
	void waitOrDelay(byte argument) override;
	bool waitContinueInput() override;
	bool waitInputRelease() override;
	void fillScreen(byte color) override;
	void specialCommand(byte command, const MewiloVar *variables, uint variableCount) override;
	void choiceFeedback(byte value) override;
	uint16 randomModulo(uint16 count) override;
	bool waitInput(MewiloInputEvent &event) override;
	bool editTextField(const MewiloTextField &field, Common::Array<byte> &text) override;
	void showRejectMessage(const byte *text, uint length) override;

private:
	friend class MewiloHostTestSuite;

	struct SavedRect {
		int16 x;
		int16 y;
		SurfacePtr surface;
	};

	struct PreparedModule {
		MewiloTotData *tot;
		MewiloI00Bank *i00;
		Common::Array<byte> i01;
		Common::String stem;

		PreparedModule();
		~PreparedModule();
	private:
		PreparedModule(const PreparedModule &);
		PreparedModule &operator=(const PreparedModule &);
	};

	Common::SeekableReadStream *openResource(const Common::String &name) const;
	bool prepareModule(const Common::String &stem, PreparedModule &prepared) const;
	void commitModule(PreparedModule &prepared);
	bool loadFont();
	bool loadModule(const Common::String &stem);
	bool executeModule(const Common::String &stem);
	void unloadModule();
	void resetForRestart();
	Common::String selectorToModule(uint16 selector) const;
	void renderFailure(const char *message, uint16 nodeOffset);
	void markDirty(int16 x, int16 y, int16 width, int16 height);
	void drawByteString(const byte *text, uint length, int16 x, int16 y, byte color, byte lineAdvance = 8);
	void drawScore(uint value, int16 x, int16 y, byte color);
	void drawInputField(const MewiloTextField &field, const Common::Array<byte> &text, bool cursor);
	byte translateInputKey(int16 key) const;
	bool playRawSample(const Common::String &name, uint16 frequency, int16 repetitions = 1, bool music = false);
	void playSample(byte *pcm, uint32 size, uint16 frequency, int16 repetitions, bool music);
	void stopMewiloAudio();
	void installCrossCursor();
	void waitMilliseconds(uint32 durationMs);
	void presentFrame();
	uint32 activeMillis() const;

	const MewiloPlatformProfile &_profile;
	MewiloExecutor _executor;
	MewiloTotData *_tot;
	MewiloI00Bank *_i00;
	Common::Array<byte> _i01;
	Common::Array<byte> _i00Scratch;
	MewiloFont _font;
	SoundDesc _sample;
	SoundDesc _musicSample;
	SoundMixer *_music;
	Common::Array<SavedRect> _savedRects;
	SurfacePtr _crossCursor;
	Common::String _moduleStem;
	int16 _pendingKey;
	Common::Array<MewiloInputEvent> _mousePresses;
	bool _acceptMousePresses;
	uint32 _pauseStart;
	uint32 _pausedMillis;
	uint _pauseDepth;
	bool _hostFailed;
};

} // End of namespace Mewilo
} // End of namespace Gob

#endif // GOB_PREGOB_MEWILO_MEWILO_H
