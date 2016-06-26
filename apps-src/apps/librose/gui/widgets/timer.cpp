/* $Id: timer.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
/*
   Copyright (C) 2009 - 2012 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "gui/widgets/timer.hpp"

#include "events.hpp"

#include <SDL_timer.h>
#include <map>

namespace gui2 {

struct ttimer2
{
	ttimer2()
		: sdl_id(0)
		, interval(0)
		, callback()
	{
	}

	SDL_TimerID sdl_id;
	Uint32 interval;
	boost::function<void(size_t id)> callback;
};

	/** Ids for the timers. */
	static size_t id = 0;

	/** The active timers. */
	static std::map<size_t, ttimer2> timers;

	/** The id of the event being executed, 0 if none. */
	static size_t executing_id = 0;

	/** Did somebody try to remove the timer during its execution? */
	static bool executing_id_removed = false;

/**
 * Helper to make removing a timer in a callback safe.
 *
 * Upon creation it sets the executing id and clears the remove request flag.
 *
 * If an remove_timer() is called for the id being executed it requests a
 * remove the timer and exits remove_timer().
 *
 * Upon destruction it tests whether there was a request to remove the id and
 * does so. It also clears the executing id. It leaves the remove request flag
 * since the execution function needs to know whether or not the event was
 * removed.
 */
class texecutor
{
public:
	texecutor(size_t id)
	{
		executing_id = id;
		executing_id_removed = false;

	}

	~texecutor()
	{
		const size_t id = executing_id;
		executing_id = 0;
		if(executing_id_removed) {
			remove_timer(id);
		}
	}
};

extern "C" {

static Uint32 timer_callback(Uint32, void* id)
{
	std::map<size_t, ttimer2>::iterator itor = timers.find(reinterpret_cast<size_t>(id));
	if (itor == timers.end()) {
		return 0;
	}

	SDL_Event event;
	SDL_UserEvent data;

	data.type = TIMER_EVENT;
	data.code = 0;
	data.data1 = id;
	data.data2 = NULL;

	event.type = TIMER_EVENT;
	event.user = data;

	SDL_PushEvent(&event);

	return itor->second.interval;
}

} // extern "C"

size_t add_timer(const Uint32 interval, const boost::function<void(size_t id)>& callback)
{
	BOOST_STATIC_ASSERT(sizeof(size_t) == sizeof(void*));

	do {
		++ id;
	} while(id == 0 || timers.find(id) != timers.end());

	ttimer2 timer;
	timer.sdl_id = SDL_AddTimer(interval, timer_callback, reinterpret_cast<void*>(id));
	if (timer.sdl_id == 0) {
		// Failed to create an sdl timer.
		return INVALID_TIMER_ID;
	}

	const bool repeat = true;
	if (repeat) {
		timer.interval = interval;
	}

	timer.callback = callback;

	timers.insert(std::make_pair(id, timer));

	return id;
}

bool remove_timer(const size_t id)
{
	std::map<size_t, ttimer2>::iterator itor = timers.find(id);
	if (itor == timers.end()) {
		// Can't remove timer since it no longer exists.
		return false;
	}

	if (id == executing_id) {
		executing_id_removed = true;
		return true;
	}

	if (!SDL_RemoveTimer(itor->second.sdl_id)) {
		/*
		 * This can happen if the caller of the timer didn't get the event yet
		 * but the timer has already been fired. This due to the fact that a
		 * timer pushes an event in the queue, which allows the following
		 * condition:
		 * - Timer fires
		 * - Push event in queue
		 * - Another event is processed and tries to remove the event.
		 */
		// The timer is already out of the SDL timer list.
	}
	timers.erase(itor);
	return true;
}

bool execute_timer(const size_t id)
{
	std::map<size_t, ttimer2>::iterator itor = timers.find(id);
	if (itor == timers.end()) {
		// Can't execute timer since it no longer exists.
		return false;
	}

	{
		texecutor executor(id);
		itor->second.callback(id);
	}

	if(!executing_id_removed && itor->second.interval == 0) {
		timers.erase(itor);
	}
	return true;
}

} //namespace gui2

