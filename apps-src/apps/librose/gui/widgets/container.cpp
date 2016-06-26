/* $Id: container.cpp 54007 2012-04-28 19:16:10Z mordante $ */
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

#include "gui/widgets/container.hpp"

#include "formula_string_utils.hpp"
#include "gui/widgets/window.hpp"

namespace gui2 {

SDL_Rect tcontainer_::get_grid_rect() const 
{ 
	const tpoint border_size = border_space();

	SDL_Rect result = get_rect();

	result.x += border_size.x / 2;
	result.y += border_size.y / 2;
	result.w -= border_size.x;
	result.h -= border_size.y;

	return result; 
}

void tcontainer_::layout_init(bool linked_group_only)
{
	// Inherited.
	tcontrol::layout_init(linked_group_only);

	grid_->layout_init(linked_group_only);
}

tpoint tcontainer_::fill_placeable_width(const int width)
{
	const tpoint border_size = border_space();
	tpoint result(grid_->fill_placeable_width(width - border_size.x));

	// If the best size has a value of 0 it's means no limit so don't
	// add the border_size might set a very small best size.
	if (result.x) {
		result.x += border_size.x;
	}

	if (result.y) {
		result.y += border_size.y;
	}

	return result;
}

void tcontainer_::place(const tpoint& origin, const tpoint& size)
{
	tcontrol::place(origin, size);

	const SDL_Rect rect = get_grid_rect();
	const tpoint client_size(rect.w, rect.h);
	const tpoint client_position(rect.x, rect.y);
	grid_->place(client_position, client_size);
}

tpoint tcontainer_::calculate_best_size() const
{
	tpoint result(grid_->get_best_size());
	const tpoint border_size = border_space();

	// If the best size has a value of 0 it's means no limit so don't
	// add the border_size might set a very small best size.
	if (result.x) {
		result.x += border_size.x;
	}

	if (result.y) {
		result.y += border_size.y;
	}

	return result;
}

void tcontainer_::set_origin(const tpoint& origin)
{
	// Inherited.
	twidget::set_origin(origin);

	const SDL_Rect rect = get_grid_rect();
	const tpoint client_position(rect.x, rect.y);
	grid_->set_origin(client_position);
}

void tcontainer_::set_visible_area(const SDL_Rect& area)
{
	// Inherited.
	twidget::set_visible_area(area);

	grid_->set_visible_area(area);
}

void tcontainer_::impl_draw_children(texture& frame_buffer, int x_offset, int y_offset)
{
	VALIDATE(get_visible() == twidget::VISIBLE && grid_->get_visible() == twidget::VISIBLE, null_str);

	grid_->draw_children(frame_buffer, x_offset, y_offset);
}

void tcontainer_::broadcast_frame_buffer(texture& frame_buffer)
{
	grid_->broadcast_frame_buffer(frame_buffer);
}

void tcontainer_::clear_texture()
{
	tcontrol::clear_texture();
	grid_->clear_texture();
}

void tcontainer_::layout_children()
{
	grid_->layout_children();
}

void tcontainer_::child_populate_dirty_list(twindow& caller,
		const std::vector<twidget*>& call_stack)
{
	std::vector<twidget*> child_call_stack = call_stack;
	grid_->populate_dirty_list(caller, child_call_stack);
}

void tcontainer_::popup_new_window()
{
	grid_->popup_new_window();
}

void tcontainer_::set_active(const bool active)
{
	// Not all our children might have the proper state so let them run
	// unconditionally.
	grid_->set_active(active);

	if(active == get_active()) {
		return;
	}

	set_dirty();

	set_self_active(active);
}

bool tcontainer_::disable_click_dismiss() const
{
	return tcontrol::disable_click_dismiss() && grid_->disable_click_dismiss();
}

void tcontainer_::init_grid(const boost::intrusive_ptr<tbuilder_grid>& grid_builder)
{
	VALIDATE(initial_grid().get_rows() == 0 && initial_grid().get_cols() == 0, null_str);

	grid_builder->build(&initial_grid());

	initial_subclass();
}

} // namespace gui2

