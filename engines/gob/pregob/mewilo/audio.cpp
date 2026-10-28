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

#include "gob/pregob/mewilo/mewilo.h"

#include "audio/decoders/raw.h"

#include "common/system.h"
#include "common/textconsole.h"
#include "common/util.h"

#include "gob/dataio.h"
#include "gob/global.h"
#include "gob/gob.h"
#include "gob/pregob/mewilo/music.h"
#include "gob/pregob/mewilo/sample_audio.h"
#include "gob/pregob/mewilo/thomson_music.h"
#include "gob/sound/sound.h"
#include "gob/sound/soundmixer.h"
#include "gob/util.h"

namespace Gob {
namespace Mewilo {

Common::SeekableReadStream *PreGob_Mewilo::openAmigaSample(const Common::String &name) const {
	Common::SeekableReadStream *stream = _vm->_dataIO->getFile("sounds/" + name);
	return stream ? stream : _vm->_dataIO->getFile(name);
}

void PreGob_Mewilo::selectSampleAudio() {
	if (_sampleAudioSelected)
		return;
	_sampleAudioSelected = true;
	if (_profile.platform != kMewiloPlatformAtariST && _profile.platform != kMewiloPlatformAmiga)
		return;
	uint spl = 0, smp = 0;
	Common::String missingSpl, missingSmp;
	for (uint i = 0; i < mewiloSampleCount(kMewiloAudioAtariSpl); ++i) {
		Common::SeekableReadStream *stream =
			_vm->_dataIO->getFile(mewiloSampleName(kMewiloAudioAtariSpl, i));
		if (stream && stream->size() > 0 && stream->size() <= 8 * 1024 * 1024)
			++spl;
		else {
			if (!missingSpl.empty())
				missingSpl += ", ";
			missingSpl += mewiloSampleName(kMewiloAudioAtariSpl, i);
		}
		delete stream;
	}
	for (uint i = 0; i < mewiloSampleCount(kMewiloAudioAmigaSmp); ++i) {
		Common::SeekableReadStream *stream =
			openAmigaSample(mewiloSampleName(kMewiloAudioAmigaSmp, i));
		Common::Array<byte> pcm;
		uint16 rate = 0;
		if (stream && readMewiloAmigaSample(*stream, pcm, rate))
			++smp;
		else {
			if (!missingSmp.empty())
				missingSmp += ", ";
			missingSmp += mewiloSampleName(kMewiloAudioAmigaSmp, i);
		}
		delete stream;
	}
	_audioBackend = chooseMewiloSampleBackend(_profile.audioBackend, spl, smp);
	const bool amiga = _audioBackend == kMewiloAudioAmigaSmp;
	debugC(1, kDebugGameFlow,
		"Mewilo: using %s audio (%u valid SPL / %u valid SMP), independently of presentation",
		amiga ? "Amiga SMP" : "Atari SPL", spl, smp);
	if ((amiga ? smp : spl) != mewiloSampleCount(_audioBackend))
		warning("Mewilo: incomplete %s audio (%u/%u). Missing or invalid: %s. "
				"SMP files may be in sounds/ or the game root; SPL files belong in the game root.",
			amiga ? "SMP" : "SPL", amiga ? smp : spl, mewiloSampleCount(_audioBackend),
			(amiga ? missingSmp : missingSpl).c_str());
}

bool PreGob_Mewilo::samplePlaying() const {
	return g_system->getMixer()->isSoundHandleActive(_sampleHandle);
}

bool PreGob_Mewilo::playAmigaSample(
	const Common::String &name, byte volume, int16 repetitions, bool music) {
	Common::SeekableReadStream *stream = openAmigaSample(name);
	Common::Array<byte> pcm;
	uint16 rate = 0;
	const bool valid = stream && readMewiloAmigaSample(*stream, pcm, rate);
	delete stream;
	if (!valid || repetitions <= 0) {
		renderFailure(
			Common::String::format("missing or invalid Amiga sample %s", name.c_str()).c_str(), 0);
		return false;
	}
	byte *owned = (byte *)malloc(pcm.size());
	if (!owned) {
		renderFailure("unable to allocate sample", 0);
		return false;
	}
	memcpy(owned, pcm.data(), pcm.size());
	Audio::SeekableAudioStream *raw =
		Audio::makeRawStream(owned, pcm.size(), rate, 0, DisposeAfterUse::YES);
	if (!raw) {
		free(owned);
		renderFailure("unable to create sample stream", 0);
		return false;
	}
	Audio::AudioStream *loop = Audio::makeLoopingAudioStream(raw, repetitions);
	Audio::Mixer *mixer = g_system->getMixer();
	mixer->stopHandle(_sampleHandle);
	// Native helper $4FBC passes the middle argument as IOAudio cycles, NOT a
	// channel index. $4A64 asks audio.device for masks {2,4,1,8}: the first free
	// voice is Paula channel 1 (left). We have no competing Amiga processes.
	mixer->playStream(music ? Audio::Mixer::kMusicSoundType : Audio::Mixer::kSFXSoundType,
		&_sampleHandle, loop, -1, MIN<uint>(volume, 64) * 255 / 64, -127);
	// $4FBC polls CheckIO, then releases the request. Every native sample call
	// is synchronous; do not let subsequent script effects overtake its audio.
	while (!shouldQuit() && !hasError() && samplePlaying()) {
		serviceEvents();
		g_system->delayMillis(1);
	}
	mixer->stopHandle(_sampleHandle);
	return !shouldQuit() && !hasError();
}

bool PreGob_Mewilo::playRawSample(
	const Common::String &name, uint16 frequency, int16 repetitions, bool music) {
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

void PreGob_Mewilo::playSample(
	byte *pcm, uint32 size, uint16 frequency, int16 repetitions, bool music) {
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
	_effectToneUntil = activeMillis();
	g_system->getMixer()->stopHandle(_sampleHandle);
	_vm->_sound->blasterStop(0, &_sample);
	_vm->_sound->speakerOff();
	_music->stop(0);
}

bool PreGob_Mewilo::playThomsonMusic(byte command) {
	if ((command != 6 && command != 8) || shouldQuit())
		return false;
	// FD and loose installations share this sealed resource opener. Firmware
	// was used only to derive the renderer; players need no system-ROM files.
	Common::SeekableReadStream *stream = openResource("PARTIE4.BIN");
	Common::Array<MewiloThomsonNote> notes;
	const bool loaded = stream && readMewiloThomsonMusic(*stream, command, notes);
	delete stream;
	Common::Array<byte> pcm;
	if (!loaded || !renderMewiloThomsonMusic(notes, pcm) || shouldQuit())
		return false;
	return playMusicPcm(pcm);
}

bool PreGob_Mewilo::playMusicPcm(const Common::Array<byte> &pcm) {
	if (pcm.empty() || shouldQuit())
		return false;
	byte *owned = (byte *)malloc(pcm.size());
	if (!owned)
		return false;
	memcpy(owned, pcm.data(), pcm.size());
	Audio::SeekableAudioStream *raw =
		Audio::makeRawStream(owned, pcm.size(), kMewiloThomsonSampleRate, 0, DisposeAfterUse::YES);
	if (!raw) {
		free(owned);
		return false;
	}
	Audio::Mixer *mixer = g_system->getMixer();
	mixer->stopHandle(_sampleHandle);
	mixer->playStream(Audio::Mixer::kMusicSoundType, &_sampleHandle, raw);
	// The producer can finish before the rate converter's buffered tail is
	// audible. Wait for the finite mixer handle, not SoundMixer::isPlaying().
	// This also preserves the unplayed tail across mixer pauses.
	while (!shouldQuit() && !hasError() && samplePlaying()) {
		serviceEvents();
		g_system->delayMillis(1);
	}
	mixer->stopHandle(_sampleHandle);
	return !shouldQuit() && !hasError();
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
	if (!renderMewiloCpcMusic(music, cycle))
		return false;
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
	case 0:
		drawScore(score, 37 * 8, 184, 7);
		return;
	case 2:
		installCrossCursor();
		showCursor();
		return;
	case 7:
		return;
	case 9:
		drawScore(score, 28 * 8, 56, 1);
		return;
	default:
		break;
	}

	if (_audioBackend == kMewiloAudioCpcMus) {
		// The resident CPC dispatch only implements music commands 6 and 8.
		if ((command == 6 && !playCpcMusic("MEWIL2.MUS")) ||
			(command == 8 && !playCpcMusic("MEWILO.MUS")))
			renderFailure("unable to play native CPC music", 0);
		return;
	}

	if (_audioBackend == kMewiloAudioAmigaSmp) {
		// Native Amiga $4FBC arguments: sample index, repetitions, volume.
		switch (command) {
		case 1:
			playAmigaSample("oiseaux.smp", 40);
			break;
		case 3:
			playAmigaSample("poule.smp", 64);
			break;
		case 4:
			playAmigaSample("henni.smp", 40);
			break;
		case 5:
			playAmigaSample("cloche.smp", 64, 2);
			break;
		case 6:
			playAmigaSample("musicfin.smp", 64, 1, true);
			break;
		case 8:
			playAmigaSample("intro.smp", 64, 1, true);
			break;
		case 10:
			playAmigaSample("cochon.smp", 64);
			break;
		case 11:
			playAmigaSample("porte.smp", 64);
			break;
		case 12:
			playAmigaSample("bouchon.smp", 40);
			if (!shouldQuit() && !hasError())
				playAmigaSample("rhum.smp", 64);
			break;
		case 13:
			playAmigaSample("riviere.smp", 64, 2);
			break;
		case 14:
			playAmigaSample("pendule.smp", 40, 2);
			break;
		default:
			debugC(4, kDebugGameFlow, "Mewilo Amiga: native FF0B no-op/unknown %u", command);
			break;
		}
		return;
	}

	if (_audioBackend == kMewiloAudioThomsonNote) {
		if ((command == 6 || command == 8) && !playThomsonMusic(command))
			renderFailure("unable to replay Thomson NOTE stream", 0);
		return;
	}

	// Native Atari ST SPL dispatch.
	switch (command) {
	case 1:
		playRawSample("OISEAUX.SPL", 7680);
		break;
	case 3:
		playRawSample("POULE.SPL", 7680);
		break;
	case 4:
		playRawSample("CHEVAL.SPL", 7680);
		break;
	case 5:
		playRawSample("CLOCHE.SPL", 7680);
		break;
	case 6:
		playRawSample("PERDU.SPL", 9600, 1, true);
		break;
	case 8:
		playRawSample("INTRO.SPL", 9600, 1, true);
		break;
	case 10:
		playRawSample("COCHON.SPL", 7680, 2);
		break;
	case 11:
		playRawSample("PORTE.SPL", 7680);
		break;
	case 12:
		playRawSample("BOUCHON.SPL", 7680);
		// Native Atari code uses its fixed-delay helper with argument 600 before
		// starting RHUM. Milliseconds remain a compatibility approximation until
		// that CPU-loop delay is calibrated.
		waitMilliseconds(600 / MAX<int16>(1, _vm->_global->_speedFactor));
		if (!shouldQuit() && !hasError())
			playRawSample("RHUM.SPL", 7680);
		break;
	case 13:
		playRawSample("RIVIERE.SPL", 7680);
		break;
	case 14:
		playRawSample("COUCOU.SPL", 7680);
		break;
	default:
		warning("Mewilo Atari: unknown FF0B command %u", command);
		break;
	}
}

void PreGob_Mewilo::choiceFeedback(byte value) {
	if (value >= 3)
		return;

	if (_audioBackend == kMewiloAudioCpcMus) {
		static const uint16 kPeriods[3] = {120, 478, 239};
		static const int32 kLengths[3] = {250, 350, 250};
		playEffectTone(mewiloCpcTonePeriodToHz(kPeriods[value]), kLengths[value]);
		return;
	}

	if (_audioBackend == kMewiloAudioAmigaSmp) {
		// All three native feedback call sites select musbon, one cycle.
		playAmigaSample("musbon.smp", 64);
		return;
	}

	// PARTIE4's feedback entry $8930 is an RTS; keep choices silent.
	if (_audioBackend == kMewiloAudioThomsonNote)
		return;

	// Native Atari feedback tones.
	static const int16 kFrequencies[3] = {131, 261, 521};
	static const int32 kLengths[3] = {150, 100, 100};
	playEffectTone(kFrequencies[value], kLengths[value]);
}

} // End of namespace Mewilo
} // End of namespace Gob
