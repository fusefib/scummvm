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

#ifndef GOB_PREGOB_MEWILO_TRANSLATION_H
#define GOB_PREGOB_MEWILO_TRANSLATION_H
#include "common/array.h"
#include "common/stream.h"
#include "common/str.h"
#include "gob/pregob/mewilo/tot.h"
namespace Gob {
class Surface;
namespace Mewilo {
class MewiloFont;

enum MewiloOutputMode { kMewiloOriginal, kMewiloCorrectedOriginal, kMewiloEnglish };
enum ReplacementFamily { kReplacementFullFrench, kReplacementFullGerman, kReplacementCpcFrench, kReplacementThomsonFrench };
enum ReplacementKind { kReplacementText = 1, kReplacementAnswers, kReplacementReject, kReplacementImage, kReplacementGlyph };
struct ReplacementKey {
	byte family, module, kind, field;
	uint16 node;
};
struct ReplacementEntry {
	ReplacementKey key;
	byte opcode;
	uint16 width, height;
	Common::Array<byte> bytes; // NUL-terminated game-encoded text OR source-resolution pen indices.
};
/** Owns an external single-family MWEN v2 pack. Load once before a new game or prepared restore;
 * do not hot-reload while clients retain entry pointers. Failure leaves the previous
 * pack unchanged. Source language and platform remain separate from output wording.
 * Header flag 1 marks a sparse corrected-original pack; flag 0 requires
 * complete English text/input coverage. Every supplied entry is checked.
 */
class MewiloReplacements {
public:
	MewiloReplacements() : _revision(0), _sparseOriginal(false) {}
	bool load(Common::SeekableReadStream &stream, Common::String &reason);
	uint16 revision() const { return _revision; }
	bool sparseOriginal() const { return _sparseOriginal; }
	uint size() const { return _entries.size(); }
	byte family() const { return _entries.empty() ? 255 : _entries[0].key.family; }
	bool validateModule(const MewiloTotData &tot, byte module, Common::String &reason) const;
	bool applyGlyphs(MewiloFont &font) const;
	// The existing save identity includes the resident output bytes, not a later
	// reopening of the sidecar. No separate hash/signature matcher is introduced.
	void writeIdentity(Common::WriteStream &stream) const;
	const ReplacementEntry *find(const ReplacementKey &key) const;
	/** Known family/module/offset plus opcode and geometry, not a checksum or scan.
	 * field is an ordinal in the current input node, NOT a relocated descriptor offset.
	 */
	const ReplacementEntry *resolve(const MewiloTotData &tot, const ReplacementKey &key) const;
	/** handled=true means the English dictionary was present: false then means
	 * WRONG ANSWER, not permission to fall back to the source-language dictionary.
	 */
	bool answerMatches(const MewiloTotData &tot, const ReplacementKey &key,
		const Common::Array<byte> &input, bool &handled) const;
private:
	Common::Array<ReplacementEntry> _entries;
	Common::Array<byte> _residentBytes;
	uint16 _revision;
	bool _sparseOriginal;
};
/** Draw a complete replacement at its original geometry. The host must first perform
 * the ordinary native decode/scratch side effects, then use this BEFORE presentation
 * tracking/dirty marking. Those side effects must not disappear on Atari.
 */
bool drawReplacementImage(const ReplacementEntry &entry, Surface &dest, int16 x, int16 y);
} // namespace Mewilo
} // namespace Gob
#endif
