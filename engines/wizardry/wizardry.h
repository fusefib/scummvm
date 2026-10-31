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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef WIZARDRY_WIZARDRY_H
#define WIZARDRY_WIZARDRY_H
#include "engines/engine.h"
#include "wizardry/detection.h"
#include "wizardry/vm/machine.h"

namespace Wizardry {

class WizardryEngine : public Engine {
public:
	WizardryEngine(OSystem *system, const WizardryGameDescription *description);
	Common::Error run() override;
	bool hasFeature(EngineFeature feature) const override;

private:
	const WizardryGameDescription *_description;
	PCodeMachine _machine;
	byte _pixels[320 * 200];
	uint32 _delayEnd = 0;
	void present();
	void processEvents();
};

} // namespace Wizardry
#endif
