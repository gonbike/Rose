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

#define GETTEXT_DOMAIN "studio-lib"

#include "gui/dialogs/grid_setting.hpp"

#include "display.hpp"
#include "formula_string_utils.hpp"
#include "gettext.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/scroll_text_box.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/report.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/message.hpp"
#include "unit_map.hpp"
#include "mkwin_controller.hpp"

#include <boost/bind.hpp>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_campaign_difficulty
 *
 * == Campaign difficulty ==
 *
 * The campaign mode difficulty menu.
 *
 * @begin{table}{dialog_widgets}
 *
 * title & & label & m &
 *         Dialog title label. $
 *
 * message & & scroll_label & o &
 *         Text label displaying a description or instructions. $
 *
 * listbox & & listbox & m &
 *         Listbox displaying user choices, defined by WML for each campaign. $
 *
 * -icon & & control & m &
 *         Widget which shows a listbox item icon, first item markup column. $
 *
 * -label & & control & m &
 *         Widget which shows a listbox item label, second item markup column. $
 *
 * -description & & control & m &
 *         Widget which shows a listbox item description, third item markup column. $
 *
 * @end{table}
 */

REGISTER_DIALOG(studio, grid_setting)

tgrid_setting::tgrid_setting(mkwin_controller& controller, display& disp, unit_map& units, unit& u, const std::string& control)
	: tsetting_dialog(u.cell())
	, controller_(controller)
	, disp_(disp)
	, units_(units)
	, u_(u)
	, control_(control)
{
}

void tgrid_setting::pre_show(CVideo& /*video*/, twindow& window)
{
	window.set_canvas_variable("border", variant("default-border"));

	std::stringstream ss;
	const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget = u_.widget();

	ss << "grid setting";
	ss << "(";
	ss << tintegrate::generate_format(control_, "blue");
	ss << ")";
	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	label->set_label(ss.str());

	ttext_box* text_box = find_widget<ttext_box>(&window, "_id", false, true);
	text_box->set_maximum_chars(max_id_len);
	text_box->set_label(cell_.id);
	window.keyboard_capture(text_box);

	if (u_.parent().u->is_stack()) {
		text_box = find_widget<ttext_box>(&window, "_grow_factor", false, true);
		text_box->set_label(str_cast(cell_.grid.grow_factor));

	} else {
		find_widget<tgrid>(&window, "_grow_factor_grid", false, true)->set_visible(twidget::INVISIBLE);
	}

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "save", false)
			, boost::bind(
				&tgrid_setting::save
				, this
				, boost::ref(window)
				, true));
}

void tgrid_setting::save(twindow& window, bool exit)
{
	ttext_box* text_box = find_widget<ttext_box>(&window, "_id", false, true);
	cell_.id = text_box->label();
	// id maybe empty.
	if (!cell_.id.empty() && !utils::isvalid_id(cell_.id, false, min_id_len, max_id_len)) {
		gui2::show_message(disp_.video(), "", utils::errstr);
		return ;
	}
	utils::lowercase2(cell_.id);

	if (u_.parent().u->is_stack()) {
		text_box = find_widget<ttext_box>(&window, "_grow_factor", false, true);
		if (!utils::isinteger(text_box->label().c_str())) {
			return;
		}
		int grow_factor = lexical_cast_default<int>(text_box->label());
		if (grow_factor < 0 || grow_factor > 9) {
			return;
		}
		cell_.grid.grow_factor = grow_factor;
	}

	if (exit) {
		window.set_retval(twindow::OK);
	}
}

}
