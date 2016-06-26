/* $Id: wml_exception.cpp 54233 2012-05-19 19:35:44Z mordante $ */
/*
   Copyright (C) 2007 - 2012 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 *  @file
 *  Implementation for wml_exception.hpp.
 */

#define GETTEXT_DOMAIN "rose-lib"

#include "global.hpp"
#include "wml_exception.hpp"

#include "display.hpp"
#include "gettext.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/widgets/settings.hpp"
#include "formula_string_utils.hpp"
#include "log.hpp"
#include "filesystem.hpp"
#include "posix2.h"

static lg::log_domain log_engine("engine");
#define WRN_NG LOG_STREAM(warn, log_engine)

void wml_exception(
		  const char* cond
		, const char* file
		, const int line
		, const char *function
		, const std::string& message
		, const std::string& dev_message)
{
	std::ostringstream sstr;
	if (cond) {
		sstr << "Condition '" << cond << "' failed at ";
	} else {
		sstr << "Unconditional failure at ";
	}

	std::string file2 = file;
	std::replace(file2.begin(), file2.end(), '\\', '/');
	sstr << file2 << ":" << line << " in function '" << function << "'.";

	if (!message.empty()) {
		sstr << "\n Information: " << message;
	}
	if (!dev_message.empty()) {
		sstr << "\n Extra development information: " << dev_message;
	}

	throw twml_exception(message, sstr.str());
}

void twml_exception::show(display &disp) const
{
	if (!gui2::settings::actived) {
		show();
		return;
	}

	std::ostringstream sstr;

	// The extra spaces between the \n are needed, otherwise the dialog doesn't show
	// an empty line.
	sstr << _("The error message is :")
		<< "\n" << user_message << "\n \n"
		<< _("When reporting the bug please include the following error message :")
		<< "\n" << dev_message;

	gui2::show_error_message(disp.video(), sstr.str());
}

void twml_exception::show() const
{
	if (display::get_singleton()) {
		show(*display::get_singleton());
		return;
	}
	std::ostringstream err;
	err << _("An error due to possibly invalid WML occurred\nThe error message is :")
		<< "\n" << user_message << "\n \n"
		<< _("When reporting the bug please include the following error message :")
		<< "\n" << dev_message;
	posix_print_mb("%s", utf8_2_ansi(err.str().c_str()));
}

std::string missing_mandatory_wml_key(
		  const std::string &section
		, const std::string &key
		, const std::string& primary_key
		, const std::string& primary_value)
{
	utils::string_map symbols;
	if(!section.empty()) {
		if(section[0] == '[') {
			symbols["section"] = section;
		} else {
			WRN_NG << __func__
					<< " parameter 'section' should contain brackets."
					<< " Added them.\n";
			symbols["section"] = "[" + section + "]";
		}
	}
	symbols["key"] = key;
	if(!primary_key.empty()) {
		assert(!primary_value.empty());

		symbols["primary_key"] = primary_key;
		symbols["primary_value"] = primary_value;

		return vgettext2("In section '[$section|]' where '$primary_key| = "
			"$primary_value' the mandatory key '$key|' isn't set.", symbols);
	} else {
		return vgettext2("In section '[$section|]' the "
			"mandatory key '$key|' isn't set.", symbols);
	}
}
