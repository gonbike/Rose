/* $Id: listbox.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#ifndef GUI_WIDGETS_LISTBOX_HPP_INCLUDED
#define GUI_WIDGETS_LISTBOX_HPP_INCLUDED

#include "gui/widgets/scroll_container.hpp"
#include "gui/widgets/toggle_panel.hpp"

namespace gui2 {

namespace implementation {
	struct tbuilder_listbox;
}

/** The listbox class. */
class tlistbox: public tscroll_container
{
	friend struct implementation::tbuilder_listbox;
public:
	class tgrid3: public tgrid
	{
		friend class tlistbox;
	public:
		tgrid3(tlistbox& listbox)
			: listbox_(listbox)
		{}

		void listbox_init();
		int listbox_insert_child(twidget& widget, int at);
		void listbox_erase_child(int at);
		void validate_children_continuous() const;

	private:
		void layout_init(bool linked_group_only) override;
		tpoint calculate_best_size() const override;
		tpoint fill_placeable_width(const int width) override;
		void place(const tpoint& origin, const tpoint& size) override;
		void set_origin(const tpoint& origin) override;	
		void set_visible_area(const SDL_Rect& area) override;
		void impl_draw_children(texture& frame_buffer, int x_offset, int y_offset) override;
		void dirty_under_rect(const SDL_Rect& clip) override;
		twidget* find_at(const tpoint& coordinate, const bool must_be_active) override;
		void layout_children() override;
		void child_populate_dirty_list(twindow& caller, const std::vector<twidget*>& call_stack) override;

		/** Inherited from tcontainer_. */
		twidget* find(const std::string& id, const bool must_be_active) { return nullptr; }

		/** Inherited from tcontrol.*/
		const twidget* find(const std::string& id, const bool must_be_active) const { return nullptr; }

	private:
		tlistbox& listbox_;
	};

	/**
	 * Constructor.
	 *
	 * @param has_minimum         Does the listbox need to have one item
	 *                            selected.
	 * @param has_maximum         Can the listbox only have one item
	 *                            selected.
	 * @param placement           How are the items placed.
	 * @param select              Select an item when selected, if false it
	 *                            changes the visible state instead.
	 */
	tlistbox(const std::vector<tlinked_group>& linked_groups);
	~tlistbox();

	void enable_select(const bool enable);

	/***** ***** ***** ***** Row handling. ***** ***** ****** *****/
	/**
	 * Adds single row to the grid.
	 *
	 * This function expect a row to have multiple widgets (either multiple
	 * columns or one column with multiple widgets).
	 *
	 *
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
	ttoggle_panel& insert_row(const std::map<std::string, std::string>& data, int at = npos);

	/**
	 * Removes a row in the listbox.
	 *
	 * @param row                 The row to remove, when not in
	 *                            range the function is ignored.
	 * @param count               The number of rows to remove, 0 means all
	 *                            rows (starting from row).
	 */
	void erase_row(int row = npos);

	/** Removes all the rows in the listbox, clearing it. */
	void clear();

	/**
	 * Makes a row active or inactive.
	 *
	 * NOTE this doesn't change the select status of the row.
	 *
	 * @param row                 The row to (de)activate.
	 * @param active              true activate, false deactivate.
	 */
	void set_row_active(const unsigned row, const bool active);

	/**
	 * Makes a row visible or invisible.
	 *
	 * @param row                 The row to show or hide.
	 * @param shown               true visible, false invisible.
	 */
	void set_row_widget_visible(int at, const std::string& id, const bool visible);

	void set_row_widget_label(int at, const std::string& id, const std::string& label);

	/**
	 * Returns the panel of the wanted row.
	 *
	 * There's only a const version since allowing callers to modify the grid
	 * behind our backs might give problems. We return a pointer instead of a
	 * reference since dynamic casting of pointers is easier (no try catch
	 * needed).
	 *
	 * @param row                 The row to get the grid from, the caller has
	 *                            to make sure the row is a valid row.
	 * @returns                   The panel of the wanted row.
	 */
	ttoggle_panel& row_panel(const int at) const;
	ttoggle_panel* cursel() const;

	/**
	 * Selectes a row.
	 *
	 * @param row                 The row to select.
	 * @param select              Select or deselect the row.
	 */
	bool select_row(const int at);

	/**
	 * Scroll to row.
	 */
	void scroll_to_row(int at = twidget::npos);

	/** Sort all items. */
	void sort(const boost::function<bool (const ttoggle_panel&, const ttoggle_panel&)>& did_compare);

	/** Returns the number of items in the listbox. */
	int rows() const;

	tgrid::titerator iterator() const;

	/** Inherited from tcontainer_. */
	void set_self_active(const bool /*active*/)  {}

	/***** ***** ***** setters / getters for members ***** ****** *****/
	void set_did_row_focus_changed(const boost::function<void (tlistbox&, ttoggle_panel&, const bool)>& callback)
	{
		did_row_focus_changed_ = callback;
	}
	void set_did_row_pre_change(const boost::function<bool (tlistbox& list, ttoggle_panel& widget)>& callback)
	{
		did_row_pre_change_ = callback;
	}
	void set_did_row_changed(const boost::function<void (tlistbox& list, ttoggle_panel& widget)>& callback)
	{
		did_row_changed_ = callback;
	}
	void set_did_row_double_click(const boost::function<void (tlistbox& list, ttoggle_panel& widget)>& callback)
	{
		did_row_double_click_ = callback;
	}
	void set_did_row_right_click(const boost::function<void (tlistbox& list, ttoggle_panel& widget)>& callback)
	{
		did_row_right_click_ = callback;
	}

	void set_did_allocated_gc(const boost::function<void (tlistbox& list, ttoggle_panel& widget)>& callback)
	{
		did_allocated_gc_ = callback;
	}

	void set_did_more_rows_gc(const boost::function<void (tlistbox& list)>& callback)
	{
		did_more_rows_gc_ = callback;
	}

	void set_list_builder(tbuilder_grid_ptr list_builder);

	tgrid* left_drag_grid() const { return left_drag_grid_; }
	int drag_at() const { return drag_at_; }
	void cancel_drag();

protected:

	void dirty_under_rect(const SDL_Rect& clip) override;

	/** Inherited from tcontainer_. */
	twidget* find_at(const tpoint& coordinate, const bool must_be_active) override;

	/** Inherited from tcontainer_. */
	void child_populate_dirty_list(twindow& caller,	const std::vector<twidget*>& call_stack) override;

	void popup_new_window() override;

	/***** ***** ***** ***** keyboard functions ***** ***** ***** *****/

	/** Inherited from tscroll_container. */
	void handle_key_up_arrow(SDL_Keymod modifier, bool& handled);

	/** Inherited from tscroll_container. */
	void handle_key_down_arrow(SDL_Keymod modifier, bool& handled);

	bool list_grid_handle_key_up_arrow();
	bool list_grid_handle_key_down_arrow();

	/** Function to call after the user clicked on a row. */
	void did_focus_changed(ttoggle_panel& widget, const bool);
	bool did_pre_change(ttoggle_panel& widget);
	void did_changed(ttoggle_panel& widget);
	void did_double_click(ttoggle_panel& widget);

private:
	void invalidate(/*const twidget& widget*/);
	void garbage_collection(ttoggle_panel& row);
	void show_row_rect(const int at);

	void mini_more_items() override;
	tpoint mini_calculate_content_grid_size(const tpoint& content_origin, const tpoint& content_size) override;

	void mini_set_content_grid_origin(const tpoint& origin, const tpoint& content_origin) override;
	void mini_set_content_grid_visible_area(const SDL_Rect& area) override;
	int mini_handle_gc(const int x_offset, const int y_offset) override;

	void mini_mouse_down(const tpoint& first) override;
	bool mini_mouse_motion(const tpoint& first, const tpoint& last) override;
	void mini_mouse_leave(const tpoint& first, const tpoint& last) override;
	void mini_wheel() override;

	int calculate_total_height_gc() const;
	int which_precise_row_gc(const int distance) const;
	int which_children_row_gc(const int distance) const;

	bool select_internal(twidget* widget, const bool selected = false);
	void gc_insert_or_erase_row(const int row, const bool insert);

	/**
	 * @todo A listbox must have the following config parameters in the
	 * instanciation:
	 * - fixed row height?
	 * - fixed column width?
	 * and if so the following ways to set them
	 * - fixed depending on header ids
	 * - fixed depending on footer ids
	 * - fixed depending on first row ids
	 * - fixed depending on list (the user has to enter a list of ids)
	 *
	 * For now it's always fixed width depending on the first row.
	 */


	/**
	 * Finishes the building initialization of the widget.
	 *
	 * @param header              Builder for the header.
	 * @param footer              Builder for the footer.
	 * @param list_data           The initial data to fill the listbox with.
	 */
	void finalize(
			tbuilder_grid_const_ptr header,
			tbuilder_grid_const_ptr footer,
			const std::vector<string_map>& list_data);

	ttoggle_panel* next_selectable(const int start_at, const bool invert);

	void layout_init(bool linked_group_only) override;
	void layout_linked_widgets(bool dirty);
	void init_linked_size_group(const std::string& id, const bool fixed_width, const bool fixed_height);
	bool has_linked_size_group(const std::string& id);
	void add_linked_widget(const std::string& id, twidget& widget, bool linked_group_only) override;
	void layout_init2();

	int in_which_row(const int x, const int y) const;
	void drag_motioned(const int offset);
	void drag_end();

private:
	/**
	 * Contains a pointer to the generator.
	 *
	 * The pointer is not owned by this class, it's stored in the content_grid_
	 * of the tscroll_container super class and freed when it's grid is
	 * freed.
	 */
	tgrid3* list_grid_;

	/** Contains the builder for the new items. */
	tbuilder_grid_const_ptr list_builder_;

	bool selectable_;
	ttoggle_panel* cursel_;

	int gc_next_precise_at_;
	int gc_first_at_;
	int gc_last_at_;
	int gc_current_content_width_;
	int gc_locked_at_;

	struct tgc_calculate_best_size_lock {
		tgc_calculate_best_size_lock(tlistbox& listbox)
			: listbox(listbox)
		{
			VALIDATE(!listbox.gc_calculate_best_size_, null_str);
			listbox.gc_calculate_best_size_ = true;
		}
		~tgc_calculate_best_size_lock()
		{
			listbox.gc_calculate_best_size_ = false;
		}
		tlistbox& listbox;
	};
	bool gc_calculate_best_size_;

	struct texplicit_select_lock {
		texplicit_select_lock(tlistbox& listbox)
			: listbox(listbox)
		{
			VALIDATE(!listbox.explicit_select_, null_str);
			listbox.explicit_select_ = true;
		}
		~texplicit_select_lock()
		{
			listbox.explicit_select_ = false;
		}
		tlistbox& listbox;
	};
	bool explicit_select_;

	int drag_judge_at_;
	int drag_at_;
	int drag_offset_;
	tgrid* left_drag_grid_;
	tpoint left_drag_grid_size_;
	tspacer* drag_spacer_;

	struct tlinked_size
	{
		tlinked_size(const bool width = false, const bool height = false)
			: widgets()
			, width(width)
			, height(height)
			, max_size(0, 0)
		{
		}

		/** The widgets linked. */
		std::vector<twidget*> widgets;

		/** Link the widgets in the width? */
		bool width;

		/** Link the widgets in the height? */
		bool height;

		tpoint max_size;
	};

	/** List of the widgets, whose size are linked together. */
	std::map<std::string, tlinked_size> linked_size_;
	bool linked_max_size_changed_;
	bool row_layout_size_changed_;

	/**
	 * This callback is called when the value in the listbox changes.
	 *
	 * @todo the implementation of the callback hasn't been tested a lot and
	 * there might be too many calls. That might happen if an arrow up didn't
	 * change the selected item.
	 */

	boost::function<void (tlistbox& list, ttoggle_panel& widget, const bool)> did_row_focus_changed_;
	boost::function<bool (tlistbox& list, ttoggle_panel& widget)> did_row_pre_change_;
	boost::function<void (tlistbox& list, ttoggle_panel& widget)> did_row_changed_;
	boost::function<void (tlistbox& list, ttoggle_panel& widget)> did_row_double_click_;
	boost::function<void (tlistbox& list, ttoggle_panel& widget)> did_row_right_click_;

	boost::function<void (tlistbox& list, ttoggle_panel& widget)> did_allocated_gc_;
	boost::function<void (tlistbox& list)> did_more_rows_gc_;

	/** Inherited from tcontrol. */
	const std::string& get_control_type() const;
};

} // namespace gui2

#endif

