/* $Id: scroll_panel.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
/*
   Copyright (C) 2009 - 2012 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/widgets/scroll_panel.hpp"

#include "gui/auxiliary/widget_definition/scroll_panel.hpp"
#include "gui/auxiliary/window_builder/scroll_panel.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/scrollbar.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_WIDGET(scroll_panel)

tpoint tscroll_panel::mini_calculate_content_grid_size(const tpoint& content_origin, const tpoint& content_size)
{
	const tpoint actual_size = content_grid_->get_best_size();

	return tpoint(std::max(actual_size.x, content_size.x), std::max(actual_size.y, content_size.y));
}

const std::string& tscroll_panel::get_control_type() const
{
    static const std::string type = "scroll_panel";
    return type;
}

} // namespace gui2

