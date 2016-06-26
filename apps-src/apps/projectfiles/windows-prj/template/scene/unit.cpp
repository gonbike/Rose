#define GETTEXT_DOMAIN "studio-lib"

#include "unit.hpp"
#include "gettext.hpp"
#include "simple_display.hpp"
#include "simple_controller.hpp"
#include "gui/dialogs/simple_scene.hpp"

unit::unit(simple_controller& controller, simple_display& disp, unit_map& units)
	: base_unit(units)
	, controller_(controller)
	, disp_(disp)
	, units_(units)
{
}

void unit::draw_unit()
{
	if (!loc_.valid()) {
		return;
	}

	if (refreshing_) {
		return;
	}
	trefreshing_lock lock(*this);
	redraw_counter_ ++;
}