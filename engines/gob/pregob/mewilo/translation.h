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
#include "common/str.h"

namespace Common {
class SeekableReadStream;
class WriteStream;
} // End of namespace Common

namespace Gob {

class Surface;

namespace Mewilo {

class MewiloFont;
class MewiloTotData;

enum MewiloOutputMode {
	kMewiloOriginal,
	kMewiloCorrectedOriginal,
	kMewiloEnglish
};

enum ReplacementFamily {
	kReplacementFullFrench,
	kReplacementFullGerman,
	kReplacementCpcFrench,
	kReplacementThomsonFrench
};

enum ReplacementKind {
	kReplacementText = 1,
	kReplacementAnswers,
	kReplacementReject,
	kReplacementImage,
	kReplacementGlyph
};

struct ReplacementKey {
	byte family, module, kind, field;
	uint16 node;
};
struct ReplacementEntry {
	ReplacementKey key;
	byte opcode;
	uint16 width, height;
	Common::Array<byte> bytes; ///< Terminated game-encoded text or source-resolution pen indices.
};

/** Owns one MWEN v2 pack. Failed loads leave the previous entries and pointers intact.
 * Header flag 1 selects sparse corrected-original text; flag 0 requires complete
 * English text and input coverage. Successful reloads invalidate entry pointers.
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
	/** Serialize the loaded content into the save identity without reopening the sidecar. */
	void writeIdentity(Common::WriteStream &stream) const;
	const ReplacementEntry *find(const ReplacementKey &key) const;
	/** Resolve a known family/module/node and verify its opcode and geometry.
	 * field is an ordinal in the input node, not a relocated descriptor offset.
	 */
	const ReplacementEntry *resolve(const MewiloTotData &tot, const ReplacementKey &key) const;
	/** handled distinguishes a rejected replacement answer from a missing dictionary.
	 */
	bool answerMatches(const MewiloTotData &tot, const ReplacementKey &key,
		const Common::Array<byte> &input, bool &handled) const;

private:
	Common::Array<ReplacementEntry> _entries;
	Common::Array<byte> _residentBytes;
	uint16 _revision;
	bool _sparseOriginal;
};

/** Draw a replacement after native decode/scratch effects and before dirty marking.
 * This ordering preserves the Atari scratch bytes needed by subsequent images.
 */
bool drawReplacementImage(const ReplacementEntry &entry, Surface &dest, int16 x, int16 y);

} // End of namespace Mewilo
} // End of namespace Gob

#endif // GOB_PREGOB_MEWILO_TRANSLATION_H
