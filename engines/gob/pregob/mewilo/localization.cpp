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

#include "common/textconsole.h"

#include "gob/dataio.h"
#include "gob/gob.h"

namespace Gob {
namespace Mewilo {

bool PreGob_Mewilo::loadReplacements() {
	if (_outputMode == kMewiloOriginal || _replacements.size())
		return true;
	int family = -1;
	if (_profile.platform == kMewiloPlatformAmstradCPC && _vm->_language == Common::FR_FRA)
		family = kReplacementCpcFrench;
	else if (_profile.platform == kMewiloPlatformThomsonTO && _vm->_language == Common::FR_FRA)
		family = kReplacementThomsonFrench;
	else if (_profile.platform == kMewiloPlatformAtariST ||
		_profile.platform == kMewiloPlatformAmiga) {
		if (_vm->_language == Common::FR_FRA)
			family = kReplacementFullFrench;
		else if (_vm->_language == Common::DE_DEU)
			family = kReplacementFullGerman;
	}
	const bool corrected = _outputMode == kMewiloCorrectedOriginal;
	const char *filename = corrected ? "mewilo-original.dat" : "mewilo-en.dat";
	// Explicit sidecar, intentionally outside the sealed Thomson FD container.
	// Native game files still never fall back to adjacent loose files in FD mode.
	Common::SeekableReadStream *stream = family < 0 ? nullptr : _vm->_dataIO->getFile(filename);
	Common::String reason;
	MewiloReplacements staged;
	const bool ok = stream && staged.load(*stream, reason) && staged.family() == family &&
		staged.sparseOriginal() == corrected;
	delete stream;
	if (ok)
		_replacements = staged;
	if (!ok)
		warning(
			"Mewilo: %s is absent, invalid, or for another source family/output mode (%s). "
			"Rebuild the matching sidecar with the external patcher, or select Original wording.",
			filename, reason.c_str());
	return ok;
}

const ReplacementEntry *PreGob_Mewilo::replacementEntry(
	const MewiloTotData &tot, uint16 node, byte kind, byte field) const {
	if (!_replacements.size())
		return nullptr;
	const int module = mewiloModuleIndex(_moduleStem, _profile.modulePrefix);
	if (module < 0)
		return nullptr;
	const ReplacementKey key = {_replacements.family(), byte(module), kind, field, node};
	return _replacements.resolve(tot, key);
}

bool PreGob_Mewilo::translatedAnswer(const MewiloTotData &tot, uint16 node, byte field,
	const Common::Array<byte> &input, bool &handled) const {
	handled = false;
	if (!_replacements.size())
		return false;
	const int module = mewiloModuleIndex(_moduleStem, _profile.modulePrefix);
	if (module < 0)
		return false;
	const ReplacementKey key = {
		_replacements.family(), byte(module), kReplacementAnswers, field, node};
	return _replacements.answerMatches(tot, key, input, handled);
}

void PreGob_Mewilo::showRejectMessageAt(
	const MewiloTotData &tot, uint16 node, const byte *text, uint length) {
	const ReplacementEntry *e = replacementEntry(tot, node, kReplacementReject);
	if (e)
		showRejectMessage(e->bytes.data(), e->bytes.size() - 1);
	else
		showRejectMessage(text, length);
}
} // End of namespace Mewilo
} // End of namespace Gob
