/* $Id: tree_view.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
/*
   Copyright (C) 2010 - 2012 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/widgets/tree_view.hpp"

#include "gui/auxiliary/widget_definition/tree_view.hpp"
#include "gui/auxiliary/window_builder/tree_view.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/tree_view_node.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/spacer.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_WIDGET(tree_view)

ttree_view::ttree_view(const std::vector<tnode_definition>& node_definitions)
	: tscroll_container(2)
	, node_definitions_(node_definitions)
	, indention_step_size_(0)
	, root_node_(new ttree_view_node(
		  "root"
		, node_definitions_
		, NULL
		, *this
		, std::map<std::string, std::string>()
		, false))
	, selected_item_(NULL)
	, left_align_(false)
	, no_indentation_(false)
{
	connect_signal<event::LEFT_BUTTON_DOWN>(
			  boost::bind(
				    &ttree_view::signal_handler_left_button_down
				  , this
				  , _2)
			, event::tdispatcher::back_pre_child);
}

ttree_view_node& ttree_view::add_node(const std::string& id
		, const std::map<std::string, std::string>& data)
{
	return get_root_node().add_child(id, data);
}

void ttree_view::remove_node(ttree_view_node* node)
{
	VALIDATE(node && node != root_node_ && node->parent_node_, null_str);
	const tpoint node_size = node->get_size();

	std::vector<ttree_view_node*>::iterator it = node->parent_node_->children_.begin();

	for ( ; it != node->parent_node_->children_.end(); ++ it) {
		if (*it == node) {
			break;
		}
	}

	VALIDATE(it != node->parent_node_->children_.end(), null_str);

	delete(*it);
	node->parent_node_->children_.erase(it);

	if (get_size() == tpoint(0, 0)) {
		return;
	}

	// Don't shrink the width, need to think about a good algorithm to do so.
	invalidate_layout();
}

void ttree_view::child_populate_dirty_list(twindow& caller
		, const std::vector<twidget*>& call_stack)
{
	// Inherited.
	tscroll_container::child_populate_dirty_list(caller, call_stack);

	assert(root_node_);
	root_node_->impl_populate_dirty_list(caller, call_stack);
}

tpoint ttree_view::mini_calculate_content_grid_size(const tpoint& content_origin, const tpoint& content_size)
{
	tpoint size = content_size;
	if (left_align_ && !empty()) {
		ttoggle_panel* item = (*root_node_->children_.begin())->panel_;
		// by this time, hasn't called place(), cannot use get_size().
		int height = item->get_best_size().y;
		if (height <= size.y) {
			int list_height = size.y / height * height;

			// reduce hight if allow height > get_best_size().y
			height = root_node_->get_best_size().y;
			if (list_height > height) {
				list_height = height;
			}
			size.y = list_height;
			if (size.y != content_size.y) {
				content_->set_size(size);
			}
		}
	}

	return content_grid_->get_best_size();
}

bool ttree_view::empty() const
{
	return root_node_->empty();
}

void ttree_view::set_select_item(ttree_view_node* node, const bool from_ui)
{
	VALIDATE(node, null_str);

	if (selected_item_ == node) {
		return;
	}

	// Deselect current item
	if (selected_item_ && selected_item_->label()) {
		selected_item_->label()->set_value(false);
	}

	selected_item_ = node;
	selected_item_->label()->set_value(true);

	if (from_ui && did_item_changed_) {
		did_item_changed_(*this, *node);
	}
}

tpoint ttree_view::adjust_content_size(const tpoint& size)
{
	if (!left_align_ || empty()) {
		return size;
	}
	ttoggle_panel* item = (*root_node_->children_.begin())->panel_;
	// by this time, hasn't called place(), cannot use get_size().
	int height = item->get_best_size().y;
	if (height > size.y) {
		return size;
	}
	int list_height = size.y / height * height;

	// reduce hight if necessary.
	height = root_node_->get_best_size().y;
	if (list_height > height) {
		list_height = height;
	}
	return tpoint(size.x, list_height);
}

void ttree_view::adjust_offset(int& x_offset, int& y_offset)
{
	if (!left_align_ || empty() || !y_offset) {
		return;
	}
	ttoggle_panel* item = (*root_node_->children_.begin())->panel_;
	int height = item->get_size().y;
	if (y_offset % height) {
		y_offset = y_offset / height * height + height;
	}
}

void ttree_view::finalize_setup()
{
	// Inherited.
	tscroll_container::finalize_setup();

	content_grid()->set_rows_cols(1, 1);
	content_grid()->set_child(
			  root_node_
			, 0
			, 0
			, tgrid::VERTICAL_GROW_SEND_TO_CLIENT
				| tgrid::HORIZONTAL_GROW_SEND_TO_CLIENT
			, 0);
}

const std::string& ttree_view::get_control_type() const
{
	static const std::string type = "tree_view";
	return type;
}

void ttree_view::signal_handler_left_button_down(const event::tevent event)
{
	get_window()->keyboard_capture(this);
}

} // namespace gui2

