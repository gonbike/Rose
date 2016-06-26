/* $Id: simple_item_selector.cpp 48440 2011-02-07 20:57:31Z mordante $ */
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

#include "display.hpp"
#include "gui/dialogs/simple_item_selector.hpp"

#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/message.hpp"

#include <boost/bind.hpp>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_simple_item_selector
 *
 * == Simple item selector ==
 *
 * A simple one-column listbox with OK and Cancel buttons.
 *
 * @begin{table}{dialog_widgets}
 *
 * title & & label & m &
 *         Dialog title label. $
 *
 * message & & control & m &
 *         Text label displaying a description or instructions. $
 *
 * listbox & & listbox & m &
 *         Listbox displaying user choices. $
 *
 * -item & & control & m &
 *         Widget which shows a listbox item label. $
 *
 * ok & & button & m &
 *         OK button. $
 *
 * cancel & & button & m &
 *         Cancel button. $
 *
 * @end{table}
 */

REGISTER_DIALOG(rose, simple_item_selector)

tsimple_item_selector::tsimple_item_selector(display& disp, const std::string& title, const std::string& message, list_type const& items, bool title_uses_markup, bool message_uses_markup)
	: disp_(disp)
	, index_(-1)
	, title_(title)
	, msg_(message)
	, markup_title_(title_uses_markup)
	, markup_msg_(message_uses_markup)
	, single_button_(false)
	, items_(items)
	, ok_label_()
	, cancel_label_()
{
}

void tsimple_item_selector::pre_show(CVideo& /*video*/, twindow& window)
{
	window.set_canvas_variable("border", variant("default-border"));

	tlabel& ltitle = find_widget<tlabel>(&window, "title", false);
	tcontrol& lmessage = find_widget<tcontrol>(&window, "message", false);
	tlistbox& list = find_widget<tlistbox>(&window, "listbox", false);
	window.keyboard_capture(&list);

	ltitle.set_label(title_);

	lmessage.set_label(msg_);

	int n = 0;
	for (std::vector<std::string>::const_iterator it = items_.begin(); it != items_.end(); ++ it) {
		std::map<std::string, std::string> data;

		data.insert(std::make_pair("number", str_cast(n)));

		data.insert(std::make_pair("item", *it));

		ttoggle_panel& row = list.insert_row(data);
		did_allocated_gc(list, row);

		n ++;
	}

	if (index_ != -1 && index_ < list.rows()) {
		list.select_row(index_);
	}

	index_ = -1;

	list.set_did_more_rows_gc(boost::bind(&tsimple_item_selector::did_more_rows_gc, this, _1));

	tbutton& button_ok = find_widget<tbutton>(&window, "ok", false);
	tbutton& button_cancel = find_widget<tbutton>(&window, "cancel", false);

	if(!ok_label_.empty()) {
		button_ok.set_label(ok_label_);
	}

	if(!cancel_label_.empty()) {
		button_cancel.set_label(cancel_label_);
	}

	if(single_button_) {
		button_cancel.set_visible(gui2::twidget::INVISIBLE);
	}
}

void tsimple_item_selector::did_allocated_gc(tlistbox& list, ttoggle_panel& widget)
{
	tbutton& like = find_widget<tbutton>(&widget, "like", false);
	connect_signal_mouse_left_click(
		like
		, boost::bind(
		&tsimple_item_selector::do_like
		, this
		, boost::ref(widget)));

	like.set_label(str_cast(widget.at()));
}

void tsimple_item_selector::did_more_rows_gc(tlistbox& list)
{
	std::vector<std::string> options;
	std::stringstream ss;

	const int max_times = 1000;
	char* data = (char*)malloc(4 * max_times);
	int r;
	const int original_count = list.rows();
	for (int i = 0; i < max_times; i ++) {
		ss.str("");
		// ss << i << "    tscrollbar_container::calculate_scrollbar";
		ss << i << "    tscrollbar_container";

		// r = rand() % 20;
		r = rand() % 10;
		memcpy(data + 4 * i, &r, 4);

		// int r = 18;
		while (r-- > 0) {
			ss << "tscrollbar_container ";
		}

		std::map<std::string, std::string> data;

		data.insert(std::make_pair("number", str_cast(original_count + i)));

		// ss.str("");
		// ss << "add " << (original_count + i);
		data.insert(std::make_pair("item", ss.str()));

		ttoggle_panel& row = list.insert_row(data);
		did_allocated_gc(list, row);
	}	
}

void tsimple_item_selector::do_like(const ttoggle_panel& widget) const
{
	{
		tlistbox& list = find_widget<tlistbox>(window_, "listbox", false);
		list.erase_row(widget.at());
		// list.erase_row(1);
		return;
	}

	std::stringstream ss;

	ss << "Hello world. You press #" << widget.at() << " row!"; 
	// ss << "Welcome to grid layout example of Rose!";
	gui2::show_message(disp_.video(), "Hello World", ss.str(), tmessage::ok_cancel_buttons);

	tlistbox& list = find_widget<tlistbox>(window_, "listbox", false);
	list.invalidate_layout();
}

void tsimple_item_selector::post_show(twindow& window)
{
	if(get_retval() != twindow::OK) {
		return;
	}

	tlistbox& list = find_widget<tlistbox>(&window, "listbox", false);
	ttoggle_panel* selected = list.cursel();
	index_ = selected? selected->at(): twidget::npos;
}

}
