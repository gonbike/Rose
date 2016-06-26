/* $Id$ */
/*
   Copyright (C) 2011 Sergey Popov <loonycyborg@gmail.com>
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

#include "gui/dialogs/network_transmission.hpp"

#include "gettext.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/progress_bar.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"

#include <boost/bind.hpp>

namespace gui2 {

const int tprogress_::finish_precent = 100;

REGISTER_DIALOG(rose, network_transmission)

tnetwork_transmission::tnetwork_transmission(const std::string& title, const std::string& subtitle, int hidden_ms)
	: window_(nullptr)
	, hidden_ticks_(hidden_ms * 1000)
	, start_ticks_(SDL_GetTicks())
	, title_(title)
	, subtitle_(subtitle)
{
}

void tnetwork_transmission::pre_show(CVideo& /*video*/, twindow& window)
{
	window.set_canvas_variable("border", variant("default-border"));

	cancel_ = find_widget<tbutton>(&window, "_cancel", false, false);
	cancel_->set_canvas_variable("image", variant("misc/delete2.png"));

	connect_signal_mouse_left_click(
		*cancel_
		, boost::bind(
		&tnetwork_transmission::cancel
		, this
		, boost::ref(window)));

	window_ = &window;
	// tlobby::thandler::join();

	window.set_visible(twidget::INVISIBLE);
}

void tnetwork_transmission::post_show(twindow& /*window*/)
{
}

void tnetwork_transmission::set_percent(const int percent)
{
	VALIDATE(percent >= 0 && percent <= 100, null_str);

	if (!window_) {
		return;
	}
	if (percent == 100) {
		window_->set_retval(twindow::OK);

	} else {
		if (SDL_GetTicks() - start_ticks_ >= hidden_ticks_) {
			window_->set_visible(twidget::VISIBLE);
		}
		size_t completed = 5, total = 10;

		if (total) {
			find_widget<tprogress_bar>(window_, "_progress", false)
				.set_percentage((completed*100.)/total);

			std::stringstream ss;
			ss << utils::si_string(completed, true, _("unit_byte^B"))
			   << "/"
			   << utils::si_string(total, true, _("unit_byte^B"));

			find_widget<tlabel>(window_, "_numeric_progress", false)
					.set_label(ss.str());
			window_->invalidate_layout();
		}
	}
}

void tnetwork_transmission::cancel_task()
{
	VALIDATE(window_, null_str);
	cancel(*window_);
}

bool tnetwork_transmission::is_ok() const
{
	return get_retval() == twindow::OK;
}

void tnetwork_transmission::cancel(twindow& window)
{
	window.set_retval(twindow::CANCEL);
}

} // namespace gui2

