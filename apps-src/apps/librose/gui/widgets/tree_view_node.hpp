/* $Id: tree_view_node.hpp 54007 2012-04-28 19:16:10Z mordante $ */
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

#ifndef GUI_WIDGETS_TREE_VIEW_NODE_HPP_INCLUDED
#define GUI_WIDGETS_TREE_VIEW_NODE_HPP_INCLUDED

#include "gui/auxiliary/window_builder/tree_view.hpp"

#include <boost/ptr_container/ptr_vector.hpp>

namespace gui2 {

class ttoggle_button;
class ttoggle_panel;
class ttree_view;

class ttree_view_node
	: public twidget
{
	friend struct ttree_view_node_implementation;
	friend class ttree_view;

public:
	static ttree_view_node& node_from_icon(twidget& icon);
	static const ttree_view_node& node_from_icon(const twidget& icon);

	typedef implementation::tbuilder_tree_view::tnode tnode_definition;
	ttree_view_node(const std::string& id
			, const std::vector<tnode_definition>& node_definitions
			, ttree_view_node* parent_node
			, ttree_view& parent_tree_view
			, const std::map<std::string, std::string>& data
			, bool branch);

	~ttree_view_node();

	/**
	 * Adds a child item to the list of child nodes.
	 *
	 * @param id                  The id of the node definition to use for the
	 *                            new node.
	 * @param data                The data to send to the set_members of the
	 *                            widgets. If the member id is not an empty
	 *                            string it is only send to the widget that has
	 *                            the wanted id (if any). If the member id is an
	 *                            empty string, it is send to all members.
	 *                            Having both empty and non-empty id's gives
	 *                            undefined behaviour.
	 * @param index               The item before which to add the new item,
	 *                            0 == begin, -1 == end.
	 */
	ttree_view_node& add_child(const std::string& id
			, const std::map<std::string, std::string>& data
			, const int index = npos
			, const bool branch = false);

	/**
	 * Is this node the root node?
	 *
	 * When the parent tree view is created it adds one special node, the root
	 * node. This node has no parent node and some other special features so
	 * several code paths need to check whether they are the parent node.
	 */
	bool is_root_node() const { return parent_node_ == NULL; }

	/**
	 * The indention level of the node.
	 *
	 * The root node starts at level 0.
	 */
	unsigned get_indention_level() const;

	/** Does the node have children? */
	bool empty() const { return children_.empty(); }

	/** Is the node folded? */
	bool is_folded() const;

	typedef boost::function<bool (const ttree_view_node&, const ttree_view_node&)> tcompare_function;
	class tsort_func
	{
	public:
		tsort_func(const tcompare_function& callback)
			: callback_(callback)
		{}
		bool operator()(ttree_view_node* a, ttree_view_node* b) const
		{
			return callback_(*a, *b);
		}
	private:
		const tcompare_function& callback_;
	};

	void sort_children(const tcompare_function& callback) {
		if (children_.size() < 2) {
			return;
		}
		std::stable_sort(children_.begin(), children_.end(), tsort_func(callback));
	}

	void fold();
	void unfold();
	void fold_children();
	void unfold_children();

	bool is_child2(const ttree_view_node& target) const;

	/** Inherited from twidget.*/
	twidget* find_at(const tpoint& coordinate, const bool must_be_active);

	/** Inherited from twidget.*/
	twidget* find(const std::string& id, const bool must_be_active);

	/** Inherited from twidget.*/
	const twidget* find(const std::string& id
			, const bool must_be_active) const;

	/**
	 * The "size" of the widget.
	 *
	 * @todo Rename this function, names to close to the size of the widget.
	 */
	size_t size() const { return children_.size(); }

	/**
	 * Removes all child items from the widget.
	 */
	void clear();

	/***** ***** ***** setters / getters for members ***** ****** *****/

	/**
	 * Returns the parent node.
	 *
	 * @pre                       is_root_node() == false.
	 */
	ttree_view_node& parent_node();

	/** The const version of @ref parent_node. */
	const ttree_view_node& parent_node() const;

	ttree_view& tree_view();

	const ttree_view& tree_view() const;

	ttoggle_panel* label() const { return panel_; }
	ttoggle_button* icon() const { return icon_; }

private:

	/**
	 * Our parent node.
	 *
	 * All nodes except the root node have a parent node.
	 */
	ttree_view_node* parent_node_;

	/** The tree view that owns us. */
	ttree_view& tree_view_;

	/** Grid holding our contents. */
	ttoggle_panel* panel_;
	// tgrid* grid_;


	/**
	 * Our children.
	 *
	 * We want the returned child nodes to remain stable so store pointers.
	 */
	std::vector<ttree_view_node*> children_;

	/**
	 * The node definitions known to use.
	 *
	 * This list is needed to create new nodes.
	 *
	 * @todo Maybe store this list in the tree_view to avoid copying the
	 * reference.
	 */
	const std::vector<tnode_definition>& node_definitions_;

	/** The icon to show the folded state. */
	ttoggle_button* icon_;

	bool branch_;

	/**
	 * "Inherited" from twidget.
	 *
	 * This version needs to call its children, which are it's child nodes.
	 */
	void impl_populate_dirty_list(twindow& caller,
			const std::vector<twidget*>& call_stack);

	tpoint calculate_best_size() const override;

	bool disable_click_dismiss() const { return true; }

	tpoint calculate_best_size(const int indention_level
			, const unsigned indention_step_size) const;

	tpoint calculate_best_size_left_align(const int indention_level
			, const unsigned indention_step_size) const;

	tpoint get_current_size() const;
	tpoint get_folded_size() const;
	tpoint get_unfolded_size() const;

	void set_origin(const tpoint& origin) override;
	unsigned set_origin(const unsigned indention_step_size, tpoint origin);

	void place(const tpoint& origin, const tpoint& size) override;
	unsigned place(const unsigned indention_step_size, tpoint origin, unsigned width);

	unsigned place_left_align(
			  const unsigned indention_step_size
			, tpoint origin
			, unsigned width);

	void set_visible_area(const SDL_Rect& area);

	void impl_draw_children(texture& frame_buffer, int x_offset, int y_offset);

	void clear_texture() override;

	// FIXME rename to icon
	void signal_handler_left_button_click(const event::tevent event);

	void signal_handler_label_left_button_click(
			  const event::tevent event
			, bool& handled
			, bool& halt);

	void signal_handler_left_button_double_click(
			  const event::tevent event
			, bool& handled
			, bool& halt);

	void init_grid(tgrid* grid, const std::map<std::string, std::string>& data);

	const std::string& get_control_type() const;
};

} // namespace gui2

#endif

