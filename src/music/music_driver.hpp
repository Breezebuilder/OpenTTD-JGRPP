/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file music_driver.hpp Base for all music playback. */

#ifndef MUSIC_MUSIC_DRIVER_HPP
#define MUSIC_MUSIC_DRIVER_HPP

#include "../driver.h"

#include <memory>
#include <mutex>
#if defined(__MINGW32__)
#include "../3rdparty/mingw-std-threads/mingw.mutex.h"
#endif

extern std::mutex _music_driver_mutex;

struct MusicSongInfo;

/** Driver for all music playback. */
class MusicDriver : public Driver {
public:
	/**
	 * Play a particular song.
	 * @param song The information for the song to play.
	 */
	virtual void PlaySong(const MusicSongInfo &song) = 0;

	/**
	 * Stop playing the current song.
	 */
	virtual void StopSong() = 0;

	/**
	 * Are we currently playing a song?
	 * @return True if a song is being played.
	 */
	virtual bool IsSongPlaying() = 0;

	/**
	 * Set the volume, if possible.
	 * @param vol The new volume.
	 */
	virtual void SetVolume(byte vol) = 0;

	/**
	 * Is playback in a failed state?
	 * @return True if playback is in a failed state.
	 */
	virtual bool IsInFailedState() { return false; }

	static std::unique_ptr<MusicDriver> ExtractDriver()
	{
		Driver **dptr = DriverFactoryBase::GetActiveDriver(Driver::DT_MUSIC);
		Driver *driver = *dptr;
		*dptr = nullptr;
		return std::unique_ptr<MusicDriver>(static_cast<MusicDriver*>(driver));
	}

	/**
	 * Get the currently active instance of the music driver.
	 */
	static MusicDriver *GetInstance() {
		std::unique_lock<std::mutex> lock(_music_driver_mutex);

		return static_cast<MusicDriver*>(*DriverFactoryBase::GetActiveDriver(Driver::DT_MUSIC));
	}
};

extern std::string _ini_musicdriver;

#endif /* MUSIC_MUSIC_DRIVER_HPP */
