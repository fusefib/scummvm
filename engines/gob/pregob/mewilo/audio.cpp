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

#include "common/textconsole.h"
#include "common/util.h"
#include "gob/gob.h"
#include "gob/global.h"
#include "gob/util.h"
#include "gob/pregob/mewilo/music.h"
#include "gob/sound/sound.h"
#include "gob/sound/soundmixer.h"
#include "gob/pregob/mewilo/mewilo.h"

namespace Gob {
namespace Mewilo {

bool PreGob_Mewilo::playRawSample(const Common::String &name, uint16 frequency, int16 repetitions, bool music) {
	Common::SeekableReadStream *stream = openResource(name);
	if (!stream) {
		renderFailure(Common::String::format("missing sample %s", name.c_str()).c_str(), 0);
		return false;
	}
	const int64 size = stream->size();
	if (size <= 0 || size > 8 * 1024 * 1024) {
		delete stream;
		renderFailure("invalid sample extent", 0);
		return false;
	}
	byte *data = new byte[(uint32)size];
	stream->seek(0);
	const uint32 got = stream->read(data, (uint32)size);
	delete stream;
	if (got != (uint32)size) {
		delete[] data;
		renderFailure("unable to read sample", 0);
		return false;
	}

	Sound::convToSigned(data, (uint32)size);
	playSample(data, (uint32)size, frequency, repetitions, music);
	return true;
}

void PreGob_Mewilo::playSample(byte *pcm, uint32 size, uint16 frequency,
		int16 repetitions, bool music) {
	SoundDesc &sample = music ? _musicSample : _sample;
	// Stop the reader before replacing its backing bytes. Separate descriptors
	// mean an animal effect cannot free or stop a currently playing song.
	if (music)
		_music->stop(0);
	else
		_vm->_sound->blasterStop(0, &_sample);
	sample.free();
	sample.set(SOUND_SND, pcm, size);
	sample._frequency = frequency;
	sample._flag = 8;
	sample._mixerFlags = 0;
	if (music)
		_music->play(sample, repetitions, frequency);
	else
		_vm->_sound->blasterPlay(&sample, repetitions, frequency);
}

void PreGob_Mewilo::stopMewiloAudio() {
	_vm->_sound->blasterStop(0, &_sample);
	_vm->_sound->speakerOff();
	_music->stop(0);
}

bool PreGob_Mewilo::playCpcMusic(const Common::String &name) {
	Common::SeekableReadStream *stream = openResource(name);
	if (!stream)
		return false;
	MewiloCpcMusicData music(*stream);
	delete stream;
	if (!music.valid())
		return false;
	Common::Array<byte> cycle;
	if (!renderMewiloCpcMusic(music, cycle)) return false;
	byte *pcm = new byte[cycle.size()];
	memcpy(pcm, cycle.data(), cycle.size());
	playSample(pcm, cycle.size(), 25000, -1, true);

	// Native code loops the music until CP/M BIOS console-status reports a key.
	// ScummVM's checkKey consumes the buffered event, so retain it for the next
	// VM input node to preserve the native "status only" behavior.
	while (!shouldQuit() && !hasError() && _music->isPlaying()) {
		serviceEvents();
		const int16 key = _vm->_util->checkKey();
		if (key) {
			_pendingKey = key;
			break;
		}
	}

	_music->stop(0);
	return !shouldQuit() && !hasError();
}

void PreGob_Mewilo::specialCommand(byte command, const MewiloVar *variables, uint variableCount) {
	const uint score = variableCount > 0x17 ? (uint16)variables[0x17].word0 : 0;

	// Presentation/VM-adjacent commands are shared across the native ports.
	switch (command) {
	case 0: drawScore(score, 37 * 8, 184, 7); return;
	case 2: installCrossCursor(); showCursor(); return;
	case 7: return;
	case 9: drawScore(score, 28 * 8, 56, 1); return;
	default: break;
	}

	if (_profile.platform == kMewiloPlatformAmstradCPC) {
		// The resident CPC dispatch only implements music commands 6 and 8.
		if ((command == 6 && !playCpcMusic("MEWIL2.MUS")) ||
			(command == 8 && !playCpcMusic("MEWILO.MUS")))
			renderFailure("unable to play native CPC music", 0);
		return;
	}

	// Native Atari ST SPL dispatch.
	switch (command) {
	case 1: playRawSample("OISEAUX.SPL", 7680); break;
	case 3: playRawSample("POULE.SPL", 7680); break;
	case 4: playRawSample("CHEVAL.SPL", 7680); break;
	case 5: playRawSample("CLOCHE.SPL", 7680); break;
	case 6: playRawSample("PERDU.SPL", 9600, 1, true); break;
	case 8: playRawSample("INTRO.SPL", 9600, 1, true); break;
	case 10: playRawSample("COCHON.SPL", 7680, 2); break;
	case 11: playRawSample("PORTE.SPL", 7680); break;
	case 12:
		playRawSample("BOUCHON.SPL", 7680);
		// Native Atari code uses its fixed-delay helper with argument 600 before
		// starting RHUM. Milliseconds remain a compatibility approximation until
		// that CPU-loop delay is calibrated.
		waitMilliseconds(600 / MAX<int16>(1, _vm->_global->_speedFactor));
		if (!shouldQuit())
			playRawSample("RHUM.SPL", 7680);
		break;
	case 13: playRawSample("RIVIERE.SPL", 7680); break;
	case 14: playRawSample("COUCOU.SPL", 7680); break;
	default:
		warning("Mewilo Atari: unknown FF0B command %u", command);
		break;
	}
}

void PreGob_Mewilo::choiceFeedback(byte value) {
	if (value >= 3)
		return;

	if (_profile.platform == kMewiloPlatformAmstradCPC) {
		static const uint16 kPeriods[3] = { 120, 478, 239 };
		static const int32 kLengths[3] = { 250, 350, 250 };
		beep(mewiloCpcTonePeriodToHz(kPeriods[value]), kLengths[value]);
		return;
	}

	// Native Atari feedback tones.
	static const int16 kFrequencies[3] = { 131, 261, 521 };
	static const int32 kLengths[3] = { 150, 100, 100 };
	beep(kFrequencies[value], kLengths[value]);
}

} // End of namespace Mewilo
} // End of namespace Gob
