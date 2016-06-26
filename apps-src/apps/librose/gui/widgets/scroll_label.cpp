/* $Id: scroll_label.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
/*
   Copyright (C) 2008 - 2012 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/widgets/scroll_label.hpp"

#include "gui/widgets/label.hpp"
#include "gui/auxiliary/widget_definition/scroll_label.hpp"
#include "gui/auxiliary/window_builder/scroll_label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/scrollbar.hpp"
#include "gui/widgets/spacer.hpp"
#include "gui/widgets/window.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_WIDGET(scroll_label)

tscroll_label::tscroll_label()
	: tscroll_container(COUNT)
	, state_(ENABLED)
	, auto_scroll_(false)
	, last_scroll_ticks_(0)
	, lbl_(nullptr)
{
	connect_signal<event::LEFT_BUTTON_DOWN>(
			  boost::bind(
				    &tscroll_label::signal_handler_left_button_down
				  , this
				  , _2)
			, event::tdispatcher::back_pre_child);
}

void tscroll_label::set_label(const std::string& label)
{
	// Inherit.
	tcontrol::set_label(label);

	if (content_grid()) {
		tlabel* widget =
				find_widget<tlabel>(content_grid(), "_label", false, true);
		std::string orig = widget->label();
		if (orig == label) {
			return;
		}
		widget->set_label(label);

		// scroll_label hasn't linked_group widget, to save time, don't calcuate linked_group.
		invalidate_layout();
	}
}

void tscroll_label::finalize_subclass()
{
	VALIDATE(content_grid_ && !lbl_, null_str);
	lbl_ = dynamic_cast<tlabel*>(content_grid()->find("_label", false));

	lbl_->set_restrict_width();
	lbl_->set_label(label());
	lbl_->set_text_font_size(text_font_size_);
	lbl_->set_text_color_tpl(text_color_tpl_);
}

tpoint tscroll_label::calculate_best_size() const
{
	const int width_from_builder = best_size_from_builder().x;
	unsigned w = width_from_builder != npos? width_from_builder: 0;

	const unsigned cfg_screen_width = settings::screen_width;
	unsigned maximum_width = cfg_screen_width;
	if (w) {
		maximum_width = w;
	}

	tlabel* label = dynamic_cast<tlabel*>(content_grid_->find("_label", false));

	ttext_maximum_width_lock lock(*label, maximum_width);
	return tscroll_container::calculate_best_size();
}

tpoint tscroll_label::mini_calculate_content_grid_size(const tpoint& content_origin, const tpoint& content_size)
{
	lbl_->set_text_maximum_width(content_size.x);

	const tpoint actual_size = content_grid_->get_best_size();

	return tpoint(std::max(actual_size.x, content_size.x), std::max(actual_size.y, content_size.y));
}

void tscroll_label::set_text_editable(bool editable)
{
	lbl_->set_text_editable(editable);
}

bool tscroll_label::exist_anim()
{
	bool dirty = false;
	Uint32 now = SDL_GetTicks();

	if (auto_scroll_ && vertical_scrollbar_->get_visible() == twidget::VISIBLE) {
		const Uint32 interval = 50;
		if (last_scroll_ticks_) {
			if (now >= last_scroll_ticks_ + interval) {
				last_scroll_ticks_ = now;
				dirty = true;

				unsigned item_position = vertical_scrollbar_->get_item_position();
				item_position += 10;
				vertical_scrollbar_->set_item_position(item_position);
				scrollbar_moved();
			}
		}
	} else {
		last_scroll_ticks_ = now;
	}

	if (!dirty) {
		dirty = tcontrol::exist_anim();
	}
	return dirty;
}

const std::string& tscroll_label::get_control_type() const
{
	static const std::string type = "scroll_label";
	return type;
}

void tscroll_label::signal_handler_left_button_down(const event::tevent event)
{
	get_window()->keyboard_capture(this);
}

} // namespace gui2

