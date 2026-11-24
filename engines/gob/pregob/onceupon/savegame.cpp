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

#include "common/md5.h"
#include "common/memstream.h"
#include "common/translation.h"
#include "gob/gob.h"
#include "gob/global.h"
#include "gob/draw.h"
#include "gob/surface.h"
#include "gob/util.h"
#include "gob/pregob/onceupon/onceupon.h"

namespace Gob {
namespace OnceUpon {

// The Intro ends at the section byte. Chapter and room entries add the
// committed profile and up to fourteen name bytes.
static const uint kIntroPayloadSize = 7;
static const uint kCheckpointPrefix = 25;
static const uint kCheckpointNameLimit = 14;

bool OnceUpon::validCheckpoint(const Checkpoint &checkpoint) const {
	if (checkpoint.intro())
		return true;
	const Progress &p = checkpoint.progress;
	if (!checkpoint.valid || p.section < 1 || p.section >= kSectionCount ||
		p.maximumSection < p.section || p.maximumSection >= kSectionCount ||
		p.difficulty < 0 || p.difficulty >= kDifficultyCount || p.house > 3 ||
		p.head > 3 || p.hair > 15 || p.jacket > 15 || p.trousers > 15 ||
		p.name.size() > kCheckpointNameLimit)
		return false;
	if (checkpoint.forest())
		return checkpoint.room < (p.section == 12 ? 12 : 6) &&
			checkpoint.energy > 0 && checkpoint.energy <= 180 && checkpoint.energy % 5 == 0;
	return (p.section & 1) && checkpoint.room == 0 && checkpoint.energy == 0;
}

void OnceUpon::captureChapterCheckpoint() {
	if (_progress.section <= 0 || _progress.difficulty >= kDifficultyCount) {
		_checkpoint = Checkpoint();
		_checkpoint.valid = _progress.section == 0;
		return;
	}
	const int16 opening = (_progress.section - 1) | 1;
	// Advancing from a chapter card to its gameplay keeps the original entry.
	if (_checkpoint.valid && _checkpoint.progress.section == opening)
		return;
	_checkpoint = Checkpoint();
	_checkpoint.progress = _progress;
	_checkpoint.progress.section = opening;
	_checkpoint.progress.maximumSection = MAX(_progress.maximumSection, _progress.section);
	_checkpoint.randomSeed = _vm->_rnd.getSeed();
	_checkpoint.valid = true;

}

void OnceUpon::captureRoomCheckpoint(uint room, uint energy) {
	_checkpoint = Checkpoint();
	_checkpoint.progress = _progress;
	_checkpoint.progress.maximumSection = MAX(_progress.maximumSection, _progress.section);
	_checkpoint.randomSeed = _vm->_rnd.getSeed();
	_checkpoint.room = room;
	_checkpoint.energy = energy;
	_checkpoint.valid = true;
}

bool OnceUpon::atSaveBoundary() const {
	if (_yield != kMovieYield)
		return true;
	return _scene && _scene->atSaveBoundary();
}

bool OnceUpon::canSaveGameState(Common::U32String *msg) const {
	const bool ready = _initialized && validCheckpoint(_checkpoint) && _yield != kNotYielded &&
		!_pendingRestore && (!_menuInputActive || _checkpoint.intro()) &&
		!_quit && _yield != kNativeEffectYield && atSaveBoundary();
	if (!ready && msg)
		*msg = _("Saving is temporarily unavailable.");
	return ready;
}

bool OnceUpon::canLoadGameState(Common::U32String *msg) const {
	const bool ready = _initialized && !_pendingRestore && !_quit;
	if (!ready && msg)
		*msg = _("Loading is temporarily unavailable.");
	return ready;
}

Common::Error OnceUpon::saveGameStream(Common::WriteStream &stream, bool isAutosave) const {
	if (!canSaveGameState())
		return Common::kWritingFailed;
	const Progress &p = _checkpoint.progress;
	byte payload[kCheckpointPrefix + kCheckpointNameLimit];
	Common::MemoryWriteStream bytes(payload, sizeof(payload));
	bytes.writeByte(_vm->getGameType() == kGameTypeAbracadabra ? 0 : 1);
	bytes.writeUint32LE(_vm->getPlatform());
	bytes.writeByte(_vm->_global->_language);
	bytes.writeByte(p.section);
	if (!_checkpoint.intro()) {
		bytes.writeByte(p.difficulty);
		// Unlocks are durable; the score and room energy deliberately roll back.
		bytes.writeByte(MAX(p.maximumSection, _progress.maximumSection));
		bytes.writeUint32LE(p.score);
		bytes.writeUint32LE(_checkpoint.randomSeed);
		bytes.writeByte(p.house);
		bytes.writeByte(p.head);
		bytes.writeByte(p.hair);
		bytes.writeByte(p.jacket);
		bytes.writeByte(p.trousers);
		bytes.writeByte(_checkpoint.room);
		bytes.writeByte(_checkpoint.energy / 5);
		bytes.writeByte(p.name.size());
		bytes.write(p.name.c_str(), p.name.size());
	}
	if (bytes.err())
		return Common::kWritingFailed;
	const uint32 size = bytes.pos();
	Common::MemoryReadStream input(payload, size);
	byte digest[16];
	if (!Common::computeStreamMD5(input, digest))
		return Common::kWritingFailed;
	stream.writeUint32BE(MKTAG('O', 'U', 'S', 'V'));
	stream.writeUint16LE(1);
	stream.writeUint32LE(size);
	stream.write(digest, sizeof(digest));
	stream.write(payload, size);
	return stream.err() ? Common::kWritingFailed : Common::kNoError;
}

Common::Error OnceUpon::loadGameStream(Common::SeekableReadStream &stream) {
	if (!canLoadGameState() || stream.readUint32BE() != MKTAG('O', 'U', 'S', 'V') || stream.readUint16LE() != 1)
		return Common::kReadingFailed;
	const uint32 size = stream.readUint32LE();
	byte expected[16], actual[16], payload[kCheckpointPrefix + kCheckpointNameLimit];
	if ((size != kIntroPayloadSize && size < kCheckpointPrefix) || size > sizeof(payload) ||
		stream.read(expected, sizeof(expected)) != sizeof(expected) ||
		stream.read(payload, size) != size || stream.err())
		return Common::kReadingFailed;
	Common::MemoryReadStream bytes(payload, size);
	if (!Common::computeStreamMD5(bytes, actual) || memcmp(expected, actual, sizeof(expected)) || !bytes.seek(0))
		return Common::kReadingFailed;
	const byte title = bytes.readByte();
	const uint32 platform = bytes.readUint32LE();
	const byte language = bytes.readByte();
	if (title != (_vm->getGameType() == kGameTypeAbracadabra ? 0 : 1) ||
		platform != (uint32)_vm->getPlatform() || language != _vm->_global->_language)
		return Common::kReadingFailed;
	Checkpoint checkpoint;
	Progress &p = checkpoint.progress;
	p.section = bytes.readByte();
	if ((p.section == 0) != (size == kIntroPayloadSize))
		return Common::kReadingFailed;
	if (p.section != 0) {
		p.difficulty = bytes.readByte();
		p.maximumSection = bytes.readByte();
		p.score = bytes.readUint32LE();
		checkpoint.randomSeed = bytes.readUint32LE();
		p.house = bytes.readByte();
		p.head = bytes.readByte();
		p.hair = bytes.readByte();
		p.jacket = bytes.readByte();
		p.trousers = bytes.readByte();
		checkpoint.room = bytes.readByte();
		const uint energy = bytes.readByte();
		if (energy > 36)
			return Common::kReadingFailed;
		checkpoint.energy = energy * 5;
		const uint length = bytes.readByte();
		if (length > kCheckpointNameLimit || length != bytes.size() - bytes.pos())
			return Common::kReadingFailed;
		char name[kCheckpointNameLimit];
		if (bytes.read(name, length) != length || memchr(name, 0, length))
			return Common::kReadingFailed;
		p.name = Common::String(name, length);
	}
	checkpoint.valid = true;
	if (!validCheckpoint(checkpoint) || bytes.pos() != bytes.size() || bytes.eos() || bytes.err())
		return Common::kReadingFailed;
	_pendingRestore = new Checkpoint(checkpoint);
	_clock.interruptWait();
	if (_scene)
		_scene->suspendMovie();
	return Common::kNoError;
}

void OnceUpon::discardRestore() {
	delete _pendingRestore;
	_pendingRestore = nullptr;
	_clock.cancelWaitInterrupt();
}

void OnceUpon::commitRestore() {
	if (!_pendingRestore)
		return;
	const Checkpoint checkpoint = *_pendingRestore;
	discardRestore();
	// Called by the story loop after the old scene's callbacks have returned.
#ifdef USE_TTS
	cancelNarration();
#endif
	stopSound();
	finishScene();
	// Restart saves reopen a canonical entry, independent of earlier visits.
	_dwellingHotspots = DwellingHotspots();
	const bool intro = checkpoint.intro();
	if (intro)
		resetProgress();
	else
		_progress = checkpoint.progress;
	_checkpoint = checkpoint;
	_clock.pause();
	_clock.restart();
	_introStage = intro ? kIntroLoading : kIntroDone;
	_yield = kNotYielded;
	_menuInputActive = false;
	_menuEscapePending = 0;
	_vm->_rnd.setSeed(intro ? Common::RandomSource::generateNewSeed() : checkpoint.randomSeed);
	_vm->_util->forceMouseUp();
	// All presentation is constructed from a canonical blank entry. In
	// particular, palette entries beyond the native sixteen have no history.
	Palette::State palette;
	palette.holdingBlack = true;
	_palette.adopt(palette);
	_vm->_draw->_frontSurface->clear();
	_vm->_draw->_backSurface->clear();
	_vm->_draw->_invalidatedCount = 0;
	_vm->_draw->_noInvalidated = false;
	hideCursor();
	if (checkpoint.forest()) {
		beginForestRoom(checkpoint.room, checkpoint.energy);
	} else if (!intro) {
		beginStorySection();
	}
	discardActivationInput();
	_clock.resume();
}

} // namespace OnceUpon
} // namespace Gob
