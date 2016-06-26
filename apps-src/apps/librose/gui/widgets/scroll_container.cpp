/* $Id: scrollbar_container.cpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
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

#include "gui/widgets/scroll_container.hpp"

#include "gui/widgets/spacer.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/horizontal_scrollbar.hpp"
#include "gui/widgets/vertical_scrollbar.hpp"
#include "rose_config.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#include "posix2.h"

namespace gui2 {

tscroll_container& tscroll_container::container_from_content_grid(const twidget& widget)
{
	tscroll_container* container = dynamic_cast<tscroll_container*>(widget.parent());
	VALIDATE(container, null_str);
	return *container;
}

tscroll_container::tscroll_container(const unsigned canvas_count, bool listbox)
	: tcontainer_(canvas_count)
	, listbox_(listbox)
	, state_(ENABLED)
	, vertical_scrollbar_mode_(auto_visible)
	, horizontal_scrollbar_mode_(auto_visible)
	, vertical_scrollbar_(nullptr)
	, horizontal_scrollbar_(nullptr)
	, dummy_vertical_scrollbar_(nullptr)
	, dummy_horizontal_scrollbar_(nullptr)
	, vertical_scrollbar2_(nullptr)
	, horizontal_scrollbar2_(nullptr)
	, content_grid_(NULL)
	, content_(NULL)
	, content_visible_area_()
	, scroll_to_end_(false)
	, calculate_reduce_(false)
	, need_layout_(false)
	, find_content_grid_(false)
	, scroll_elapse_(std::make_pair(0, 0))
	, next_scrollbar_invisible_ticks_(0)
	, first_coordinate_(construct_null_coordinate())
	, require_capture_(true)
	, clear_click_threshold_(2 * twidget::hdpi_scale)
{
	set_container_grid(grid_);
	grid_.set_parent(this);

	connect_signal<event::SDL_KEY_DOWN>(boost::bind(
			&tscroll_container::signal_handler_sdl_key_down
				, this, _2, _3, _5, _6));
}

tscroll_container::~tscroll_container() 
{ 
	if (vertical_scrollbar2_->widget->parent() == content_grid_) {
		reset_scrollbar(*get_window());
	}

	scroll_timer_.reset();

	delete dummy_vertical_scrollbar_;
	delete dummy_horizontal_scrollbar_;
	delete content_grid_; 
}

void tscroll_container::layout_init(bool linked_group_only)
{
	content_grid_->layout_init(linked_group_only);
}

tpoint tscroll_container::mini_get_best_text_size() const
{
	return content_grid_->calculate_best_size();
}

tpoint tscroll_container::calculate_best_size() const
{
	// container_ must be (0, 0)
	return tcontrol::calculate_best_size();
}

tpoint tscroll_container::fill_placeable_width(const int width)
{
	// no for simple, call it directly.
	return calculate_best_size();
}

void tscroll_container::set_scrollbar_mode(tscrollbar_& scrollbar,
		const tscroll_container::tscrollbar_mode& scrollbar_mode,
		const unsigned items, const unsigned visible_items)
{
	scrollbar.set_item_count(items);
	scrollbar.set_visible_items(visible_items);
	// old item_position maybe overflow the new boundary.
	if (scrollbar.get_item_position()) {
		scrollbar.set_item_position(scrollbar.get_item_position());
	}

	if (&scrollbar != dummy_horizontal_scrollbar_ && &scrollbar != dummy_vertical_scrollbar_) {
		if (scrollbar.float_widget()) {
			bool visible = false;
			if (scrollbar_mode == auto_visible) {
				const bool scrollbar_needed = items > visible_items;
				visible = scrollbar_needed;
			}
			get_window()->find_float_widget(scrollbar.id())->set_visible(visible);
		} else {
			const bool scrollbar_needed = items > visible_items;
			scrollbar.set_visible(scrollbar_needed? twidget::VISIBLE: twidget::HIDDEN);
		}
	}
}

void tscroll_container::place(const tpoint& origin, const tpoint& size)
{
	need_layout_ = false;

	// Inherited.
	tcontainer_::place(origin, size);

	if (!size.x || !size.y) {
		return;
	}

	const tpoint content_origin = content_->get_origin();
	const tpoint content_size = content_->get_size();
	
	const unsigned origin_y_offset = vertical_scrollbar_->get_item_position();

	tpoint content_grid_size = mini_calculate_content_grid_size(content_origin, content_size);

	if (content_grid_size.x < (int)content_->get_width()) {
		content_grid_size.x = content_->get_width();
	}
	/*
	if (content_grid_size.y < (int)content_->get_height()) {
		content_grid_size.y = content_->get_height();
	}
	*/
	// of couse, can use content_grid_origin(inclued x_offset/yoffset) as orgin, but there still use content_orgin. there is tow reason.
	// 1)mini_set_content_grid_origin maybe complex.
	// 2)I think place with xoffset/yoffset is not foten.
	content_grid_->place(content_origin, content_grid_size);
	// report's content_grid_size is vallid after tgrid2::place.
	content_grid_size = content_grid_->get_size();

	// Set vertical scrollbar. correct vertical item_position if necessary.
	set_scrollbar_mode(*vertical_scrollbar_,
			vertical_scrollbar_mode_,
			content_grid_size.y,
			content_->get_height());
	if (vertical_scrollbar_ != dummy_vertical_scrollbar_) {
		set_scrollbar_mode(*dummy_vertical_scrollbar_,
			vertical_scrollbar_mode_,
			content_grid_size.y,
			content_->get_height());
	}

	// Set horizontal scrollbar. correct horizontal item_position if necessary.
	set_scrollbar_mode(*horizontal_scrollbar_,
			horizontal_scrollbar_mode_,
			content_grid_size.x,
			content_->get_width());
	if (horizontal_scrollbar_ != dummy_horizontal_scrollbar_) {
		set_scrollbar_mode(*dummy_horizontal_scrollbar_,
			horizontal_scrollbar_mode_,
			content_grid_size.x,
			content_->get_width());
	}

	// now both vertical and horizontal item_postion are right.
	const unsigned x_offset = horizontal_scrollbar_->get_item_position();
	unsigned y_offset = vertical_scrollbar_->get_item_position();

	if (content_grid_size.x >= content_size.x) {
		VALIDATE(x_offset + horizontal_scrollbar_->get_visible_items() <= horizontal_scrollbar_->get_item_count(), null_str);
	}
	if (content_grid_size.y >= content_size.y) {
		VALIDATE(y_offset + vertical_scrollbar_->get_visible_items() <= vertical_scrollbar_->get_item_count(), null_str);
	}

	// posix_print("tscroll_container::place, content_grid.h: %i, call mini_handle_gc(, %i), origin_y_offset: %i\n", content_grid_size.y, y_offset, origin_y_offset);
	y_offset = mini_handle_gc(x_offset, y_offset);
	// posix_print("tscroll_container::place, post mini_handle_gc, y_offset: %i\n", y_offset);

	// const tpoint content_grid_origin = tpoint(content_->get_x() - x_offset, content_->get_y() - y_offset);

	// of couse, can use content_grid_origin(inclued x_offset/yoffset) as orgin, but there still use content_orgin. there is tow reason.
	// 1)mini_set_content_grid_origin maybe complex.
	// 2)I think place with xoffset/yoffset is not foten.
	// content_grid_size = content_grid_->get_size();
	// content_grid_->place(content_grid_origin, content_grid_size);

	// if (x_offset || y_offset) {
		// if previous exist item_postion, recover it.
		const tpoint content_grid_origin = tpoint(content_->get_x() - x_offset, content_->get_y() - y_offset);
		mini_set_content_grid_origin(content_->get_origin(), content_grid_origin);
	// }

	content_visible_area_ = content_->get_rect();
	// Now set the visible part of the content.
	mini_set_content_grid_visible_area(content_visible_area_);
}

tpoint tscroll_container::validate_content_grid_origin(const tpoint& content_origin, const tpoint& content_size, const tpoint& origin, const tpoint& size) const
{
	// verify desire_origin
	//  content_grid origin---> | <--- desire_origin
	//                          | <--- vertical scrollbar's item_position
	//  content origin -------> |
	//  content size            |
	tpoint origin2 = origin;
	VALIDATE(origin2.y <= content_origin.y, "y of content_grid must <= content.y!");
	VALIDATE(size.y >= content_size.y, "content_grid must >= content!");

	int item_position = (int)vertical_scrollbar_->get_item_position();
	if (origin2.y + item_position != content_origin.y) {
		origin2.y = content_origin.y - item_position;
	}
	if (item_position + content_size.y > size.y) {
		item_position = size.y - content_size.y;
		vertical_scrollbar_->set_item_position2(item_position);

		origin2.y = content_origin.y - item_position;
	}

	VALIDATE(origin2.y <= content_origin.y, "(2)y of content_grid must <= content.y!");
	return origin2;
}

void tscroll_container::set_origin(const tpoint& origin)
{
	// Inherited.
	tcontainer_::set_origin(origin);

	const tpoint content_origin = content_->get_origin();
	mini_set_content_grid_origin(origin, content_origin);

	// Changing the origin also invalidates the visible area.
	mini_set_content_grid_visible_area(content_visible_area_);
}

void tscroll_container::mini_set_content_grid_origin(const tpoint& origin, const tpoint& content_grid_origin)
{
	content_grid_->set_origin(content_grid_origin);
}

void tscroll_container::mini_set_content_grid_visible_area(const SDL_Rect& area)
{
	content_grid_->set_visible_area(area);
}

void tscroll_container::set_visible_area(const SDL_Rect& area)
{
	// Inherited.
	tcontainer_::set_visible_area(area);

	// Now get the visible part of the content.
	content_visible_area_ = intersect_rects(area, content_->get_rect());

	content_grid_->set_visible_area(content_visible_area_);
}

void tscroll_container::dirty_under_rect(const SDL_Rect& clip)
{
	if (visible_ != VISIBLE) {
		return;
	}
	content_grid_->dirty_under_rect(clip);
}

twidget* tscroll_container::find_at(const tpoint& coordinate, const bool must_be_active)
{
	if (visible_ != VISIBLE) {
		return nullptr;
	}

	VALIDATE(content_ && content_grid_, null_str);

	twidget* result = tcontainer_::find_at(coordinate, must_be_active);
	if (result == content_) {
		result = content_grid_->find_at(coordinate, must_be_active);
		if (!result) {
			// to support SDL_WHEEL_DOWN/SDL_WHEEL_UP, must can find at "empty" area.
			result = content_grid_;
		}
	}
	return result;
}

twidget* tscroll_container::find(const std::string& id, const bool must_be_active)
{
	// Inherited.
	twidget* result = tcontainer_::find(id, must_be_active);

	// Can be called before finalize so test instead of assert for the grid.
	// if (!result && find_content_grid_ && content_grid_) {
	if (!result) {
		result = content_grid_->find(id, must_be_active);
	}

	return result;
}

const twidget* tscroll_container::find(const std::string& id, const bool must_be_active) const
{
	// Inherited.
	const twidget* result = tcontainer_::find(id, must_be_active);

	// Can be called before finalize so test instead of assert for the grid.
	if (!result && find_content_grid_ && content_grid_) {
		result = content_grid_->find(id, must_be_active);
	}
	return result;
}

SDL_Rect tscroll_container::get_float_widget_ref_rect() const
{
	SDL_Rect ret{x_, y_, (int)w_, (int)h_};
	if (content_grid_->get_height() < content_->get_height()) {
		ret.h = content_grid_->get_height();
	}
	return ret;
}

bool tscroll_container::disable_click_dismiss() const
{
	// return tcontainer_::disable_click_dismiss() || content_grid_->disable_click_dismiss();
	return content_grid_->disable_click_dismiss();
}

void tscroll_container::finalize_setup()
{
	const std::string horizontal_id = "_tpl_horizontal_scrollbar";
	const std::string vertical_id = "_tpl_vertical_scrollbar";

	/***** Setup vertical scrollbar *****/
	vertical_scrollbar2_ = twindow::init_instance->find_float_widget(vertical_id);
	VALIDATE(vertical_scrollbar2_, null_str);

	// dummy_vertical_scrollbar_ = vertical_scrollbar_ = find_widget<tscrollbar_>(this, "_vertical_scrollbar", false, true);
	dummy_vertical_scrollbar_ = vertical_scrollbar_ = new tvertical_scrollbar();
	vertical_scrollbar_->set_definition("default");
	vertical_scrollbar_->set_visible(twidget::INVISIBLE);

	/***** Setup horizontal scrollbar *****/
	horizontal_scrollbar2_ = twindow::init_instance->find_float_widget(horizontal_id);
	VALIDATE(horizontal_scrollbar2_, null_str);

	// dummy_horizontal_scrollbar_ = horizontal_scrollbar_ = find_widget<tscrollbar_>(this, "_horizontal_scrollbar", false, true);
	dummy_horizontal_scrollbar_ = horizontal_scrollbar_ = new thorizontal_scrollbar();
	horizontal_scrollbar_->set_definition("default");
	horizontal_scrollbar_->set_visible(twidget::INVISIBLE);

	/***** Setup the content *****/
	content_ = new tspacer();
	content_->set_definition("default");

	content_grid_ = dynamic_cast<tgrid*>(
			grid().swap_child("_content_grid", content_, true));
	assert(content_grid_);

	content_grid_->set_parent(this);
	/***** Let our subclasses initialize themselves. *****/
	finalize_subclass();

	{
		content_grid_->connect_signal<event::WHEEL_UP>(
			boost::bind(
				  &tscroll_container::signal_handler_sdl_wheel_up
				, this
				, _3
				, _6)
			, event::tdispatcher::back_post_child);

		content_grid_->connect_signal<event::WHEEL_UP>(
			boost::bind(
				  &tscroll_container::signal_handler_sdl_wheel_up
				, this
				, _3
				, _6)
			, event::tdispatcher::back_child);

		content_grid_->connect_signal<event::WHEEL_DOWN>(
				boost::bind(
					&tscroll_container::signal_handler_sdl_wheel_down
					, this
					, _3
					, _6)
				, event::tdispatcher::back_post_child);

		content_grid_->connect_signal<event::WHEEL_DOWN>(
				boost::bind(
					&tscroll_container::signal_handler_sdl_wheel_down
					, this
					, _3
					, _6)
				, event::tdispatcher::back_child);

		content_grid_->connect_signal<event::WHEEL_LEFT>(
				boost::bind(
					&tscroll_container::signal_handler_sdl_wheel_left
					, this
					, _3
					, _6)
				, event::tdispatcher::back_post_child);

		content_grid_->connect_signal<event::WHEEL_LEFT>(
				boost::bind(
					&tscroll_container::signal_handler_sdl_wheel_left
					, this
					, _3
					, _6)
				, event::tdispatcher::back_child);

		content_grid_->connect_signal<event::WHEEL_RIGHT>(
				boost::bind(
					&tscroll_container::signal_handler_sdl_wheel_right
					, this
					, _3
					, _6)
				, event::tdispatcher::back_post_child);

		content_grid_->connect_signal<event::WHEEL_RIGHT>(
				boost::bind(
					&tscroll_container::signal_handler_sdl_wheel_right
					, this
					, _3
					, _6)
				, event::tdispatcher::back_child);

		content_grid_->connect_signal<event::MOUSE_ENTER>(
				boost::bind(
					&tscroll_container::signal_handler_mouse_enter
					, this
					, _5
					, true)
				, event::tdispatcher::back_pre_child);

		content_grid_->connect_signal<event::MOUSE_ENTER>(
				boost::bind(
					&tscroll_container::signal_handler_mouse_enter
					, this
					, _5
					, false)
				, event::tdispatcher::back_child);

		content_grid_->connect_signal<event::LEFT_BUTTON_DOWN>(
				boost::bind(
					&tscroll_container::signal_handler_left_button_down
					, this
					, _5
					, true)
				, event::tdispatcher::back_pre_child);

		content_grid_->connect_signal<event::LEFT_BUTTON_DOWN>(
				boost::bind(
					&tscroll_container::signal_handler_left_button_down
					, this
					, _5
					, false)
				, event::tdispatcher::back_child);

		content_grid_->connect_signal<event::LEFT_BUTTON_UP>(
				boost::bind(
					&tscroll_container::signal_handler_left_button_up
					, this
					, _5
					, true)
				, event::tdispatcher::back_pre_child);

		content_grid_->connect_signal<event::LEFT_BUTTON_UP>(
				boost::bind(
					&tscroll_container::signal_handler_left_button_up
					, this
					, _5
					, false)
				, event::tdispatcher::back_child);

		content_grid_->connect_signal<event::MOUSE_LEAVE>(boost::bind(
					&tscroll_container::signal_handler_mouse_leave
					, this
					, _5
					, true)
				 , event::tdispatcher::back_pre_child);

		content_grid_->connect_signal<event::MOUSE_LEAVE>(boost::bind(
					&tscroll_container::signal_handler_mouse_leave
					, this
					, _5
					, false)
				 , event::tdispatcher::back_child);

		content_grid_->connect_signal<event::MOUSE_MOTION>(
				boost::bind(
					&tscroll_container::signal_handler_mouse_motion
					, this
					, _3
					, _5
					, _6
					, true)
				, event::tdispatcher::back_pre_child);

		content_grid_->connect_signal<event::MOUSE_MOTION>(
				boost::bind(
					&tscroll_container::signal_handler_mouse_motion
					, this
					, _3
					, _5
					, _6
					, false)
				, event::tdispatcher::back_child);
	}
}

void tscroll_container::set_vertical_scrollbar_mode(const tscrollbar_mode scrollbar_mode)
{
	vertical_scrollbar_mode_ = scrollbar_mode;
}

void tscroll_container::set_horizontal_scrollbar_mode(const tscrollbar_mode scrollbar_mode)
{
	horizontal_scrollbar_mode_ = scrollbar_mode;
	{
		int ii = 0;
		horizontal_scrollbar_mode_ = always_invisible;
	}
}

void tscroll_container::impl_draw_children(
		  texture& frame_buffer
		, int x_offset
		, int y_offset)
{
	assert(get_visible() == twidget::VISIBLE
			&& content_grid_->get_visible() == twidget::VISIBLE);

	// Inherited.
	tcontainer_::impl_draw_children(frame_buffer, x_offset, y_offset);

	content_grid_->draw_children(frame_buffer, x_offset, y_offset);
}

void tscroll_container::broadcast_frame_buffer(texture& frame_buffer)
{
	tcontainer_::broadcast_frame_buffer(frame_buffer);

	content_grid_->broadcast_frame_buffer(frame_buffer);
}

void tscroll_container::clear_texture()
{
	tcontainer_::clear_texture();
	content_grid_->clear_texture();
}

void tscroll_container::layout_children()
{
	if (need_layout_) {
		place(get_origin(), get_size());

		// since place scroll_container again, set it dirty.
		set_dirty();

	} else {
		// Inherited.
		tcontainer_::layout_children();

		content_grid_->layout_children();
	}
}

void tscroll_container::invalidate_layout()
{
	need_layout_ = true;
}

void tscroll_container::child_populate_dirty_list(twindow& caller, const std::vector<twidget*>& call_stack)
{
	// Inherited.
	tcontainer_::child_populate_dirty_list(caller, call_stack);

	std::vector<twidget*> child_call_stack(call_stack);
	content_grid_->populate_dirty_list(caller, child_call_stack);
}

void tscroll_container::popup_new_window()
{
	if (vertical_scrollbar2_->widget->parent() == content_grid_) {
		reset_scrollbar(*get_window());
	}
	// content_grid_->popup_new_window();
}

void tscroll_container::show_content_rect(const SDL_Rect& rect)
{
	if (content_grid_->get_height() <= content_->get_height()) {
		return;
	}

	VALIDATE(rect.y >= 0, null_str);
	VALIDATE(horizontal_scrollbar_ && vertical_scrollbar_, null_str);

	// posix_print("show_content_rect------rect(%i, %i, %i, %i), item_position: %i\n", 
	//	rect.x, rect.y, rect.w, rect.h, vertical_scrollbar_->get_item_position());

	// bottom. make rect's bottom align to content_'s bottom.
	const int wanted_bottom = rect.y + rect.h;
	const int current_bottom = vertical_scrollbar_->get_item_position() + content_->get_height();
	int distance = wanted_bottom - current_bottom;
	if (distance > 0) {
		// posix_print("show_content_rect, setp1, move from %i, to %i\n", vertical_scrollbar_->get_item_position(), vertical_scrollbar_->get_item_position() + distance);
		vertical_set_item_position(vertical_scrollbar_->get_item_position() + distance);
	}

	// top. make rect's top align to content_'s top.
	if (rect.y < static_cast<int>(vertical_scrollbar_->get_item_position())) {
		// posix_print("show_content_rect, setp2, move from %i, to %i\n", vertical_scrollbar_->get_item_position(), rect.y);
		vertical_set_item_position(rect.y);
	}

	if (vertical_scrollbar_ != dummy_vertical_scrollbar_) {
		dummy_vertical_scrollbar_->set_item_position(vertical_scrollbar_->get_item_position());
	}

	// Update.
	// scrollbar_moved(true);
}

void tscroll_container::vertical_set_item_position(const unsigned item_position)
{
	vertical_scrollbar_->set_item_position(item_position);
	if (dummy_vertical_scrollbar_ != vertical_scrollbar_) {
		dummy_vertical_scrollbar_->set_item_position(item_position);
	}
}

void tscroll_container::horizontal_set_item_position(const unsigned item_position)
{
	horizontal_scrollbar_->set_item_position(item_position);
	if (dummy_horizontal_scrollbar_ != horizontal_scrollbar_) {
		dummy_horizontal_scrollbar_->set_item_position(item_position);
	}
}

void tscroll_container::vertical_scrollbar_moved()
{ 
	VALIDATE(get_visible() == twidget::VISIBLE, null_str);

	scrollbar_moved();

	VALIDATE(vertical_scrollbar_ != dummy_vertical_scrollbar_, null_str);
	dummy_vertical_scrollbar_->set_item_position(vertical_scrollbar_->get_item_position());
}

void tscroll_container::horizontal_scrollbar_moved()
{ 
	VALIDATE(get_visible() == twidget::VISIBLE, null_str);

	scrollbar_moved();

	VALIDATE(horizontal_scrollbar_ != dummy_horizontal_scrollbar_, null_str);
	dummy_horizontal_scrollbar_->set_item_position(horizontal_scrollbar_->get_item_position());
}

void tscroll_container::scroll_vertical_scrollbar(
		const tscrollbar_::tscroll scroll)
{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(scroll);
	scrollbar_moved();
}

void tscroll_container::handle_key_home(SDL_Keymod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_ && horizontal_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::BEGIN);
	horizontal_scrollbar_->scroll(tscrollbar_::BEGIN);
	scrollbar_moved();

	handled = true;
}

void tscroll_container::handle_key_end(SDL_Keymod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::END);
	scrollbar_moved();

	handled = true;
}

void tscroll_container::
		handle_key_page_up(SDL_Keymod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::JUMP_BACKWARDS);
	scrollbar_moved();

	handled = true;
}

void tscroll_container::
		handle_key_page_down(SDL_Keymod /*modifier*/, bool& handled)

{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::JUMP_FORWARD);
	scrollbar_moved();

	handled = true;
}

void tscroll_container::
		handle_key_up_arrow(SDL_Keymod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::ITEM_BACKWARDS);
	scrollbar_moved();

	handled = true;
}

void tscroll_container::
		handle_key_down_arrow( SDL_Keymod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::ITEM_FORWARD);
	scrollbar_moved();

	handled = true;
}

void tscroll_container
		::handle_key_left_arrow(SDL_Keymod /*modifier*/, bool& handled)
{
	assert(horizontal_scrollbar_);

	horizontal_scrollbar_->scroll(tscrollbar_::ITEM_BACKWARDS);
	scrollbar_moved();

	handled = true;
}

void tscroll_container
		::handle_key_right_arrow(SDL_Keymod /*modifier*/, bool& handled)
{
	assert(horizontal_scrollbar_);

	horizontal_scrollbar_->scroll(tscrollbar_::ITEM_FORWARD);
	scrollbar_moved();

	handled = true;
}

void tscroll_container::scrollbar_moved(bool gc_handled)
{
	/*** Update the content location. ***/
	int x_offset = horizontal_scrollbar_->get_item_position();
	int y_offset = vertical_scrollbar_->get_item_position();

	if (!gc_handled) {
		// if previous handled gc, do not, until mini_set_content_grid_origin, miin_set_content_grid_visible_area.
		y_offset = mini_handle_gc(x_offset, y_offset);
	}

	adjust_offset(x_offset, y_offset);
	const tpoint content_grid_origin = tpoint(
			content_->get_x() - x_offset, content_->get_y() - y_offset);

	mini_set_content_grid_origin(content_->get_origin(), content_grid_origin);
	mini_set_content_grid_visible_area(content_visible_area_);
	// content_grid_->set_dirty();
}

const std::string& tscroll_container::get_control_type() const
{
	static const std::string type = "scrollbar_container";
	return type;
}

void tscroll_container::signal_handler_sdl_key_down(
		const event::tevent event
		, bool& handled
		, const SDL_Keycode key
		, SDL_Keymod modifier)
{
	switch(key) {
		case SDLK_HOME :
			handle_key_home(modifier, handled);
			break;

		case SDLK_END :
			handle_key_end(modifier, handled);
			break;


		case SDLK_PAGEUP :
			handle_key_page_up(modifier, handled);
			break;

		case SDLK_PAGEDOWN :
			handle_key_page_down(modifier, handled);
			break;


		case SDLK_UP :
			handle_key_up_arrow(modifier, handled);
			break;

		case SDLK_DOWN :
			handle_key_down_arrow(modifier, handled);
			break;

		case SDLK_LEFT :
			handle_key_left_arrow(modifier, handled);
			break;

		case SDLK_RIGHT :
			handle_key_right_arrow(modifier, handled);
			break;
		default:
			/* ignore */
			break;
		}
}

void tscroll_container::scroll_timer_handler(const bool vertical, const bool up, const int level)
{
	if (scroll_elapse_.first != scroll_elapse_.second) {
		VALIDATE(scroll_elapse_.first > scroll_elapse_.second, null_str);
		const bool scrolled = scroll(vertical, up, level, false);
		if (scrolled) {
			scrollbar_moved();
			scroll_elapse_.second ++;
		} else {
			scroll_elapse_.first = scroll_elapse_.second;
		}
	}
	if (next_scrollbar_invisible_ticks_) {
		if (SDL_GetTicks() >= next_scrollbar_invisible_ticks_) {
			reset_scrollbar(*get_window());
		}
	}
	bool scroll_can_remove = scroll_elapse_.first == scroll_elapse_.second;
	bool invisible_can_remove = !next_scrollbar_invisible_ticks_;
	if (scroll_can_remove && invisible_can_remove) {
		posix_print("------tscroll_container::scroll_timer_handler, will remove timer.\n");
		scroll_timer_.reset();
	}
}

bool tscroll_container::scroll(const bool vertical, const bool up, const int level, const bool first)
{
	VALIDATE(level > 0, null_str);

	tscrollbar_& scrollbar = vertical? *vertical_scrollbar_: *horizontal_scrollbar_;

	const bool wheel = level > tevent_handler::swipe_wheel_level_gap;
	int level2 = wheel? level - tevent_handler::swipe_wheel_level_gap: level;
	const int offset = level2 * scrollbar.get_visible_items() / tevent_handler::swipe_max_normal_level;
	const unsigned int item_position = scrollbar.get_item_position();
	const unsigned int item_count = scrollbar.get_item_count();
	const unsigned int visible_items = scrollbar.get_visible_items();
	unsigned int item_position2;
	if (up) {
		item_position2 = item_position + offset;
		item_position2 = item_position2 > item_count - visible_items? item_count - visible_items: item_position2;
	} else {
		item_position2 = (int)item_position >= offset? item_position - offset : 0;
	}
	if (item_position2 == item_position) {
		return false;
	}
	scrollbar.set_item_position(item_position2);
	if (!wheel && first) {
		// [3, 10]
		const int min_times = 3;
		const int max_times = 10;
		int times = min_times + (max_times - min_times) * level2 / tevent_handler::swipe_max_normal_level;
		scroll_elapse_ = std::make_pair(times, 0);
		scroll_timer_.reset(add_timer(200, boost::bind(&tscroll_container::scroll_timer_handler, this, vertical, up, level)));
	}
	if (vertical) {
		if (dummy_vertical_scrollbar_ != vertical_scrollbar_) {
			dummy_vertical_scrollbar_->set_item_position(item_position2);
		}
	} else {
		if (dummy_horizontal_scrollbar_ != horizontal_scrollbar_) {
			dummy_horizontal_scrollbar_->set_item_position(item_position2);
		}
	}
	return true;
}

void tscroll_container::signal_handler_sdl_wheel_up(bool& handled, const tpoint& coordinate2)
{
	VALIDATE(vertical_scrollbar_, null_str);
	mini_wheel();

	if (scroll(true, true, coordinate2.y, true)) {
		scrollbar_moved();
	}
	handled = true;
}

void tscroll_container::signal_handler_sdl_wheel_down(bool& handled, const tpoint& coordinate2)
{
	VALIDATE(vertical_scrollbar_, null_str);
	mini_wheel();

	if (scroll(true, false, coordinate2.y, true)) {
		scrollbar_moved();
	}
	handled = true;
}

void tscroll_container::signal_handler_sdl_wheel_left(bool& handled, const tpoint& coordinate2)
{
	VALIDATE(horizontal_scrollbar_, null_str);
	mini_wheel();

	if (scroll(false, true, coordinate2.x, true)) {
		scrollbar_moved();
	}
	handled = true;
}

void tscroll_container::signal_handler_sdl_wheel_right(bool& handled, const tpoint& coordinate2)
{
	VALIDATE(horizontal_scrollbar_, null_str);
	mini_wheel();

	if (scroll(false, false, coordinate2.x, true)) {
		scrollbar_moved();
	}
	handled = true;
}

void tscroll_container::signal_handler_mouse_enter(const tpoint& coordinate, bool pre_child)
{
	twindow* window = get_window();

	if (next_scrollbar_invisible_ticks_) {
		next_scrollbar_invisible_ticks_ = 0;
	}

	if (vertical_scrollbar_ == dummy_vertical_scrollbar_) {
		posix_print("tscroll_container::signal_handler_mouse_enter------, %s\n", id().empty()? "<nil>": id().c_str());
		VALIDATE(horizontal_scrollbar_ == dummy_horizontal_scrollbar_, null_str);
		if (vertical_scrollbar2_->widget.get()->parent() != window) {
			// scrollbar is in other scroll_container.
			VALIDATE(vertical_scrollbar2_->widget.get()->parent() != content_grid_, null_str);
			window->reset_scrollbar();
		}

		vertical_scrollbar2_->set_ref_widget(this, null_point);
		vertical_scrollbar2_->set_visible(true);
		vertical_scrollbar2_->widget->set_parent(content_grid_);
		tscrollbar_& vertical_scrollbar = *dynamic_cast<tscrollbar_*>(vertical_scrollbar2_->widget.get());
		set_scrollbar_mode(vertical_scrollbar, vertical_scrollbar_mode_, vertical_scrollbar_->get_item_count(), vertical_scrollbar_->get_visible_items());
		vertical_scrollbar.set_item_position(vertical_scrollbar_->get_item_position());
		vertical_scrollbar.set_did_modified(boost::bind(&tscroll_container::vertical_scrollbar_moved, this));

		vertical_scrollbar_ = &vertical_scrollbar;

		VALIDATE(horizontal_scrollbar2_->widget.get()->parent() == window, null_str);
		horizontal_scrollbar2_->set_ref_widget(this, null_point);
		horizontal_scrollbar2_->set_visible(true);
		horizontal_scrollbar2_->widget->set_parent(content_grid_);
		tscrollbar_& horizontal_scrollbar = *dynamic_cast<tscrollbar_*>(horizontal_scrollbar2_->widget.get());
		set_scrollbar_mode(horizontal_scrollbar, horizontal_scrollbar_mode_, horizontal_scrollbar_->get_item_count(), horizontal_scrollbar_->get_visible_items());
		horizontal_scrollbar.set_item_position(horizontal_scrollbar_->get_item_position());
		horizontal_scrollbar.set_did_modified(boost::bind(&tscroll_container::horizontal_scrollbar_moved, this));
		horizontal_scrollbar_ = &horizontal_scrollbar;

		posix_print("------tscroll_container::signal_handler_mouse_enter\n");
	}
}

void tscroll_container::signal_handler_left_button_down(const tpoint& coordinate, bool pre_child)
{
	if (require_capture_) {
		get_window()->keyboard_capture(this);
	}

	if (pre_child) {
		if (vertical_scrollbar2_->widget->get_visible() == twidget::VISIBLE) {
			if (point_in_rect(coordinate.x, coordinate.y, vertical_scrollbar2_->widget->get_rect())) {
				return;
			}
		}
		if (horizontal_scrollbar2_->widget->get_visible() == twidget::VISIBLE) {
			if (point_in_rect(coordinate.x, coordinate.y, horizontal_scrollbar2_->widget->get_rect())) {
				return;
			}
		}
	}

	VALIDATE(point_in_rect(coordinate.x, coordinate.y, content_->get_rect()), null_str);

	if (scroll_timer_.valid()) {
		scroll_elapse_.first = scroll_elapse_.second = 0;
	}

	VALIDATE(is_null_coordinate(first_coordinate_), null_str);
	first_coordinate_ = coordinate;
	if (!pre_child && require_capture_) {
		get_window()->mouse_capture(true);
	}
	mini_mouse_down(first_coordinate_);
}

void tscroll_container::signal_handler_left_button_up(const tpoint& coordinate, bool pre_child)
{
	if (is_null_coordinate(first_coordinate_)) {
		return;
	}

	twindow* window = get_window();
	VALIDATE(window->mouse_focus_widget() || point_in_rect(coordinate.x, coordinate.y, content_->get_rect()), null_str);
	set_null_coordinate(first_coordinate_);
	mini_mouse_leave(first_coordinate_, coordinate);
	// posix_print("########signal_handler_mouse_up(%s), set_null_coordinate\n", id().empty()? "<nil>": id().c_str());
}

void tscroll_container::reset_scrollbar(twindow& window)
{
	posix_print("------tscroll_container::reset_scrollbar------%s\n", id().empty()? "<nil>": id().c_str());

	VALIDATE(vertical_scrollbar_ == vertical_scrollbar2_->widget.get() && horizontal_scrollbar_ == horizontal_scrollbar2_->widget.get(), null_str);

	vertical_scrollbar2_->set_ref_widget(nullptr, null_point);
	vertical_scrollbar2_->widget->set_parent(&window);

	tscrollbar_* scrollbar = dynamic_cast<tscrollbar_*>(vertical_scrollbar2_->widget.get());
	scrollbar->set_did_modified(NULL);
	vertical_scrollbar_ = dummy_vertical_scrollbar_;

	horizontal_scrollbar2_->set_ref_widget(nullptr, null_point);
	horizontal_scrollbar2_->widget->set_parent(&window);

	scrollbar = dynamic_cast<tscrollbar_*>(horizontal_scrollbar2_->widget.get());
	scrollbar->set_did_modified(NULL);
	horizontal_scrollbar_ = dummy_horizontal_scrollbar_;

	if (next_scrollbar_invisible_ticks_) {
		next_scrollbar_invisible_ticks_ = 0;
	}
}

void tscroll_container::signal_handler_mouse_leave(const tpoint& coordinate, bool pre_child)
{
	twindow* window = get_window();

	// maybe will enter float_widget, and this float_widget is in content_.
	if (vertical_scrollbar_ == vertical_scrollbar2_->widget.get()) {
		// althogh enter set vertical_scrollbar2_ to vertical_scroolbar_, but maybe not equal.
		// 1. popup new dialog. tdialog::show will call reset_scrollbar. reseted.
		// 2. mouse_leave call this function. 
		VALIDATE(horizontal_scrollbar_ == horizontal_scrollbar2_->widget.get(), null_str);
		if (!point_in_rect(coordinate.x, coordinate.y, content_->get_rect())) {
			const int scrollbar_out_threshold = 1000;
			next_scrollbar_invisible_ticks_ = SDL_GetTicks() + scrollbar_out_threshold;
			if (!scroll_timer_.valid()) {
				scroll_timer_.reset(add_timer(200, boost::bind(&tscroll_container::scroll_timer_handler, this, false, false, 0)));
			}
		}
	}

	if (is_null_coordinate(first_coordinate_)) {
		return;
	}

	if (is_magic_coordinate(coordinate) || (window->mouse_focus_widget() != content_grid_ && !window->point_in_normal_widget(coordinate.x, coordinate.y, *content_))) {
		// posix_print("########signal_handler_mouse_leave(%s), set_null_coordinate\n", id().empty()? "<nil>": id().c_str());
		set_null_coordinate(first_coordinate_);
		mini_mouse_leave(first_coordinate_, coordinate);
	}
}

void tscroll_container::signal_handler_mouse_motion(bool& handled, const tpoint& coordinate, const tpoint& coordinate2, bool pre_child)
{
	if (is_null_coordinate(first_coordinate_)) {
		return;
	}

	twindow* window = get_window();
	VALIDATE(window->mouse_focus_widget() || point_in_rect(coordinate.x, coordinate.y, content_->get_rect()), null_str);

	const int abs_diff_x = abs(coordinate.x - first_coordinate_.x);
	const int abs_diff_y = abs(coordinate.y - first_coordinate_.y);
	if (require_capture_ && window->mouse_focus_widget() != content_grid_) {
		if (pre_child) {
			if (abs_diff_x >= clear_click_threshold_ || abs_diff_x >= clear_click_threshold_) {
				window->mouse_capture(true, content_grid_);
			}
		} else {
			window->mouse_capture(true);
		}
	}

	if (window->mouse_click_widget()) {
		if (abs_diff_x >= clear_click_threshold_ || abs_diff_x >= clear_click_threshold_) {
			window->clear_mouse_click();
		}
	}

	if (!mini_mouse_motion(first_coordinate_, coordinate)) {
		return;
	}

	VALIDATE(vertical_scrollbar_ && horizontal_scrollbar_, null_str);
	int abs_x_offset = abs(coordinate2.x);
	int abs_y_offset = abs(coordinate2.y);
	
	if (!game_config::mobile) {
		abs_y_offset = 0;
	}

	if (abs_y_offset >= abs_x_offset) {
		abs_x_offset = 0;
	} else {
		abs_y_offset = 0;
	}

	if (abs_y_offset) {
		unsigned int item_position = vertical_scrollbar_->get_item_position();
		if (coordinate2.y < 0) {
			item_position = item_position + abs_y_offset;
		} else {
			item_position = (int)item_position >= abs_y_offset? item_position - abs_y_offset: 0;
		}
		vertical_set_item_position(item_position);
	}

	if (abs_x_offset) {
		unsigned int item_position = horizontal_scrollbar_->get_item_position();
		if (coordinate2.x < 0) {
			item_position = item_position + abs_x_offset;
		} else {
			item_position = (int)item_position >= abs_x_offset? item_position - abs_x_offset: 0;
		}
		horizontal_set_item_position(item_position);
	}

	if (coordinate2.x || coordinate2.y) {
		scrollbar_moved();
		handled = true;
	}
}

} // namespace gui2

