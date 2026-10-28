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

#include "gob/pregob/mewilo/detection.h"

#include "common/endian.h"
#include "common/stream.h"

namespace Gob {
namespace {

bool sampleMarker(const Common::FSList &files, const char *name, bool amiga) {
	for (Common::FSList::const_iterator it = files.begin(); it != files.end(); ++it) {
		const Common::FSNode &file = *it;
		if (file.isDirectory() || !file.getName().equalsIgnoreCase(name))
			continue;
		Common::SeekableReadStream *s = file.createReadStream();
		bool ok = s && s->size() > 0 && s->size() <= 8 * 1024 * 1024;
		if (ok && amiga) {
			byte h[6];
			ok = s->size() > 6 && s->read(h, 6) == 6 &&
				READ_BE_UINT32(h) == uint64(s->size() - 6) && READ_BE_UINT16(h + 4) != 0;
		}
		delete s;
		if (ok)
			return true;
	}
	return false;
}
} // End of anonymous namespace

void preferMewiloNativeProfile(const Common::FSList &files, DetectedGames &games) {
	bool relevant = false;
	for (uint i = 0; i < games.size(); ++i) {
		const DetectedGame &g = games[i];
		if (g.gameId == "mewilo" && !g.hasUnknownFiles &&
			(g.platform == Common::kPlatformAtariST || g.platform == Common::kPlatformAmiga))
			relevant = true;
	}
	if (!relevant)
		return;
	const bool spl = sampleMarker(files, "INTRO.SPL", false);
	bool smp = sampleMarker(files, "intro.smp", true);
	if (!smp)
		for (Common::FSList::const_iterator it = files.begin(); it != files.end(); ++it) {
			const Common::FSNode &dir = *it;
			if (!dir.isDirectory() || !dir.getName().equalsIgnoreCase("sounds"))
				continue;
			Common::FSList children;
			if (dir.getChildren(children, Common::FSNode::kListFilesOnly) &&
				sampleMarker(children, "intro.smp", true)) {
				smp = true;
				break;
			}
		}
	if (spl == smp)
		return; // Ambiguous or incomplete: preserve explicit alternatives.
	const Common::Platform preferred = smp ? Common::kPlatformAmiga : Common::kPlatformAtariST;
	for (uint i = 0; i < games.size();) {
		const DetectedGame &g = games[i];
		bool alternative = false;
		if (g.gameId == "mewilo" && !g.hasUnknownFiles && g.platform != preferred &&
			(g.platform == Common::kPlatformAtariST || g.platform == Common::kPlatformAmiga)) {
			for (uint j = 0; j < games.size(); ++j) {
				const DetectedGame &candidate = games[j];
				if (candidate.gameId == g.gameId && candidate.language == g.language &&
					candidate.platform == preferred && !candidate.hasUnknownFiles)
					alternative = true;
			}
		}
		if (alternative)
			games.remove_at(i);
		else
			++i;
	}
}
} // End of namespace Gob
