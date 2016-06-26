/* $Id: mp_login.cpp 50955 2011-08-30 19:41:15Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/dialogs/edit_box.hpp"

#include "display.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/text_box2.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/scroll_label.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/message.hpp"
#include <hero.hpp>
#include "gettext.hpp"
#include "formula_string_utils.hpp"

#include <boost/bind.hpp>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_mp_login
 *
 * == Multiplayer connect ==
 *
 * This shows the dialog to log in to the MP server
 *
 * @begin{table}{dialog_widgets}
 *
 * user_name & & text_box & m &
 *         The login user name. $
 *
 * password & & text_box & m &
 *         The password. $
 *
 * remember_password & & toggle_button & o &
 *         A toggle button to offer to remember the password in the
 *         preferences. $
 *
 * password_reminder & & button & o &
 *         Request a password reminder. $
 *
 * change_username & & button & o &
 *         Use a different username. $
 *
 * login_label & & button & o &
 *         Displays the information received from the server. $
 *
 * @end{table}
 */

REGISTER_DIALOG(rose, edit_box)

tedit_box::tedit_box(display& disp, tparam& param)
	: disp_(disp)
	, param_(param)
{
}

void tedit_box::pre_show(CVideo& /*video*/, twindow& window)
{
	window.set_canvas_variable("border", variant("default-border"));

	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	if (!param_.title.empty()) {
		label->set_label(param_.title);
	}
	if (!param_.remark.empty()) {
		label = find_widget<tlabel>(&window, "remark", false, true);
		label->set_label(param_.remark);
	}

	ok_ = find_widget<tbutton>(&window, "ok", false, true);
	if (!param_.ok.empty()) {
		ok_->set_label(param_.ok);
	}

	ttext_box2* user_widget = find_widget<ttext_box2>(&window, "txt", false, true);
	window.keyboard_capture(&user_widget->text_box());

	user_widget->text_box().set_canvas_variable("cursor_color", variant(0x000000ff));
	user_widget->text_box().set_placeholder(param_.placeholder);
	if (param_.max_length != -1) {
		user_widget->text_box().set_maximum_chars(param_.max_length);
	}
	user_widget->set_text_changed_callback(boost::bind(&tedit_box::text_changed_callback, this, _1));
	user_widget->text_box().set_value(param_.def);
}

void tedit_box::post_show(twindow& window)
{
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	SDL_StopTextInput();
#endif
}

void tedit_box::text_changed_callback(ttext_box2* widget)
{
	param_.result = widget->label();
	bool active = !param_.result.empty();
	if (active && param_.text_changed) {
		active = param_.text_changed(param_.result);
	}
	ok_->set_active(active);
}

} // namespace gui2

