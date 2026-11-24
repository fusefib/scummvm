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

#include "common/stream.h"
#include "common/textconsole.h"
#include "common/tokenizer.h"

#include "gob/dataio.h"
#include "gob/gob.h"
#include "gob/pregob/onceupon/forest.h"

namespace Gob {
namespace OnceUpon {

bool Forest::readEnemyRow(Common::SeekableReadStream &stream, int16 values[6]) {
	while (!stream.eos() && !stream.err()) {
		const Common::String line = stream.readLine();
		Common::StringTokenizer tokens(line, " \t\r\n\x1a");
		if (tokens.empty())
			continue;
		Common::String token = tokens.nextToken();
		if (token[0] != '-' && token[0] != '+' && (token[0] < '0' || token[0] > '9'))
			continue;
		for (uint field = 0; field < 6; ++field) {
			if (field) {
				if (tokens.empty())
					return false;
				token = tokens.nextToken();
			}
			const bool negative = token[0] == '-';
			uint digit = negative || token[0] == '+' ? 1 : 0;
			if (digit == token.size())
				return false;
			int32 value = 0;
			for (; digit < token.size(); ++digit) {
				if (token[digit] < '0' || token[digit] > '9')
					return false;
				value = value * 10 + token[digit] - '0';
				if (value > (negative ? 32768 : 32767))
					return false;
			}
			values[field] = negative ? -value : value;
		}
		return tokens.empty() && !stream.err();
	}
	return false;
}

void Forest::loadEnemies() {
	const Common::String name = Common::String::format("foret%c.le%u", routeDescription().enemyPrefix, _difficulty + 1);
	Common::ScopedPtr<Common::SeekableReadStream> stream(_vm->_dataIO->getFile(name));
	if (!stream)
		error("Once Upon a Time: missing forest enemy table %s", name.c_str());
	// Six rooms use four records each. Some shipped files contain unused rows.
	for (uint scene = 0; scene < 6; ++scene)
		for (uint i = 0; i < 4; ++i) {
			int16 values[6];
			if (!readEnemyRow(*stream, values))
				error("Once Upon a Time: invalid row %u in forest enemy table %s", scene * 4 + i + 1, name.c_str());
			Enemy &enemy = _enemies[scene][i];
			enemy.lethal = values[0];
			enemy.animation = values[1];
			enemy.triggerX = values[2];
			enemy.y = values[3];
			enemy.x = values[4];
			enemy.destructible = values[5];
		}
}

} // End of namespace OnceUpon
} // End of namespace Gob
