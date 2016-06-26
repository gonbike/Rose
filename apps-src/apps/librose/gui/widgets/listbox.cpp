/* $Id: listbox.cpp 54521 2012-06-30 17:46:54Z mordante $ */
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

#include "gui/widgets/listbox.hpp"

#include "gui/auxiliary/widget_definition/listbox.hpp"
#include "gui/auxiliary/window_builder/listbox.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/spacer.hpp"
#include "gui/widgets/toggle_panel.hpp"

#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/image.hpp"
#include "formula_string_utils.hpp"

#include <boost/bind.hpp>

#include "posix2.h"

namespace gui2 {

REGISTER_WIDGET(listbox)

tlistbox::tlistbox(const std::vector<tlinked_group>& linked_groups)
	: tscroll_container(2, true) // FIXME magic number
	, list_builder_(NULL)
	, did_row_changed_(NULL)
	, list_grid_(NULL)
	, cursel_(nullptr)
	, selectable_(true)
	, drag_judge_at_(twidget::npos)
	, drag_at_(npos)
	, drag_offset_(0)
	, left_drag_grid_(nullptr)
	, left_drag_grid_size_(0, 0)
	, drag_spacer_(nullptr)
	, gc_next_precise_at_(0)
	, gc_first_at_(twidget::npos)
	, gc_last_at_(twidget::npos)
	, gc_current_content_width_(twidget::npos)
	, gc_locked_at_(twidget::npos)
	, gc_calculate_best_size_(false)
	, explicit_select_(false)
	, linked_max_size_changed_(false)
	, row_layout_size_changed_(false)
{
	// require_capture_ = false;

	for (std::vector<tlinked_group>::const_iterator it = linked_groups.begin(); it != linked_groups.end(); ++ it) {
		const tlinked_group& lg = *it;
		if (has_linked_size_group(lg.id)) {
			utils::string_map symbols;
			symbols["id"] = lg.id;
			t_string msg = vgettext2("Linked '$id' group has multiple definitions.", symbols);

			VALIDATE(false, msg);
		}
		init_linked_size_group(lg.id, lg.fixed_width, lg.fixed_height);
	}
}

tlistbox::~tlistbox()
{
	if (left_drag_grid_) {
		delete left_drag_grid_;
	}
	if (drag_spacer_) {
		delete drag_spacer_;
	}
}

ttoggle_panel& tlistbox::insert_row(const std::map<std::string, std::string>& data, int at)
{
	if (at != twidget::npos) {
		const int rows = list_grid_->children_vsize();
		VALIDATE(at >= 0 && at < rows, null_str);
	}
	ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(list_builder_->widgets[0]->build());
	widget->set_did_mouse_enter_leave(boost::bind(&tlistbox::did_focus_changed, this, _1, _2));
	widget->set_did_state_pre_change(boost::bind(&tlistbox::did_pre_change, this, _1));
	widget->set_did_state_changed(boost::bind(&tlistbox::did_changed, this, _1));
	widget->set_did_double_click(boost::bind(&tlistbox::did_double_click, this, _1));

	widget->set_child_members(data);
	widget->at_ = list_grid_->listbox_insert_child(*widget, at);
	at = widget->at_; // at maybe twidget::npos

	if (gc_current_content_width_ > 0 && ((at >= gc_first_at_ && at <= gc_last_at_) || (gc_next_precise_at_ > 0 && at <= gc_next_precise_at_))) {
		tgrid::tchild* children = list_grid_->children();
		
		// calculate it.
		ttoggle_panel& panel = *dynamic_cast<ttoggle_panel*>(children[at].widget_);
		{
			tclear_restrict_width_cell_size_lock lock;
			panel.get_best_size();
		}
		tpoint size = panel.fill_placeable_width(gc_current_content_width_);
		panel.gc_width_ = size.x;
		panel.gc_height_ = size.y;
		panel.twidget::set_size(tpoint(0, 0));
		panel.set_dirty();

		gc_insert_or_erase_row(at, true);

		gc_locked_at_ = at;
	}

	invalidate();

	return *widget;
}

int tlistbox::calculate_total_height_gc() const
{
	if (gc_next_precise_at_ == 0) {
		return 0;
	}
	
	tgrid::tchild* children = list_grid_->children();

	const ttoggle_panel* row = dynamic_cast<ttoggle_panel*>(children[gc_next_precise_at_ - 1].widget_);
	const int precise_height = row->gc_distance_ + row->gc_height_;
	const int average_height = precise_height / gc_next_precise_at_;
	return precise_height + (list_grid_->children_vsize() - gc_next_precise_at_) * average_height;
}

#define PRECISE_DISTANCE_THRESHOLD	500000000
#define join_estimated(value)		((value) + PRECISE_DISTANCE_THRESHOLD)
#define split_estimated(value)		((value) - PRECISE_DISTANCE_THRESHOLD)
#define is_estimated(value)			((value) != twidget::npos && (value) >= PRECISE_DISTANCE_THRESHOLD)
#define is_precise(value)			((value) != twidget::npos && (value) < PRECISE_DISTANCE_THRESHOLD)
#define distance_value(value)		(is_estimated(value)? split_estimated(value): value)

int tlistbox::which_precise_row_gc(const int distance) const
{
	VALIDATE(gc_next_precise_at_ > 0, null_str);

	tgrid::tchild* children = list_grid_->children();
	// 1. check in precise rows
	const ttoggle_panel* current_gc_row = dynamic_cast<ttoggle_panel*>(children[gc_next_precise_at_ - 1].widget_);
	if (distance > current_gc_row->gc_distance_ + current_gc_row->gc_height_) {
		return twidget::npos;
	} else if (distance == current_gc_row->gc_distance_ + current_gc_row->gc_height_) {
		return gc_next_precise_at_;
	}
	const int max_one_by_one_range = 2;
	int start = 0;
	int end = gc_next_precise_at_ - 1;
	int mid = (end - start) / 2 + start;
	while (mid - start > 1) {
		current_gc_row = dynamic_cast<ttoggle_panel*>(children[mid].widget_);
		if (distance < current_gc_row->gc_distance_) {
			end = mid;

		} else if (distance > current_gc_row->gc_distance_) {
			start = mid;

		} else {
			return mid;
		}
		mid = (end - start) / 2 + start;
	}

	int row = start;
	for (; row <= end; row ++) {
		current_gc_row = dynamic_cast<ttoggle_panel*>(children[row].widget_);
		if (distance >= current_gc_row->gc_distance_ && distance < current_gc_row->gc_distance_ + current_gc_row->gc_height_) {
			break;
		}
	}
	VALIDATE(row <= end, null_str);

	return row;
}

int tlistbox::which_children_row_gc(const int distance) const
{
	// return twidget::npos;

	tgrid::tchild* children = list_grid_->children();
	int childs = list_grid_->children_vsize();

	VALIDATE(childs > 0, null_str);

	// const int first_at = dynamic_cast<ttoggle_panel*>(children[0].widget_)->at_;
	// const int last_at = dynamic_cast<ttoggle_panel*>(children[childs - 1].widget_)->at_;
	const int first_at = gc_first_at_;
	const int last_at = gc_last_at_;

	const ttoggle_panel* last_precise_row = dynamic_cast<ttoggle_panel*>(children[gc_next_precise_at_ - 1].widget_);
	const int precise_height = last_precise_row->gc_distance_ + last_precise_row->gc_height_;
	const int average_height = precise_height / gc_next_precise_at_;

	ttoggle_panel* current_gc_row = dynamic_cast<ttoggle_panel*>(children[first_at].widget_);
	if (distance >= distance_value(current_gc_row->gc_distance_) - average_height && distance < distance_value(current_gc_row->gc_distance_)) {
		return first_at - 1;
	}
	current_gc_row = dynamic_cast<ttoggle_panel*>(children[last_at].widget_);
	int children_end_distance = distance_value(current_gc_row->gc_distance_) + current_gc_row->gc_height_;
	if (distance >= children_end_distance && distance < children_end_distance + average_height) {
		return last_at + 1;
	}

	for (int n = first_at; n <= last_at; n ++) {
		current_gc_row = dynamic_cast<ttoggle_panel*>(children[n].widget_);
		const int distance2 = distance_value(current_gc_row->gc_distance_) + current_gc_row->gc_height_;
		if (distance >= distance_value(current_gc_row->gc_distance_) && distance < distance_value(current_gc_row->gc_distance_) + current_gc_row->gc_height_) {
			return n;
		}
	}
	return twidget::npos;
}

int tlistbox::mini_handle_gc(const int x_offset, const int y_offset)
{
	const int rows = list_grid_->children_vsize();
	if (!rows) {
		return y_offset;
	}
	tgrid::tchild* children = list_grid_->children();
	ttoggle_panel* current_gc_row = nullptr;

	const SDL_Rect content_rect = content_->get_rect();
	if (gc_calculate_best_size_) {
		VALIDATE(gc_first_at_ == twidget::npos, null_str);
	} else {
		VALIDATE(content_rect.h > 0, null_str);
	}

	if (gc_locked_at_ != twidget::npos) {
		VALIDATE(gc_locked_at_ >= 0 && gc_locked_at_ < rows, null_str);
		VALIDATE(!gc_calculate_best_size_, null_str);
	}

	int least_height = content_rect.h * 2; // 2 * content_.height
	const int half_content_height = content_rect.h / 2;

	if (!gc_calculate_best_size_ && (gc_locked_at_ != twidget::npos || gc_first_at_ != twidget::npos)) {
		VALIDATE(gc_last_at_ >= gc_first_at_ && gc_next_precise_at_ > 0, null_str);

		current_gc_row = dynamic_cast<ttoggle_panel*>(children[gc_next_precise_at_ - 1].widget_);
		VALIDATE(is_precise(current_gc_row->gc_distance_) && current_gc_row->gc_height_ != twidget::npos, null_str);
		const int precise_height = current_gc_row->gc_distance_ + current_gc_row->gc_height_;
		const int average_height = precise_height / gc_next_precise_at_;

		const int first_at = gc_first_at_;
		const int last_at = gc_last_at_;
		const ttoggle_panel* first_gc_row = gc_first_at_ != twidget::npos? dynamic_cast<ttoggle_panel*>(children[first_at].widget_): nullptr;
		const ttoggle_panel* last_gc_row = gc_first_at_ != twidget::npos? dynamic_cast<ttoggle_panel*>(children[last_at].widget_): nullptr;
		const int start_distance = y_offset >= half_content_height? y_offset - half_content_height: 0;
		int start_distance2 = twidget::npos;
		int estimated_start_row;
		bool children_changed = false;
		std::pair<int, int> smooth_scroll(twidget::npos, 0);

		if (gc_locked_at_ != twidget::npos) {
			// 1) calculate start row.
			if (gc_locked_at_ >= first_at && gc_locked_at_ <= last_at) {
				start_distance2 = distance_value(first_gc_row->gc_distance_);

				int valid_height = distance_value(last_gc_row->gc_distance_) + last_gc_row->gc_height_ - distance_value(first_gc_row->gc_distance_);
				// posix_print("mini_handle_gc, gc_locked_at_(%i), branch1, pre [%i, %i]\n", gc_locked_at_, gc_first_at_, gc_last_at_);
				int row = last_gc_row->at_ + 1;
				while (valid_height < least_height && row < rows) {
					current_gc_row = dynamic_cast<ttoggle_panel*>(children[row].widget_);
					if (current_gc_row->gc_height_ == twidget::npos) {
						ttoggle_panel& panel = *current_gc_row;

						{
							tclear_restrict_width_cell_size_lock lock;
							panel.get_best_size();
						}
						tpoint size = panel.fill_placeable_width(content_rect.w);

						panel.gc_width_ = size.x;
						panel.gc_height_ = size.y;

						panel.twidget::set_size(tpoint(0, 0));
					}
					VALIDATE(current_gc_row->gc_height_ != twidget::npos, null_str);
					valid_height += current_gc_row->gc_height_;
					row ++;
				}
				gc_last_at_ = row - 1;

				row = first_gc_row->at_ - 1;
				while (valid_height < least_height && row >= 0) {
					current_gc_row = dynamic_cast<ttoggle_panel*>(children[row].widget_);
					if (current_gc_row->gc_height_ == twidget::npos) {
						ttoggle_panel& panel = *current_gc_row;

						{
							tclear_restrict_width_cell_size_lock lock;
							panel.get_best_size();
						}
						tpoint size = panel.fill_placeable_width(content_rect.w);

						panel.gc_width_ = size.x;
						panel.gc_height_ = size.y;

						panel.twidget::set_size(tpoint(0, 0));
					}
					VALIDATE(current_gc_row->gc_height_ != twidget::npos, null_str);
					valid_height += current_gc_row->gc_height_;

					start_distance2 -= current_gc_row->gc_height_;
					VALIDATE(start_distance2 >= 0, null_str);

					row --;

				}
				gc_first_at_ = row + 1;

			} else {
				// posix_print("mini_handle_gc, gc_locked_at_(%i), branch2, pre [%i, %i]\n", gc_locked_at_, gc_first_at_, gc_last_at_);

				if (gc_locked_at_ < gc_next_precise_at_) {
					current_gc_row = dynamic_cast<ttoggle_panel*>(children[gc_locked_at_].widget_);
					start_distance2 = current_gc_row->gc_distance_;
				} else {
					start_distance2 = precise_height + (gc_locked_at_ - gc_next_precise_at_) * average_height;
				}

				int valid_height = 0;
				int row = gc_locked_at_;
				while (valid_height < least_height && row < rows) {
					current_gc_row = dynamic_cast<ttoggle_panel*>(children[row].widget_);
					if (current_gc_row->gc_height_ == twidget::npos) {
						ttoggle_panel& panel = *current_gc_row;

						{
							tclear_restrict_width_cell_size_lock lock;
							panel.get_best_size();
						}
						tpoint size = panel.fill_placeable_width(content_rect.w);

						panel.gc_width_ = size.x;
						panel.gc_height_ = size.y;

						panel.twidget::set_size(tpoint(0, 0));
					}
					VALIDATE(current_gc_row->gc_height_ != twidget::npos, null_str);
					valid_height += current_gc_row->gc_height_;
					row ++;
				}
				gc_last_at_ = row - 1;

				row = gc_locked_at_ - 1;
				while (valid_height < least_height && row >= 0) {
					current_gc_row = dynamic_cast<ttoggle_panel*>(children[row].widget_);
					if (current_gc_row->gc_height_ == twidget::npos) {
						ttoggle_panel& panel = *current_gc_row;

						{
							tclear_restrict_width_cell_size_lock lock;
							panel.get_best_size();
						}
						tpoint size = panel.fill_placeable_width(content_rect.w);

						panel.gc_width_ = size.x;
						panel.gc_height_ = size.y;

						panel.twidget::set_size(tpoint(0, 0));
					}
					VALIDATE(current_gc_row->gc_height_ != twidget::npos, null_str);
					valid_height += current_gc_row->gc_height_;
					row --;
				}
				gc_first_at_ = row + 1;
				
				if (first_at != twidget::npos) {
					if (last_at < gc_first_at_) {
						for (int n = first_at; n <= last_at; n ++) {
							// Garbage Collection!
							current_gc_row = dynamic_cast<ttoggle_panel*>(children[n].widget_);
							garbage_collection(*current_gc_row);
						}
					} else {
						// first_at--gc_first_at_--gc_last_at--last_at
						for (int n = first_at; n < gc_first_at_; n ++) {
							// Garbage Collection!
							current_gc_row = dynamic_cast<ttoggle_panel*>(children[n].widget_);
							garbage_collection(*current_gc_row);
						}
						for (int n = gc_last_at_ + 1; n <= last_at; n ++) {
							// Garbage Collection!
							current_gc_row = dynamic_cast<ttoggle_panel*>(children[n].widget_);
							garbage_collection(*current_gc_row);
						}
					}
				}
			}

			// posix_print("mini_handle_gc, gc_locked_at_(%i), post [%i, %i], children_changed(%s)\n", gc_locked_at_, gc_first_at_, gc_last_at_, children_changed? "true": "false");

			estimated_start_row = gc_first_at_;

			// as if all from gc_next_precise_at_, distance maybe change becase of linked_group.
			// let all enter calculate distance/height again.
			children_changed = true;

		} else if (y_offset < distance_value(first_gc_row->gc_distance_)) {
			if (first_at == 0 || last_at == 0) { // first row maybe enough height.
				return y_offset;
			}

			// posix_print("mini_handle_gc, y_offset: %i, start_distance: %i, content_grid.h - content.h: %i\n", y_offset, start_distance, content_grid_->get_height() - content_->get_height());
			// posix_print("mini_handle_gc, pre[%i, %i], cursel: %i, gc_next_precise_at_: %i\n", gc_first_at_, gc_last_at_, cursel_? cursel_->at_: -1, gc_next_precise_at_);

			// yoffset----
			// --first_gc_row---
			// 1) calculate start row.
			estimated_start_row = which_precise_row_gc(start_distance);
			if (estimated_start_row == twidget::npos) {
				estimated_start_row = which_children_row_gc(start_distance);
				if (estimated_start_row == twidget::npos) {
					estimated_start_row = gc_next_precise_at_ + (start_distance - precise_height) / average_height;
					VALIDATE(estimated_start_row < rows, null_str);
				}
			}
			if (estimated_start_row >= first_at) {
				estimated_start_row = first_at - 1;
			}
			// since drag up, up one at least.
			VALIDATE(estimated_start_row < first_at, null_str);

			// 2)at top, insert row if necessary.
			int valid_height = 0;
			if (estimated_start_row < gc_next_precise_at_) {
				// start_distance - current_gc_row->gc_distance_ maybe verty large.
				current_gc_row = dynamic_cast<ttoggle_panel*>(children[estimated_start_row].widget_);
				VALIDATE(start_distance >= current_gc_row->gc_distance_ && start_distance < current_gc_row->gc_distance_ + current_gc_row->gc_height_, null_str);
				valid_height = -1 * (start_distance - current_gc_row->gc_distance_);
			}
			int row = estimated_start_row;
			while (valid_height < least_height && row < rows) {
				current_gc_row = dynamic_cast<ttoggle_panel*>(children[row].widget_);
				if (current_gc_row->gc_height_ == twidget::npos) {
					ttoggle_panel& panel = *current_gc_row;

					{
						tclear_restrict_width_cell_size_lock lock;
						panel.get_best_size();
					}
					tpoint size = panel.fill_placeable_width(content_rect.w);

					panel.gc_width_ = size.x;
					panel.gc_height_ = size.y;

					panel.twidget::set_size(tpoint(0, 0));
				}
				VALIDATE(current_gc_row->gc_height_ != twidget::npos, null_str);

				valid_height += current_gc_row->gc_height_;

				if (estimated_start_row == first_at - 1 && row == estimated_start_row) {
					// slow move. make sure current row(first_at) can be on top.
					if (valid_height > least_height || least_height - valid_height < content_rect.h) {
						valid_height = least_height - content_rect.h;
					}
				}

				row ++;
			}
			VALIDATE(row > estimated_start_row, null_str);

			// 3) at bottom, erase children
			if (row > first_at) { // this allocate don't include row, so don't inlucde "==".
				smooth_scroll = std::make_pair(first_at, distance_value(first_gc_row->gc_distance_));
			}

			const int start_at = row > first_at? row: first_at;
			for (int n = start_at; n <= last_at; n ++) {
				// Garbage Collection!
				current_gc_row = dynamic_cast<ttoggle_panel*>(children[n].widget_);
				garbage_collection(*current_gc_row);
			}

			gc_first_at_ = estimated_start_row;
			gc_last_at_ = row - 1;

			// posix_print("mini_handle_gc, erase bottom(from %i to %i), post[%i, %i]\n", start_at, last_at, gc_first_at_, gc_last_at_);

			start_distance2 = start_distance;
			children_changed = true;


		} else if (y_offset + content_rect.h >= distance_value(last_gc_row->gc_distance_) + last_gc_row->gc_height_) {
			if (last_at == rows - 1) {
				return y_offset;
			}

			// posix_print("mini_handle_gc, y_offset: %i, start_distance: %i, content_grid.h - content.h: %i\n", y_offset, start_distance, content_grid_->get_height() - content_->get_height());
			// posix_print("mini_handle_gc, pre[%i, %i], cursel: %i, gc_next_precise_at_: %i\n", gc_first_at_, gc_last_at_, cursel_? cursel_->at_: -1, gc_next_precise_at_);

			// --last_gc_row---
			// yoffset----
			// 1) calculate start row.
			estimated_start_row = which_precise_row_gc(start_distance);
			if (estimated_start_row == twidget::npos) {
				estimated_start_row = which_children_row_gc(start_distance);
				if (estimated_start_row == twidget::npos) {
					estimated_start_row = gc_next_precise_at_ + (start_distance - precise_height) / average_height;
					VALIDATE(estimated_start_row < rows, null_str);

				}
			}

			//
			// don't use below statment. if first_at height enogh, it will result to up-down jitter.
			// 
			// if (estimated_start_row == first_at) {
			//	estimated_start_row = first_at + 1;
			// }

			// 2)at bottom, insert row if necessary.
			if (estimated_start_row <= last_at) {
				current_gc_row = dynamic_cast<ttoggle_panel*>(children[estimated_start_row].widget_);
				smooth_scroll = std::make_pair(estimated_start_row, distance_value(current_gc_row->gc_distance_));
			}

			int valid_height = 0;
			if (estimated_start_row < gc_next_precise_at_) {
				// start_distance - current_gc_row->gc_distance_ maybe verty large.
				current_gc_row = dynamic_cast<ttoggle_panel*>(children[estimated_start_row].widget_);
				VALIDATE(start_distance >= current_gc_row->gc_distance_ && start_distance < current_gc_row->gc_distance_ + current_gc_row->gc_height_, null_str);
				valid_height = -1 * (start_distance - current_gc_row->gc_distance_);
			}
			int row = estimated_start_row;
			while (valid_height < least_height && row < rows) {
				current_gc_row = dynamic_cast<ttoggle_panel*>(children[row].widget_);
				if (current_gc_row->gc_height_ == twidget::npos) {
					ttoggle_panel& panel = *current_gc_row;
					
					{
						tclear_restrict_width_cell_size_lock lock;
						panel.get_best_size();
					}
					tpoint size = panel.fill_placeable_width(content_->get_width());
					
					current_gc_row->gc_width_ = size.x;
					current_gc_row->gc_height_ = size.y;

					panel.twidget::set_size(tpoint(0, 0));
				}

				VALIDATE(current_gc_row->gc_height_ != twidget::npos, null_str);
				valid_height += current_gc_row->gc_height_;

				if (valid_height >= least_height && row <= last_at) {
					least_height += current_gc_row->gc_height_;
				}
								
				row ++;
			}

			if (row == rows) {
				// it is skip alloate and will to end almost.
				// avoid start large height and end samll height, it will result top part not displayed.
				if (valid_height < content_rect.h && estimated_start_row > 1) {
					int at = estimated_start_row - 1;
					while (valid_height < content_rect.h && at >= 0) {
						current_gc_row = dynamic_cast<ttoggle_panel*>(children[at].widget_);
						if (current_gc_row->gc_height_ == twidget::npos) {
							ttoggle_panel& panel = *current_gc_row;
							
							{
								tclear_restrict_width_cell_size_lock lock;
								panel.get_best_size();
							}
							tpoint size = panel.fill_placeable_width(content_->get_width());
							
							current_gc_row->gc_width_ = size.x;
							current_gc_row->gc_height_ = size.y;

							panel.twidget::set_size(tpoint(0, 0));
						}

						VALIDATE(current_gc_row->gc_height_ != twidget::npos, null_str);
						valid_height += current_gc_row->gc_height_;
						at --;
					}
					estimated_start_row = at + 1;
					VALIDATE(estimated_start_row > 0, null_str);
				}
			}

			// 3) at top, erase children
			const int stop_at = estimated_start_row - 1 > last_at? last_at: estimated_start_row - 1;
			for (int at = first_at; at <= stop_at; at ++) {
				// Garbage Collection!
				current_gc_row = dynamic_cast<ttoggle_panel*>(children[at].widget_);
				garbage_collection(*current_gc_row);
			}

			gc_first_at_ = estimated_start_row;
			gc_last_at_ = row - 1;

			// posix_print("mini_handle_gc, erase top(from %i to %i), post[%i, %i]\n", first_at, stop_at, gc_first_at_, gc_last_at_);

			start_distance2 = start_distance;
			children_changed = true;
		}

		if (children_changed) {
			VALIDATE(estimated_start_row >= 0, null_str);

			layout_init2();

			// 4) from top to bottom fill distance
			bool is_precise = true;

			int next_distance = twidget::npos;
			for (int at = gc_first_at_; at <= gc_last_at_; at ++) {
				current_gc_row = dynamic_cast<ttoggle_panel*>(children[at].widget_);
				if (at != gc_first_at_) {
					if (is_precise) {
						current_gc_row->gc_distance_ = next_distance;
					} else {
						// if before is estimated, next must not be precise.
						VALIDATE(!is_precise(current_gc_row->gc_distance_), null_str);
						current_gc_row->gc_distance_ = join_estimated(next_distance);
					}

				} else {
					if (!is_precise(current_gc_row->gc_distance_)) {
						// must be skip.
						VALIDATE(at == estimated_start_row || current_gc_row->gc_distance_ != twidget::npos, null_str);
						VALIDATE(start_distance2 != twidget::npos, null_str);
						current_gc_row->gc_distance_ = join_estimated(start_distance2);
					}
					is_precise = !is_estimated(current_gc_row->gc_distance_);
				}
				if (is_precise && at == gc_next_precise_at_) {
					gc_next_precise_at_ ++;
				}
				next_distance = distance_value(current_gc_row->gc_distance_) + current_gc_row->gc_height_;
			}

			bool extend_precise_distance = is_precise && gc_next_precise_at_ == gc_last_at_ + 1;
			if (gc_next_precise_at_ == gc_first_at_) {
				// new first_at_ is equal to gc_next_precise_at_.
				VALIDATE(!is_precise, null_str);
				current_gc_row = dynamic_cast<ttoggle_panel*>(children[gc_next_precise_at_ - 1].widget_);
				next_distance = current_gc_row->gc_distance_ + current_gc_row->gc_height_;

				extend_precise_distance = true;
			}
			if (extend_precise_distance) {
				VALIDATE(gc_next_precise_at_ >= 1, null_str);
				for (; gc_next_precise_at_ < rows; gc_next_precise_at_ ++) {
					current_gc_row = dynamic_cast<ttoggle_panel*>(children[gc_next_precise_at_].widget_);
					if (current_gc_row->gc_height_ == twidget::npos) {
						break;
					} else {
						VALIDATE(!is_precise(current_gc_row->gc_distance_), null_str);
						current_gc_row->gc_distance_ = next_distance;
					}
					next_distance = current_gc_row->gc_distance_ + current_gc_row->gc_height_;
				}
			}

			const int height = calculate_total_height_gc();
			int new_item_position = vertical_scrollbar_->get_item_position();

			if (smooth_scroll.first != twidget::npos) {
				// exist align-row, modify [gc_first_at_, gc_last_at_]'s distance best it.
				current_gc_row = dynamic_cast<ttoggle_panel*>(children[smooth_scroll.first].widget_);
				int distance_diff = distance_value(current_gc_row->gc_distance_) - smooth_scroll.second;

				posix_print("base align-row(#%i), distance_diff: %i, adjust new_item_position from %i to %i\n", smooth_scroll.first, distance_diff, new_item_position, new_item_position + distance_diff);
				new_item_position = new_item_position + distance_diff;

				ttoggle_panel* first_children_row = dynamic_cast<ttoggle_panel*>(children[gc_first_at_].widget_);
				ttoggle_panel* last_children_row = dynamic_cast<ttoggle_panel*>(children[gc_last_at_].widget_);
				if (new_item_position < distance_value(first_children_row->gc_distance_) || (new_item_position + content_rect.h >= distance_value(last_children_row->gc_distance_) + last_children_row->gc_height_)) {
					posix_print("base align-row(#%i), adjust new_item_position from %i to %i\n", smooth_scroll.first, new_item_position, new_item_position - distance_diff);
					// new_item_position = new_item_position - distance_diff;
					new_item_position = distance_value(last_children_row->gc_distance_) + last_children_row->gc_height_ - content_rect.h;
				}

			}

			// if height called base this time distance > calculate_total_height_gc, substruct children's distance to move upward.
			int height2 = height;

			if (gc_last_at_ > gc_next_precise_at_) {
				VALIDATE(gc_first_at_ > gc_next_precise_at_, null_str);
				ttoggle_panel* last_children_row = dynamic_cast<ttoggle_panel*>(children[gc_last_at_].widget_);

				const ttoggle_panel* next_precise_row = dynamic_cast<ttoggle_panel*>(children[gc_next_precise_at_ - 1].widget_);
				const int precise_height2 = next_precise_row->gc_distance_ + next_precise_row->gc_height_;
				const int average_height2 = precise_height / gc_next_precise_at_;

				height2 = distance_value(last_children_row->gc_distance_) + last_children_row->gc_height_;
				// bottom out children.
				height2 += average_height2 * (rows - last_children_row->at_ - 1);
			}

			const int bonus_height = height2 - height;
			if (bonus_height) {
				for (int n = gc_first_at_; n <= gc_last_at_; n ++) {
					current_gc_row = dynamic_cast<ttoggle_panel*>(children[n].widget_);
					VALIDATE(is_estimated(current_gc_row->gc_distance_), null_str);
					posix_print("row#%i, adjust distance from %i to %i\n", n, distance_value(current_gc_row->gc_distance_), distance_value(current_gc_row->gc_distance_) - bonus_height);
					current_gc_row->gc_distance_ = join_estimated(distance_value(current_gc_row->gc_distance_)) - bonus_height;
				}
				new_item_position -= bonus_height;
			}

			// update height
			SDL_Rect rect = list_grid_->get_rect();
			SDL_Rect content_grid_rect = content_grid_->get_rect();
			const int diff = height - rect.h;
			list_grid_->twidget::place(tpoint(rect.x, rect.y), tpoint(rect.w, rect.h + diff));
			content_grid_->twidget::place(tpoint(content_grid_rect.x, content_grid_rect.y), tpoint(content_grid_rect.w, content_grid_rect.h + diff));

			// posix_print("mini_handle_gc, [%i, %i], list_grid_height: %i(%i), gc_next_precise_at_: %i\n", gc_first_at_, gc_last_at_, list_grid_->get_height(), height, gc_next_precise_at_);
			{
				current_gc_row = dynamic_cast<ttoggle_panel*>(children[rows - 1].widget_);
				// posix_print("last row#%i: [%i, %i], bonus_height: %i, diff: %i\n", 
				//	rows - 1, distance_value(current_gc_row->gc_distance_), current_gc_row->gc_height_,
				//	bonus_height, diff);
			}

			if (diff != 0) {
				set_scrollbar_mode(*vertical_scrollbar_,
					vertical_scrollbar_mode_,
					content_grid_->get_height(),
					content_->get_height());

				if (vertical_scrollbar_ != dummy_vertical_scrollbar_) {
					set_scrollbar_mode(*dummy_vertical_scrollbar_,
						vertical_scrollbar_mode_,
						content_grid_->get_height(),
						content_->get_height());
				}
			}

			if (new_item_position != vertical_scrollbar_->get_item_position()) {
				vertical_scrollbar_->set_item_position(new_item_position);
				if (vertical_scrollbar_ != dummy_vertical_scrollbar_) {
					dummy_vertical_scrollbar_->set_item_position(new_item_position);
				}
			}

			if (gc_locked_at_ != twidget::npos) {
				show_row_rect(gc_locked_at_);
				gc_locked_at_ = twidget::npos;
			}

			list_grid_->validate_children_continuous();
		}

	} else {
		VALIDATE(gc_locked_at_ == twidget::npos, null_str);
		// since no row, must first place.
		VALIDATE(!x_offset && !y_offset, null_str);
		VALIDATE(gc_first_at_ == twidget::npos && gc_first_at_ == twidget::npos, null_str);
		VALIDATE(rows > 0, null_str);

		if (gc_calculate_best_size_) {
			// must be at tgrid3::calculate_best_size. only calculate first least_height's rows.
			least_height = (int)settings::screen_height;
		}

		// 1. construct first row, and calculate row height.
		int last_distance = 0, row = 0;
		gc_first_at_ = 0;
		while ((!least_height || last_distance < least_height) && row < rows) {
			current_gc_row = dynamic_cast<ttoggle_panel*>(children[row].widget_);
			if (row < gc_next_precise_at_) {
				VALIDATE(current_gc_row->gc_distance_ == last_distance && current_gc_row->gc_height_ != twidget::npos, null_str);
			} else {
				if (current_gc_row->gc_height_ == twidget::npos) {
					ttoggle_panel& panel = *current_gc_row;
					
					tpoint size(0, 0);
					{
						tclear_restrict_width_cell_size_lock lock;
						size = panel.get_best_size();
					}
					if (!gc_calculate_best_size_) {
						size = panel.fill_placeable_width(content_->get_width());
					}
					panel.gc_width_ = size.x;
					panel.gc_height_ = size.y;

					panel.twidget::set_size(tpoint(0, 0));
				}
				current_gc_row->gc_distance_ = last_distance;
				gc_next_precise_at_ ++;
			}

			last_distance = current_gc_row->gc_distance_ + current_gc_row->gc_height_;
			row ++;
		}
		gc_last_at_ = row - 1;

		if (gc_next_precise_at_ == row && gc_next_precise_at_ < rows) {
			// current_gc_row = dynamic_cast<ttoggle_panel*>(children[gc_next_precise_at_].widget_);
			// current_gc_row->gc_distance_ = current_gc_row->gc_height_ = twidget::npos;

			ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[gc_next_precise_at_ - 1].widget_);
			int next_distance = widget->gc_distance_ + widget->gc_height_;
			for (; gc_next_precise_at_ < rows; gc_next_precise_at_ ++) {
				ttoggle_panel* current_gc_row = dynamic_cast<ttoggle_panel*>(children[gc_next_precise_at_].widget_);
				if (current_gc_row->gc_height_ == twidget::npos) {
					break;
				} else {
					VALIDATE(!is_precise(current_gc_row->gc_distance_), null_str);
					current_gc_row->gc_distance_ = next_distance;
				}
				next_distance = current_gc_row->gc_distance_ + current_gc_row->gc_height_;
			}

		}
		list_grid_->validate_children_continuous();

		if (!gc_calculate_best_size_) {
			layout_init2();

			// update height.
			int height = calculate_total_height_gc();
			SDL_Rect rect = list_grid_->get_rect();
			SDL_Rect content_grid_rect = content_grid_->get_rect();
			const int diff = height - rect.h;

			posix_print("mini_handle_gc, [%i, %i], height(%i) - rect.h(%i) = diff(%i)\n", gc_first_at_, gc_last_at_, height, rect.h, diff);

			list_grid_->twidget::place(tpoint(rect.x, rect.y), tpoint(rect.w, rect.h + diff));
			content_grid_->twidget::place(tpoint(content_grid_rect.x, content_grid_rect.y), tpoint(content_grid_rect.w, content_grid_rect.h + diff));
		}

		// remember this content_width.
		gc_current_content_width_ = gc_calculate_best_size_? 0: content_rect.w;
	}

	return vertical_scrollbar_->get_item_position();
}

void tlistbox::cancel_drag()
{
	if (left_drag_grid_ && drag_at_ != twidget::npos) {
		drag_end();
	}
}

// caller will erase or insert row. havn't erase it, but have inserted it.
void tlistbox::gc_insert_or_erase_row(const int row, const bool insert)
{
	const int rows = list_grid_->children_vsize();
	tgrid::tchild* children = list_grid_->children();

	bool corrected_distance = false;
	if (row < gc_next_precise_at_) {
		// some row height changed in precise rows.
		int next_distance = twidget::npos;
		if (insert) {
			next_distance = 0;
			if (row > 0) {
				ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[row - 1].widget_);
				next_distance = widget->gc_distance_ + widget->gc_height_;
			}
			gc_next_precise_at_ ++;
		}
		for (int at = row; at < gc_next_precise_at_; at ++) {
			ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[at].widget_);
			if (!insert || at != row) {
				VALIDATE(is_precise(widget->gc_distance_) && widget->gc_height_ != twidget::npos, null_str);
			} else {
				VALIDATE(widget->gc_distance_ == twidget::npos && widget->gc_height_ != twidget::npos, null_str);
			}
			if (at == row) {
				// row will erase or invisible.
				if (insert) {
					widget->gc_distance_ = next_distance;
					next_distance = widget->gc_distance_ + widget->gc_height_;
				} else {
					next_distance = widget->gc_distance_;
				}
			} else {
				widget->gc_distance_ = next_distance;
				next_distance = widget->gc_distance_ + widget->gc_height_;
			}
		}

		if (!insert) {
			gc_next_precise_at_ --;
		}
		corrected_distance = true;

	} else if (row == gc_next_precise_at_) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[gc_next_precise_at_ - 1].widget_);
		int next_distance = widget->gc_distance_ + widget->gc_height_;
		if (insert) {
			ttoggle_panel* current_gc_row = dynamic_cast<ttoggle_panel*>(children[gc_next_precise_at_].widget_);
			current_gc_row->gc_distance_ = next_distance;
			gc_next_precise_at_ ++;

		} else {
			for (++ gc_next_precise_at_; gc_next_precise_at_ < rows; gc_next_precise_at_ ++) {
				ttoggle_panel* current_gc_row = dynamic_cast<ttoggle_panel*>(children[gc_next_precise_at_].widget_);
				if (current_gc_row->gc_height_ == twidget::npos) {
					break;
				} else {
					VALIDATE(!is_precise(current_gc_row->gc_distance_), null_str);
					current_gc_row->gc_distance_ = next_distance;
				}
				next_distance = current_gc_row->gc_distance_ + current_gc_row->gc_height_;
			}
			gc_next_precise_at_ --;
		}
	}

	// session2: set gc_first_at_, gc_last_at
	if (row < gc_first_at_) {
		if (insert) {
			gc_first_at_ ++;
			gc_last_at_ ++;
		} else {
			gc_first_at_ --;
			gc_last_at_ --;
		}

	} else if (row >= gc_first_at_ && row <= gc_last_at_) {
		if (gc_first_at_ == gc_last_at_) {
			if (insert) {
				gc_first_at_ ++;
				gc_last_at_ ++;
			} else {
				gc_first_at_ = gc_last_at_ = twidget::npos;
			}

		} else if (insert) {
			// of couse, can fill toggle_panel of row, and expand [gc_first_at_, gc_last_at_].
			// but if use it, if app insert in [gc_first_at_, gc_last_at_] conitnue, it will slower and slower!
			// so use decrease [gc_first_at_, gc_last_at_].
			if (row == gc_first_at_) {
				gc_first_at_ ++;
				gc_last_at_ ++;
			} else {
				// insert. |->row   row<-|
				const int diff_first = row - gc_first_at_;
				const int diff_last = gc_last_at_ - row;
				if (diff_first < diff_last) {
					for (int at = gc_first_at_; at < row; at ++) {
						ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[at].widget_);
						garbage_collection(*widget);
					}
					VALIDATE(row < gc_last_at_, null_str);
					gc_first_at_ = row + 1;

				} else {
					for (int at = row + 1; at <= gc_last_at_ + 1; at ++) {
						ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[at].widget_);
						garbage_collection(*widget);
					}

					VALIDATE(row > gc_first_at_, null_str);
					gc_last_at_ = row - 1;
				}
			}
			
		} else {
			// erase.
			if (!corrected_distance) {
				int next_distance = twidget::npos;
				for (int at = row; at <= gc_last_at_; at ++) {
					ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[at].widget_);
					VALIDATE(is_estimated(widget->gc_distance_) && widget->gc_height_ != twidget::npos, null_str);
					if (at == row) {
						// row will erase or invisible.
						next_distance = distance_value(widget->gc_distance_);
					} else {
						widget->gc_distance_ = join_estimated(next_distance);
						next_distance = distance_value(widget->gc_distance_) + widget->gc_height_;
					}
				}
			}
			gc_last_at_ --;
		}
	}
}

void tlistbox::erase_row(int at)
{
	const int rows = list_grid_->children_vsize();
	if (!rows) {
		return;
	}
	if (at == twidget::npos) {
		at = rows - 1;
	}
	VALIDATE(at >= 0 && at < rows, null_str);

	if (left_drag_grid_ && drag_at_ == at) {
		// end drag require right at_, so execute before erase operator.
		drag_end();
	}

	const bool auto_select = true;
	bool require_reselect = selectable_ && auto_select && (!cursel_ || cursel_->at_ == at);

	gc_insert_or_erase_row(at, false);

	list_grid_->listbox_erase_child(at);

	// update subsequent panel's at_.
	tgrid::tchild* children = list_grid_->children();
	const int post_rows = list_grid_->children_vsize();

	if (post_rows && require_reselect) {
		ttoggle_panel* new_at = next_selectable(at, true);
		if (new_at) {
			select_row(new_at->at_);
		}
	}

	if (gc_current_content_width_ > 0) {
		// caller maybe call erase_row continue, will result to large effect burden
		if (at < post_rows) {
			gc_locked_at_ = at;
		} else if (post_rows) {
			gc_locked_at_ = at - 1;
		}
	}
	invalidate();
}

void tlistbox::clear()
{
	const int rows = list_grid_->children_vsize();
	if (!rows) {
		return;
	}

	for (int at = 0; at < rows; at ++) {
		list_grid_->listbox_erase_child(twidget::npos);
	}

	gc_first_at_ = gc_last_at_ = twidget::npos;
	gc_next_precise_at_ = 0;

	invalidate();
}

class tsort_func
{
public:
	tsort_func(const boost::function<bool (const ttoggle_panel& widget, const ttoggle_panel&)>& did_compare)
		: did_compare_(did_compare)
	{}

	bool operator()(ttoggle_panel* a, ttoggle_panel* b) const
	{
		return did_compare_(*a, *b);
	}

private:
	boost::function<bool (const ttoggle_panel& widget, const ttoggle_panel&)> did_compare_;
};

void tlistbox::sort(const boost::function<bool (const ttoggle_panel& widget, const ttoggle_panel&)>& did_compare)
{
	tgrid::tchild* children = list_grid_->children();
	const int rows = list_grid_->children_vsize();

	if (rows < 2) {
		return;
	}

	int align_row = twidget::npos;
	if (gc_current_content_width_ > 0 && gc_first_at_ != twidget::npos) {
		const int item_position = vertical_scrollbar_->get_item_position();
		for (int at = gc_first_at_; at <= gc_last_at_; at ++) {
			ttoggle_panel* panel = dynamic_cast<ttoggle_panel*>(children[at].widget_);
			if (distance_value(panel->gc_distance_) >= item_position) {
				align_row = at;
				break;
			}
		}
	}

	if (gc_first_at_ != twidget::npos) {
		for (int at = gc_first_at_; at <= gc_last_at_; at ++) {
			ttoggle_panel* panel = dynamic_cast<ttoggle_panel*>(children[at].widget_);
			garbage_collection(*panel);
		}
		gc_first_at_ = gc_last_at_ = twidget::npos;
	}
	ttoggle_panel* original_cursel = cursel_;
	bool check_cursel = cursel_ != NULL;

	std::vector<ttoggle_panel*> tmp;
	tmp.resize(rows, NULL);
	for (int n = 0; n < rows; n ++) {
		tmp[n] = dynamic_cast<ttoggle_panel*>(children[n].widget_);
	}
	std::stable_sort(tmp.begin(), tmp.end(), tsort_func(did_compare));
	for (int n = 0; n < rows; n ++) {
		children[n].widget_ = tmp[n];
		ttoggle_panel* panel = tmp[n];
		if (check_cursel && panel->at_ == cursel_->at_) {
			cursel_ = panel;
			check_cursel = false;
		}
		panel->at_ = n;
		panel->gc_distance_ = twidget::npos; // distnace is invalid, reset to npos.
	}

	gc_next_precise_at_ = 0;
	int next_distance = 0;
	for (int at = 0; at < rows; at ++, gc_next_precise_at_ ++) {
		ttoggle_panel* panel = dynamic_cast<ttoggle_panel*>(children[at].widget_);
		if (panel->gc_height_ == twidget::npos) {
			break;
		}
		panel->gc_distance_ = next_distance;
		next_distance = panel->gc_distance_ + panel->gc_height_;

	}

	// if (align_row != twidget::npos && gc_next_precise_at_) {
	if (false) {
		// scroll_to_row maybe result to jittle. so don't use it.
		scroll_to_row(align_row);
	} else {
		invalidate();
	}
}

int tlistbox::rows() const
{
	return list_grid_->children_vsize();
}

void tlistbox::set_row_active(const unsigned row, const bool active)
{
	tcontrol* widget = dynamic_cast<tcontrol*>(list_grid_->child(0, 0).widget_);
	widget->set_active(active);
}

void tlistbox::set_row_widget_visible(int at, const std::string& id, const bool visible)
{
	const int rows = list_grid_->children_vsize();
	if (!rows) {
		return;
	}
	if (at == twidget::npos) {
		at = rows - 1;
	}
	VALIDATE(at >= 0 && at < rows, null_str);

	tgrid::tchild* children = list_grid_->children();
	ttoggle_panel& panel = *dynamic_cast<ttoggle_panel*>(children[at].widget_);
	twidget* widget = find_widget<twidget>(&panel.grid(), id, false, true);

	twidget::tvisible original = widget->get_visible();
	VALIDATE(original != twidget::HIDDEN, null_str);
	if ((visible && original == twidget::VISIBLE) || (!visible && original == twidget::INVISIBLE)) {
		return;
	}

	{
		twindow::tinvalidate_layout_blocker block(*this->get_window());
		widget->set_visible(visible? twidget::VISIBLE: twidget::INVISIBLE);
	}

	if (gc_first_at_ != twidget::npos && at >= gc_first_at_ && at <= gc_last_at_) {
		invalidate();
	}
}

void tlistbox::set_row_widget_label(int at, const std::string& id, const std::string& label)
{
	const int rows = list_grid_->children_vsize();
	if (!rows) {
		return;
	}
	if (at == twidget::npos) {
		at = rows - 1;
	}
	VALIDATE(at >= 0 && at < rows, null_str);

	tgrid::tchild* children = list_grid_->children();
	ttoggle_panel& panel = *dynamic_cast<ttoggle_panel*>(children[at].widget_);
	tcontrol* widget = find_widget<tcontrol>(&panel.grid(), id, false, true);

	if (widget->label() == label) {
		return;
	}
	widget->set_label(label);

	if (gc_first_at_ != twidget::npos && at >= gc_first_at_ && at <= gc_last_at_) {
		invalidate();
	}
}

void tlistbox::invalidate(/*const twidget& widget*/)
{
	twindow* window = get_window();
	if (!window->layouted()) {
		return;
	}

	bool require_layout_window = false;
/*
	if (require_layout_window) {
		VALIDATE(!multi_line_, null_str);
		int height = widget.get_best_size().y;
		if (height <= (int)content_->get_height()) {
			require_layout_window = false;
		}	
	}
*/
	if (require_layout_window) {
		get_window()->invalidate_layout();
	} else {
		// it must be same-size, to save time, don't calcuate linked_group.
		invalidate_layout();
	}
}

void tlistbox::garbage_collection(ttoggle_panel& row)
{
	row.clear_texture();
}

void tlistbox::show_row_rect(const int at)
{
	const tgrid::tchild* children = list_grid_->children();
	const int rows = list_grid_->children_vsize();

	VALIDATE(at >=0 && at < rows && gc_first_at_ != twidget::npos, null_str);

	const ttoggle_panel* first_widget = dynamic_cast<ttoggle_panel*>(children[gc_first_at_].widget_);
	const ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[at].widget_);
	SDL_Rect rect{0, distance_value(widget->gc_distance_), 0, widget->gc_height_};
	
	tgrid* header = find_widget<tgrid>(content_grid_, "_header_grid", true, false);
	int header_height = header->get_best_size().y;

	// rect.x is distance from list_grid_.
	rect.h += header_height;

	show_content_rect(rect);
	if ((int)vertical_scrollbar_->get_item_position() < distance_value(first_widget->gc_distance_)) {
		// posix_print("show_row_rect, item_position(%i) < gc_first_at_'s distance (%i), set item_position to this distance\n", 
		//	at, vertical_scrollbar_->get_item_position(), distance_value(first_widget->gc_distance_));
		vertical_scrollbar_->set_item_position(distance_value(first_widget->gc_distance_));
	}

	// posix_print("tlistbox::show_row_rect------post [%i, %i], item_position: %i\n", gc_first_at_, gc_last_at_, vertical_scrollbar_->get_item_position());
	
	if (vertical_scrollbar_ != dummy_vertical_scrollbar_) {
		dummy_vertical_scrollbar_->set_item_position(vertical_scrollbar_->get_item_position());
	}

	// Update.
	scrollbar_moved(true);
}

ttoggle_panel& tlistbox::row_panel(const int at) const
{
	const tgrid::tchild* children = list_grid_->children();
	const int childs = list_grid_->children_vsize();
	VALIDATE(at >= 0 && at < childs, null_str);

	return *dynamic_cast<ttoggle_panel*>(children[at].widget_);
}

bool tlistbox::select_row(const int at)
{
	ttoggle_panel* desire_widget = nullptr;
	if (at != twidget::npos) {
		const tgrid::tchild* children = list_grid_->children();
		const int childs = list_grid_->children_vsize();
		VALIDATE(at >= 0 && at < childs, null_str);

		if (children[at].widget_ == cursel_) {
			return true;
		}
		desire_widget = &row_panel(at);
	}

	texplicit_select_lock lock(*this);
	return select_internal(desire_widget);
}

void tlistbox::enable_select(const bool enable)
{
	if (enable == selectable_) {
		return;
	}
	selectable_ = enable;
	if (enable) {
		VALIDATE(!cursel_, null_str);
	} else {
		select_internal(nullptr);
	}
}

// @widget: maybe null.
bool tlistbox::select_internal(twidget* widget, const bool selected)
{
	ttoggle_panel* desire_panel = nullptr;
	if (widget) {
		desire_panel = dynamic_cast<ttoggle_panel*>(widget);
		VALIDATE(!cursel_ || desire_panel->at_ != cursel_->at_, null_str);
	}

	// first call set_value(true). because did_pre_change require cursel() to get previous.
	ttoggle_panel* original_cursel = cursel_;
	bool changed = true;
	if (desire_panel) {
		if (selectable_ && !selected) {
			changed = desire_panel->set_value(true);
		}

		if (changed) {
			if (selectable_) {
				VALIDATE(desire_panel->get_value(), null_str);
				cursel_ = desire_panel;
			} else {// non-select mode.
				if (selected) {
					// it is called during user click. de-select.
					desire_panel->set_value(false);
				}
			}
		}
	}

	// rule: there is one selectable row at most at any time.
	if (changed && original_cursel) {
		VALIDATE(original_cursel->get_value(), "Previous toggle panel must be selected!");
		original_cursel->set_value(false);
		if (!desire_panel) {
			VALIDATE(original_cursel == cursel_, null_str);
			cursel_ = nullptr;
		}
	}

	if (changed && (cursel_ || (!selectable_ && desire_panel)) && did_row_changed_) {
		int origin_rows = list_grid_->children_vsize();
		did_row_changed_(*this, *desire_panel);
		// did_row_changed_ maybe insert/erase row. for example browse.
		// VALIDATE(list_grid_->children_vsize() == origin_rows, null_str);
	}

	if (!selectable_) {
		VALIDATE(!cursel_, null_str);
	}
	return changed;
}

ttoggle_panel* tlistbox::cursel() const
{
	return cursel_;
}

void tlistbox::did_focus_changed(ttoggle_panel& widget, const bool enter)
{
	if (did_row_focus_changed_) {
		did_row_focus_changed_(*this, widget, enter);
	}
}

bool tlistbox::did_pre_change(ttoggle_panel& widget)
{
	bool allow = true;
	if (selectable_ && did_row_pre_change_) {
		int origin_rows = list_grid_->children_vsize();
		allow = did_row_pre_change_(*this, widget);
		VALIDATE(list_grid_->children_vsize() == origin_rows, null_str);
	}
	return allow;
}

void tlistbox::did_changed(ttoggle_panel& widget)
{
	if (!explicit_select_) {
		select_internal(&widget, true);
	}
}

void tlistbox::did_double_click(ttoggle_panel& widget)
{
	if (did_row_double_click_) {
		did_row_double_click_(*this, widget);
	}
}

void tlistbox::mini_set_content_grid_origin(const tpoint& origin, const tpoint& content_origin)
{
	// Here need call twidget::set_visible_area in conent_grid_ only.
	content_grid()->twidget::set_origin(content_origin);

	tgrid* header = find_widget<tgrid>(content_grid(), "_header_grid", true, false);
	tpoint size = header->get_size();

	header->set_origin(tpoint(content_origin.x, origin.y));
	list_grid_->set_origin(tpoint(content_origin.x, content_origin.y + size.y));

	VALIDATE(drag_at_ == twidget::npos, "please call cancel_drag() before popup new window.");
/*
	if (left_drag_grid_ && left_drag_grid_->get_visible() == twidget::VISIBLE) {
		const twidget* parent = list_grid_->child(drag_at_).widget_;
		left_drag_grid_->set_origin(tpoint(left_drag_grid_->get_x(), parent->get_y()));	
	}
*/
}

void tlistbox::mini_set_content_grid_visible_area(const SDL_Rect& area)
{
	// Here need call twidget::set_visible_area in conent_gri_ only.
	content_grid()->twidget::set_visible_area(area);

	tgrid* header = find_widget<tgrid>(content_grid(), "_header_grid", true, false);
	tpoint size = header->get_size();
	SDL_Rect header_area = intersect_rects(area, header->get_rect());
	header->set_visible_area(header_area);
	
	SDL_Rect list_area = area;
	list_area.y = area.y + size.y;
	list_area.h = area.h - size.y;
	list_grid_->set_visible_area(list_area);
}

void tlistbox::dirty_under_rect(const SDL_Rect& clip)
{
	if (visible_ != VISIBLE) {
		return;
	}

	if (left_drag_grid_ && left_drag_grid_->get_visible() == twidget::VISIBLE) {
		VALIDATE(drag_at_ != twidget::npos, null_str);

		left_drag_grid_->dirty_under_rect(clip);
	}
	tscroll_container::dirty_under_rect(clip);
}

twidget* tlistbox::find_at(const tpoint& coordinate, const bool must_be_active)
{
	if (visible_ != VISIBLE) {
		return nullptr;
	}

	twidget* result = NULL;
	if (left_drag_grid_ && left_drag_grid_->get_visible() == twidget::VISIBLE) {
		VALIDATE(drag_at_ != twidget::npos, null_str);

		tcontrol* widget = dynamic_cast<tcontrol*>(list_grid_->child(drag_at_).widget_);
		result = left_drag_grid_->find_at(coordinate, must_be_active);
	}
	if (!result) {
		result = tscroll_container::find_at(coordinate, must_be_active);
	}
	return result;
}

void tlistbox::child_populate_dirty_list(twindow& caller, const std::vector<twidget*>& call_stack)
{
	if (left_drag_grid_ && drag_at_ != twidget::npos) {
		VALIDATE(left_drag_grid_->get_visible() == twidget::VISIBLE && drag_spacer_->get_visible() == twidget::VISIBLE, null_str);

		{
			std::vector<twidget*> child_call_stack(call_stack);
			child_call_stack.push_back(list_grid_->child(drag_at_).widget_);
			left_drag_grid_->populate_dirty_list(caller, child_call_stack);
		}
		{
			std::vector<twidget*> child_call_stack(call_stack);
			child_call_stack.push_back(list_grid_->child(drag_at_).widget_);
			drag_spacer_->populate_dirty_list(caller, child_call_stack);
		}
	}

	tscroll_container::child_populate_dirty_list(caller, call_stack);
}

void tlistbox::popup_new_window()
{
	cancel_drag();
	tscroll_container::popup_new_window();
}

void tlistbox::scroll_to_row(int at)
{
	const int rows = list_grid_->children_vsize();
	if (!rows) {
		return;
	}
	if (at == twidget::npos) {
		at = rows - 1;
	}
	gc_locked_at_ = at;
	mini_handle_gc(horizontal_scrollbar_->get_item_position(), vertical_scrollbar_->get_item_position());
}

tgrid::titerator tlistbox::iterator() const
{
	tgrid::titerator ret{list_grid_->children(), list_grid_->children_vsize()};
	return ret;
}

void tlistbox::layout_init(bool linked_group_only)
{
	// normal flow, do nothing.
}

void tlistbox::layout_linked_widgets(bool dirty)
{
	for (std::map<std::string, tlinked_size>::iterator it = linked_size_.begin(); it != linked_size_.end(); ++ it) {
		tlinked_size& linked_size = it->second;
		
		int at = 0;
		for (std::vector<twidget*>::const_iterator it2 = linked_size.widgets.begin(); it2 != linked_size.widgets.end(); ++ it2, at ++) {
			twidget* widget = *it2;
			tpoint size = widget->layout_size();
			if (dirty) {
				if (linked_size.width) {
					size.x = linked_size.max_size.x;
				}
				if (linked_size.height) {
					size.y = linked_size.max_size.y;
				}
				widget->set_layout_size(size);

			} else {
				if (linked_size.width) {
					VALIDATE(size.x == linked_size.max_size.x, null_str);
				}
				if (linked_size.height) {
					VALIDATE(size.y == linked_size.max_size.y, null_str);
				}
			}
		}
		linked_size.widgets.clear();
	}
}

void tlistbox::init_linked_size_group(const std::string& id, const bool fixed_width, const bool fixed_height)
{
	VALIDATE(fixed_width || fixed_height, null_str);
	VALIDATE(!has_linked_size_group(id), null_str);

	linked_size_[id] = tlinked_size(fixed_width, fixed_height);
}

bool tlistbox::has_linked_size_group(const std::string& id)
{
	return linked_size_.find(id) != linked_size_.end();
}

void tlistbox::add_linked_widget(const std::string& id, twidget& widget, bool linked_group_only)
{
	VALIDATE(!id.empty(), null_str);

	std::map<std::string, tlinked_size>::iterator it = linked_size_.find(id);
	VALIDATE(it != linked_size_.end(), null_str);
	tlinked_size& lg = it->second;

	if (!linked_group_only) {
		VALIDATE(lg.widgets.empty(), null_str);
		return;
	}

	tpoint size = widget.get_best_size();
	bool max_size_changed = false, layout_size_changed = false;
	if (lg.width) {
		if (size.x > lg.max_size.x) {
			lg.max_size.x = size.x;
			max_size_changed = true;

		} else if (size.x < lg.max_size.x) {
			size.x = lg.max_size.x;
			layout_size_changed = true;
		}
	}
	if (lg.height) {
		if (size.y > lg.max_size.y) {
			lg.max_size.y = size.y;
			max_size_changed = true;

		} else if (size.y < lg.max_size.y) {
			size.y = lg.max_size.y;
			layout_size_changed = true;
		}
	}

	if (!linked_max_size_changed_) {
		linked_max_size_changed_ = max_size_changed;
	}

	if (!row_layout_size_changed_) {
		row_layout_size_changed_ = layout_size_changed;
	}

	std::vector<twidget*>& widgets = lg.widgets;
	VALIDATE(std::find(widgets.begin(), widgets.end(), &widget) == widgets.end(), null_str);
	widgets.push_back(&widget);

	widget.set_layout_size(size);
}

void tlistbox::layout_init2()
{
	const int rows = list_grid_->children_vsize();
	VALIDATE(rows && gc_first_at_ != twidget::npos, null_str);
	VALIDATE(!gc_calculate_best_size_ && content_->get_width() > 0, null_str);
	VALIDATE(!linked_max_size_changed_, null_str);

	tgrid* header = find_widget<tgrid>(content_grid_, "_header_grid", true, false);
	tgrid::tchild* children = list_grid_->children();
	std::set<int> get_best_size_rows;
	{
		tlink_group_owner_lock lock(*this);

		// header
		header->layout_init(true);
		if (row_layout_size_changed_) {
			// set flag. this row require get_best_size again.
			header->set_width(0);
			row_layout_size_changed_ = false;
		}

		// body
		for (int row = gc_first_at_; row <= gc_last_at_; ++ row) {
			ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[row].widget_);
			if (widget->get_visible() != twidget::VISIBLE) {
				continue;
			}

			VALIDATE(!row_layout_size_changed_, null_str);


			// ignore linked_group of toggle_panel
			widget->grid().layout_init(true);
			if (row_layout_size_changed_) {
				// set flag. this row require get_best_size again.
				get_best_size_rows.insert(row);
				row_layout_size_changed_ = false;
			}
		}
		// footer
	}

	layout_linked_widgets(linked_max_size_changed_);

	// maybe result width changed.
	int max_width = content_->get_width();
	int min_height_changed_at = twidget::npos;

	// header
	if (linked_max_size_changed_ || !header->get_width()) {
		tpoint size = header->get_best_size();
		if (size.x > max_width) {
			max_width = size.x;
		}
		header->twidget::set_size(tpoint(0, size.y));
	}

	for (int row = gc_first_at_; row <= gc_last_at_; ++ row) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[row].widget_);
		if (widget->get_visible() != twidget::VISIBLE) {
			continue;
		}
		if (linked_max_size_changed_ || get_best_size_rows.count(row)) {
			ttoggle_panel& panel = *widget;

			{
				tclear_restrict_width_cell_size_lock lock;
				panel.get_best_size();
			}
			tpoint size = panel.fill_placeable_width(content_->get_width());
			if (min_height_changed_at == twidget::npos && size.y != panel.gc_height_) {
				min_height_changed_at = row;
			}
			
			panel.gc_width_ = size.x;
			panel.gc_height_ = size.y;
			panel.twidget::set_width(0);
		}
		if (widget->gc_width_ > max_width) {
			max_width = widget->gc_width_;
		}
	}

	const bool width_changed = max_width != content_grid_->get_width();
	if (width_changed || !header->get_width()) {
		header->place(header->get_origin(), tpoint(max_width, header->get_height()));
	}

	for (int row = gc_first_at_; row <= gc_last_at_; ++ row) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[row].widget_);
		if (widget->get_visible() != twidget::VISIBLE) {
			continue;
		}
		if (width_changed || !widget->get_width()) {
			widget->place(widget->get_origin(), tpoint(max_width, widget->gc_height_));
		}
	}

	if (width_changed) {
		list_grid_->twidget::set_width(max_width);
		content_grid_->twidget::set_width(max_width);

		set_scrollbar_mode(*horizontal_scrollbar_,
			horizontal_scrollbar_mode_,
			content_grid_->get_width(),
			content_->get_width());

		if (horizontal_scrollbar_ != dummy_horizontal_scrollbar_) {
			set_scrollbar_mode(*dummy_horizontal_scrollbar_,
				horizontal_scrollbar_mode_,
				content_grid_->get_width(),
				content_->get_width());
		}
	}

	if (min_height_changed_at != twidget::npos) {
		int next_distance;
		for (int at = min_height_changed_at; at < gc_next_precise_at_; at ++) {
			ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[at].widget_);
			VALIDATE(is_precise(widget->gc_distance_) && widget->gc_height_ != twidget::npos, null_str);
			if (at != min_height_changed_at) {
				widget->gc_distance_ = next_distance;
				next_distance = widget->gc_distance_ + widget->gc_height_;
			}
			next_distance = widget->gc_distance_ + widget->gc_height_;
		}
	}

	linked_max_size_changed_ = false;
}

int tlistbox::in_which_row(const int x, const int y) const
{
	// only find in [gc_first_at_, gc_last_at_]
	if (gc_first_at_ == twidget::npos) {
		return twidget::npos;
	}
	const tgrid::tchild* children = list_grid_->children();
	for (int at = gc_first_at_; at <= gc_last_at_; at ++) {
		const twidget* widget = children[at].widget_;
		if (point_in_rect(x, y, widget->get_rect())) {
			return at;
		}
	}
	return twidget::npos;
}

bool tlistbox::list_grid_handle_key_up_arrow()
{
	if (cursel_ == nullptr) {
		return false;
	}
	if (!cursel_->at_) {
		return false;
	}
	const tgrid::tchild* children = list_grid_->children();
	const int childs = list_grid_->children_vsize();

	if (!childs) {
		return false;
	}

	for (int n = cursel_->at_ - 1; n >= 0; -- n) {
		// NOTE we check the first widget to be active since grids have no
		// active flag. This method might not be entirely reliable.
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[n].widget_);
		if (widget->can_selectable()) {
			return select_row(widget->at_);
		}
	}
	return false;
}

// invert: if can not find next, invert=false think find fail immeditaly. invert=true, will find before continue.
ttoggle_panel* tlistbox::next_selectable(const int start_at, const bool invert)
{
	VALIDATE(start_at >= 0, null_str);

	int new_at = twidget::npos;
	const tgrid::tchild* children = list_grid_->children();
	const int size = list_grid_->children_vsize();
	if (size) {
		VALIDATE(start_at <= size, null_str);

		new_at = start_at;
		for (; new_at < size; new_at ++) {
			const tgrid::tchild& child = children[new_at];
			const ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(child.widget_);
			if (widget->can_selectable()) {
				break;
			}
		}
		if (new_at == size) {
			if (!invert) {
				return nullptr;
			}
			for (new_at = start_at - 1; new_at >= 0; new_at --) {
				const tgrid::tchild& child = children[new_at];
				const ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(child.widget_);
				if (widget->can_selectable()) {
					break;
				}
			}
			if (new_at < 0) {
				new_at = twidget::npos;
			}
		}
	}
	return new_at != twidget::npos? dynamic_cast<ttoggle_panel*>(children[new_at].widget_): nullptr;
}

bool tlistbox::list_grid_handle_key_down_arrow()
{
	if (cursel_ == NULL) {
		return false;
	}

	tgrid::tchild* children = list_grid_->children();
	const int childs = list_grid_->children_vsize();

	ttoggle_panel* widget = next_selectable(cursel_->at_ + 1, false);
	if (widget) {
		return select_row(widget->at_);
	}
	return false;
}

void tlistbox::handle_key_up_arrow(SDL_Keymod modifier, bool& handled)
{
	bool changed = list_grid_handle_key_up_arrow();

	if (changed) {
		scroll_to_row(cursel_->at_);
	}

	handled = true;
}

void tlistbox::handle_key_down_arrow(SDL_Keymod modifier, bool& handled)
{
	bool changed = list_grid_handle_key_down_arrow();

	if (changed) {
		scroll_to_row(cursel_->at_);
	}

	handled = true;
}

//
// listbox
//
void tlistbox::tgrid3::listbox_init()
{
	VALIDATE(!children_vsize_, "Must no child in listbox!");
	VALIDATE(!rows_ && !cols_, "Must no row and col in listbox!");

	cols_ = 1; // It is toggle_panel widget.
	col_width_.push_back(0);
	col_grow_factor_.push_back(0);
}

int tlistbox::tgrid3::listbox_insert_child(twidget& widget, int at)
{
	// make sure the new child is valid before deferring
	widget.set_parent(this);
	if (at == npos || at > children_vsize_) {
		at = children_vsize_;
	}

	// below memmove require children_size_ are large than children_vsize_!
	// resize_children require large than exactly rows_*cols_. because children_vsize_ maybe equal it.

	// i think, all grow factor is 0. except last stuff.
	row_grow_factor_.push_back(0);
	rows_ ++;
	row_height_.push_back(0);

	resize_children((rows_ * cols_) + 1);
	if (children_vsize_ - at) {
		memmove(&(children_[at + 1]), &(children_[at]), (children_vsize_ - at) * sizeof(tchild));
	}

	children_[at].widget_ = &widget;
	children_vsize_ ++;

	// replacement
	children_[at].flags_ = VERTICAL_ALIGN_TOP | HORIZONTAL_GROW_SEND_TO_CLIENT;

	for (int at2 = at + 1; at2 < children_vsize_; at2 ++) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children_[at2].widget_);
		widget->at_ ++;
	}

	return at;
}

void tlistbox::tgrid3::listbox_erase_child(int at)
{
	if (at == npos) {
		at = children_vsize_ - 1;
	}
	if (at < 0 || at >= children_vsize_) {
		return;
	}
	if (children_[at].widget_ == listbox_.cursel_) {
		listbox_.cursel_ = nullptr;
	}
	delete children_[at].widget_;
	children_[at].widget_ = NULL;
	if (at < children_vsize_ - 1) {
		memcpy(&(children_[at]), &(children_[at + 1]), (children_vsize_ - at - 1) * sizeof(tchild));

	}
	children_[children_vsize_ - 1].widget_ = NULL;

	children_vsize_ --;
	rows_ --;
	row_grow_factor_.pop_back();
	row_height_.pop_back();

	for (int at2 = at; at2 < children_vsize_; at2 ++) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children_[at2].widget_);
		widget->at_ --;
	}
}

void tlistbox::tgrid3::validate_children_continuous() const
{
	if (listbox_.gc_first_at_ == twidget::npos) {
		return;
	}

	VALIDATE(listbox_.gc_first_at_ <= listbox_.gc_last_at_, null_str);

	int next_row_at = 0;
	int next_distance = 0;
	const int cursel_at = listbox_.cursel_? listbox_.cursel_->at_: twidget::npos;
	for (int at = 0; at < children_vsize_; at ++) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children_[at].widget_);
		if (at < listbox_.gc_first_at_ || at > listbox_.gc_last_at_) {
			// rows out of [gc_first_at_, gc_last_at_] must not exist canvas.
			const tgrid& grid = widget->layout_grid();
			const tchild* children = grid.children();
			const int vsize = grid.children_vsize();
			for (int n = 0; n < vsize; n ++) {
				const tcontrol* control = dynamic_cast<tcontrol*>(children[n].widget_);
				if (control && !control->canvas().empty()) {
					const tcanvas& canvas = control->canvas(control->get_state());
					const texture& target = canvas.canvas_tex();
					VALIDATE(!target.get(), null_str);
				}
			}
		}
		// all row must visible
		VALIDATE(widget->get_visible() == twidget::VISIBLE, null_str);

		// distance
		if (at < listbox_.gc_next_precise_at_) {
			VALIDATE(is_precise(widget->gc_distance_) && widget->gc_height_ != twidget::npos, null_str);
			VALIDATE(widget->gc_distance_ == next_distance, null_str);
			next_distance += widget->gc_height_;

		} else if (at == listbox_.gc_next_precise_at_) {
			VALIDATE(widget->gc_distance_ == twidget::npos && widget->gc_height_ == twidget::npos, null_str);

		} else if (at >= listbox_.gc_first_at_ && at <= listbox_.gc_last_at_) {
			if (at == listbox_.gc_first_at_) {
				VALIDATE(is_estimated(widget->gc_distance_) && widget->gc_height_ != twidget::npos, null_str);
				next_distance = distance_value(widget->gc_distance_) + widget->gc_height_;

			} else {
				VALIDATE(is_estimated(widget->gc_distance_) && widget->gc_height_ != twidget::npos, null_str);
				VALIDATE(distance_value(widget->gc_distance_) == next_distance, null_str);
				next_distance = distance_value(widget->gc_distance_) + widget->gc_height_;
			}

		} else {
			// if gc_distance != npos, it's gc_height_ must valid. but if gc_height_ is valid, gc_distance_ maybe npos.
			VALIDATE(widget->gc_distance_ == twidget::npos || widget->gc_height_ != twidget::npos, null_str);
		}

		// at_ must be continues.
		VALIDATE(next_row_at == widget->at_, null_str);
		next_row_at ++;

		// support must one selectable.
		if (widget->get_value()) {
			VALIDATE(widget->at_ == cursel_at, null_str);
		} else {
			VALIDATE(widget->at_ != cursel_at, null_str);
		}
	}
}

void tlistbox::tgrid3::layout_init(bool linked_group_only)
{
	// listbox doesn't use normal layout_init flow.
	VALIDATE(false, null_str);
}

tpoint tlistbox::tgrid3::calculate_best_size() const
{
	VALIDATE(cols_ == 1, null_str);
	VALIDATE(row_height_.size() == rows_, null_str);
	VALIDATE(col_width_.size() == cols_, null_str);

	VALIDATE(row_grow_factor_.size() == rows_, null_str);
	VALIDATE(col_grow_factor_.size() == cols_, null_str);

	if (!children_vsize_) {
		return tpoint(0, 0);
	}

	if (listbox_.gc_first_at_ == twidget::npos) {
		tgc_calculate_best_size_lock lock(listbox_);
		listbox_.mini_handle_gc(0, 0);
		VALIDATE(listbox_.gc_first_at_ != twidget::npos, null_str);
	}

	int max_width = 0;
	for (int row = listbox_.gc_first_at_; row <= listbox_.gc_last_at_; ++ row) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children_[row].widget_);
		VALIDATE(widget, null_str);

		if (widget->gc_width_ > max_width) {
			max_width = widget->gc_width_;
		}

		VALIDATE(widget->gc_height_ != twidget::npos, null_str);
		row_height_[row] = widget->gc_height_;
	}
	col_width_[0] = max_width;

	return tpoint(col_width_[0], listbox_.calculate_total_height_gc());
}

tpoint tlistbox::tgrid3::fill_placeable_width(const int width)
{
	return tpoint(twidget::npos, twidget::npos);
}

void tlistbox::tgrid3::place(const tpoint& origin, const tpoint& size)
{
	twidget::place(origin, size);
	if (!children_vsize_) {
		return;
	}

	VALIDATE(listbox_.gc_first_at_ != twidget::npos, null_str);
	VALIDATE(listbox_.gc_current_content_width_ != twidget::npos, null_str);

	VALIDATE(cols_ == 1, null_str);
	VALIDATE(row_height_.size() == rows_, null_str);
	VALIDATE(col_width_.size() == cols_, null_str);

	VALIDATE(row_grow_factor_.size() == rows_, null_str);
	VALIDATE(col_grow_factor_.size() == cols_, null_str);

	tpoint orig = origin;
	for (int row = listbox_.gc_first_at_; row <= listbox_.gc_last_at_; ++ row) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children_[row].widget_);
		VALIDATE(widget, null_str);

		orig.y = origin.y + distance_value(widget->gc_distance_);
		widget->place(orig, tpoint(size.x, row_height_[row]));
	}
}

void tlistbox::tgrid3::set_origin(const tpoint& origin)
{
	// Inherited.
	twidget::set_origin(origin);

	if (!children_vsize_) {
		return;
	}
	static int times = 0;
	VALIDATE(listbox_.gc_first_at_ != twidget::npos, null_str);

	for (int row = listbox_.gc_first_at_; row <= listbox_.gc_last_at_; row ++) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children_[row].widget_);
		VALIDATE(widget, null_str);
/*
				SDL_Rect content = listbox_.content_spacer()->get_rect();
				SDL_Rect content_grid = listbox_.content_grid()->get_rect();
				posix_print("[%i]set_origin[#%i] conteng_grid(%i, %i, %i, %i), content(%i, %i, %i, %i), total_height_gc(%i)\n",
					times, row,
					content_grid.x, content_grid.y, content_grid.w, content_grid.h,
					content.x, content.y, content.w, content.h, listbox_.calculate_total_height_gc());
				posix_print("[%i]set_origin[#%i] vertical scrollbar, item_position(%i), item_count(%i), visible_items(%i)\n",
					times, row,
					listbox_.vertical_scrollbar_->get_item_position(), listbox_.vertical_scrollbar_->get_item_count(), listbox_.vertical_scrollbar_->get_visible_items());
				posix_print("[%i]set_origin[#%i] tgrid3::set_origin, origin(%i, %i)(y2: %i, distance: %i), height: %i, content(%i, %i)\n", 
					times, row,
					origin.x, origin.y, origin.y + distance_value(widget->gc_distance_),
					widget->gc_distance_, widget->gc_height_,
					content.x, content.y);
*/
		// distance  
		//    0    ---> origin.y
		//    1    ---> origin.y + 1
		widget->set_origin(tpoint(origin.x, origin.y + distance_value(widget->gc_distance_)));
	}
	times ++;
}

void tlistbox::tgrid3::set_visible_area(const SDL_Rect& area)
{
	// Inherited.
	twidget::set_visible_area(area);

	if (!children_vsize_) {
		return;
	}

	for (int row = listbox_.gc_first_at_; row <= listbox_.gc_last_at_; row ++) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children_[row].widget_);

		widget->set_visible_area(area);
	}
}

void tlistbox::tgrid3::impl_draw_children(texture& frame_buffer, int x_offset, int y_offset)
{
	VALIDATE(get_visible() == twidget::VISIBLE, null_str);
	clear_dirty();

	if (!children_vsize_) {
		return;
	}

	for (int row = listbox_.gc_first_at_; row <= listbox_.gc_last_at_; row ++) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children_[row].widget_);

		if (widget->get_visible() != twidget::VISIBLE) {
			continue;
		}

		if (widget->get_drawing_action() == twidget::NOT_DRAWN) {
			continue;
		}

		widget->draw_background(frame_buffer, x_offset, y_offset);
		widget->draw_children(frame_buffer, x_offset, y_offset);
		widget->draw_foreground(frame_buffer, x_offset, y_offset);
		widget->clear_dirty();
	}
}

void tlistbox::tgrid3::dirty_under_rect(const SDL_Rect& clip)
{
	if (visible_ != VISIBLE) {
		return;
	}
	
	if (!children_vsize_) {
		return;
	}

	for (int row = listbox_.gc_first_at_; row <= listbox_.gc_last_at_; row ++) {
		const tchild& child = children_[row];

		if (child.widget_->get_visible() != VISIBLE) {
			continue;
		}

		child.widget_->dirty_under_rect(clip);
	}
}

twidget* tlistbox::tgrid3::find_at(const tpoint& coordinate, const bool must_be_active)
{
	if (visible_ != VISIBLE) {
		return nullptr;
	}
	
	if (!children_vsize_) {
		return nullptr;
	}

	for (int row = listbox_.gc_first_at_; row <= listbox_.gc_last_at_; row ++) {
		const tchild& child = children_[row];

		if (child.widget_->get_visible() != VISIBLE) {
			continue;
		}

		twidget* widget = child.widget_->find_at(coordinate, must_be_active);
		if (widget) {
			return widget;
		}
	}
	return nullptr;
}

void tlistbox::tgrid3::layout_children()
{
	if (!children_vsize_) {
		return;
	}

	for (int row = listbox_.gc_first_at_; row <= listbox_.gc_last_at_; row ++) {
		tchild& child = children_[row];

		child.widget_->layout_children();
	}
}

void tlistbox::tgrid3::child_populate_dirty_list(twindow& caller, const std::vector<twidget*>& call_stack)
{
	if (!children_vsize_) {
		return;
	}
	VALIDATE(!call_stack.empty() && call_stack.back() == this, null_str);

	for (int row = listbox_.gc_first_at_; row <= listbox_.gc_last_at_; row ++) {
		tchild& child = children_[row];

		std::vector<twidget*> child_call_stack = call_stack;
		child.widget_->populate_dirty_list(caller, child_call_stack);
	}
}

namespace {

/**
 * Swaps an item in a grid for another one.*/
void swap_grid(tgrid* grid,
		tgrid* content_grid, twidget* widget, const std::string& id)
{
	assert(content_grid);
	assert(widget);

	// Make sure the new child has same id.
	widget->set_id(id);

	// Get the container containing the wanted widget.
	tgrid* parent_grid = NULL;
	if(grid) {
		parent_grid = find_widget<tgrid>(grid, id, false, false);
	}
	if(!parent_grid) {
		parent_grid = find_widget<tgrid>(content_grid, id, true, false);
	}
	parent_grid = dynamic_cast<tgrid*>(parent_grid->parent());
	assert(parent_grid);

	// Replace the child.
	widget = parent_grid->swap_child(id, widget, false);
	assert(widget);

	delete widget;
}

} // namespace

void tlistbox::finalize(
		tbuilder_grid_const_ptr header,
		tbuilder_grid_const_ptr footer,
		const std::vector<string_map>& list_data)
{
	// "Inherited."
	tscroll_container::finalize_setup();

	if (header) {
		swap_grid(&grid(), content_grid(), header->build(), "_header_grid");
	}

	if (footer) {
		swap_grid(&grid(), content_grid(), footer->build(), "_footer_grid");
	}

	list_grid_ = new tgrid3(*this);
	list_grid_->listbox_init();
	swap_grid(NULL, content_grid(), list_grid_, "_list_grid");
}

void tlistbox::mini_more_items()
{
	if (did_more_rows_gc_) {
		did_more_rows_gc_(*this);
	}
}

tpoint tlistbox::mini_calculate_content_grid_size(const tpoint& content_origin, const tpoint& content_size)
{
	const int rows = list_grid_->children_vsize();
	if (rows) {
		if (left_drag_grid_ && drag_at_ != twidget::npos) {
			// hope don't enter it.
			drag_end();
		}

		// once modify content rect, must call it.
		if (gc_first_at_ != twidget::npos && gc_current_content_width_ != content_size.x) {
			tlink_group_owner_lock lock(*this);

			// if content width change, require reset.
			tgrid* header = find_widget<tgrid>(content_grid_, "_header_grid", true, false);
			header->layout_init(false);

			gc_first_at_ = gc_last_at_ = twidget::npos;
			gc_next_precise_at_ = 0;
			tgrid::tchild* children = list_grid_->children();
			for (int at = 0; at < rows; at ++) {
				ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[at].widget_);
				widget->gc_distance_ = widget->gc_height_ = widget->gc_width_ = twidget::npos;
				widget->layout_init(false);
			}

			for (std::map<std::string, tlinked_size>::iterator it = linked_size_.begin(); it != linked_size_.end(); ++ it) {
				tlinked_size& linked_size = it->second;
				linked_size.max_size.x = linked_size.max_size.y = 0;
			}
		}
		if (gc_first_at_ == twidget::npos) {
			mini_handle_gc(0, 0);
		}
	}
	return content_grid_->get_best_size();
}

void tlistbox::drag_motioned(const int offset)
{
	VALIDATE(drag_judge_at_ == twidget::npos && drag_at_ != twidget::npos && offset >= 0, null_str);
	VALIDATE(left_drag_grid_->get_visible() == twidget::VISIBLE, null_str);

	const tgrid::tchild* children = list_grid_->children();
	twidget* widget = children[drag_at_].widget_;
	widget->set_origin(tpoint(content_grid_->get_x() - offset, widget->get_y()));
	SDL_Rect area = ::create_rect(content_->get_x(), widget->get_y(), content_->get_width() - drag_offset_, widget->get_height());
	widget->set_visible_area(area);

	area = ::create_rect(left_drag_grid_->get_x() + left_drag_grid_size_.x - offset, left_drag_grid_->get_y(), 
		offset, left_drag_grid_->get_height());
	if (offset > left_drag_grid_size_.x) {
		area.x = left_drag_grid_->get_x();
		area.w = left_drag_grid_size_.x;
	}
	left_drag_grid_->set_visible_area(area);

	area.x = area.y = area.w = area.h = 0;
	if (offset > left_drag_grid_size_.x) {
		area.x = content_->get_x() + content_->get_width() - offset;
		area.y = widget->get_y();
		area.w = offset - left_drag_grid_size_.x;
		area.h = widget->get_height();
	}
	if (area != drag_spacer_->get_rect()) {
		drag_spacer_->place(tpoint(area.x, area.y), tpoint(area.w, area.h));
	}
}

void tlistbox::drag_end()
{
	VALIDATE(drag_judge_at_ == twidget::npos && drag_at_ >= 0 && drag_at_ < list_grid_->children_vsize(), null_str);
	VALIDATE(left_drag_grid_->get_visible() == twidget::VISIBLE, null_str);

	{
		twindow::tinvalidate_layout_blocker block(*this->get_window());
		left_drag_grid_->set_visible(twidget::INVISIBLE);
		drag_spacer_->set_visible(twidget::INVISIBLE);
	}

	const tgrid::tchild* children = list_grid_->children();
	twidget* widget = children[drag_at_].widget_;
	widget->set_origin(tpoint(content_grid_->get_x(), widget->get_y()));
	SDL_Rect area = ::create_rect(content_->get_x(), widget->get_y(), content_->get_width(), widget->get_height());
	widget->set_visible_area(area);

	drag_at_ = twidget::npos;
	drag_offset_ = 0;
}

void tlistbox::mini_mouse_down(const tpoint& first)
{
	VALIDATE(drag_judge_at_ == twidget::npos, null_str);

	if (drag_at_ == twidget::npos) {
		VALIDATE(!drag_offset_, null_str);
		if (left_drag_grid_ && horizontal_scrollbar_->get_item_position() + horizontal_scrollbar_->get_visible_items() == horizontal_scrollbar_->get_item_count()) {
			drag_judge_at_ = in_which_row(first.x, first.y);
		}
	} else {
		drag_end();
	}
}

bool tlistbox::mini_mouse_motion(const tpoint& first, const tpoint& last)
{
	if (drag_judge_at_ == twidget::npos && drag_at_ == twidget::npos) {
		return true;

	} else if (drag_at_ == twidget::npos) {
		// judging...
		if (last.x > first.x) {
			// must not move to right.
			drag_judge_at_ = twidget::npos;
			return true;

		}
		const tgrid::tchild* children = list_grid_->children();
		twidget* widget = children[drag_judge_at_].widget_;
		if (!point_in_rect(last.x, last.y, widget->get_rect())) {
			drag_judge_at_ = twidget::npos;
			return true;
		}
		const int abs_diff_x = abs(last.x - first.x);
		const int abs_diff_y = abs(last.y - first.y);
		if (abs_diff_y > 2 * clear_click_threshold_) {
			// vertical move
			drag_judge_at_ = twidget::npos;
			return true;
		}
		if (first.x - last.x >= clear_click_threshold_) {
			drag_at_ = drag_judge_at_;
			drag_judge_at_ = twidget::npos;

			VALIDATE(left_drag_grid_->get_visible() == twidget::INVISIBLE, null_str);
			// app's drag_started_ maybe modify grid's content, requrie calcualte size again.
			left_drag_grid_size_ = left_drag_grid_->get_best_size();
			VALIDATE(left_drag_grid_size_.y <= (int)widget->get_height(), null_str);

			if (selectable_ && cursel_ && cursel_->at_ != drag_at_) {
				select_row(drag_at_);
			}

			left_drag_grid_->place(tpoint(content_->get_x() + content_->get_width() - left_drag_grid_size_.x, widget->get_y()),
				tpoint(left_drag_grid_size_.x, widget->get_height()));
			{
				twindow::tinvalidate_layout_blocker block(*this->get_window());
				left_drag_grid_->set_visible(twidget::VISIBLE);
				drag_spacer_->set_visible(twidget::VISIBLE);
			}

			drag_offset_ = abs_diff_x;
			drag_motioned(drag_offset_);
		}
		return false;

	}

	// draging
	VALIDATE(drag_judge_at_ == twidget::npos && drag_at_ != twidget::npos, null_str);
	VALIDATE(left_drag_grid_->get_visible() == twidget::VISIBLE, null_str);

	int diff = first.x - last.x;
	if (diff > left_drag_grid_size_.x) {
		// diff = left_drag_grid_size_.x;
	} else if (diff < 0) {
		diff = 0;
	}

	drag_offset_ = diff;
	drag_motioned(drag_offset_);
	return false;
}

void tlistbox::mini_mouse_leave(const tpoint& first, const tpoint& last)
{
	drag_judge_at_ = twidget::npos;

	if (drag_at_ != twidget::npos) {
		// const tgrid::tchild* children = list_grid_->children();
		// const twidget* widget = children[drag_at_].widget_;
		const int all_threshold = left_drag_grid_size_.x / 3;
		if (drag_offset_ >= all_threshold) {
			drag_offset_ = left_drag_grid_->get_width();
			drag_motioned(drag_offset_);

		} else {
			drag_end();
		}
	}
}

void tlistbox::mini_wheel()
{
	if (drag_at_ != twidget::npos) {
		drag_end();
	}
}

void tlistbox::set_list_builder(tbuilder_grid_ptr list_builder)
{ 
	VALIDATE(!list_builder_.get(), null_str);

	list_builder_ = list_builder;
	VALIDATE(list_builder_.get(), null_str);

	size_t s = list_builder_->widgets.size();

	if (list_builder_->widgets.size() >= 2) {
		left_drag_grid_ = dynamic_cast<tgrid*>(list_builder->widgets[1]->build());
		left_drag_grid_->set_visible(twidget::INVISIBLE);

		// don't set row(toggle_panel) to it's parent.
		// grid mayb containt "erase" button, it will erase row(toggle_panel). if it is parent, thine became complex.
		left_drag_grid_->set_parent(this);

		drag_spacer_ = new tspacer();
		drag_spacer_->set_definition("default");
		drag_spacer_->set_visible(twidget::INVISIBLE);
		drag_spacer_->set_parent(this);
	}
}

const std::string& tlistbox::get_control_type() const
{
	static const std::string type = "listbox";
	return type;
}

} // namespace gui2