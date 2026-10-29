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

#include "common/system.h"
#include "common/util.h"

#include "gob/global.h"
#include "gob/gob.h"
#include "gob/util.h"

namespace Gob {
namespace Mewilo {

void PreGob_Mewilo::pauseChanged(bool paused) {
	if (paused) {
		if (_pauseDepth++ == 0)
			_pauseStart = g_system->getMillis();
	} else if (_pauseDepth && --_pauseDepth == 0) {
		_pausedMillis += g_system->getMillis() - _pauseStart;
		// A click used to close a modal must not become a game choice.
		_mousePresses.clear();
		_pendingKey = 0;
	}
}

uint32 PreGob_Mewilo::activeMillis() const {
	const uint32 now = _pauseDepth ? _pauseStart : g_system->getMillis();
	return now - _pausedMillis;
}

void PreGob_Mewilo::serviceEvents() {
	// No waitEndFrame here: a synchronous NOTE or short FF03 wait must not
	// acquire an additional 15-fps delay. Leave buffered keys for waitInput().
	_vm->_util->processInput();
	presentFrame();
	if (!shouldQuit() && !hasError())
		g_system->delayMillis(1);
}

void PreGob_Mewilo::waitMilliseconds(uint32 durationMs) {
	MewiloTimingPlan plan;
	plan.milliseconds = durationMs;
	waitPresentation(plan, 0);
}

bool PreGob_Mewilo::waitContinueInput() {
	MewiloInputEvent event;
	return waitInput(event);
}

bool PreGob_Mewilo::waitInputRelease() {
	int16 x, y;
	MouseButtons buttons;
	while (!shouldQuit() && !hasError() && !_pendingRestore) {
		_vm->_util->getMouseState(&x, &y, &buttons);
		if (buttons == kMouseButtonsNone)
			return true;
		serviceEvents();
	}
	return false;
}

void PreGob_Mewilo::waitOrDelay(byte argument) {
	if (!argument) {
		if (waitContinueInput())
			waitInputRelease();
	} else {
		MewiloTimingPlan plan;
		plan.milliseconds = mewiloNativeDelayMillis(_presentationPlatform, argument, plan.calibrated);
		waitPresentation(plan, 0);
	}
}

void PreGob_Mewilo::waitPresentation(const MewiloTimingPlan &plan, uint16 site) {
	const uint32 start = activeMillis();
	if (plan.anchor == kMewiloVisibleHold || plan.minimumExposure)
		_presentation.dirty(plan.region);
	if (!plan.exposureRegion.isEmpty())
		_presentation.dirty(plan.exposureRegion);
	// Submit the finished composition before starting a visible-exposure clock.
	// No change to native audio duration, input ownership or script state.
	presentFrame();
	uint32 elapsed = 0;
	while (!shouldQuit() && !hasError() && !_pendingRestore) {
		const uint32 now = activeMillis();
		elapsed = plan.anchor == kMewiloVisibleHold ? _presentation.visibleAge(plan.region, now)
			: now - start;
		const uint32 remaining = _presentation.waitRemaining(
			plan, MAX<int16>(1, _vm->_global->_speedFactor), start, now);
		if (!remaining)
			break;
		serviceEvents();
		if (shouldQuit() || hasError() || _pendingRestore)
			break;
		// Re-evaluate after event/presentation work (including a modal pause).
		const uint32 after = activeMillis();
		g_system->delayMillis(MIN<uint32>(10,
			_presentation.waitRemaining(
				plan, MAX<int16>(1, _vm->_global->_speedFactor), start, after)));
	}
	// Ctrl+F/G can change the speed while events are serviced. Report the
	// effective budget used to finish the wait, not the earlier speed's target.
	const uint speed = MAX<int16>(1, _vm->_global->_speedFactor);
	debugC(4, kDebugGameFlow,
		"MEWILO_PRESENT wait module=%s site=%04X rule=%s anchor=%u calibrated=%u "
		"source=%u presentation=%u speed=%u animation=%u reading=%u budget=%u elapsed=%u minimum=%u "
		"visible=%u aborted=%u t=%u frame=%u rect=%d,%d,%d,%d",
		_moduleStem.c_str(), site, plan.rule, (uint)plan.anchor, plan.calibrated,
		(uint)_profile.platform, (uint)_presentationPlatform, speed, plan.animation, plan.reading,
		mewiloScaleTime(plan.milliseconds, speed), elapsed,
		mewiloScaleTime(plan.minimumExposure, speed),
		_presentation.visibleAge(
			plan.exposureRegion.isEmpty() ? plan.region : plan.exposureRegion, activeMillis()),
		shouldQuit() || hasError() || _pendingRestore, activeMillis(), _presentation.frame(),
		plan.region.left, plan.region.top, plan.region.right, plan.region.bottom);
}

void PreGob_Mewilo::waitOrDelayAt(const MewiloTotData &tot, uint16 site, byte argument) {
	if (!argument) {
		waitOrDelay(0);
		return;
	}
	MewiloTimingPlan plan = mewiloTimingPlan(tot, _moduleStem.c_str(), _vm->_language,
		_presentationPlatform, site, argument, _normalizeTiming, _animationFrameMillis);
	debugC(4, kDebugGameFlow,
		"MEWILO_PRESENT cue module=%s site=%04X operand=%u frame_ms=%u readability=%u rule=%s",
		_moduleStem.c_str(), site, argument, _animationFrameMillis, _normalizeTiming, plan.rule);
	const ReplacementEntry *text =
		plan.textNode ? replacementEntry(tot, plan.textNode, kReplacementText) : nullptr;
	if (text) {
		plan.milliseconds =
			MAX<uint32>(plan.minimumDelay,
				mewiloReadingMillis(text->bytes.data(), text->bytes.size()));
	}
	waitPresentation(plan, site);
}

} // End of namespace Mewilo
} // End of namespace Gob
