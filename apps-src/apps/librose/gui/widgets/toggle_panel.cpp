/* $Id: toggle_panel.cpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
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

#include "gui/widgets/toggle_panel.hpp"

#include "gui/auxiliary/widget_definition/toggle_panel.hpp"
#include "gui/auxiliary/window_builder/toggle_panel.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "sound.hpp"
#include "theme.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

namespace gui2 {

REGISTER_WIDGET(toggle_panel)

ttoggle_panel::ttoggle_panel()
	: tpanel(COUNT + 1)
	, state_(ENABLED)
	, retval_(0)
	, at_(npos)
	, frame_(false)
	, hlt_selected_(true)
	, hlt_focussed_(true)
	, did_state_pre_change_()
	, did_state_changed_()
	, did_double_click_()
	, gc_distance_(twidget::npos)
	, gc_width_(twidget::npos)
	, gc_height_(twidget::npos)
{
	set_wants_mouse_left_double_click();

	connect_signal<event::MOUSE_ENTER>(boost::bind(
				&ttoggle_panel::signal_handler_mouse_enter, this, _2, _3));
	connect_signal<event::MOUSE_ENTER>(boost::bind(
				&ttoggle_panel::signal_handler_mouse_enter, this, _2, _3), event::tdispatcher::back_post_child);
	connect_signal<event::MOUSE_LEAVE>(boost::bind(
				&ttoggle_panel::signal_handler_mouse_leave, this, _2, _3));
	connect_signal<event::MOUSE_LEAVE>(boost::bind(
				&ttoggle_panel::signal_handler_mouse_leave, this, _2, _3), event::tdispatcher::back_post_child);

	connect_signal<event::LEFT_BUTTON_CLICK>(boost::bind(
				&ttoggle_panel::signal_handler_left_button_click
					, this, _2, _3, _5));
	connect_signal<event::LEFT_BUTTON_CLICK>(boost::bind(
				  &ttoggle_panel::signal_handler_left_button_click
				, this, _2, _3, _5), event::tdispatcher::back_post_child);

	connect_signal<event::LEFT_BUTTON_DOUBLE_CLICK>(boost::bind(
				  &ttoggle_panel::signal_handler_left_button_double_click
				, this, _2, _3));
	connect_signal<event::LEFT_BUTTON_DOUBLE_CLICK>(boost::bind(
				  &ttoggle_panel::signal_handler_left_button_double_click
				, this, _2, _3)
			, event::tdispatcher::back_post_child);
}

void ttoggle_panel::set_child_members(const std::map<std::string, std::string>& data)
{
	// typedef boost problem work around.
	for (std::map<std::string, std::string>::const_iterator it = data.begin(); it != data.end(); ++ it) {
		tcontrol* control = dynamic_cast<tcontrol*>(find(it->first, false));
		if (control) {
			control->set_label(it->second);
		}
	}
}

void ttoggle_panel::set_child_icon(const std::string& id, const std::string& icon)
{
	tcontrol* control = dynamic_cast<tcontrol*>(find(id, false));
	if (control) {
		control->set_icon(icon);
	}
}

void ttoggle_panel::set_active(const bool active)
{
	if(active) {
		if(get_value()) {
			set_state(ENABLED_SELECTED);
		} else {
			set_state(ENABLED);
		}
	} else {
		if(get_value()) {
			set_state(DISABLED_SELECTED);
		} else {
			set_state(DISABLED);
		}
	}
}

tpoint ttoggle_panel::border_space() const
{
	boost::intrusive_ptr<const ttoggle_panel_definition::tresolution> conf =
		boost::dynamic_pointer_cast<const ttoggle_panel_definition::tresolution>(config());
	assert(conf);

	return tpoint(conf->left_border + conf->right_border,
		conf->top_border + conf->bottom_border);
}

bool ttoggle_panel::set_value(const bool selected)
{
	if (selected == get_value()) {
		return true;
	}

	if (selected) {
		if (did_state_pre_change_) {
			if (!did_state_pre_change_(*this)) {
				return false;
			}
		}
		set_state(static_cast<tstate>(state_ + ENABLED_SELECTED));
		if (did_state_changed_) {
			did_state_changed_(*this);
		}
	} else {
		set_state(static_cast<tstate>(state_ - ENABLED_SELECTED));
	}
	return true;
}

void ttoggle_panel::set_retval(const int retval)
{
	retval_ = retval;
}

void ttoggle_panel::set_state(const tstate state)
{
	if(state == state_) {
		return;
	}

	state_ = state;
	set_dirty();

	boost::intrusive_ptr<const ttoggle_panel_definition::tresolution> conf =
		boost::dynamic_pointer_cast<const ttoggle_panel_definition::tresolution>(config());
	assert(conf);
}

void ttoggle_panel::update_canvas()
{
	// Inherit.
	tcontrol::update_canvas();

	bool last_row = false;
	tgrid* grid = dynamic_cast<tgrid*>(parent_);
	if (grid && grid->children_vsize()) {
		last_row = grid->child(grid->children_vsize() - 1).widget_ == this;
	}
	
	// set icon in canvases
	uint32_t color;
	BOOST_FOREACH(tcanvas& canva, canvas()) {
		canva.set_variable("last_row", variant(last_row));
		color = hlt_focussed_? theme::instance.item_focus_color: 0;
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
		// for mobile device, disable focussed always.
		color = 0;
#endif
		canva.set_variable("focus_color", variant(color));

		color = hlt_selected_? theme::instance.item_highlight_color: 0;
		canva.set_variable("highlight_color", variant(color));
	}
}

bool ttoggle_panel::exist_anim()
{
	return tcontrol::exist_anim() || (state_ >= ENABLED_SELECTED && canvas(COUNT).exist_anim());
}

bool ttoggle_panel::can_selectable() const
{
	return get_active() && get_visible() == twidget::VISIBLE;
}

void ttoggle_panel::set_canvas_highlight(bool focussed, bool selected) 
{
	bool dirty = false;
	if (hlt_focussed_ != focussed) {
		hlt_focussed_ = focussed;
		dirty = true;
	}
	if (hlt_selected_ != selected) {
		hlt_selected_ = selected;
		dirty = true;
	}
	if (dirty) {
		update_canvas();
	}
}

void ttoggle_panel::impl_draw_foreground(texture& frame_buffer, int x_offset, int y_offset)
{
	if (state_ >= ENABLED_SELECTED) {
		std::vector<int> anims;
		canvas(COUNT).blit(*this, frame_buffer, get_rect(), get_dirty(), anims);
	}
}

const std::string& ttoggle_panel::get_control_type() const
{
	static const std::string type = "toggle_panel";
	return type;
}

void ttoggle_panel::signal_handler_mouse_enter(const event::tevent event, bool& handled)
{
	if (!get_active()) {
		handled = true;
		return;
	}
	if (get_value()) {
		set_state(FOCUSSED_SELECTED);
	} else {
		set_state(FOCUSSED);
	}

	if (did_mouse_enter_leave_) {
		did_mouse_enter_leave_(*this, true);
	}
	handled = true;
}

void ttoggle_panel::signal_handler_mouse_leave(const event::tevent event, bool& handled)
{
	if (!get_active()) {
		handled = true;
		return;
	}
	if (get_value()) {
		set_state(ENABLED_SELECTED);
	} else {
		set_state(ENABLED);
	}

	if (did_mouse_enter_leave_) {
		did_mouse_enter_leave_(*this, false);
	}
	handled = true;
}

void ttoggle_panel::signal_handler_left_button_click(const event::tevent event, bool& handled, const int type)
{
	handled = true;
	// sound::play_UI_sound(settings::sound_toggle_panel_click);
	if (!set_value(true)) {
		return;
	}

	if (did_click_) {
		did_click_(*this, type);
	}
}

void ttoggle_panel::signal_handler_left_button_double_click(const event::tevent event, bool& handled)
{
	handled = true;

	if (retval_) {
		twindow* window = get_window();
		window->set_retval(retval_);
	}

	if (did_double_click_) {
		did_double_click_(*this);
	}
}

} // namespace gui2

