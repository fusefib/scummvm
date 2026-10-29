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

#include "gob/pregob/mewilo/sample_audio.h"
#include "common/endian.h"
#include "common/util.h"

namespace Gob {
namespace Mewilo {

static const char *const kSplNames[] = {
	"INTRO.SPL", "COUCOU.SPL", "RHUM.SPL", "OISEAUX.SPL", "PERDU.SPL",
	"CHEVAL.SPL", "RIVIERE.SPL", "PORTE.SPL", "CLOCHE.SPL", "COCHON.SPL",
	"POULE.SPL", "BOUCHON.SPL"};
static const char *const kSmpNames[] = {
	"intro.smp", "pendule.smp", "rhum.smp", "oiseaux.smp", "musicfin.smp",
	"henni.smp", "riviere.smp", "porte.smp", "cloche.smp", "cochon.smp",
	"poule.smp", "musbon.smp", "bouchon.smp"};
uint mewiloSampleCount(MewiloAudioBackend backend) {
	return backend == kMewiloAudioAtariSpl ? ARRAYSIZE(kSplNames) :
		backend == kMewiloAudioAmigaSmp ? ARRAYSIZE(kSmpNames) : 0;
}

const char *mewiloSampleName(MewiloAudioBackend backend, uint index) {
	if (index >= mewiloSampleCount(backend))
		return nullptr;
	return backend == kMewiloAudioAtariSpl ? kSplNames[index] : kSmpNames[index];
}
MewiloAudioBackend chooseMewiloSampleBackend(MewiloAudioBackend preferred,
	uint splFiles, uint smpFiles) {
	if (preferred != kMewiloAudioAtariSpl && preferred != kMewiloAudioAmigaSmp)
		return preferred;
	const bool atari = preferred == kMewiloAudioAtariSpl;
	const uint own = atari ? splFiles : smpFiles, other = atari ? smpFiles : splFiles;
	const MewiloAudioBackend alternative = atari ? kMewiloAudioAmigaSmp : kMewiloAudioAtariSpl;
	if (own >= mewiloSampleCount(preferred))
		return preferred;
	if (other >= mewiloSampleCount(alternative) || (!own && other))
		return alternative;
	return preferred;
}

bool readMewiloAmigaSample(Common::SeekableReadStream &stream,
	Common::Array<byte> &pcm, uint16 &rate) {
	pcm.clear();
	rate = 0;
	const int64 size = stream.size();
	// Native samples are small. Bound allocation even for a modified game set.
	if (size < 7 || size > 8 * 1024 * 1024 || !stream.seek(0))
		return false;
	byte header[6];
	if (stream.read(header, sizeof(header)) != sizeof(header))
		return false;
	const uint32 length = READ_BE_UINT32(header);
	const uint16 frequency = READ_BE_UINT16(header + 4);
	if (!frequency || length != uint32(size - sizeof(header)))
		return false;
	pcm.resize(length);
	if (stream.read(pcm.data(), length) != length || stream.err()) {
		pcm.clear();
		return false;
	}
	rate = frequency;
	return true;
}

void renderMewiloAmigaCue(Common::Array<byte> &pcm, uint16 &rate) {
	// Native $106FE repeats an eight-step triangle on Paula channel 0, period
	// 300, until its CPU loop disables DMA. Approximate that 28.2 ms gate with
	// finite signed PCM, including the last partial cycle rather than rounding
	// to a whole waveform repetition.
	rate = 11823;
	pcm.resize(334);
	for (uint i = 0; i < pcm.size(); ++i) {
		const int phase = i & 7;
		const int level = (phase < 4 ? 2 - phase : phase - 6) * 60;
		pcm[i] = byte(level);
	}
}

} // namespace Mewilo
} // namespace Gob
