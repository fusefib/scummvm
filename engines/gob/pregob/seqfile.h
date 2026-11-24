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
 *
 * This file is dual-licensed.
 * In addition to the GPLv3 license mentioned above, this code is also
 * licensed under LGPL 2.1. See LICENSES/COPYING.LGPL file for the
 * full text of the license.
 *
 */

#ifndef GOB_PREGOB_SEQFILE_H
#define GOB_PREGOB_SEQFILE_H

#include "common/system.h"
#include "common/array.h"
#include "common/ptr.h"

#include "gob/util.h"

namespace Common {
	class String;
	class SeekableReadStream;
}

namespace Gob {

class GobEngine;

class DECFile;
class ANIFile;
class ANIObject;

/** A SEQ file, describing a complex animation sequence.
 *
 *  Used in early hardcoded gob games.
 *  The principle is similar to the Mult class (see mult.h), but instead
 *  of depending on all the externally loaded animations, backgrounds and
 *  objects, a SEQ file references animation and background directly by
 *  filename.
 */
class SEQFile {
public:
	/** Optional immutable resource owner; existing clients keep private assets. */
	class ResourceProvider {
	public:
		virtual ~ResourceProvider() {}
		virtual Common::SharedPtr<const ANIFile> acquireAnimation(const Common::String &name) = 0;
		virtual Common::SharedPtr<const DECFile> acquireBackground(const Common::String &name) = 0;
	};

	SEQFile(GobEngine *vm, const Common::String &fileName, ResourceProvider *resources = nullptr);
	virtual ~SEQFile();

	/** Start frame-by-frame playback. Zero uses the rate in the SEQ header. */
	void beginPlay(uint16 frameRate = 0);
	bool stepPlay();
	void endPlay();
	bool isAborted() const { return _abortPlay; }
	void resumePlay() { _abortPlay = false; }

	/** Yield after the current publication without repeating its effects. */


protected:
	GobEngine *_vm;


	/** Returns the current frame number. */
	uint16 getFrame() const;

	/** Seek to a specific frame. */
	void seekFrame(uint16 frame);

	/** Ends SEQ playback. */
	void abortPlay();

	virtual void changeFrameRate(int16 rate);
	virtual void waitFrame();

	/** Callback for special frame events. */
	virtual void handleFrameEvent();
	/** Callback for special user input handling. */
	virtual void handleInput(int16 key, int16 mouseX, int16 mouseY, MouseButtons mouseButtons);


private:
	/** Number of animation objects that are visible at the same time. */
	static const uint kObjectCount = 4;

	/** A key for changing the background. */
	struct BackgroundKey {
		uint16 frame; ///< Frame the change is to happen.

		const DECFile *background; ///< The background to use.
	};

	/** A key for playing an object animation. */
	struct AnimationKey {

		uint16 frame; ///< Frame the change is to happen.

		const ANIFile *ani; ///< The ANI to use.

		uint16 animation; ///< The animation to use.

		int16 x; ///< X position of the animation.
		int16 y; ///< Y position of the animation.

		int16 order; ///< Used to determine in which order to draw the objects.
	};

	/** A managed animation object. */
	struct Object {
		ANIObject *object; ///< The actual animation object.

		int16 order; ///< The current drawing order.
	};

	typedef Common::Array<Common::SharedPtr<const DECFile> > Backgrounds;
	typedef Common::Array<Common::SharedPtr<const ANIFile> > Animations;

	typedef Common::Array<BackgroundKey> BackgroundKeys;
	typedef Common::Array<AnimationKey>  AnimationKeys;


	uint16 _frame;     ///< The current frame.
	bool   _abortPlay; ///< Was the end of the playback requested?

	int16 _authoredFrameRate;
	int16 _savedFrameRate;
	bool _playing;
	// Consumed keys stay consumed across frame-word wraparound.
	uint _nextBackgroundKey;
	uint _nextAnimationKey[kObjectCount];

	Backgrounds _backgrounds; ///< All backgrounds in this SEQ.
	Animations  _animations;  ///< All animations in this SEQ.

	BackgroundKeys _bgKeys;   ///< The background change keyframes.
	AnimationKeys _animKeys[kObjectCount]; ///< The keys in each original object track.

	Object _objects[kObjectCount]; ///< The managed animation objects.


	// -- Loading helpers --

	void load(Common::SeekableReadStream &seq, ResourceProvider *resources);

	const ANIFile *findANI(uint16 index, uint16 &animation);

	// -- Playback helpers --

	void playFrame();

	/** Get a list of objects ordered by drawing order. */
	uint getOrderedObjects(Object *objects[kObjectCount]);

	void clearAnims(); ///< Remove all animation frames.
	void drawAnims();  ///< Draw the animation frames.

};

} // End of namespace Gob

#endif // GOB_PREGOB_SEQFILE_H
