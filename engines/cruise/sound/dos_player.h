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

#ifndef CRUISE_SOUND_PLAYER_H
#define CRUISE_SOUND_PLAYER_H

#include "common/mutex.h"
#include "common/scummsys.h"
#include "common/serializer.h"

namespace Cruise {

class PCSoundDriver;

class PCSoundFxPlayer {
private:
	typedef byte *(*ResourceLoader)(const char *name, uint32 *size);
	typedef void (*ResourceFreer)(void *data);

	enum {
		NUM_INSTRUMENTS = 15,
		NUM_CHANNELS = 4,

		VOLUME_TABLE_OFFSET = 0,
		INSTRUMENT_NAME_OFFSET = 20,
		INSTRUMENT_RECORD_SIZE = 30,
		INSTRUMENT_NAME_SIZE = 12,
		NUM_ORDERS_OFFSET = 470,
		TEMPO_OFFSET = 471,
		ORDER_TABLE_OFFSET = 472,
		AUX_DATA_OFFSET = 600,
		AUX_DATA_SIZE = 1800,
		PATTERN_DATA_OFFSET = AUX_DATA_OFFSET + AUX_DATA_SIZE,
		PATTERN_SIZE = 1024,
		ROW_SIZE = 16,
		CHANNEL_EVENT_SIZE = 4,
		EVENT_INSTRUMENT_OFFSET = 2,
		REPLACEMENT_DELAY_MS = 50,
		REPLACEMENT_STALL_MS = 2000
	};

	bool loadInternal(const char *song);
	void stopInternal();
	void fadeOutInternal();
	void unloadInternal();
	void update();
	void handleEvents();
	void handlePattern(int channel, const byte *patternData);

	char _musicName[33];
	bool _playing;
	bool _musicStarted;
	bool _resumeAfterLoad;
	bool _retirementPending;
	int _currentPos;
	int _currentOrder;
	int _numOrders;
	int _eventsDelay;
	int16 _looping;
	int _fadeOutCounter;
	int _updateTicksCounter;
	int _instrumentsChannelTable[NUM_CHANNELS];
	byte *_sfxData;
	uint32 _sfxDataSize;
	byte *_instrumentsData[NUM_INSTRUMENTS];
	PCSoundDriver *_driver;
	ResourceLoader _resourceLoader;
	ResourceFreer _resourceFreer;
	mutable Common::Mutex _mutex;

protected:
	virtual bool replacementShouldQuit() const;
	virtual uint32 replacementMillis() const;
	virtual bool replacementDelay(uint32 delayMs);

public:
	PCSoundFxPlayer(PCSoundDriver *driver, ResourceLoader resourceLoader,
			ResourceFreer resourceFreer);
	virtual ~PCSoundFxPlayer();

	bool load(const char *song);
	void play();
	void stop();
	void unload();
	void retire();
	void fadeOut();
	void requestFade();
	void doSync(Common::Serializer &s);
	void finalizeLoad();
	void syncSounds();

	static void updateCallback(void *ref);

	bool songLoaded() const;
	bool songPlayed() const;
	uint8 numOrders() const;
	void setNumOrders(uint8 v);
	void setPattern(int offset, uint8 value);
	const char *musicName() const;

	// The original game never reads the looping variable; songs loop unconditionally.
	int16 looping() const;
	void setLooping(int16 v);
};

} // End of namespace Cruise

#endif
