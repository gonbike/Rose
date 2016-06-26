/* $Id: campaign_difficulty.cpp 49602 2011-05-22 17:56:13Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>
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

#include "gui/dialogs/menu.hpp"

#include "display.hpp"
#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "wml_exception.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/label.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_DIALOG(rose, menu)

tmenu::tmenu(display& disp, const std::vector<titem>& items, int val, const tmenu* parent)
	: disp_(disp)
	, items_(items)
	, index_(twidget::npos)
	, parent_(parent)
	, mouse_motioned_(false)
	, focused_panel_(NULL)
	, selected_val_(twidget::npos)
{
	VALIDATE(!items.empty(), null_str);

	for (std::vector<titem>::const_iterator it = items.begin(); it != items.end(); ++ it) {
		if (it->val == val) {
			index_ = std::distance(items.begin(), it);
			break;
		}
	}
}

void tmenu::pre_show(CVideo& /*video*/, twindow& window)
{
	window_ = &window;

	window.set_canvas_variable("border", variant("menu-border"));
	if (parent_) {
		window.set_leave_dismiss(true, boost::bind(&tmenu::did_leave_dismiss, this, boost::ref(window), _1, _2));
		window.set_click_dismiss_except(boost::bind(&tmenu::did_click_dismiss_except, this, boost::ref(window), _1, _2));
	}

	tlabel* title = find_widget<tlabel>(&window, "title", false, true);
	title->set_visible(twidget::INVISIBLE);

	find_widget<twidget>(&window, "ok", false, true)->set_visible(twidget::INVISIBLE);
	find_widget<twidget>(&window, "cancel", false, true)->set_visible(twidget::INVISIBLE);

	tlistbox& list = find_widget<tlistbox>(&window, "listbox", false);
	list.enable_select(false);
	window.keyboard_capture(&list);

	std::map<std::string, std::string> data;

	for (std::vector<titem>::const_iterator it = items_.begin(); it != items_.end(); ++ it) {
		const titem& item = *it;
		
		data["label"] = item.str;

		ttoggle_panel& panel = list.insert_row(data);
		// panel.set_canvas_highlight(true, false);

		if (item.submenu.empty()) {
			tcontrol* widget = dynamic_cast<tcontrol*>(panel.find("submenu", false));
			widget->set_visible(twidget::HIDDEN);
		}
	}
	// make all item display focussed-background.
	list.select_row(twidget::npos);

	list.set_did_row_changed(boost::bind(&tmenu::item_click, this, boost::ref(window), _1, _2));
	list.set_did_row_focus_changed(boost::bind(&tmenu::item_focus_changed, this, boost::ref(window), _1, _2, _3));
}

const ttoggle_panel& tmenu::focused_panel() const
{
	return *focused_panel_;
}

bool tmenu::did_leave_dismiss(twindow& window, const int x, const int y)
{
	VALIDATE(parent_, null_str);

	if (point_in_rect(x, y, window.get_rect())) {
		mouse_motioned_ = true;
		return false;
	}

	const ttoggle_panel& focus = parent_->focused_panel();
	const SDL_Rect focus_rect = focus.get_rect();
	if (!mouse_motioned_) {
		return !point_in_rect(x, y, focus_rect);
	} else {
		const twindow* parent_window = focus.get_window();
		const SDL_Rect rect = parent_window->get_rect();
		SDL_Rect top = empty_rect, bottom = empty_rect;
		if (focus.at()) {
			// maybe exist top gap.
			top = ::create_rect(rect.x, rect.y, rect.w, focus_rect.y - rect.y);
		}
		if (focus.at() != parent_->items().size() - 1) {
			// maybe exist bottom gap.
			bottom = ::create_rect(rect.x, focus_rect.y + focus_rect.h, rect.w, rect.h - (focus_rect.y + focus_rect.h- rect.y));
		}
		return point_in_rect(x, y, top) || point_in_rect(x, y, bottom);
	}
}

bool tmenu::did_click_dismiss_except(twindow& window, const int x, const int y)
{
	if (parent_ == NULL) {
		return false;
	}

	const ttoggle_panel& focus = parent_->focused_panel();
	return point_in_rect(x, y, focus.get_rect());
}

void tmenu::item_click(twindow& window, tlistbox& list, ttoggle_panel& widget)
{
	VALIDATE(focused_panel_ == &widget, null_str);

	const titem& item = items_[focused_panel_->at()];
	VALIDATE(focused_panel_ == &widget, null_str);
	if (!item.submenu.empty()) {
		return;
	}
	index_ = widget.at();
	window.set_retval(twindow::OK);
}

void tmenu::item_focus_changed(twindow& window, tlistbox& list, twidget& panel, const bool enter)
{
	if (!enter) {
		return;
	}

	focused_panel_ = dynamic_cast<ttoggle_panel*>(&panel);
	const titem& item = items_[focused_panel_->at()];
	if (!item.submenu.empty()) {
		// this focus set but not draw, so require one absolute_draw.
		absolute_draw();

		const SDL_Rect panel_rect = panel.get_rect();
		const int x = panel_rect.x + panel_rect.w;
		const int y = panel_rect.y;

		gui2::tmenu dlg(disp_, item.submenu, twidget::npos, this);
		dlg.show(disp_.video(), x, y);
		int retval = dlg.get_retval();
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}

		request_close(gui2::twindow::OK, dlg.selected_val());
	}
}

void tmenu::request_close(int retval, int selected_val)
{
	selected_val_ = selected_val;
	window_->set_retval(retval);
}

void tmenu::post_show(twindow& window)
{
}

}
