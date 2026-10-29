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

void PreGob_Mewilo::playEffectTone(int16 frequency, int32 duration) {
	_effectToneUntil = activeMillis() + duration;
	beep(frequency, duration);
}

bool PreGob_Mewilo::audioIdle() const {
	return !samplePlaying() && !_music->isPlaying() && !_vm->_sound->blasterPlayingSound() &&
		int32(activeMillis() - _effectToneUntil) >= 0;
}

bool PreGob_Mewilo::samplePlaying() const {
	return g_system->getMixer()->isSoundHandleActive(_sampleHandle);
}

bool PreGob_Mewilo::playAmigaSample(
	const Common::String &name, byte volume, int16 repetitions, bool music, uint16 rateOffset) {
	Common::SeekableReadStream *stream = openAmigaSample(name);
	Common::Array<byte> pcm;
	uint16 rate = 0;
	const bool valid = stream && readMewiloAmigaSample(*stream, pcm, rate);
	delete stream;
	if (!valid || repetitions <= 0) {
		hostFailure(
			Common::String::format("missing or invalid Amiga sample %s", name.c_str()).c_str(), 0);
		return false;
	}
	const uint32 effectiveRate = uint32(rate) + rateOffset;
	if (effectiveRate > 65535) {
		hostFailure("invalid Amiga sample playback rate", 0);
		return false;
	}
	byte *owned = (byte *)malloc(pcm.size());
	if (!owned) {
		hostFailure("unable to allocate sample", 0);
		return false;
	}
	memcpy(owned, pcm.data(), pcm.size());
	// Native helper $4FBC passes the middle argument as IOAudio cycles, NOT a
	// channel index. $4A64 asks audio.device for masks {2,4,1,8}: the first free
	// voice is Paula channel 1 (left). We have no competing Amiga processes.
	return playSample(owned, pcm.size(), (uint16)effectiveRate, repetitions, music,
		MIN<uint>(volume, 64) * 255 / 64, -127);
}

bool PreGob_Mewilo::playNativeCue() {
	Common::Array<byte> pcm;
	uint16 rate;
	byte volume = 255;
	int8 balance = 0;
	if (_profile.platform == kMewiloPlatformAmiga) {
		renderMewiloAmigaCue(pcm, rate);
		volume = 40 * 255 / 64;
		balance = 127; // Paula channel 0, unlike the file samples on channel 1.
	} else if (_profile.platform == kMewiloPlatformThomsonTO) {
		if (!renderMewiloThomsonBell(pcm)) {
			hostFailure("unable to render Thomson BEL", 0);
			return false;
		}
		rate = kMewiloThomsonSampleRate;
	} else {
		return true;
	}
	byte *owned = (byte *)malloc(pcm.size());
	if (!owned) {
		hostFailure("unable to allocate input cue", 0);
		return false;
	}
	memcpy(owned, pcm.data(), pcm.size());
	return playSample(owned, pcm.size(), rate, 1, false, volume, balance);
}

void PreGob_Mewilo::inputFeedback(MewiloInputFeedback reason) {
	const bool amiga = _profile.platform == kMewiloPlatformAmiga;
	const bool thomson = _profile.platform == kMewiloPlatformThomsonTO;
	if ((reason == kMewiloInputEscapeWithoutCallback && amiga) ||
		(reason == kMewiloInputFieldFull && (amiga || thomson)) ||
		((reason == kMewiloInputInvalidNumber || reason == kMewiloInputEmptyDelete) && thomson))
		playNativeCue();
}

bool PreGob_Mewilo::playRawSample(
	const Common::String &name, uint16 frequency, int16 repetitions, bool music) {
	Common::SeekableReadStream *stream = openResource(name);
	if (!stream) {
		hostFailure(Common::String::format("missing sample %s", name.c_str()).c_str(), 0);
		return false;
	}
	const int64 size = stream->size();
	if (size <= 0 || size > 8 * 1024 * 1024 || repetitions <= 0 || !frequency) {
		delete stream;
		hostFailure("invalid sample extent", 0);
		return false;
	}
	byte *data = (byte *)malloc((uint32)size);
	if (!data) {
		delete stream;
		hostFailure("unable to allocate sample", 0);
		return false;
	}
	stream->seek(0);
	const uint32 got = stream->read(data, (uint32)size);
	delete stream;
	if (got != (uint32)size) {
		free(data);
		hostFailure("unable to read sample", 0);
		return false;
	}

	Sound::convToSigned(data, (uint32)size);
	// Atari $17052 waits for its timer interrupt to clear the active flag.
	// Music and effects both finish before the next script command runs.
	return playSample(data, (uint32)size, frequency, repetitions, music);
}

bool PreGob_Mewilo::playSample(byte *pcm, uint32 size, uint16 frequency,
		int16 repetitions, bool music, byte volume, int8 balance) {
	Audio::SeekableAudioStream *raw =
		Audio::makeRawStream(pcm, size, frequency, 0, DisposeAfterUse::YES);
	if (!raw) {
		free(pcm);
		hostFailure("unable to create sample stream", 0);
		return false;
	}
	Audio::AudioStream *loop = Audio::makeLoopingAudioStream(raw, repetitions);
	Audio::Mixer *mixer = g_system->getMixer();
	mixer->stopHandle(_sampleHandle);
	mixer->playStream(music ? Audio::Mixer::kMusicSoundType : Audio::Mixer::kSFXSoundType,
		&_sampleHandle, loop, -1, volume, balance);
	// The rate converter can retain an audible tail after the producer ends.
	// Wait for the mixer handle, preserving that tail across modal pauses.
	while (!shouldQuit() && !hasError() && samplePlaying()) {
		updateWaitCursor();
		serviceEvents();
		g_system->delayMillis(1);
	}
	mixer->stopHandle(_sampleHandle);
	return !shouldQuit() && !hasError();
}

void PreGob_Mewilo::stopMewiloAudio() {
	_effectToneUntil = activeMillis();
	g_system->getMixer()->stopHandle(_sampleHandle);
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
	return playSample(owned, pcm.size(), kMewiloThomsonSampleRate, 1, true);
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
	_music->stop(0);
	_musicSample.free();
	_musicSample.set(SOUND_SND, pcm, cycle.size());
	_musicSample._frequency = 25000;
	_musicSample._flag = 8;
	_musicSample._mixerFlags = 0;
	_music->play(_musicSample, -1, 25000);

	// Native code loops the music until CP/M BIOS console-status reports a key.
	// ScummVM's checkKey consumes the buffered event, so retain it for the next
	// VM input node to preserve the native "status only" behavior.
	resetWaitCursor();
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

void PreGob_Mewilo::playSoundCommand(byte command) {
	if (_audioBackend == kMewiloAudioCpcMus) {
		// The resident CPC dispatch only implements music commands 6 and 8.
		if ((command == 6 && !playCpcMusic("MEWIL2.MUS")) ||
			(command == 8 && !playCpcMusic("MEWILO.MUS")))
			hostFailure("unable to play native CPC music", 0);
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
			hostFailure("unable to replay Thomson NOTE stream", 0);
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
		// The native 600 is a direct busy-loop counter, without FF03's x3.
		// Convert its recovered cycle count at nominal 8 MHz, not PCM units.
		waitMilliseconds(mewiloAtariFixedDelayMillis(600));
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
		// Native code queues SOUND, then spins using its duration. Use the
		// finite tone's duration as a portable budget for that blocking work.
		waitMilliseconds(kLengths[value]);
		_vm->_sound->speakerOff();
		_effectToneUntil = activeMillis();
		return;
	}

	if (_audioBackend == kMewiloAudioAmigaSmp) {
		// Native feedback temporarily changes musbon's rate, then restores it.
		// Keep the adjustment local so resident content and identity stay intact.
		static const uint16 kRateOffsets[3] = {4000, 2000, 0};
		playAmigaSample("musbon.smp", 64, 1, false, kRateOffsets[value]);
		return;
	}

	// PARTIE4's feedback entry $8930 is an RTS; keep choices silent.
	if (_audioBackend == kMewiloAudioThomsonNote)
		return;

	// Native Atari feedback tones.
	static const int16 kFrequencies[3] = {131, 261, 521};
	static const uint16 kDelayUnits[3] = {150, 100, 100};
	const int32 duration = mewiloAtariFixedDelayMillis(kDelayUnits[value]);
	playEffectTone(kFrequencies[value], duration);
	// Native feedback waits for the helper, then turns the PSG voice off.
	// Keep its ordering while servicing events and excluding modal pause time.
	waitMilliseconds(duration);
	_vm->_sound->speakerOff();
	_effectToneUntil = activeMillis();
}

} // End of namespace Mewilo
} // End of namespace Gob
