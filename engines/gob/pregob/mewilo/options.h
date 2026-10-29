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

#ifndef GOB_PREGOB_MEWILO_OPTIONS_H
#define GOB_PREGOB_MEWILO_OPTIONS_H

#include "gob/pregob/mewilo/platform.h"
#include "gob/pregob/mewilo/translation.h"

namespace GUI {
class GuiObject;
class OptionsContainerWidget;
} // End of namespace GUI

namespace Gob {
namespace Mewilo {

MewiloOutputMode mewiloOutputMode(const Common::String &domain = Common::String());
uint32 mewiloAnimationFrameMillis(const Common::String &domain = Common::String());
MewiloPlatform mewiloPresentationPlatform(Common::Platform detected, bool thomsonTO);
GUI::OptionsContainerWidget *createMewiloOptions(
	GUI::GuiObject *boss, const Common::String &name, const Common::String &target);

} // End of namespace Mewilo
} // End of namespace Gob

#endif // GOB_PREGOB_MEWILO_OPTIONS_H
