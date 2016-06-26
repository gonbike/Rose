/* $Id: widget.cpp 54218 2012-05-19 08:46:15Z mordante $ */
/*
   Copyright (C) 2007 - 2012 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/auxiliary/event/message.hpp"
#include "gui/dialogs/dialog.hpp"

namespace gui2 {

#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
int cfg_2_os_size(const int cfg_size) { return cfg_size * gui2::twidget::hdpi_scale; }
#else
int cfg_2_os_size(const int cfg_size) { return cfg_size * gui2::twidget::hdpi_scale * 3 / 4; }
// int cfg_2_os_size(const int cfg_size) { return cfg_size * gui2::twidget::hdpi_scale; }
#endif

const int twidget::npos = -1;

const std::string twidget::tpl_widget_id_prefix = "_tpl_";

bool twidget::is_tpl_widget_id(const std::string& id)
{
	return id.find(tpl_widget_id_prefix) == 0;
}

int twidget::hdpi_scale = 1;
const int twidget::max_effectable_point = 540; // 1920x1080
bool twidget::current_landscape = true;

const twidget* twidget::fire_event = nullptr;
twidget* twidget::link_group_owner = nullptr;
bool twidget::clear_restrict_width_cell_size = false;

bool twidget::landscape_from_orientation(torientation orientation, bool def)
{
	if (orientation != auto_orientation) {
		return orientation == landscape_orientation;
	}
	return def;
}

bool twidget::orientation_effect_resolution(const int width, const int height)
{
	const int max_effectable_pixel = hdpi_scale * max_effectable_point;
	return width <= max_effectable_pixel || height <= max_effectable_pixel;
}

tpoint twidget::orientation_swap_size(int width, int height)
{
	if (!orientation_effect_resolution(width, height)) {
		return tpoint(width, height);
	}
	if (!current_landscape) {
		int tmp = width;
		width = height;
		height = tmp;
	}
	return tpoint(width, height);
}

twidget::twidget()
	: id_("")
	, parent_(NULL)
	, x_(0)
	, y_(0)
	, w_(0)
	, h_(0)
	, dirty_(true)
	, redraw_(false)
	, visible_(VISIBLE)
	, drawing_action_(DRAWN)
	, clip_rect_(empty_rect)
	, fix_rect_(null_rect)
	, cookie_(NULL)
	, layout_size_(tpoint(0,0))
	, linked_group_()
	, drag_(drag_none)
	, float_widget_(false)
	, restrict_width_(false)
	, terminal_(true)
{
}

twidget::~twidget()
{
	twidget* p = parent();
	while (p) {
		fire2(event::NOTIFY_REMOVAL, *p);
		p = p->parent();
	}

	if (this == fire_event) {
		fire_event = nullptr;
	}

	twindow* window = get_window();

	if (window) {
		if (!linked_group_.empty()) {
			window->remove_linked_widget(linked_group_, this);
		}
		tdialog* dialog = window->dialog();
		if (dialog) {
			dialog->destruct_widget(this);
		}
	}

}

void twidget::set_id(const std::string& id)
{
	id_ = id;
}

void twidget::layout_init(bool linked_group_only)
{
	if (!linked_group_only) {
		layout_size_ = tpoint(0,0);
		if (!linked_group_.empty()) {
			link_group_owner->add_linked_widget(linked_group_, *this, linked_group_only);
		}
	} else if (!linked_group_.empty()) {
		twindow* window = get_window();
		VALIDATE(link_group_owner != window, null_str);
		link_group_owner->add_linked_widget(linked_group_, *this, linked_group_only);
	}
}

tpoint twidget::get_best_size() const
{
	if (is_null_rect(fix_rect_)) {
		tpoint result = layout_size_;
		if (result == tpoint(0, 0)) {
			result = calculate_best_size();
		}

		return result;

	} else {
		return tpoint(fix_rect_.w, fix_rect_.h);
	}
}

void twidget::place(const tpoint& origin, const tpoint& size)
{
	// x, y maybe < 0. for example: content_grid_.
	VALIDATE(size.x >= 0 && size.y >= 0, null_str);

	if (is_null_rect(fix_rect_)) {
		x_ = origin.x;
		y_ = origin.y;
		w_ = size.x;
		h_ = size.y;

	} else {
		// x_ = fix_rect_.x;
		// y_ = fix_rect_.y;
		x_ = origin.x;
		y_ = origin.y;

		w_ = fix_rect_.w;
		h_ = fix_rect_.h;
	}

	set_dirty();
}

void twidget::set_width(const int width)
{
	// release of visutal studio cannot detect assert, use breakpoint.
	VALIDATE(width >= 0, null_str);

	if (width == w_) {
		return;
	}

	w_ = width;
	set_dirty();
}

void twidget::set_size(const tpoint& size)
{
	// release of visutal studio cannot detect assert, use breakpoint.
	VALIDATE(size.x >= 0 && size.y >= 0, null_str);

	w_ = size.x;
	h_ = size.y;

	set_dirty();
}

twidget* twidget::find_at(const tpoint& coordinate, const bool must_be_active)
{
	return is_at(coordinate, must_be_active) ? this : NULL;
}

SDL_Rect twidget::get_dirty_rect() const
{
	return drawing_action_ == DRAWN
			? get_rect()
			: clip_rect_;
}

void twidget::move(const int x_offset, const int y_offset)
{
	x_ += x_offset;
	y_ += y_offset;
}

twindow* twidget::get_window()
{
	// Go up into the parent tree until we find the top level
	// parent, we can also be the toplevel so start with
	// ourselves instead of our parent.
	twidget* result = this;
	while (result->parent_) {
		result = result->parent_;
	}

	// on error dynamic_cast return 0 which is what we want.
	return dynamic_cast<twindow*>(result);
}

const twindow* twidget::get_window() const
{
	// Go up into the parent tree until we find the top level
	// parent, we can also be the toplevel so start with
	// ourselves instead of our parent.
	const twidget* result = this;
	while (result->parent_) {
		result = result->parent_;
	}

	// on error dynamic_cast return 0 which is what we want.
	return dynamic_cast<const twindow*>(result);
}

tdialog* twidget::dialog()
{
	twindow* window = get_window();
	return window ? window->dialog() : NULL;
}

void twidget::populate_dirty_list(twindow& caller, std::vector<twidget*>& call_stack)
{
	VALIDATE(call_stack.empty() || call_stack.back() != this, null_str);

	if (visible_ == INVISIBLE || (!dirty_ && !redraw_ && visible_ == HIDDEN)) {
		// when change from VISIBLE to HIDDEN, require populate it to dirty list.
		return;
	}

	if (get_drawing_action() == NOT_DRAWN) {
		return;
	}

	call_stack.push_back(this);

	if (dirty_ || exist_anim() || redraw_) {
		caller.add_to_dirty_list(call_stack);
	} else {
		// virtual function which only does something for container items.
		child_populate_dirty_list(caller, call_stack);
	}
}

void twidget::set_layout_size(const tpoint& size) 
{
	layout_size_ = size; 
}

void twidget::set_visible(const tvisible visible)
{
	if (visible == visible_) {
		return;
	}

	// Switching to or from invisible should invalidate the layout.
	const bool need_resize = visible_ == INVISIBLE || visible == INVISIBLE;
	visible_ = visible;

	if (need_resize) {
		twindow *window = get_window();
		if(window) {
			window->invalidate_layout();
		}
	}

	// set_dirty();
	redraw_ = true;
}

twidget::tdrawing_action twidget::get_drawing_action() const
{
	return (w_ == 0 || h_ == 0)? NOT_DRAWN: drawing_action_;
}

void twidget::set_origin(const tpoint& origin)
{
	if (origin.x == x_ && origin.y == y_) {
		return;
	}

	x_ = origin.x;
	y_ = origin.y;

	redraw_ = true;
}

void twidget::set_visible_area(const SDL_Rect& area)
{
	SDL_Rect original_clip_rect_ = clip_rect_;
	tdrawing_action original_action = drawing_action_;

	clip_rect_ = intersect_rects(area, get_rect());

	if (clip_rect_ == get_rect()) {
		drawing_action_ = DRAWN;
	} else if (clip_rect_ == empty_rect) {
		drawing_action_ = NOT_DRAWN;
	} else {
		drawing_action_ = PARTLY_DRAWN;
	}

	if (!redraw_ && drawing_action_ != NOT_DRAWN) {
		if (drawing_action_ != original_action) {
			redraw_ = true;
		} else if (drawing_action_ == PARTLY_DRAWN && original_clip_rect_ != clip_rect_) {
			redraw_ = true;
		}
	}
}

void twidget::set_dirty()
{
	dirty_ = true;
}

void twidget::clear_dirty()
{
	dirty_ = false;
	redraw_ = false;
}

void twidget::dirty_under_rect(const SDL_Rect& clip)
{
	VALIDATE(terminal_, null_str);

	SDL_Rect rect = get_dirty_rect();
	if (SDL_HasIntersection(&clip, &rect)) {
		redraw_ = true;
	}
}

SDL_Rect twidget::calculate_blitting_rectangle(
		  const int x_offset
		, const int y_offset)
{
	SDL_Rect result = get_rect();
	result.x += x_offset;
	result.y += y_offset;
	return result;
}

SDL_Rect twidget::calculate_clipping_rectangle(const texture& tex, const int x_offset, const int y_offset)
{
	SDL_Rect clip_rect = clip_rect_;
	clip_rect.x += x_offset;
	clip_rect.y += y_offset;

	int tex_w, tex_h;
	SDL_QueryTexture(tex.get(), NULL, NULL, &tex_w, &tex_h);

	SDL_Rect surf_clip_rect;
	SDL_RenderGetClipRect(get_renderer(), &surf_clip_rect);
	if (is_empty_rect(surf_clip_rect)) {
		surf_clip_rect.w = tex_w;
		surf_clip_rect.h = tex_h;
	}
	if (surf_clip_rect.w != tex_w || surf_clip_rect.h != tex_h) {
		return intersect_rects(clip_rect, surf_clip_rect);
	}
	return clip_rect;
}

void twidget::draw_background(texture& frame_buffer, int x_offset, int y_offset)
{
	if (drawing_action_ == PARTLY_DRAWN) {
		const SDL_Rect clipping_rectangle = calculate_clipping_rectangle(frame_buffer, x_offset, y_offset);
		if (!clipping_rectangle.w || !clipping_rectangle.h) {
			return;
		}
		if (is_empty_rect(clipping_rectangle)) {
			int ii = 0;
		}

		texture_clip_rect_setter clip(&clipping_rectangle);
		impl_draw_background(frame_buffer, x_offset, y_offset);

	} else {
		impl_draw_background(frame_buffer, x_offset, y_offset);
	}
}

void twidget::draw_children(texture& frame_buffer, int x_offset, int y_offset)
{
	if (drawing_action_ == PARTLY_DRAWN) {
		const SDL_Rect clipping_rectangle = calculate_clipping_rectangle(frame_buffer, x_offset, y_offset);
		if (!clipping_rectangle.w || !clipping_rectangle.h) {
			return;
		}

		texture_clip_rect_setter clip(&clipping_rectangle);
		impl_draw_children(frame_buffer, x_offset, y_offset);
	} else {
		impl_draw_children(frame_buffer, x_offset, y_offset);
	}
}

void twidget::draw_foreground(texture& frame_buffer, int x_offset, int y_offset)
{
	VALIDATE(visible_ == VISIBLE, null_str);

	if (drawing_action_ == PARTLY_DRAWN) {
		const SDL_Rect clipping_rectangle = calculate_clipping_rectangle(frame_buffer, x_offset, y_offset);
		if (!clipping_rectangle.w || !clipping_rectangle.h) {
			return;
		}

		texture_clip_rect_setter clip(&clipping_rectangle);
		impl_draw_foreground(frame_buffer, x_offset, y_offset);
	} else {
		impl_draw_foreground(frame_buffer, x_offset, y_offset);
	}
}

bool twidget::is_at(const tpoint& coordinate, const bool must_be_active) const
{
	if (visible_ != VISIBLE) {
		return false;
	}

	if (drawing_action_ == NOT_DRAWN) {
		return false;
	} else if (drawing_action_ == PARTLY_DRAWN) {
		return point_in_rect(coordinate.x, coordinate.y, clip_rect_);
	}

	return coordinate.x >= x_
			&& coordinate.x < (x_ + static_cast<int>(w_))
			&& coordinate.y >= y_
			&& coordinate.y < (y_ + static_cast<int>(h_)) ? true : false;
}
} // namespace gui2
