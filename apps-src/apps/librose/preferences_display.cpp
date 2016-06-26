/* $Id: preferences_display.cpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "rose-lib"

/**
 *  @file
 *  Manage display-related preferences, e.g. screen-size, etc.
 */

#include "global.hpp"
#include "preferences_display.hpp"

#include "base_instance.hpp"
#include "display.hpp"
#include "preferences.hpp"
#include "gettext.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/widgets/window.hpp"
#include "marked-up_text.hpp"
#include "wml_separators.hpp"

#include <boost/foreach.hpp>

namespace preferences {

bool detect_video_settings(CVideo& video, std::pair<int,int>& resolution, int& bpp, int& video_flags)
{
	video_flags = fullscreen() ? SDL_WINDOW_FULLSCREEN: 0;
	resolution = preferences::resolution();

	bpp = 32;
	return true;
}

void set_fullscreen(display& disp, const bool ison)
{
	_set_fullscreen(ison);

	CVideo& video = disp.video();
	const std::pair<int,int>& res = resolution();
	if (video.isFullScreen() != ison) {
		const int flags = ison ? SDL_WINDOW_FULLSCREEN: 0;
		int bpp = video.modePossible(res.first, res.second, 32, flags);
		VALIDATE(bpp > 0, "bpp must be large than 0!");

		// tpoint orientation_size = gui2::twidget::orientation_swap_size(res.first, res.second);

		video.setMode(res.first, res.second, bpp, flags);
		display::require_change_resolution = true;
	}
}

void set_scroll_to_action(bool ison)
{
	_set_scroll_to_action(ison);
}

// width, height are consistent with current orientation.
bool set_resolution(display& disp, const unsigned width, const unsigned height, bool scene)
{
	// Make sure resolutions are always divisible by 4
	//res.first &= ~3;
	//res.second &= ~3;

	SDL_Rect rect = screen_area();
	if (rect.w == width && rect.h == height) {
		return true;
	}

	tpoint landscape_size = gui2::twidget::orientation_swap_size(width, height);

	if (landscape_size.x < preferences::min_allowed_width() || landscape_size.y < preferences::min_allowed_height()) {
		return false;
	}

	CVideo& video = disp.video();
	const int flags = fullscreen() ? SDL_WINDOW_FULLSCREEN : 0;
	int bpp = video.modePossible(width, height, 32, flags);
	VALIDATE(bpp > 0, "bpp must be large than 0!");

	video.setMode(width, height, bpp, flags);
	if (scene) {
		display::require_change_resolution = true;
	}

	const std::string postfix = fullscreen() ? "resolution" : "windowsize";
	preferences::set('x' + postfix, lexical_cast<std::string>(landscape_size.x / gui2::twidget::hdpi_scale));
	preferences::set('y' + postfix, lexical_cast<std::string>(landscape_size.y / gui2::twidget::hdpi_scale));
	
	return true;
}

void set_turbo(display& disp, bool ison)
{
	_set_turbo(ison);
	disp.set_turbo(ison);
}

void set_turbo_speed(display& disp, double speed)
{
	save_turbo_speed(speed);
	disp.set_turbo_speed(speed);
}

void set_grid(display& disp, bool ison)
{
	_set_grid(ison);
	disp.set_grid(ison);
}

void set_default_move(bool ison) 
{
	_set_default_move(ison);
}

bool compare_resolutions(const std::pair<int,int>& lhs, const std::pair<int,int>& rhs)
{
	return lhs.first*lhs.second < rhs.first*rhs.second;
}

bool show_video_mode_dialog(display& disp)
{
	CVideo& video = disp.video();

	bool fullScreen = fullscreen();

	std::vector<std::pair<int,int> > resolutions;
	int display = SDL_GetWindowDisplayIndex(disp.video().getWindow());
	int modes = SDL_GetNumDisplayModes(display);
	SDL_DisplayMode mode = {SDL_PIXELFORMAT_UNKNOWN, 0, 0, 0, 0 };
	SDL_Rect screen_rect = disp.video().bound();
	for (int n = 0; n < modes; n ++) {
		SDL_GetDisplayMode(display, n, &mode);
		if (SDL_BYTESPERPIXEL(mode.format) != SDL_BYTESPERPIXEL(video.getformat().format)) {
			continue;
		}
		if (mode.w % gui2::twidget::hdpi_scale || mode.h % gui2::twidget::hdpi_scale) {
			continue;
		}
		if (!fullScreen && (mode.w >= screen_rect.w || mode.h >= screen_rect.h)) {
			continue;
		}
		if (mode.w >= min_allowed_width() && mode.h >= min_allowed_height()) {
			resolutions.push_back(std::pair<int, int>(mode.w, mode.h));
		}
	}

	if (resolutions.empty()) {
		gui2::show_transient_message(disp.video(), "", _("There are no alternative video modes available"));
		return false;
	}

	tpoint landscape_size = gui2::twidget::orientation_swap_size(video.getx(), video.gety());
	const std::pair<int,int> current_res(landscape_size.x, landscape_size.y);
	resolutions.push_back(current_res);
	if (!fullScreen) {
		resolutions.push_back(std::make_pair(480 * gui2::twidget::hdpi_scale, 320 * gui2::twidget::hdpi_scale));
		resolutions.push_back(std::make_pair(568 * gui2::twidget::hdpi_scale, 320 * gui2::twidget::hdpi_scale));
		resolutions.push_back(std::make_pair(667 * gui2::twidget::hdpi_scale, 375 * gui2::twidget::hdpi_scale));
	}

	std::sort(resolutions.begin(),resolutions.end(),compare_resolutions);
	resolutions.erase(std::unique(resolutions.begin(),resolutions.end()),resolutions.end());

	std::vector<gui2::tval_str> options;
	unsigned current_choice = 0;

	for(size_t k = 0; k < resolutions.size(); ++k) {
		std::pair<int, int> const& res = resolutions[k];
		std::ostringstream option;

		if (res == current_res) {
			current_choice = static_cast<unsigned>(k);		
		}

		option << res.first << "x" << res.second;
		// widescreen threshold is 16:10
		if ((double)res.first/res.second >= 16.0/10.0) {
			option << _(" (widescreen)");
		}
		options.push_back(gui2::tval_str(k, option.str()));
	}

	gui2::tcombo_box dlg(options, current_choice, _("Choose Resolution"), true);
	dlg.show(disp.video());
	if (dlg.get_retval() != gui2::twindow::OK) {
		return false;
	}
	int choice = dlg.selected_index();
	if (resolutions[choice] == current_res) {
		return false;
	}

	std::pair<int, int>& res = resolutions[static_cast<size_t>(choice)];

	tpoint normal_size = gui2::twidget::orientation_swap_size(res.first, res.second);
	set_resolution(disp, normal_size.x, normal_size.y);
	return true;
}

// true: resolution modified result to return. false: other reason return.
int show_preferences_dialog(display& disp, bool first)
{
	while (true) {
		int res = instance->show_preferences_dialog(disp, first);
		if (first) {
			first = false;
		}
		if (res == preferences::CHANGE_RESOLUTION) {
			if (preferences::show_video_mode_dialog(disp)) {
				return res;
			}

		} else if (res == preferences::MAKE_FULLSCREEN) {
			preferences::set_fullscreen(disp, true);
			return res;

		} else if (res == preferences::MAKE_WINDOWED) {
			preferences::set_fullscreen(disp, false);
			return res;

		} else {
			return res;
		}
	}
}

bool is_resolution_retval(int res) 
{ 
	return res >= MIN_RESOLUTION && res <= MAX_RESOLUTION; 
}

} // end namespace preferences

