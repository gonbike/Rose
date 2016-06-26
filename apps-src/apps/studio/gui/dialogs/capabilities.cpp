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

#include "gui/dialogs/capabilities.hpp"

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
#include "unit.hpp"
#include "serialization/string_utils.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_DIALOG(studio, capabilities)

tcapabilities::tcapabilities(display& disp, const std::vector<std::unique_ptr<tapp_copier> >& app_copiers, int app_at)
	: disp_(disp)
	, app_copiers_(app_copiers)
	, app_at_(app_at)
	, current_capabilities_(null_cfg)
	, app_text_(NULL)
	, bundle_id_text_(NULL)
	, ok_(NULL)
{
	VALIDATE(app_at == twidget::npos || app_at_ < (int)app_copiers.size(), null_str);
	apps_in_apps_sln_ = apps_sln::apps_in(game_config::apps_src_path + "/apps");
	VALIDATE(apps_in_apps_sln_.size() >= 1, null_str);
}

void tcapabilities::pre_show(CVideo& /*video*/, twindow& window)
{
	window.set_canvas_variable("border", variant("default-border"));

	const tapp_copier* current_copier = NULL;
	std::stringstream ss;

	ss.str("");
	if (app_at_ == twidget::npos) {
		ss << _("New app"); 
	} else {
		ss << _("Edit app capabilities");
		current_copier = app_copiers_[app_at_].get();
		current_capabilities_ = *current_copier;
	}

	ok_ =  find_widget<tbutton>(&window, "save", false, true);
	ok_->set_active(false);

	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	label->set_label(ss.str());

	app_text_ = find_widget<ttext_box>(&window, "app", false, true);
	if (!current_copier) {
		app_text_->set_did_text_changed(boost::bind(&tcapabilities::did_changed_app, this, _1));
		app_text_->set_placeholder("hello");
		window.keyboard_capture(app_text_);
	} else {
		app_text_->set_label(current_copier->app);
		app_text_->set_active(false);
	}
	window.keyboard_capture(app_text_);

	bundle_id_text_ = find_widget<ttext_box>(&window, "bundle_id", false, true);
	bundle_id_text_->set_did_text_changed(boost::bind(&tcapabilities::did_changed_bundle_id, this, _1));
	if (!current_copier) {
		bundle_id_text_->set_placeholder("com.leagor.hello");
	} else {
		bundle_id_text_->set_label(current_copier->bundle_id.id());
		if (is_private_app(current_capabilities_.app)) {
			bundle_id_text_->set_active(false);
		}
	}

	ttoggle_button* toggle = find_widget<ttoggle_button>(&window, "ble", false, true);
	toggle->set_callback_state_change(boost::bind(&tcapabilities::did_changed_ble, this, _1));
	if (current_copier) {
		toggle->set_value(current_copier->ble);
		if (is_private_app(current_capabilities_.app)) {
			toggle->set_active(false);
		}
	}
	toggle = find_widget<ttoggle_button>(&window, "healthkit", false, true);
	toggle->set_callback_state_change(boost::bind(&tcapabilities::did_changed_healthkit, this, _1));
	if (current_copier) {
		toggle->set_value(current_copier->healthkit);
		if (is_private_app(current_capabilities_.app)) {
			toggle->set_active(false);
		}
	}

	connect_signal_mouse_left_click(
			  *ok_
			, boost::bind(
				&tcapabilities::save
				, this
				, boost::ref(window)
				, _3, _4));
}



void tcapabilities::did_changed_app(ttext_box& widget)
{
	if (!ok_ || app_at_ != twidget::npos) {
		return;
	}

	current_capabilities_.app = widget.label();
	ok_->set_active(is_valid_app(current_capabilities_.app) && is_valid_bundle_id(current_capabilities_.bundle_id));
}

void tcapabilities::did_changed_bundle_id(ttext_box& widget)
{
	if (!ok_ || !bundle_id_text_) {
		return;
	}
	current_capabilities_.bundle_id = widget.label();
	

	if (app_at_ == twidget::npos) {
		ok_->set_active(is_valid_app(current_capabilities_.app) && is_valid_bundle_id(current_capabilities_.bundle_id));

	} else {
		ok_->set_active(is_valid_bundle_id(current_capabilities_.bundle_id) && current_capabilities_ != *(app_copiers_[app_at_].get()));
	}
}

void tcapabilities::did_changed_ble(twidget& widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(&widget);
	current_capabilities_.ble = toggle->get_value();

	if (app_at_ == twidget::npos) {
		return;
	}
	ok_->set_active(is_valid_bundle_id(current_capabilities_.bundle_id) && current_capabilities_ != *(app_copiers_[app_at_].get()));
}

void tcapabilities::did_changed_healthkit(twidget& widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(&widget);
	current_capabilities_.healthkit = toggle->get_value();

	if (app_at_ == twidget::npos) {
		return;
	}
	ok_->set_active(is_valid_bundle_id(current_capabilities_.bundle_id) && current_capabilities_ != *(app_copiers_[app_at_].get()));
}

void tcapabilities::save(twindow& window, bool& handled, bool& halt)
{
	ttext_box* text_box = find_widget<ttext_box>(&window, "app", false, true);
	const std::string& app = text_box->label();
	if (app.empty()) {
		handled = true;
		halt = true;
		return;
	}
	window.set_retval(twindow::OK);
}

bool tcapabilities::is_valid_app(const std::string& app) const
{
	VALIDATE(app_at_ == twidget::npos, null_str);

	if (!utils::isvalid_nick(app)) {
		return false;
	}
	
	if (apps_in_apps_sln_.find(app) != apps_in_apps_sln_.end()) {
		return false;
	}
	for (std::vector<std::unique_ptr<tapp_copier> >::const_iterator it = app_copiers_.begin(); it != app_copiers_.end(); ++ it) {
		const tapp_copier& copier = **it;
		if (copier.app == app) {
			return false;
		}
	}
	return true;
}

bool tcapabilities::is_valid_bundle_id(const tdomain& bundle_id) const
{
	if (!bundle_id.valid()) {
		return false;
	}
	
	int at = 0;
	for (std::vector<std::unique_ptr<tapp_copier> >::const_iterator it = app_copiers_.begin(); it != app_copiers_.end(); ++ it, at ++) {
		const tapp_copier& copier = **it;
		if (app_at_ == at) {
			continue;
		}
		if (copier.bundle_id.id() == bundle_id.id()) {
			return false;
		}
	}
	return true;
}

}
