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

#include "gui/dialogs/control_setting.hpp"

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
#include "gui/widgets/stack.hpp"
#include "gui/widgets/report.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/auxiliary/window_builder.hpp"
#include "unit.hpp"
#include "mkwin_controller.hpp"
#include "theme.hpp"
#include <cctype>

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_DIALOG(studio, control_setting)

const std::string untitled = "_untitled";

void show_id_error(display& disp, const std::string& id, const std::string& errstr)
{
	std::stringstream err;
	utils::string_map symbols;

	symbols["id"] = id;
	err << vgettext2("Invalid '$id' value!", symbols);
	err << "\n\n" << errstr;
	gui2::show_message(disp.video(), "", err.str());
}

bool verify_formula(display& disp, const std::string& id, const std::string& expression2)
{
	if (expression2.empty()) {
		show_id_error(disp, id, _("Must not empty."));
		return false;
	}
	try {
		std::string expression = "(" + expression2 + ")";
		game_logic::formula f(expression, nullptr);

	} catch (game_logic::formula_error& e)	{
		std::stringstream err;
		utils::string_map symbols;

		symbols["id"] = id;
		err << vgettext2("Error $id expression!", symbols);
		err << "\n\n" << e.what();
		gui2::show_message(disp.video(), "", err.str());
		return false;
	}
	return true;
}

std::map<int, tlayout> horizontal_layout;
std::map<int, tlayout> vertical_layout;

std::map<int, tscroll_mode> horizontal_mode;
std::map<int, tscroll_mode> vertical_mode;

std::map<int, tparam3> orientations;

void init_layout_mode()
{
	if (horizontal_layout.empty()) {
		horizontal_layout.insert(std::make_pair(tgrid::HORIZONTAL_GROW_SEND_TO_CLIENT, 
			tlayout(tgrid::HORIZONTAL_GROW_SEND_TO_CLIENT, _("Align left. Maybe stretch"))));
		horizontal_layout.insert(std::make_pair(tgrid::HORIZONTAL_ALIGN_LEFT, 
			tlayout(tgrid::HORIZONTAL_ALIGN_LEFT, _("Align left"))));
		horizontal_layout.insert(std::make_pair(tgrid::HORIZONTAL_ALIGN_CENTER, 
			tlayout(tgrid::HORIZONTAL_ALIGN_CENTER, _("Align center"))));
		horizontal_layout.insert(std::make_pair(tgrid::HORIZONTAL_ALIGN_RIGHT, 
			tlayout(tgrid::HORIZONTAL_ALIGN_RIGHT, _("Align right"))));
	}

	if (vertical_layout.empty()) {
		vertical_layout.insert(std::make_pair(tgrid::VERTICAL_GROW_SEND_TO_CLIENT, 
			tlayout(tgrid::VERTICAL_GROW_SEND_TO_CLIENT, _("Align top. Maybe stretch"))));
		vertical_layout.insert(std::make_pair(tgrid::VERTICAL_ALIGN_TOP, 
			tlayout(tgrid::VERTICAL_ALIGN_TOP, _("Align top"))));
		vertical_layout.insert(std::make_pair(tgrid::VERTICAL_ALIGN_CENTER, 
			tlayout(tgrid::VERTICAL_ALIGN_CENTER, _("Align center"))));
		vertical_layout.insert(std::make_pair(tgrid::VERTICAL_ALIGN_BOTTOM, 
			tlayout(tgrid::VERTICAL_ALIGN_BOTTOM, _("Align bottom"))));
	}

	if (horizontal_mode.empty()) {
		horizontal_mode.insert(std::make_pair(tscroll_container::always_invisible, 
			tscroll_mode(tscroll_container::always_invisible, _("Always invisible"))));
		horizontal_mode.insert(std::make_pair(tscroll_container::auto_visible, 
			tscroll_mode(tscroll_container::auto_visible, _("Auto visible"))));
	}

	if (vertical_mode.empty()) {
		vertical_mode.insert(std::make_pair(tscroll_container::always_invisible, 
			tscroll_mode(tscroll_container::always_invisible, _("Always invisible"))));
		vertical_mode.insert(std::make_pair(tscroll_container::auto_visible, 
			tscroll_mode(tscroll_container::auto_visible, _("Auto visible"))));
	}

	std::stringstream err;
	utils::string_map symbols;
	
	if (orientations.empty()) {
		orientations.insert(std::make_pair(twidget::auto_orientation, tparam3(twidget::auto_orientation, "auto", _("Auto"))));
		orientations.insert(std::make_pair(twidget::landscape_orientation, tparam3(twidget::landscape_orientation, "landscape", _("Landscape"))));
		orientations.insert(std::make_pair(twidget::portrait_orientation, tparam3(twidget::portrait_orientation, "portrait", _("Portrait"))));
	}
}

tcontrol_setting::tcontrol_setting(display& disp, mkwin_controller& controller, unit& u, const std::vector<std::string>& textdomains, const std::vector<tlinked_group>& linkeds, bool float_widget)
	: tsetting_dialog(u.cell())
	, disp_(disp)
	, controller_(controller)
	, u_(u)
	, textdomains_(textdomains)
	, linkeds_(linkeds)
	, float_widget_(float_widget)
	, bar_(NULL)
{
}

void tcontrol_setting::update_title(twindow& window)
{
	std::stringstream ss;
	const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget = u_.widget();

	ss.str("");
	if (!u_.cell().id.empty()) {
		ss << tintegrate::generate_format(u_.cell().id, "green"); 
	} else {
		ss << tintegrate::generate_format("---", "green"); 
	}
	ss << "    ";

	ss << widget.first;
	if (widget.second.get()) {
		ss << "(" << tintegrate::generate_format(widget.second->id, "blue") << ")"; 
	}
	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	label->set_label(ss.str());
}

void tcontrol_setting::pre_show(CVideo& /*video*/, twindow& window)
{
	window.set_canvas_variable("border", variant("default-border"));

	update_title(window);

	// prepare navigate bar.
	std::vector<std::string> labels;
	labels.push_back(_("Base"));
	labels.push_back(_("Size"));
	labels.push_back(_("Advanced"));

	bar_ = find_widget<treport>(&window, "bar", false, true);
	bar_->set_did_item_pre_change(boost::bind(&tcontrol_setting::did_item_pre_change_report, this, _1, _2, _3));
	bar_->set_did_item_changed(boost::bind(&tcontrol_setting::did_item_changed_report, this, _1, _2));
	bar_->set_boddy(find_widget<twidget>(&window, "bar_panel", false, true));
	int index = 0;
	for (std::vector<std::string>::const_iterator it = labels.begin(); it != labels.end(); ++ it) {
		tcontrol& widget = bar_->create_child(null_str, null_str, index ++);
		widget.set_label(*it);
		bar_->insert_child(widget);
	}

	page_panel_ = find_widget<tstack>(&window, "panel", false, true);
	page_panel_->set_radio_layer(BASE_PAGE);
	
	pre_base(window);

	if (u_.has_size() || u_.has_restrict_width()) {
		pre_size(window);
	} else {
		bar_->set_child_visible(SIZE_PAGE, false);
		page_panel_->layer(SIZE_PAGE)->set_visible(twidget::INVISIBLE);
	}

	if (u_.widget().second.get()) {
		pre_advanced(window);
	} else {
		bar_->set_child_visible(ADVANCED_PAGE, false);
		page_panel_->layer(ADVANCED_PAGE)->set_visible(twidget::INVISIBLE);
	}

	// until
	bar_->select(BASE_PAGE);

	connect_signal_mouse_left_click(
		 find_widget<tbutton>(&window, "save", false)
		, boost::bind(
			&tcontrol_setting::save
			, this
			, boost::ref(window)
			, _3, _4));
}

void tcontrol_setting::pre_base(twindow& window)
{
	// horizontal layout
	set_horizontal_layout_label(window);
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "_set_horizontal_layout", false)
			, boost::bind(
				&tcontrol_setting::set_horizontal_layout
				, this
				, boost::ref(window)));

	// vertical layout
	set_vertical_layout_label(window);
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "_set_vertical_layout", false)
			, boost::bind(
				&tcontrol_setting::set_vertical_layout
				, this
				, boost::ref(window)));

	// linked_group
	set_linked_group_label(window);
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "_set_linked_group", false)
			, boost::bind(
				&tcontrol_setting::set_linked_group
				, this
				, boost::ref(window)));

	// linked_group
	set_textdomain_label(window, true);
	set_textdomain_label(window, false);
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "textdomain_label", false)
			, boost::bind(
				&tcontrol_setting::set_textdomain
				, this
				, boost::ref(window)
				, true));
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "textdomain_tooltip", false)
			, boost::bind(
				&tcontrol_setting::set_textdomain
				, this
				, boost::ref(window)
				, false));

	// border size
	ttext_box* text_box = find_widget<ttext_box>(&window, "_border", false, true);
	text_box->set_label(str_cast(cell_.widget.cell.border_size_));
	if (cell_.widget.cell.flags_ & tgrid::BORDER_LEFT) {
		find_widget<ttoggle_button>(&window, "_border_left", false, true)->set_value(true);
	}
	if (cell_.widget.cell.flags_ & tgrid::BORDER_RIGHT) {
		find_widget<ttoggle_button>(&window, "_border_right", false, true)->set_value(true);
	}
	if (cell_.widget.cell.flags_ & tgrid::BORDER_TOP) {
		find_widget<ttoggle_button>(&window, "_border_top", false, true)->set_value(true);
	}
	if (cell_.widget.cell.flags_ & tgrid::BORDER_BOTTOM) {
		find_widget<ttoggle_button>(&window, "_border_bottom", false, true)->set_value(true);
	}

	text_box = find_widget<ttext_box>(&window, "_id", false, true);
	text_box->set_maximum_chars(max_id_len);
	text_box->set_label(cell_.id);
	if (float_widget_) {
		VALIDATE(!cell_.id.empty(), null_str);
		text_box->set_active(false);
	}

	find_widget<tscroll_text_box>(&window, "_label", false, true)->set_label(cell_.widget.label);
	find_widget<tscroll_text_box>(&window, "_tooltip", false, true)->set_label(cell_.widget.tooltip);

	if (controller_.in_theme_top()) {
		find_widget<tbutton>(&window, "_set_horizontal_layout", false).set_active(false);
		find_widget<tbutton>(&window, "_set_vertical_layout", false).set_active(false);
		find_widget<ttext_box>(&window, "_border", false).set_active(false);
		find_widget<ttoggle_button>(&window, "_border_left", false).set_active(false);
		find_widget<ttoggle_button>(&window, "_border_right", false).set_active(false);
		find_widget<ttoggle_button>(&window, "_border_top", false).set_active(false);
		find_widget<ttoggle_button>(&window, "_border_bottom", false).set_active(false);
		find_widget<tbutton>(&window, "_set_linked_group", false).set_active(false);

		bool readonly_id = u_.is_main_map();
		find_widget<ttext_box>(&window, "_id", false).set_active(!readonly_id);

		find_widget<tbutton>(&window, "textdomain_label", false).set_active(false);
		find_widget<tscroll_text_box>(&window, "_label", false).set_active(false);
		find_widget<tbutton>(&window, "textdomain_tooltip", false).set_active(false);
		find_widget<tscroll_text_box>(&window, "_tooltip", false).set_active(false);

	}
}

void tcontrol_setting::pre_rect(twindow& window)
{
}

void tcontrol_setting::pre_size(twindow& window)
{
	tgrid* size_page = page_panel_->layer(SIZE_PAGE);

	find_widget<tgrid>(size_page, "_size_grid", false, true)->set_visible(u_.has_size()? twidget::VISIBLE: twidget::INVISIBLE);
	if (u_.has_size()) {
		if (u_.has_size_is_max()) {
			connect_signal_mouse_left_click(
				  find_widget<tbutton>(&window, "size_is_max", false)
				, boost::bind(
					&tcontrol_setting::size_is_max
					, this
					, boost::ref(window)));
			set_size_is_max_label(window);
		} else {
			find_widget<tbutton>(&window, "size_is_max", false).set_visible(twidget::INVISIBLE);
		}

		find_widget<tscroll_text_box>(size_page, "_width", false, true)->set_label(cell_.widget.width);
		find_widget<tscroll_text_box>(size_page, "_height", false, true)->set_label(cell_.widget.height);
	}

	find_widget<tgrid>(size_page, "_restrict_width_grid", false, true)->set_visible(u_.has_restrict_width()? twidget::VISIBLE: twidget::INVISIBLE);
	if (u_.has_restrict_width()) {
		find_widget<ttoggle_button>(size_page, "restrict_width", false, true)->set_value(cell_.widget.restrict_width);
	}
}

void tcontrol_setting::pre_advanced(twindow& window)
{
	tgrid* advanced_page = page_panel_->layer(ADVANCED_PAGE);

	std::stringstream ss;
	const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget = u_.widget();

	tlabel* label;
	ttext_box* text_box;
	if (!u_.is_tree_view()) {
		tgrid* grid = find_widget<tgrid>(&window, "_grid_tree_view", false, true);
		grid->set_visible(twidget::INVISIBLE);

	} else {
		text_box = find_widget<ttext_box>(&window, "indention_step_size", false, true);
		text_box->set_label(str_cast(cell_.widget.tree_view.indention_step_size));
		text_box = find_widget<ttext_box>(&window, "node_id", false, true);
		text_box->set_label(cell_.widget.tree_view.node_id);
	}

	if (!u_.is_slider()) {
		tgrid* grid = find_widget<tgrid>(&window, "_grid_slider", false, true);
		grid->set_visible(twidget::INVISIBLE);

	} else {
		text_box = find_widget<ttext_box>(&window, "minimum_value", false, true);
		text_box->set_label(str_cast(cell_.widget.slider.minimum_value));
		text_box = find_widget<ttext_box>(&window, "maximum_value", false, true);
		text_box->set_label(str_cast(cell_.widget.slider.maximum_value));
		text_box = find_widget<ttext_box>(&window, "step_size", false, true);
		text_box->set_label(str_cast(cell_.widget.slider.step_size));
	}

	if (!u_.is_text_box2()) {
		tgrid* grid = find_widget<tgrid>(&window, "_grid_text_box2", false, true);
		grid->set_visible(twidget::INVISIBLE);

	} else {
		connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "set_text_box2_text_box", false)
			, boost::bind(
				&tcontrol_setting::set_text_box2_text_box
				, this
				, boost::ref(window)));
		label = find_widget<tlabel>(&window, "text_box2_text_box", false, true);
		label->set_label(cell_.widget.text_box2.text_box.empty()? "default": cell_.widget.text_box2.text_box);
	}

	if (!u_.is_report()) {
		tgrid* grid = find_widget<tgrid>(&window, "_grid_report", false, true);
		grid->set_visible(twidget::INVISIBLE);

	} else {
		connect_signal_mouse_left_click(
			  find_widget<tbutton>(advanced_page, "multi_line", false)
			, boost::bind(
				&tcontrol_setting::set_multi_line
				, this
				, boost::ref(window)));
		set_multi_line_label(window);

		tbutton* button = find_widget<tbutton>(advanced_page, "report_toggle", false, true);
		connect_signal_mouse_left_click(
			  find_widget<tbutton>(advanced_page, "report_toggle", false)
			, boost::bind(
				&tcontrol_setting::set_report_toggle
				, this
				, boost::ref(window)));
		button->set_label(cell_.widget.report.toggle? _("Toggle button"): _("Button"));

		connect_signal_mouse_left_click(
			  find_widget<tbutton>(advanced_page, "report_definition", false)
			, boost::bind(
				&tcontrol_setting::set_report_definition
				, this
				, boost::ref(window)));
		set_report_definition_label(window);

		

		ttext_box* text_box = find_widget<ttext_box>(&window, "unit_width", false, true);
		text_box->set_placeholder(_("Usable variable: screen_width, screen_height"));
		text_box->set_label(cell_.widget.report.unit_width);

		text_box = find_widget<ttext_box>(&window, "unit_height", false, true);
		text_box->set_placeholder(_("Usable variable: screen_width, screen_height"));
		text_box->set_label(cell_.widget.report.unit_height);

		text_box = find_widget<ttext_box>(&window, "gap", false, true);
		text_box->set_label(str_cast(cell_.widget.report.gap));
	}

	if (u_.is_scroll()) {
		// horizontal mode
		set_horizontal_mode_label(window);
		connect_signal_mouse_left_click(
				  find_widget<tbutton>(&window, "_set_horizontal_mode", false)
				, boost::bind(
					&tcontrol_setting::set_horizontal_mode
					, this
					, boost::ref(window)));

		// vertical layout
		set_vertical_mode_label(window);
		connect_signal_mouse_left_click(
				  find_widget<tbutton>(&window, "_set_vertical_mode", false)
				, boost::bind(
					&tcontrol_setting::set_vertical_mode
					, this
					, boost::ref(window)));
	} else {
		find_widget<tgrid>(&window, "_grid_scrollbar", false).set_visible(twidget::INVISIBLE);
	}

	if (u_.has_gc()) {
		ttoggle_button* toggle = find_widget<ttoggle_button>(advanced_page, "gc", false, true);
		toggle->set_value(cell_.widget.gc);

	} else {
		find_widget<tgrid>(advanced_page, "_grid_gc", false).set_visible(twidget::INVISIBLE);
	}

	if (u_.has_drag()) {
		ttoggle_button* toggle = find_widget<ttoggle_button>(advanced_page, "_drag_left", false, true);
		toggle->set_value(cell_.widget.drag & twidget::drag_left);
		toggle = find_widget<ttoggle_button>(advanced_page, "_drag_right", false, true);
		toggle->set_value(cell_.widget.drag & twidget::drag_right);
		toggle = find_widget<ttoggle_button>(advanced_page, "_drag_up", false, true);
		toggle->set_value(cell_.widget.drag & twidget::drag_up);
		toggle = find_widget<ttoggle_button>(advanced_page, "_drag_down", false, true);
		toggle->set_value(cell_.widget.drag & twidget::drag_down);

	} else {
		find_widget<tgrid>(advanced_page, "_grid_drag", false).set_visible(twidget::INVISIBLE);
	}

	if (u_.is_stack()) {
		find_widget<tbutton>(advanced_page, "set_stack_mode", false).set_label(gui2::implementation::form_stack_mode_str(cell_.widget.stack.mode));
		connect_signal_mouse_left_click(
				  find_widget<tbutton>(&window, "set_stack_mode", false)
				, boost::bind(
					&tcontrol_setting::set_stack_mode
					, this
					, boost::ref(window)));
	} else {
		find_widget<tgrid>(advanced_page, "_grid_stacked_widget", false).set_visible(twidget::INVISIBLE);
	}

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "_set_definition", false)
			, boost::bind(
				&tcontrol_setting::set_definition
				, this
				, boost::ref(window)));

	if (u_.widget().second->text_font_size) {
		connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "_set_text_font_size", false)
			, boost::bind(
				&tcontrol_setting::set_text_font_size
				, this
				, boost::ref(window)));
		set_text_font_size_label(window);

		connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "_set_text_color_tpl", false)
			, boost::bind(
				&tcontrol_setting::set_text_color_tpl
				, this
				, boost::ref(window)));
		set_text_color_tpl_label(window);

	} else {
		find_widget<tbutton>(&window, "_set_text_font_size", false).set_visible(twidget::INVISIBLE);
		find_widget<tlabel>(&window, "_text_font_size", false).set_visible(twidget::INVISIBLE);
		find_widget<tbutton>(&window, "_set_text_color_tpl", false).set_visible(twidget::INVISIBLE);
	}

	set_advanced_misc(window);
}

bool tcontrol_setting::did_item_pre_change_report(treport& report, ttoggle_button& widget, ttoggle_button* previous)
{
	if (!previous) {
		return true;
	}

	twindow& window = *widget.get_window();
	bool ret = true;
	int previous_page = (int)previous->cookie();
	if (previous_page == BASE_PAGE) {
		ret = save_base(window);
	} else if (previous_page == SIZE_PAGE) {
		ret = save_size(window);
	} else if (previous_page == ADVANCED_PAGE) {
		ret = save_advanced(window);
	}
	return ret;
}

void tcontrol_setting::did_item_changed_report(treport& report, ttoggle_button& widget)
{
	int page = (int)widget.cookie();
	page_panel_->set_radio_layer(page);
}

void tcontrol_setting::save(twindow& window, bool& handled, bool& halt)
{
	bool ret = true;
	int current_page = (int)bar_->cursel()->cookie();
	if (current_page == BASE_PAGE) {
		ret = save_base(window);
	} else if (current_page == SIZE_PAGE) {
		ret = save_size(window);
	} else if (current_page == ADVANCED_PAGE) {
		ret = save_advanced(window);
	}
	if (!ret) {
		handled = true;
		halt = true;
		return;
	}
	window.set_retval(twindow::OK);
}

bool tcontrol_setting::save_base(twindow& window)
{
	ttext_box* text_box = find_widget<ttext_box>(&window, "_border", false, true);
	int border = lexical_cast_default<int>(text_box->label());
	if (border < 0 || border > 50) {
		return false;
	}
	cell_.widget.cell.border_size_ = border;

	ttoggle_button* toggle = find_widget<ttoggle_button>(&window, "_border_left", false, true);
	if (toggle->get_value()) {
		cell_.widget.cell.flags_ |= tgrid::BORDER_LEFT;
	} else {
		cell_.widget.cell.flags_ &= ~tgrid::BORDER_LEFT;
	}
	toggle = find_widget<ttoggle_button>(&window, "_border_right", false, true);
	if (toggle->get_value()) {
		cell_.widget.cell.flags_ |= tgrid::BORDER_RIGHT;
	} else {
		cell_.widget.cell.flags_ &= ~tgrid::BORDER_RIGHT;
	}
	toggle = find_widget<ttoggle_button>(&window, "_border_top", false, true);
	if (toggle->get_value()) {
		cell_.widget.cell.flags_ |= tgrid::BORDER_TOP;
	} else {
		cell_.widget.cell.flags_ &= ~tgrid::BORDER_TOP;
	}
	toggle = find_widget<ttoggle_button>(&window, "_border_bottom", false, true);
	if (toggle->get_value()) {
		cell_.widget.cell.flags_ |= tgrid::BORDER_BOTTOM;
	} else {
		cell_.widget.cell.flags_ &= ~tgrid::BORDER_BOTTOM;
	}

	text_box = find_widget<ttext_box>(&window, "_id", false, true);
	cell_.id = text_box->label();
	// id maybe empty.
	if (!cell_.id.empty() && !utils::isvalid_id(cell_.id, false, min_id_len, max_id_len)) {
		gui2::show_message(disp_.video(), "", utils::errstr);
		return false;
	}
	utils::lowercase2(cell_.id);

	tscroll_text_box* scroll_text_box = find_widget<tscroll_text_box>(&window, "_label", false, true);
	cell_.widget.label = scroll_text_box->label();
	scroll_text_box = find_widget<tscroll_text_box>(&window, "_tooltip", false, true);
	cell_.widget.tooltip = scroll_text_box->label();

	return true;
}

bool tcontrol_setting::save_size(twindow& window)
{
	tgrid* size_page = page_panel_->layer(SIZE_PAGE);

	if (u_.has_size()) {
		tscroll_text_box* scroll_text_box = find_widget<tscroll_text_box>(size_page, "_width", false, true);
		cell_.widget.width = scroll_text_box->label();
		scroll_text_box = find_widget<tscroll_text_box>(size_page, "_height", false, true);
		cell_.widget.height = scroll_text_box->label();
	}
	if (u_.has_restrict_width()) {
		cell_.widget.restrict_width = find_widget<ttoggle_button>(size_page, "restrict_width", false, true)->get_value();
	}
	return true;
}

bool tcontrol_setting::save_advanced(twindow& window)
{
	tgrid* advanced_page = page_panel_->layer(ADVANCED_PAGE);

	if (u_.is_tree_view()) {
		ttext_box* text_box = find_widget<ttext_box>(&window, "indention_step_size", false, true);
		cell_.widget.tree_view.indention_step_size = lexical_cast_default<int>(text_box->label());
		text_box = find_widget<ttext_box>(&window, "node_id", false, true);
		cell_.widget.tree_view.node_id = text_box->get_value();
		if (cell_.widget.tree_view.node_id.empty()) {
			show_id_error(disp_, "node's id", _("Can not empty!"));
			return false;
		}
	}

	if (u_.is_slider()) {
		ttext_box* text_box = find_widget<ttext_box>(&window, "minimum_value", false, true);
		cell_.widget.slider.minimum_value = lexical_cast_default<int>(text_box->label());
		text_box = find_widget<ttext_box>(&window, "maximum_value", false, true);
		cell_.widget.slider.maximum_value = lexical_cast_default<int>(text_box->label());
		if (cell_.widget.slider.minimum_value >= cell_.widget.slider.maximum_value) {
			show_id_error(disp_, "Minimum or Maximum value", _("Maximum value must large than minimum value!"));
			return false;
		}
		text_box = find_widget<ttext_box>(&window, "step_size", false, true);
		cell_.widget.slider.step_size = lexical_cast_default<int>(text_box->label());
		if (cell_.widget.slider.step_size <= 0) {
			show_id_error(disp_, "Step size", _("Step size must > 0!"));
			return false;
		}
		if (cell_.widget.slider.step_size >= cell_.widget.slider.maximum_value - cell_.widget.slider.minimum_value) {
			show_id_error(disp_, "Step size", _("Step size must < maximum_value - minimum_value!"));
			return false;
		}
	}

	if (u_.is_report()) {
		if (cell_.widget.report.multi_line) {
			ttext_box* text_box = find_widget<ttext_box>(&window, "report_fixed_cols", false, true);
			cell_.widget.report.fixed_cols = lexical_cast_default<int>(text_box->label());
			if (cell_.widget.report.fixed_cols < 0) {
				show_id_error(disp_, _("Fixed clos"), _("must not be less than 0!"));
				return false;
			}

		} else {
			cell_.widget.report.segment_switch = find_widget<ttoggle_button>(&window, "report_segment_switch", false, true)->get_value();
		}

		ttext_box* text_box = find_widget<ttext_box>(&window, "unit_width", false, true);
		cell_.widget.report.unit_width = text_box->label();
		utils::lowercase2(cell_.widget.report.unit_width);
		if (!verify_formula(disp_, "unit_width's formula", cell_.widget.report.unit_width)) {
			return false;
		}
		
		text_box = find_widget<ttext_box>(&window, "unit_height", false, true);
		cell_.widget.report.unit_height = text_box->label();
		utils::lowercase2(cell_.widget.report.unit_height);
		if (!verify_formula(disp_, "unit_height's formula", cell_.widget.report.unit_height)) {
			return false;
		}
		text_box = find_widget<ttext_box>(&window, "gap", false, true);
		int gap = lexical_cast_default<int>(text_box->label());
		if (gap < 0) {
			show_id_error(disp_, _("Gap"), _("must not be less than 0!"));
			return false;
		}

		cell_.widget.report.gap = gap;
	}

	if (u_.has_gc()) {
		ttoggle_button* toggle = find_widget<ttoggle_button>(advanced_page, "gc", false, true);
		cell_.widget.gc = toggle->get_value();
	}

	if (u_.has_drag()) {
		cell_.widget.drag = 0;
		ttoggle_button* toggle = find_widget<ttoggle_button>(&window, "_drag_left", false, true);
		if (toggle->get_value()) {
			cell_.widget.drag |= twidget::drag_left;
		}
		toggle = find_widget<ttoggle_button>(&window, "_drag_right", false, true);
		if (toggle->get_value()) {
			cell_.widget.drag |= twidget::drag_right;
		}
		toggle = find_widget<ttoggle_button>(&window, "_drag_up", false, true);
		if (toggle->get_value()) {
			cell_.widget.drag |= twidget::drag_up;
		}
		toggle = find_widget<ttoggle_button>(&window, "_drag_down", false, true);
		if (toggle->get_value()) {
			cell_.widget.drag |= twidget::drag_down;
		}
	}

	return true;
}

void tcontrol_setting::set_horizontal_layout(twindow& window)
{
	std::stringstream ss;
	std::vector<tval_str> items;

	for (std::map<int, tlayout>::const_iterator it = horizontal_layout.begin(); it != horizontal_layout.end(); ++ it) {
		ss.str("");
		ss << tintegrate::generate_img(it->second.icon + "~SCALE(24, 24)") << it->second.description;
		items.push_back(tval_str(it->first, ss.str()));
	}

	unsigned h_flag = cell_.widget.cell.flags_ & tgrid::HORIZONTAL_MASK;
	gui2::tcombo_box dlg(items, h_flag);
	dlg.show(disp_.video());

	h_flag = dlg.selected_val();
	cell_.widget.cell.flags_ = (cell_.widget.cell.flags_ & ~tgrid::HORIZONTAL_MASK) | h_flag;

	set_horizontal_layout_label(window);
}

void tcontrol_setting::set_vertical_layout(twindow& window)
{
	std::stringstream ss;
	std::vector<tval_str> items;

	for (std::map<int, tlayout>::const_iterator it = vertical_layout.begin(); it != vertical_layout.end(); ++ it) {
		ss.str("");
		ss << tintegrate::generate_img(it->second.icon + "~SCALE(24, 24)") << it->second.description;
		items.push_back(tval_str(it->first, ss.str()));
	}

	unsigned h_flag = cell_.widget.cell.flags_ & tgrid::VERTICAL_MASK;
	gui2::tcombo_box dlg(items, h_flag);
	dlg.show(disp_.video());

	h_flag = dlg.selected_val();
	cell_.widget.cell.flags_ = (cell_.widget.cell.flags_ & ~tgrid::VERTICAL_MASK) | h_flag;

	set_vertical_layout_label(window);
}

void tcontrol_setting::set_horizontal_layout_label(twindow& window)
{
	const unsigned h_flag = cell_.widget.cell.flags_ & tgrid::HORIZONTAL_MASK;
	std::stringstream ss;

	ss << horizontal_layout.find(h_flag)->second.description;
	tlabel* label = find_widget<tlabel>(&window, "_horizontal_layout", false, true);
	label->set_label(ss.str());
}

void tcontrol_setting::set_vertical_layout_label(twindow& window)
{
	const unsigned v_flag = cell_.widget.cell.flags_ & tgrid::VERTICAL_MASK;
	std::stringstream ss;

	ss << vertical_layout.find(v_flag)->second.description;
	tlabel* label = find_widget<tlabel>(&window, "_vertical_layout", false, true);
	label->set_label(ss.str());
}

void tcontrol_setting::set_linked_group(twindow& window)
{
	std::stringstream ss;
	std::vector<tval_str> items;

	int index = -1;
	int def = -1;
	items.push_back(tval_str(index ++, ""));
	for (std::vector<tlinked_group>::const_iterator it = linkeds_.begin(); it != linkeds_.end(); ++ it) {
		const tlinked_group& linked = *it;
		ss.str("");
		ss << linked.id;
		if (linked.fixed_width) {
			ss << "  " << tintegrate::generate_format("width", "blue");
		}
		if (linked.fixed_height) {
			ss << "  " << tintegrate::generate_format("height", "blue");
		}
		if (cell_.widget.linked_group == linked.id) {
			def = index;
		}
		items.push_back(tval_str(index ++, ss.str()));
	}

	gui2::tcombo_box dlg(items, def);
	dlg.show(disp_.video());

	index = dlg.selected_val();
	if (index >= 0) {
		cell_.widget.linked_group = linkeds_[index].id;
	} else {
		cell_.widget.linked_group.clear();
	}

	set_linked_group_label(window);
}

void tcontrol_setting::set_linked_group_label(twindow& window)
{
	std::stringstream ss;

	ss << cell_.widget.linked_group;
	ttext_box* text_box = find_widget<ttext_box>(&window, "_linked_group", false, true);
	text_box->set_label(ss.str());
	text_box->set_active(false);

	if (linkeds_.empty()) {
		find_widget<tbutton>(&window, "_set_linked_group", false, true)->set_active(false);
	}
}

void tcontrol_setting::set_textdomain(twindow& window, bool label)
{
	std::stringstream ss;
	std::vector<tval_str> items;

	std::string& textdomain = label? cell_.widget.label_textdomain: cell_.widget.tooltip_textdomain;
	
	int index = -1;
	int def = -1;
	items.push_back(tval_str(index ++, ""));
	for (std::vector<std::string>::const_iterator it = textdomains_.begin(); it != textdomains_.end(); ++ it) {
		ss.str("");
		ss << *it;
		if (*it == textdomain) {
			def = index;
		}
		items.push_back(tval_str(index ++, ss.str()));
	}

	gui2::tcombo_box dlg(items, def);
	dlg.show(disp_.video());

	index = dlg.selected_val();
	if (index >= 0) {
		textdomain = textdomains_[index];
	} else {
		textdomain.clear();
	}

	set_textdomain_label(window, label);
}

void tcontrol_setting::set_textdomain_label(twindow& window, bool label)
{
	std::stringstream ss;

	const std::string id = label? "textdomain_label": "textdomain_tooltip";
	std::string str = label? cell_.widget.label_textdomain: cell_.widget.tooltip_textdomain;
	if (str.empty()) {
		str = "---";
	}

	tbutton* button = find_widget<tbutton>(&window, id, false, true);
	button->set_label(str);

	if (textdomains_.empty()) {
		button->set_active(false);
	}
}

//
// size page
//
void tcontrol_setting::size_is_max(twindow& window)
{
	if (cell_.widget.size_is_max) {
		cell_.widget.size_is_max = false;
	} else {
		cell_.widget.size_is_max = true;
	}
	set_size_is_max_label(window);
}

void tcontrol_setting::set_size_is_max_label(twindow& window)
{
	std::string type_str;
	if (cell_.widget.size_is_max) {
		type_str = _("Max size");
	} else {
		type_str = _("Best size");
	}

	tbutton& button = find_widget<tbutton>(&window, "size_is_max", false);
	button.set_label(type_str);
}

//
// avcanced page
//
void tcontrol_setting::set_horizontal_mode(twindow& window)
{
	std::stringstream ss;
	std::vector<tval_str> items;

	for (std::map<int, tscroll_mode>::const_iterator it = horizontal_mode.begin(); it != horizontal_mode.end(); ++ it) {
		ss.str("");
		ss << it->second.description;
		items.push_back(tval_str(it->first, ss.str()));
	}

	gui2::tcombo_box dlg(items, cell_.widget.horizontal_mode);
	dlg.show(disp_.video());

	cell_.widget.horizontal_mode = (tscroll_container::tscrollbar_mode)dlg.selected_val();

	set_horizontal_mode_label(window);
}

void tcontrol_setting::set_horizontal_mode_label(twindow& window)
{
	std::stringstream ss;

	ss << horizontal_mode.find(cell_.widget.horizontal_mode)->second.description;
	tlabel* label = find_widget<tlabel>(&window, "_horizontal_mode", false, true);
	label->set_label(ss.str());
}

void tcontrol_setting::set_vertical_mode(twindow& window)
{
	std::stringstream ss;
	std::vector<tval_str> items;

	for (std::map<int, tscroll_mode>::const_iterator it = vertical_mode.begin(); it != vertical_mode.end(); ++ it) {
		ss.str("");
		ss << it->second.description;
		items.push_back(tval_str(it->first, ss.str()));
	}

	gui2::tcombo_box dlg(items, cell_.widget.vertical_mode);
	dlg.show(disp_.video());

	cell_.widget.vertical_mode = (tscroll_container::tscrollbar_mode)dlg.selected_val();

	set_vertical_mode_label(window);
}

void tcontrol_setting::set_vertical_mode_label(twindow& window)
{
	std::stringstream ss;

	ss << vertical_mode.find(cell_.widget.vertical_mode)->second.description;
	tlabel* label = find_widget<tlabel>(&window, "_vertical_mode", false, true);
	label->set_label(ss.str());
}

void tcontrol_setting::set_definition(twindow& window)
{
	std::stringstream ss;
	std::vector<tval_str> items;

	const std::string current_definition = utils::generate_app_prefix_id(u_.widget().second->app, u_.widget().second->id);

	const gui2::tgui_definition::tcontrol_definition_map& controls = gui2::gui.control_definition;
	const std::map<std::string, gui2::tcontrol_definition_ptr>& definitions = controls.find(u_.widget().first)->second;

	int index = 0;
	int def = 0;
	for (std::map<std::string, gui2::tcontrol_definition_ptr>::const_iterator it = definitions.begin(); it != definitions.end(); ++ it) {
		ss.str("");
		std::pair<std::string, std::string> pair = utils::split_app_prefix_id(it->first);
		ss << tintegrate::generate_format(pair.second, "blue");
		if (!pair.first.empty()) {
			ss << " [" << pair.first << "]";
		}
		if (!it->second->description.str().empty()) {
			ss << " (" << it->second->description << ")";
		}
		if (it->first == current_definition) {
			def = index;
		}
		items.push_back(tval_str(index ++, ss.str()));
	}

	gui2::tcombo_box dlg(items, def);
	dlg.show(disp_.video());

	std::map<std::string, gui2::tcontrol_definition_ptr>::const_iterator it = definitions.begin();
	std::advance(it, dlg.selected_val());
	u_.set_widget_definition(it->second);
	update_title(window);

	set_definition_label(window, it->first);
}

void tcontrol_setting::set_definition_label(twindow& window, const std::string& id2)
{
	std::stringstream ss;

	std::pair<std::string, std::string> pair = utils::split_app_prefix_id(id2);
	ss.str("");
	ss << pair.second;
	if (!pair.first.empty()) {
		ss << "[" << pair.first << "]";
	}
	find_widget<tlabel>(&window, "_definition", false).set_label(ss.str());
}

void tcontrol_setting::set_text_font_size(twindow& window)
{
	std::stringstream ss;
	std::vector<tval_str> items;

	int index = 0;
	int def = 0;
	for (int n = -3; n <= 3; n ++) {
		ss.str("");
		ss << "relative ";
		if (n >= 0) {
			ss << " +" << n;
		} else {
			ss << n;
		}
		items.push_back(tval_str(n + font::default_relative_size, ss.str()));
	}

	gui2::tcombo_box dlg(items, cell_.widget.text_font_size);
	dlg.show(disp_.video());
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}
	
	cell_.widget.text_font_size = dlg.selected_val();
	set_text_font_size_label(window);
}

void tcontrol_setting::set_text_font_size_label(twindow& window)
{
	std::stringstream ss;

	if (cell_.widget.text_font_size < font_min_relative_size) {
		ss << cell_.widget.text_font_size;
	} else {
		ss << "relative ";
		if (cell_.widget.text_font_size >= font::default_relative_size) {
			ss << "+" << cell_.widget.text_font_size - font::default_relative_size;
		} else {
			ss << cell_.widget.text_font_size - font::default_relative_size;
		}
	}

	find_widget<tlabel>(&window, "_text_font_size", false).set_label(ss.str());
}

void tcontrol_setting::set_text_color_tpl(twindow& window)
{
	std::stringstream ss;
	std::vector<tval_str> items;

	int index = 0;
	int def = 0;
	for (int n = theme::default_tpl; n < theme::color_tpls; n ++) {
		items.push_back(tval_str(n, theme::color_tpl_name(n)));
	}

	gui2::tcombo_box dlg(items, cell_.widget.text_color_tpl);
	dlg.show(disp_.video());
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}
	
	cell_.widget.text_color_tpl = dlg.selected_val();
	set_text_color_tpl_label(window);
}

void tcontrol_setting::set_text_color_tpl_label(twindow& window)
{
	find_widget<tbutton>(&window, "_set_text_color_tpl", false).set_label(theme::color_tpl_name(cell_.widget.text_color_tpl));
}

void tcontrol_setting::set_text_box2_text_box(twindow& window)
{
	std::stringstream ss;
	std::vector<tval_str> items;

	const std::string current_definition = cell_.widget.text_box2.text_box.empty()? "default": cell_.widget.text_box2.text_box;

	const gui2::tgui_definition::tcontrol_definition_map& controls = gui2::gui.control_definition;
	const std::map<std::string, gui2::tcontrol_definition_ptr>& definitions = controls.find("text_box")->second;

	int index = 0;
	int def = 0;
	for (std::map<std::string, gui2::tcontrol_definition_ptr>::const_iterator it = definitions.begin(); it != definitions.end(); ++ it) {
		ss.str("");
		ss << tintegrate::generate_format(it->first, "blue") << "(" << it->second->description << ")";
		if (it->first == current_definition) {
			def = index;
		}
		items.push_back(tval_str(index ++, ss.str()));
	}

	gui2::tcombo_box dlg(items, def);
	dlg.show(disp_.video());

	std::map<std::string, gui2::tcontrol_definition_ptr>::const_iterator it = definitions.begin();
	std::advance(it, dlg.selected_val());
	cell_.widget.text_box2.text_box = it->first;

	find_widget<tlabel>(&window, "text_box2_text_box", false).set_label(it->first);
}

void tcontrol_setting::set_stack_mode(twindow& window)
{
	std::stringstream ss;
	std::vector<tval_str> items;

	for (int n = 0; n < tstack::modes; n ++) {
		ss.str("");
		ss << implementation::form_stack_mode_str((tstack::tmode)n);
		items.push_back(tval_str(n, ss.str()));
	}

	gui2::tcombo_box dlg(items, cell_.widget.stack.mode);
	dlg.show(disp_.video());

	tstack::tmode ret = (tstack::tmode)dlg.selected_val();
	if (ret == cell_.widget.stack.mode) {
		return;
	}
	cell_.widget.stack.mode = (tstack::tmode)dlg.selected_val();
	find_widget<tbutton>(&window, "set_stack_mode", false).set_label(implementation::form_stack_mode_str(ret));
}

void tcontrol_setting::set_advanced_misc(twindow& window)
{
	const gui2::tgui_definition::tcontrol_definition_map& controls = gui2::gui.control_definition;
	const std::map<std::string, gui2::tcontrol_definition_ptr>& definitions = controls.find(u_.widget().first)->second;

	tbutton* button = find_widget<tbutton>(&window, "_set_definition", false, true);
	button->set_active(definitions.size() > 1);
	
	set_definition_label(window, utils::generate_app_prefix_id(u_.widget().second->app, u_.widget().second->id));
}

void tcontrol_setting::set_multi_line(twindow& window)
{
	cell_.widget.report.multi_line = !cell_.widget.report.multi_line;
	set_multi_line_label(window);
}

void tcontrol_setting::set_multi_line_label(twindow& window)
{
	tbutton* button = find_widget<tbutton>(&window, "multi_line", false, true);
	button->set_label(cell_.widget.report.multi_line? _("Multi line"): _("Tabbar"));

	if (cell_.widget.report.multi_line) {
		find_widget<tgrid>(&window, "report_multiline_grid", false, true)->set_visible(twidget::VISIBLE);
		find_widget<tgrid>(&window, "report_tabbar_grid", false, true)->set_visible(twidget::INVISIBLE);
		find_widget<ttext_box>(&window, "report_fixed_cols", false, true)->set_label(str_cast(cell_.widget.report.fixed_cols));
	} else {
		find_widget<tgrid>(&window, "report_multiline_grid", false, true)->set_visible(twidget::INVISIBLE);
		find_widget<tgrid>(&window, "report_tabbar_grid", false, true)->set_visible(twidget::VISIBLE);
		find_widget<ttoggle_button>(&window, "report_segment_switch", false, true)->set_value(cell_.widget.report.segment_switch);
	}
}

void tcontrol_setting::set_report_toggle(twindow& window)
{
	cell_.widget.report.toggle = !cell_.widget.report.toggle;
	tbutton* button = find_widget<tbutton>(&window, "report_toggle", false, true);

	button->set_label(cell_.widget.report.toggle? _("Toggle button"): _("Button"));

	cell_.widget.report.definition = "default";
	set_report_definition_label(window);
}

void tcontrol_setting::set_report_definition(twindow& window)
{
	std::stringstream ss;
	std::vector<tval_str> items;

	const gui2::tgui_definition::tcontrol_definition_map& controls = gui2::gui.control_definition;
	const std::string control = cell_.widget.report.toggle? "toggle_button": "button";
	const std::map<std::string, gui2::tcontrol_definition_ptr>& definitions = controls.find(control)->second;

	int index = 0;
	int def = 0;
	for (std::map<std::string, gui2::tcontrol_definition_ptr>::const_iterator it = definitions.begin(); it != definitions.end(); ++ it) {
		ss.str("");
		ss << tintegrate::generate_format(it->first, "blue") << "(" << it->second->description << ")";
		if (it->first == cell_.widget.report.definition) {
			def = index;
		}
		items.push_back(tval_str(index ++, ss.str()));
	}

	gui2::tcombo_box dlg(items, def);
	dlg.show(disp_.video());

	std::map<std::string, gui2::tcontrol_definition_ptr>::const_iterator it = definitions.begin();
	std::advance(it, dlg.selected_val());
	cell_.widget.report.definition = it->first;

	set_report_definition_label(window);
}

void tcontrol_setting::set_report_definition_label(twindow& window)
{
	find_widget<tlabel>(&window, "report_definition_label", false, true)->set_label(cell_.widget.report.definition);
}

}
