/* $Id: campaign_difficulty.hpp 49603 2011-05-22 17:56:17Z mordante $ */
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

#ifndef GUI_DIALOGS_CONTROL_SETTING_HPP_INCLUDED
#define GUI_DIALOGS_CONTROL_SETTING_HPP_INCLUDED

#include "gui/dialogs/cell_setting.hpp"
#include "unit.hpp"

class display;
class unit;
class mkwin_controller;

namespace gui2 {

struct tlinked_group;
class tstack;

class tcontrol_setting : public tsetting_dialog
{
public:
	enum {BASE_PAGE, SIZE_PAGE, ADVANCED_PAGE};

	explicit tcontrol_setting(display& disp, mkwin_controller& controller, unit& u, const std::vector<std::string>& textdomains, const std::vector<tlinked_group>& linkeds, bool float_widget);

protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void update_title(twindow& window);

	void save(twindow& window, bool& handled, bool& halt);

	void set_horizontal_layout(twindow& window);
	void set_vertical_layout(twindow& window);
	
	void set_horizontal_layout_label(twindow& window);
	void set_vertical_layout_label(twindow& window);

	void set_linked_group(twindow& window);
	void set_linked_group_label(twindow& window);

	void set_textdomain(twindow& window, bool label);
	void set_textdomain_label(twindow& window, bool label);

	bool did_item_pre_change_report(treport& report, ttoggle_button& widget, ttoggle_button* previous);
	void did_item_changed_report(treport& report, ttoggle_button& widget);

	void pre_base(twindow& window);
	void pre_rect(twindow& window);
	void pre_size(twindow& window);
	void pre_advanced(twindow& window);

	bool save_base(twindow& window);
	bool save_size(twindow& window);
	bool save_advanced(twindow& window);

	//
	// size page
	//
	void size_is_max(twindow& window);
	void set_size_is_max_label(twindow& window);

	//
	// advanced page
	//
	void set_horizontal_mode(twindow& window);
	void set_horizontal_mode_label(twindow& window);
	void set_vertical_mode(twindow& window);
	void set_vertical_mode_label(twindow& window);
	void set_advanced_misc(twindow& window);
	void set_definition(twindow& window);
	void set_definition_label(twindow& window, const std::string& id2);
	void set_text_font_size(twindow& window);
	void set_text_font_size_label(twindow& window);
	void set_text_color_tpl(twindow& window);
	void set_text_color_tpl_label(twindow& window);
	void set_multi_line(twindow& window);
	void set_multi_line_label(twindow& window);
	void set_report_toggle(twindow& window);
	void set_report_definition(twindow& window);
	void set_report_definition_label(twindow& window);
	void set_text_box2_text_box(twindow& window);
	void set_stack_mode(twindow& window);

protected:
	display& disp_;
	mkwin_controller& controller_;
	unit& u_;
	const std::vector<std::string>& textdomains_;
	const std::vector<tlinked_group>& linkeds_;
	bool float_widget_;
	treport* bar_;
	tstack* page_panel_;
};


}


#endif /* ! GUI_DIALOGS_CAMPAIGN_DIFFICULTY_HPP_INCLUDED */
