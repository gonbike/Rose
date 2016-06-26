/* $Id: editor_controller.cpp 47755 2010-11-29 12:57:31Z shadowmaster $ */
/*
   Copyright (C) 2008 - 2010 by Tomasz Sniatowski <kailoran@gmail.com>
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

#include "mkwin_controller.hpp"
#include "mkwin_display.hpp"

#include "gettext.hpp"
#include "integrate.hpp"
#include "formula_string_utils.hpp"
#include "preferences.hpp"
#include "sound.hpp"
#include "filesystem.hpp"
#include "hotkeys.hpp"
#include "config_cache.hpp"
#include "preferences_display.hpp"
#include "gui/dialogs/mkwin_scene.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/system.hpp"
#include "gui/dialogs/row_setting.hpp"
#include "gui/dialogs/column_setting.hpp"
#include "gui/dialogs/window_setting.hpp"
#include "gui/dialogs/control_setting.hpp"
#include "gui/dialogs/linked_group.hpp"
#include "gui/dialogs/drawing_setting.hpp"
#include "gui/dialogs/grid_setting.hpp"
#include "gui/dialogs/browse.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/listbox.hpp"
#include "serialization/parser.hpp"

#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <numeric>

#include "gui/auxiliary/formula.hpp"

const std::string noise_str = "_magic5a5anoise";
std::string noise_config_key(const std::string& key)
{
	return key + noise_str;
}

std::string denoise_config_key(const std::string& noised)
{
	const size_t noise_size = noise_str.size();
	size_t noised_size = noised.size();

	if (noised_size > noise_size && noised.find(noise_str) == noised_size - noise_size) {
		return noised.substr(0, noised_size - noise_size);
	}
	return null_str;
}

std::string noise_config_key_tpl(const std::string& key, const std::string& id)
{
	return key + noise_str + "_" + id;
}

bool is_noised_config_key(const std::string& noised)
{
	const size_t noise_size = noise_str.size();
	size_t noised_size = noised.size();

	return noised_size > noise_size && noised.find(noise_str) == noised_size - noise_size;
}


void square_fill_frame(t_translation::t_map& tiles, size_t start, const t_translation::t_terrain& terrain, bool front, bool back)
{
	if (front) {
		// first column(border)
		for (size_t n = start; n < tiles[0].size() - start; n ++) {
			tiles[start][n] = terrain;
		}
		// first line(border)
		for (size_t n = start + 1; n < tiles.size() - start; n ++) {
			tiles[n][start] = terrain;
		}
	}

	if (back) {
		// last column(border)
		for (size_t n = start; n < tiles[0].size() - start; n ++) {
			tiles[tiles.size() - start - 1][n] = terrain;
		}
		// last line(border)
		for (size_t n = start; n < tiles.size() - start - 1; n ++) {
			tiles[n][tiles[0].size() - start - 1] = terrain;
		}
	}
}

std::string generate_map_data(int width, int height, bool colorful)
{
	VALIDATE(width > 0 && height > 0, "map must not be empty!");

	const t_translation::t_terrain normal = t_translation::read_terrain_code("Gg");
	const t_translation::t_terrain border = t_translation::read_terrain_code("Gs");
	const t_translation::t_terrain control = t_translation::read_terrain_code("Gd");
	const t_translation::t_terrain forbidden = t_translation::read_terrain_code("Gll");

	t_translation::t_map tiles(width + 2, t_translation::t_list(height + 2, normal));
	if (colorful) {
		square_fill_frame(tiles, 0, border, true, true);
		square_fill_frame(tiles, 1, control, true, false);

		const size_t border_size = 1;
		tiles[border_size][border_size] = forbidden;
	}

	// tiles[border_size][tiles[0].size() - border_size - 1] = forbidden;

	// tiles[tiles.size() - border_size - 1][border_size] = forbidden;
	// tiles[tiles.size() - border_size - 1][tiles[0].size() - border_size - 1] = forbidden;

	std::string str = tmap::default_map_header + t_translation::write_game_map(t_translation::t_map(tiles));

	return str;
}

std::string generate_anchor_rect(int x, int y, int w, int h)
{
	std::stringstream ss;
	ss << x << "," << y << "," << x + w << "," << y + h;
	return ss.str();
}

tmenu2::~tmenu2()
{
	for (std::vector<titem>::iterator it = items.begin(); it != items.end(); ++ it) {
		titem& item = *it;
		if (item.submenu) {
			delete item.submenu;
			item.submenu = NULL;
		}
	}
}

tmenu2* tmenu2::top_menu()
{
	tmenu2* result = this;
	while (result->parent) {
		result = result->parent;
	}
	return result;
}

bool tmenu2::id_existed(const std::string& id) const
{
	for (std::vector<titem>::const_iterator it = items.begin(); it != items.end(); ++ it) {
		const titem& item = *it;
		if (item.id == id) {
			return true;
		}
		if (item.submenu) {
			if (item.submenu->id_existed(id)) {
				return true;
			}
		}
	}
	return false;
}

void tmenu2::submenus(std::vector<tmenu2*>& result) const
{
	for (std::vector<titem>::const_iterator it = items.begin(); it != items.end(); ++ it) {
		const titem& item = *it;
		if (item.submenu) {
			result.push_back(item.submenu);
			item.submenu->submenus(result);
		}
	}
}

void tmenu2::generate(config& cfg) const
{
	if (items.empty()) {
		return;
	}

	std::vector<tmenu2*> submenus;
	std::stringstream ss;
	for (std::vector<titem>::const_iterator it = items.begin(); it != items.end(); ++ it) {
		const titem& item = *it;
		if (!ss.str().empty()) {
			ss << ", ";
		}
		ss << item.id;
		if (item.submenu) {
			ss << "_m";
			submenus.push_back(item.submenu);
		}
		if (item.hide || item.param) {
			ss << "|";
			if (item.hide) {
				ss << "h";
			}
			if (item.param) {
				ss << "p";
			}
		}
	}
	if (!report.empty()) {
		cfg["report"] = report;
	}
	cfg[id] = ss.str();

	for (std::vector<tmenu2*>::const_iterator it = submenus.begin(); it != submenus.end(); ++ it) {
		const tmenu2& menu = **it;
		menu.generate(cfg);
	}
}

const int mkwin_controller::default_child_width = 5;
const int mkwin_controller::default_child_height = 4;
const int mkwin_controller::default_dialog_width = 48;
const int mkwin_controller::default_dialog_height = 24;

std::string mkwin_controller::widget_template_cfg()
{
	return game_config::path + "/data/gui/default/macros/template.cfg";
}

void tfloat_widget::from(mkwin_controller& controller, mkwin_display& disp, unit_map& units, const config& cfg, const std::string& type, const std::string& definition, const std::string& id)
{
	gui2::tfloat_widget_builder::from(cfg);
	
	u.reset(new unit(controller, disp, units, std::make_pair(type, disp.find_widget(disp, type, definition, id)), nullptr, -1));
	u->from(cfg);
}

void tfloat_widget::generate(const std::set<std::string>& reserve_float_widget_ids, config& cfg) const
{
	if (reserve_float_widget_ids.find(u->cell().id) == reserve_float_widget_ids.end()) {
		gui2::tfloat_widget_builder::generate(cfg);
	}
	u->generate(cfg);
}

mkwin_controller::mkwin_controller(const config &top_config, CVideo& video, const std::map<std::string, std::string>& app_tdomains)
	: base_controller(SDL_GetTicks(), top_config, video)
	, gui_(NULL)
	, map_(null_str)
	, units_(*this, map_, true)
	, current_unit_(NULL)
	, copied_unit_(NULL)
	, last_unit_(NULL)
	, file_()
	, top_()
	, theme_top_()
	, preview_(false)
	, theme_widget_start_(0)
	, top_rect_cfg_(NULL)
	, context_menus_(1, tmenu2(null_str, "main", NULL))
	, callback_sstr_()
	, app_tdomains_(app_tdomains)
{
	reserve_float_widget_ids_.insert("_tpl_horizontal_scrollbar");
	reserve_float_widget_ids_.insert("_tpl_vertical_scrollbar");
	reserve_float_widget_ids_.insert("_tpl_tooltip");
	reserve_float_widget_ids_.insert("_tpl_magnifier");
	reserve_float_widget_ids_.insert("_tpl_edit_select");
	reserve_float_widget_ids_.insert("_tpl_edit_copy");
	reserve_float_widget_ids_.insert("_tpl_edit_paste");
	reserve_float_widget_ids_.insert("_tpl_common_float_widgets");

	gui2::init_layout_mode();

	textdomains_.push_back("rose-lib");
	textdomains_.push_back(game_config::app + "-lib");

	VALIDATE(default_dialog_width >= default_child_width, null_str);
	VALIDATE(default_dialog_height >= default_child_height, null_str);

	int original_width = default_child_width;
	int original_height = default_child_height;
	map_ = tmap(generate_map_data(original_width, original_height, preview_));

	units_.create_coor_map(map_.w(), map_.h());
}

mkwin_controller::~mkwin_controller()
{
	if (gui_) {
		delete gui_;
		gui_ = NULL;
	}
}

void mkwin_controller::app_create_display()
{
	gui_ = new mkwin_display(*this, units_, video_, map_);
}

void mkwin_controller::app_post_initialize()
{
	fill_spacer(NULL, -1);
	units_.save_map_to(top_, false);

	set_status();
	gui_->show_context_menu();
}

events::mouse_handler_base& mkwin_controller::get_mouse_handler_base()
{
	return *this;
}

void mkwin_controller::app_execute_command(int command, const std::string& sparam)
{
	using namespace gui2;
	unit* u = units_.find_unit(selected_hex_, true);

	switch (command) {
		case tmkwin_scene::HOTKEY_SELECT:
			click_widget(gui_->spacer.first, gui_->spacer.second->id);
			break;
		case tmkwin_scene::HOTKEY_GRID:
			click_widget("grid", null_str);
			break;

		case tmkwin_scene::HOTKEY_RCLICK:
			do_right_click();
			break;

		case tmkwin_scene::HOTKEY_INSERT_TOP:
			insert_row(u, true);
			break;
		case tmkwin_scene::HOTKEY_INSERT_BOTTOM:
			insert_row(u, false);
			break;
		case tmkwin_scene::HOTKEY_ERASE_ROW:
			erase_row(u);
			break;

		case tmkwin_scene::HOTKEY_INSERT_LEFT:
			insert_column(u, true);
			break;
		case tmkwin_scene::HOTKEY_INSERT_RIGHT:
			insert_column(u, false);
			break;
		case tmkwin_scene::HOTKEY_ERASE_COLUMN:
			erase_column(u);
			break;

		case tmkwin_scene::HOTKEY_SETTING:
			widget_setting(false);
			break;
		case tmkwin_scene::HOTKEY_LINKED_GROUP:
			linked_group_setting(u);
			break;
		case tmkwin_scene::HOTKEY_SPECIAL_SETTING:
			widget_setting(true);
			break;

		case HOTKEY_COPY:
			copy_widget(u, false);
			break;
		case HOTKEY_CUT:
			copy_widget(u, true);
			break;
		case HOTKEY_PASTE:
			paste_widget();
			break;
		case tmkwin_scene::HOTKEY_ERASE:
			erase_widget();
			break;

		case tmkwin_scene::HOTKEY_INSERT_CHILD:
			u->insert_child(default_child_width, default_child_height);
			if (!in_theme_top()) {
				layout_dirty();
			}
			gui_->show_context_menu();
			break;
		case tmkwin_scene::HOTKEY_ERASE_CHILD:
			u->parent().u->erase_child(u->parent().number);
			if (!in_theme_top()) {
				layout_dirty();
			}
			gui_->show_context_menu();
			break;

		case tmkwin_scene::HOTKEY_UNPACK:
			unpack_widget_tpl(u);
			break;

		case tmkwin_scene::HOTKEY_BUILD:
			{
				gui2::tmkwin_scene* scene = dynamic_cast<gui2::tmkwin_scene*>(gui_->get_theme());
				scene->do_build();
			}
			break;

		case tmkwin_scene::HOTKEY_PREVIEW:
			if (!preview_ && !verify()) {
				break;
			}
			if (!preview_) {
				theme_goto_top(false);
			} else {
				theme_into_widget(nullptr);
			}
			break;
		case HOTKEY_SYSTEM:
			system();
			break;

		default:
			base_controller::app_execute_command(command, sparam);
	}
}

void mkwin_controller::insert_row(unit* u, bool top)
{
	const unit::tparent& parent = u->parent();
	unit::tchild& child = parent.u? parent.u->child(parent.number): top_;

	int row = u->get_location().y - child.window->get_location().y - 1;
	if (!top) {
		row ++;
	}
	int w = (int)child.cols.size();
	std::vector<unit*> v;
	for (int x = 0; x < w; x ++) {
		v.push_back(new unit(*this, *gui_, units_, gui_->spacer, parent.u, parent.number));
	}
	std::vector<unit*>::iterator it = child.units.begin();
	if (row) {
		std::advance(it, row * w);
	}
	child.units.insert(it, v.begin(), v.end());

	it = child.rows.begin();
	if (row) {
		std::advance(it, row);
	}
	child.rows.insert(it, new unit(*this, *gui_, units_, unit::ROW, parent.u, parent.number));

	layout_dirty();
	gui_->show_context_menu();
}

void mkwin_controller::erase_row(unit* u)
{
	const unit::tparent& parent = u->parent();
	unit::tchild& child = parent.u? parent.u->child(parent.number): top_;

	int row = u->get_location().y - child.window->get_location().y - 1;
	int w = (int)child.cols.size();
	std::vector<unit*>::iterator it;
	for (int x = 0; x < w; x ++) {
		unit* u = child.units[row * w];
		units_.erase(u->get_location());

		it = child.units.begin();
		std::advance(it, row * w);
		child.units.erase(it);
	}
	units_.erase(u->get_location());

	it = child.rows.begin();
	if (row) {
		std::advance(it, row);
	}
	child.rows.erase(it);

	layout_dirty();

	if (row >= (int)child.rows.size()) {
		selected_hex_.y = child.window->get_location().y + child.rows.size();
	}
	gui_->show_context_menu();
}

void mkwin_controller::insert_column(unit* u, bool left)
{
	const unit::tparent& parent = u->parent();
	unit::tchild& child = parent.u? parent.u->child(parent.number): top_;

	int col = u->get_location().x - child.window->get_location().x - 1;
	if (!left) {
		col ++;
	}
	int w = (int)child.cols.size();
	int h = (int)child.rows.size();
	
	std::vector<unit*>::iterator it;
	for (int y = 0; y < h; y ++) {
		unit* u = new unit(*this, *gui_, units_, gui_->spacer, parent.u, parent.number);

		it = child.units.begin();
		std::advance(it, y * w + col + y);
		child.units.insert(it, u);
	}
	
	it = child.cols.begin();
	if (col) {
		std::advance(it, col);
	}
	child.cols.insert(it, new unit(*this, *gui_, units_, unit::COLUMN, parent.u, parent.number));

	layout_dirty();
	gui_->show_context_menu();
}

void mkwin_controller::erase_column(unit* u)
{
	const unit::tparent& parent = u->parent();
	unit::tchild& child = parent.u? parent.u->child(parent.number): top_;

	int col = u->get_location().x - child.window->get_location().x - 1;
	int w = (int)child.cols.size();
	int h = (int)child.rows.size();
	std::vector<unit*>::iterator it;
	for (int y = 0; y < h; y ++) {
		unit* u = child.units[y * w + col - y];
		units_.erase(u->get_location());

		it = child.units.begin();
		std::advance(it, y * w + col - y);
		child.units.erase(it);
	}
	units_.erase(u->get_location());

	it = child.cols.begin();
	if (col) {
		std::advance(it, col);
	}
	child.cols.erase(it);

	layout_dirty();
	if (col >= (int)child.cols.size()) {
		selected_hex_.x = child.window->get_location().x + child.cols.size();
	}
	gui_->show_context_menu();
}

void mkwin_controller::fill_object_list()
{
	gui2::tmkwin_scene* scene = dynamic_cast<gui2::tmkwin_scene*>(gui_->get_theme());
	scene->fill_object_list(units_);
}

void mkwin_controller::generate_theme_top(unit::tchild& top, const int xdim, const int ydim)
{
	top.clear(true);

	const int width = 1 + ceil(1.0 * (xdim * gui2::twidget::hdpi_scale) / image::tile_size);
	const int height = 1 + ceil(1.0 * (ydim  * gui2::twidget::hdpi_scale) / image::tile_size);
	
	const int zoom = gui_->hex_width();
	const double zoom_factor = gui_->get_zoom_factor();
	// const double zoom_factor = 1;
	for (int y = 0; y < height; y ++) {
		for (int x = 0; x < width; x ++) {
			SDL_Rect rect = create_rect(x * zoom, y * zoom, zoom, zoom);
			if (!x && !y) {
				top.window = new unit2(*this, *gui_, units_, unit::WINDOW, nullptr, rect);
			} else if (x && !y) {
				top.cols.push_back(new unit2(*this, *gui_, units_, unit::COLUMN, nullptr, rect));
			} else if (!x && y) {
				top.rows.push_back(new unit2(*this, *gui_, units_, unit::ROW, nullptr, rect));
			}
		}
	}

	const int x_offset = zoom, y_offset = zoom;
	const int least_width = 16 * gui2::twidget::hdpi_scale, least_height = 16 * gui2::twidget::hdpi_scale;
	game_logic::map_formula_callable variables;

	variables.add("screen_width", variant(xdim));
	variables.add("screen_height", variant(ydim));
	variables.add("svga", variant(xdim >= 800 && ydim >= 600));
	variables.add("vga", variant(xdim >= 640 && ydim >= 480));

	tpoint origin(x_offset, y_offset);
	const tpoint size(xdim * zoom_factor * gui2::twidget::hdpi_scale, ydim * zoom_factor * gui2::twidget::hdpi_scale);
	top_.place(*this, *gui_, units_, variables, origin, size, top.units);
}

void mkwin_controller::theme_into_widget(unit* u)
{
	gui_->set_grid(false);

	preview_ = false;
	top_ = top2_;
	current_unit_ = nullptr;

	units_.set_consistent(true);
	layout_dirty(true);

	{
		gui2::tcontrol* widget = dynamic_cast<gui2::tcontrol*>(gui_->get_theme_object("preview"));
		widget->set_label("misc/into.png");
		widget->set_tooltip(_("Preivew"));
	}

	selected_hex_ = map_location();

	fill_object_list();

	gui_->show_context_menu();
}

void mkwin_controller::theme_goto_top(bool auto_select)
{
	unit* u = units_.find_unit(map_location(0, 0))->parent().u;
	if (auto_select) {
		selected_hex_ = u->get_location();
	} else {
		selected_hex_ = map_location();
	}

	gui_->set_grid(true);

	current_unit_ = NULL;
	units_.zero_map();

	click_widget(gui_->spacer.first, gui_->spacer.second->id);
	preview_ = true;
	generate_theme_top(theme_top_, 640, 480);
	top2_ = top_;

	top_ = theme_top_;

	units_.set_consistent(false);
	reload_map(top_.cols.size() + 1, top_.rows.size() + 1);
	units_.restore_map_from(top_, 0, 0, true);

	// normal, scene not use top_.units
	top_.units.clear();

	{
		gui2::tcontrol* widget = dynamic_cast<gui2::tcontrol*>(gui_->get_theme_object("preview"));
		widget->set_label("misc/into.png");
		widget->set_tooltip(_("Back to design"));
	}

	if (selected_hex_.valid()) {
		gui_->scroll_to_xy(u->get_rect().x, u->get_rect().y, display::ONSCREEN);
	}

	fill_object_list();

	gui_->show_context_menu();
}

void mkwin_controller::widget_setting(bool special)
{
	if (!selected_hex_.valid()) {
		return;
	}

	unit* u = units_.find_unit(selected_hex_);

	gui2::tsetting_dialog *dlg = NULL;
	if (special) {
		if (u->is_drawing()) {
			dlg = new gui2::tdrawing_setting(*gui_, *u);
		}

	} else if (u->type() == unit::WIDGET) {
		const unit* tmp = u->parent().u;
		while (tmp) {
			if (tmp->is_listbox()) {
				break;
			}
			tmp = tmp->parent().u;
		}
		const std::vector<gui2::tlinked_group>& linked_groups = tmp? tmp->cell().widget.listbox.linked_groups: linked_groups_;
		dlg = new gui2::tcontrol_setting(*gui_, *this, *u, textdomains_, linked_groups, false);

	} else if (u->type() == unit::ROW) {
		dlg = new gui2::trow_setting(*gui_, *u);

	} else if (u->type() == unit::COLUMN) {
		dlg = new gui2::tcolumn_setting(*gui_, *u);

	} else if (u->type() == unit::WINDOW) {
		if (!u->parent().u) {
			dlg = new gui2::twindow_setting(*gui_, *this, *u, textdomains_);
		} else {
			const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget = u->parent().u->widget();
			dlg = new gui2::tgrid_setting(*this, *gui_, units_, *u, widget.first);
		}
	}

	dlg->show(gui_->video());
	int res = dlg->get_retval();
	if (res != gui2::twindow::OK) {
		delete dlg;
		return;
	}
	u->set_cell(dlg->cell());
	delete dlg;
}

void mkwin_controller::linked_group_setting(unit* u)
{
	std::vector<gui2::tlinked_group>& linked_groups = u->is_listbox()? u->cell().widget.listbox.linked_groups: linked_groups_;

	gui2::tlinked_group2 dlg(*gui_, *u, linked_groups);
	dlg.show(gui_->video());
	int res = dlg.get_retval();
	if (res != gui2::twindow::OK) {
		return;
	}

	linked_groups = dlg.linked_groups();
}

void mkwin_controller::copy_widget(unit* u, bool cut)
{
	cut_ = cut;
	set_copied_unit(u);

	gui_->show_context_menu();
}

bool mkwin_controller::paste_widget2(const unit& from, const map_location to_loc)
{
	bool require_layout_dirty = false;

	unit* to = units_.find_unit(to_loc);
	unit::tparent parent = to->parent();
	units_.erase(to_loc);
	unit* clone = new unit(from);
	// from and to maybe in different page.
	clone->set_parent(parent.u, parent.number);	
	units_.insert(to_loc, clone);

	to = units_.find_unit(to_loc);
	replace_child_unit(to);
	if (to->has_child()) {
		require_layout_dirty = true;
	}
	return require_layout_dirty;
}

void mkwin_controller::paste_widget()
{
	if (!selected_hex_.valid() || !copied_unit_) {
		return;
	}

	unit* u = units_.find_unit(selected_hex_);
	bool require_layout_dirty = false;
	std::vector<const unit*> require_cut_widgets;

	if (copied_unit_->type() == unit::WIDGET) {
		require_layout_dirty = paste_widget2(*copied_unit_, selected_hex_);
		require_cut_widgets.push_back(copied_unit_);

	} else if (copied_unit_->type() == unit::ROW) {
		unit::tparent from_parent = copied_unit_->parent();
		unit::tparent to_parent = u->parent();

		const unit::tchild& from_child = from_parent.u? from_parent.u->child(from_parent.number): top_;
		const unit::tchild& to_child = to_parent.u? to_parent.u->child(to_parent.number): top_;

		int from_row = copied_unit_->get_location().y - from_child.window->get_location().y - 1;
		int to_row = u->get_location().y - to_child.window->get_location().y - 1;
		int w = (int)from_child.cols.size();
		for (int x = 0; x < w; x ++) {
			const unit* from = from_child.units[from_row * w + x];
			const unit* to = to_child.units[to_row * w + x];
			require_layout_dirty |= paste_widget2(*from, to->get_location());
			require_cut_widgets.push_back(from);
		}

	} else if (copied_unit_->type() == unit::COLUMN) {
		unit::tparent from_parent = copied_unit_->parent();
		unit::tparent to_parent = u->parent();

		const unit::tchild& from_child = from_parent.u? from_parent.u->child(from_parent.number): top_;
		const unit::tchild& to_child = to_parent.u? to_parent.u->child(to_parent.number): top_;

		int from_col = copied_unit_->get_location().x - from_child.window->get_location().x - 1;
		int to_col = u->get_location().x - to_child.window->get_location().x - 1;
		int from_w = (int)from_child.cols.size();
		int to_w = (int)to_child.cols.size();
		int h = (int)from_child.rows.size();
		for (int y = 0; y < h; y ++) {
			const unit* from = from_child.units[y * from_w + from_col];
			const unit* to = to_child.units[y * to_w + to_col];
			require_layout_dirty |= paste_widget2(*from, to->get_location());
			require_cut_widgets.push_back(from);
		}

	}
	if (cut_) {
		for (std::vector<const unit*>::const_iterator it = require_cut_widgets.begin(); it != require_cut_widgets.end(); ++ it) {
			const unit* erasing = *it;
			const map_location loc = erasing->get_location();
			unit::tparent parent = erasing->parent();
			units_.erase(loc);
			VALIDATE(!copied_unit_ || copied_unit_->type() != unit::WIDGET, null_str);

			units_.insert(loc, new unit(*this, *gui_, units_, gui_->spacer, parent.u, parent.number));
			unit* u = units_.find_unit(loc);
			replace_child_unit(u);
		}
	}
	if (require_layout_dirty) {
		layout_dirty();
	}

	gui_->show_context_menu();
}

void mkwin_controller::erase_widget()
{
	if (!selected_hex_.valid()) {
		return;
	}

	unit* erasing = units_.find_unit(selected_hex_);
	unit::tparent parent = erasing->parent();

	if (erasing->type() == unit::WINDOW && parent.u && parent.u->is_grid()) {
		selected_hex_ = parent.u->get_location();
		erase_widget();
		return;
	}

	bool has_child = erasing->has_child();
	if (has_child) {
		bool all_spacer = true;
		const std::vector<unit::tchild>& children = erasing->children();
		for (std::vector<unit::tchild>::const_iterator it = children.begin(); it != children.end(); ++ it) {
			if (!it->is_all_spacer()) {
				all_spacer = false;
				break;
			}
		}
		if (!all_spacer) {
			utils::string_map symbols;
			std::stringstream ss;
			
			symbols["id"] = tintegrate::generate_format(erasing->cell().id.empty()? "---": erasing->cell().id, "red");
			ss << vgettext2("$id holds grid that not all spacer. Do you want to delete?", symbols);
			int res = gui2::show_message(gui_->video(), "", ss.str(), gui2::tmessage::yes_no_buttons);
			if (res != gui2::twindow::OK) {
				return;
			}
		}
	}
	units_.erase(selected_hex_);
	if (in_theme_top()) {
		selected_hex_ = map_location();

	} else {
		units_.insert(selected_hex_, new unit(*this, *gui_, units_, gui_->spacer, parent.u, parent.number));
		unit* u = units_.find_unit(selected_hex_);
		replace_child_unit(u);

		if (has_child) {
			layout_dirty();
		}
	}

	fill_object_list();
	gui_->show_context_menu();
}

void mkwin_controller::layout_dirty(bool force_change_map)
{
	const int default_expand_size = 4;
	
	VALIDATE(!in_theme_top(), "Must on be in !scene or scene's second layer!");

	bool require_change = false;
	int w = map_.w(), h = map_.h();
	int new_w, new_h;
	if (preview_) {
		tpoint ret = units_.restore_map_from(current_unit_->children(), false);
		new_w = ret.x;
		new_h = ret.y;
	} else {
		units_.recalculate_size(top_);
		new_w = top_.window->cell().window.cover_width;
		new_h = top_.window->cell().window.cover_height;
	}
	if (force_change_map || w < new_w) {
		require_change = true;
		w = new_w + default_expand_size;
	} else if (w > default_dialog_width && w > new_w + default_expand_size) {
		require_change = true;
		w = std::max(new_w + default_expand_size, default_dialog_width);
	}

	if (force_change_map || h < new_h) {
		require_change = true;
		h = new_h + default_expand_size;
	} else if (h > default_dialog_height && h > new_h + default_expand_size) {
		require_change = true;
		h = std::max(new_h + default_expand_size, default_dialog_height);
	}

	if (require_change) {
		reload_map(w, h);
	}

	units_.layout(top_);
	if (require_change) {
		gui_->recalculate_minimap();
	} else {
		gui_->redraw_minimap();
	}
}

void mkwin_controller::set_status()
{
	gui2::tcontrol* widget = dynamic_cast<gui2::tcontrol*>(gui_->get_theme_object("status"));
	int w = widget->get_width();
	int h = widget->get_height();
	std::vector<image::tblit> result;
	result.push_back(image::tblit("misc/background45.png", image::UNSCALED, w, h));
	if (copied_unit_) {
		SDL_Rect dst_clip = empty_rect;
		std::string png = cut_? "buttons/cut.png": "buttons/copy.png";
		// surface fg = scale_surface(image::get_image(png), w / 2, h / 2);
		// sdl_blit(fg, NULL, result, &dst_clip);
		result.push_back(image::tblit(png, image::UNSCALED, w / 2, h / 2, SDL_Rect(), dst_clip.x, dst_clip.y));

		const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget = copied_unit_->widget();
		png = copied_unit_->image();
		// fg = scale_surface(image::get_image(png), w / 2, h / 2);
		surface fg = image::get_image(png);
		if (fg) {
			dst_clip.x = w / 2;
			dst_clip.y = 0;
			// sdl_blit(fg, NULL, result, &dst_clip);
			result.push_back(image::tblit(png, image::UNSCALED, w / 2, h / 2, SDL_Rect(), dst_clip.x, dst_clip.y));
		}
	}
	gui_->widget_set_blits("status", result);
	gui_->redraw_minimap();
}

void mkwin_controller::set_copied_unit(unit* u)
{
	copied_unit_ = u;
	if (gui_) {
		set_status();
	}
}

void mkwin_controller::fill_spacer(unit* parent, int number)
{
	int width = map_.w();
	int height = map_.h();

	int zoom = gui_->hex_width();
	for (int y = 0; y < height; y ++) {
		for (int x = 0; x < width; x ++) {
			map_location loc = map_location(x, y);
			if (!units_.find_unit(loc, true)) {
				if (x && y) {
					if (!in_theme_top()) {
						units_.insert(loc, new unit(*this, *gui_, units_, gui_->spacer, parent, number));
					}
				} else {
					int type = unit::NONE;
					if (!x && !y) {
						type = unit::WINDOW;
					} else if (x) {
						type = unit::COLUMN;
					} else if (y) {
						type = unit::ROW;
					}
					if (!in_theme_top()) {
						units_.insert(loc, new unit(*this, *gui_, units_, type, parent, number));
					} else {
						SDL_Rect rect = create_rect(x * zoom, y * zoom, zoom, zoom);
						units_.insert2(*gui_, new unit2(*this, *gui_, units_, type, parent, rect));
					}
				}
			}
		}
	}
}

class tclick_dismiss
{
public:
	tclick_dismiss(gui2::tcell_setting& cell)
		: cell_(cell)
	{
		original_ = cell.window.click_dismiss;
		cell_.window.click_dismiss = true;
	}
	~tclick_dismiss()
	{
		cell_.window.click_dismiss = original_;
	}

private:
	gui2::tcell_setting& cell_;
	bool original_;
};

bool mkwin_controller::verify()
{
	if (!window_has_valid(true)) {
		return false;
	}
	return true;
}

unit* mkwin_controller::get_window() const
{
	unit* window = NULL;
	if (current_unit_) {
		window = theme_top_.window;
	} else {
		window = units_.find_unit(map_location(0, 0));
	}
	return window;
}

bool mkwin_controller::callback_valid_id(const unit* u, bool show_error)
{
	std::string err;
	std::stringstream ss;
	utils::string_map symbols;

	const std::string id = u->cell().id;
	symbols["id"] = tintegrate::generate_format(id.empty()? "--": id, "red");
	symbols["widget"] = tintegrate::generate_format(u->widget_tag(), "red");

	if (u->is_stack()) {
		const std::vector<unit::tchild>& children = u->children();
		for (std::vector<unit::tchild>::const_iterator it = children.begin(); it != children.end(); ++ it) {
			const unit::tchild& child = *it;
			if (child.is_all_spacer()) {
				err = vgettext2("widget($widget) has all spacer layer!", symbols);
				break;
			}
		}

	} else if (u->is_tree_view()) {
		const std::vector<unit::tchild>& children = u->children();
		VALIDATE(children.size() == 1, null_str); // now only one node type.
		const unit::tchild& child = children[0];
		if (child.rows.size() != 1) {
			err = vgettext2("widget($widget) is tree view, only one row in 1th grid.", symbols);
		} else if (!child.units[0]->is_spacer()) {
			err = vgettext2("widget($widget) is tree view, first widget must be spacer.", symbols);
		} else if (child.cols[0]->cell().column.grow_factor) {
			err = vgettext2("widget($widget) is tree view, first widget's grow_factor must be 0.", symbols);
		}
	}

	if (show_error && !err.empty()) {
		gui2::show_message(gui_->video(), "", err);
	}
	return err.empty();
}

bool mkwin_controller::enumerate_child2(const unit::tchild& child, bool show_error) const
{
	if (!fcallback_(child.window, show_error)) {
		return false;
	}

	for (std::vector<unit*>::const_iterator it = child.units.begin(); it != child.units.end(); ++ it) {
		const unit* u = *it;
		const std::vector<unit::tchild>& children = u->children();
		for (std::vector<unit::tchild>::const_iterator it2 = children.begin(); it2 != children.end(); ++ it2) {
			if (!enumerate_child2(*it2, show_error)) {
				return false;
			}
		}
		if (!fcallback_(u, show_error)) {
			return false;
		}
	}
	return true;
}

bool mkwin_controller::enumerate_child(const std::vector<unit*>& top_units, bool show_error) const
{
	for (std::vector<unit*>::const_iterator it = top_units.begin(); it != top_units.end(); ++ it) {
		const unit* u = *it;
		const std::vector<unit::tchild>& children = u->children();
		for (std::vector<unit::tchild>::const_iterator it2 = children.begin(); it2 != children.end(); ++ it2) {
			if (!enumerate_child2(*it2, show_error)) {
				return false;
			}
		}
		if (!fcallback_(u, show_error)) {
			return false;
		}
	}
	return true;
}

bool mkwin_controller::window_has_valid(bool show_error)
{
	std::stringstream err;
	err << _("Window isn't ready!") << "\n\n";

	const unit* window = get_window();
	if (!window) {
		return false;
	}

	if (window->cell().id.empty() || window->cell().id == gui2::untitled) {
		if (show_error) {
			gui2::show_id_error(*gui_, "id", err.str());
		}
		return false;
	}

	if (window->cell().window.app.empty()) {
		if (show_error) {
			gui2::show_id_error(*gui_, "app", err.str());
		}
		return false;
	}

	std::vector<unit*> top_units = form_top_units();
	{
		tenumerate_lock lock(*this, boost::bind(&mkwin_controller::callback_valid_id, this, _1, _2));
		if (!enumerate_child(top_units, show_error)) {
			return false;
		}
	}
	return true;
}

void mkwin_controller::system()
{
	enum {_LOAD, _SAVE, _SAVE_AS, _PREFERENCES, _QUIT};

	int retval;
	std::vector<gui2::tsystem::titem> items;
	std::vector<int> rets;

	const config top = generate_window_cfg().child("window");
	const unit* window = get_window();
	bool window_dirty = original_.first != window->cell().window.app || original_.second != top;

	{
		std::string str = _("Save");

		items.push_back(gui2::tsystem::titem(_("Load")));
		rets.push_back(_LOAD);

		items.push_back(gui2::tsystem::titem(str, window_has_valid(false) && window_dirty && !file_.empty()));
		rets.push_back(_SAVE);

		items.push_back(gui2::tsystem::titem(_("Save As"), window_has_valid(false)));
		rets.push_back(_SAVE_AS);

		items.push_back(gui2::tsystem::titem(_("Preferences")));
		rets.push_back(_PREFERENCES);

		items.push_back(gui2::tsystem::titem(_("Quit")));
		rets.push_back(_QUIT);

		gui2::tsystem dlg(items);
		try {
			dlg.show(gui_->video());
			retval = dlg.get_retval();
		} catch(twml_exception& e) {
			e.show(*gui_);
			return;
		}
		if (retval == gui2::twindow::OK) {
			return;
		}
	}
	bool require_show_context_menu = false;
	if (rets[retval] == _LOAD) {
		load_window();	

	} else if (rets[retval] == _SAVE) {
		save_window(false);
		require_show_context_menu = true;

	} else if (rets[retval] == _SAVE_AS) {
		save_window(true);
		require_show_context_menu = true;

	} else if (rets[retval] == _PREFERENCES) {
		preferences::show_preferences_dialog(*gui_, true);
		// gui_->redraw_everything();

	} else if (rets[retval] == _QUIT) {
		quit_confirm(QUIT_NORMAL, window_dirty);
	}

	if (require_show_context_menu) {
		gui_->show_context_menu();
	}
}

#include "loadscreen.hpp"

config window_cfg_from_gui_bin()
{
	config cfg;
	wml_config_from_file(game_config::path + "/xwml/" + "gui.bin", cfg);
	const config& gui_cfg = cfg.find_child("gui", "id", "default");
	return gui_cfg.find_child("window", "id", "chat2");
	// return gui_cfg.find_child("window", "id", "recruit");
}

std::vector<std::string> mkwin_controller::generate_textdomains(const std::string& file, bool scene) const
{
	std::string dir = directory_name(file);
	static const std::string dir_separators = "\\/";

	std::string::size_type pos = dir.find_last_of(dir_separators);
	if (pos == dir.size() - 1) {
		dir = dir.substr(0, dir.size() - 1);
	}

	std::string name = file_name(dir);
	if (scene) {
		if (name != "scene") {
			return textdomains_;
		}
	} else if (name != "window") {
		return textdomains_;
	}

	dir = directory_name(dir);
	if (dir.size() < 2) { // 0, 1
		return textdomains_;
	}
	dir = dir.substr(0, dir.size() - 1);
	name = file_name(dir);
	const std::string app = game_config::extract_app_from_app_dir(name);

	std::vector<std::string> res;
	res.push_back("rose-lib");
	if (!app.empty()) {
		res.push_back(app + "-lib");
	}

	return res;
}

void mkwin_controller::load_window()
{
	bool browse_cfg = true;
	std::stringstream err;

	game_config::config_cache& cache = game_config::config_cache::instance();
	config window_cfg;
	if (browse_cfg) {
		std::string initial = game_config::preferences_dir_utf8 + "/editor/maps";
		gui2::tbrowse::tparam param(gui2::tbrowse::TYPE_FILE, true, initial, _("Choose a Window to Open"));
		if (preview_) {
			param.extra = gui2::tbrowse::tentry(game_config::path + "/data/gui/default/scene", _("gui/scene"), "misc/dir-res.png");
		} else {
			param.extra = gui2::tbrowse::tentry(game_config::path + "/data/gui/default/window", _("gui/window"), "misc/dir-res.png");
		}
		gui2::tbrowse dlg(*gui_, param);
		dlg.show(gui_->video());
		int res = dlg.get_retval();
		if (res != gui2::twindow::OK) {
			return;
		}
		std::string file = param.result;

		config cfg;

		{
			game_config::config_cache_transaction main_transaction;
			cache.get_config(widget_template_cfg(), cfg);
			cfg.clear();

			cache.get_config(file, cfg);
		}

		const config& window_cfg1 = cfg.child("window");
		if (!window_cfg1) {
			err << _("Invalid dialog cfg file!");
			gui2::show_message(gui_->video(), "", err.str());
			return;
		}
		window_cfg = window_cfg1;
		
		file_ = file;

		refresh_title();

		textdomains_ = generate_textdomains(file_, preview_);

	} else {
		window_cfg = window_cfg_from_gui_bin();
	}

	const config& res_cfg = window_cfg.child("resolution");
	if (!res_cfg) {
		return;
	}

	tused_widget_tpl_lock tpl_lock(*this);
	{
		const config& top_grid = res_cfg.child("grid");
		if (!top_grid) {
			return;
		}

		top_.erase(units_);
		
		top_.from(*this, *gui_, units_, NULL, -1, top_grid);
		top_.window->from(window_cfg);
		form_linked_groups(res_cfg);
		form_context_menus(res_cfg);
		form_float_widgets(res_cfg);

		layout_dirty();
	}

	if (!used_widget_tpl_.empty()) {
		// erase [linked_group] belong to widget template.
		std::set<std::string> erase_ids;
		{
			game_config::config_cache_transaction main_transaction;

			config cfg;
			cache.get_config(widget_template_cfg(), cfg);

			preproc_map& map = cache.make_copy_map();

			for (std::set<const config*>::const_iterator it = used_widget_tpl_.begin(); it != used_widget_tpl_.end(); ++ it ) {
				const config& tpl_cfg = **it;
				preproc_map::const_iterator it2 = map.find(tpl_cfg["linked_group"]);
				if (it2 != map.end()) {
					const preproc_define& define = it2->second;
					::read(cfg, define.value);
					BOOST_FOREACH (const config& linked, cfg.child_range("linked_group")) {
						erase_ids.insert(linked["id"]);
					}
				}
			}
		}
		
		for (std::vector<gui2::tlinked_group>::iterator it = linked_groups_.begin(); it != linked_groups_.end(); ) {
			const std::string& id = it->id;
			if (erase_ids.find(id) != erase_ids.end()) {
				it = linked_groups_.erase(it);
			} else {
				++ it;
			}
		}
	}

	fill_object_list();
	selected_hex_ = map_location();

	original_.first = top_.window->cell().window.app;
	// window_cfg is format from *.cfg directly that parsed macro.
	// here requrie format that is tpl_widget.
	original_.second = generate_window_cfg().child("window");
}

void mkwin_controller::save_window(bool as)
{
	VALIDATE(as || !file_.empty(), "When save, file_ must not be empty!");

	const unit* u = get_window();
	std::string fname;
	if (!as) {
		fname = file_;
	} else {
		fname = game_config::preferences_dir_utf8 + "/editor/maps/";

		gui2::tbrowse::tparam param(gui2::tbrowse::TYPE_FILE, false, fname, _("Choose a Window to Save"), _("Save"));
		if (preview_) {
			param.extra = gui2::tbrowse::tentry(game_config::path + "/data/themes", _("data/themes"), "misc/dir-res.png");
		} else {
			param.extra = gui2::tbrowse::tentry(game_config::path + "/data/gui/default/window", _("gui/window"), "misc/dir-res.png");
		}

		gui2::tbrowse dlg(*gui_, param);
		dlg.show(gui_->video());
		int res = dlg.get_retval();
		if (res != gui2::twindow::OK) {
			return;
		}
		fname = param.result;
	}

	const std::string textdomain = u->cell().window.app + "-lib";
	std::stringstream ss;

	ss << "#textdomain " << textdomain << "\n";
	ss << "\n";

	VALIDATE(!preview_, null_str);

	tused_widget_tpl_lock tpl_lock(*this);
	config top = generate_window_cfg();
	if (!as) {
		original_.second = top.child("window");
	}

	if (!used_widget_tpl_.empty()) {
		config& res_cfg = top.child("window").child("resolution");
		for (std::set<const config*>::const_iterator it = used_widget_tpl_.begin(); it != used_widget_tpl_.end(); ++ it ) {
			const config& tpl_cfg = **it;
			const std::string& id = tpl_cfg["id"].str();
			if (!tpl_cfg["linked_group"].str().empty()) {
				res_cfg[noise_config_key_tpl("linked_group", id)] = tpl_cfg["linked_group"].str();
			}
		}
	}
		

	::write(ss, top, 0, textdomain);
	std::string data = ss.str();
	if (!used_widget_tpl_.empty()) {
		for (std::set<const config*>::const_iterator it = used_widget_tpl_.begin(); it != used_widget_tpl_.end(); ++ it ) {
			const config& tpl_cfg = **it;
			const std::string& id = tpl_cfg["id"].str();
			std::stringstream src, dst;

			std::string key = noise_config_key_tpl("widget", id);
			src << key << "=\"" << tpl_cfg["widget"].str() << "\"";
			dst << "{" << tpl_cfg["widget"].str() << "}";

			size_t pos = 0;
			while ((pos = data.find(src.str(), pos)) != std::string::npos) {
				data.replace(pos, src.str().size(), dst.str());
				pos += dst.str().size();
			}

			if (!tpl_cfg["linked_group"].str().empty()) {
				src.str("");
				key = noise_config_key_tpl("linked_group", id);
				src << key << "=\"" << tpl_cfg["linked_group"].str() << "\"";
				dst.str("");
				dst << "{" << tpl_cfg["linked_group"].str() << "}";
				pos = 0;
				while ((pos = data.find(src.str(), pos)) != std::string::npos) {
					data.replace(pos, src.str().size(), dst.str());
					pos += dst.str().size();
				}
			}
		}
	}
	write_file(fname, data.c_str(), data.size());

	if (!as) {
		original_.first = u->cell().window.app;
	}
}

void mkwin_controller::quit_confirm(QUIT_MODE mode, bool dirty)
{
	std::string message = _("Do you really want to quit?");
	if (dirty) {
		utils::string_map symbols;
		symbols["window"] = preview_? _("Theme"): _("Dialog");
		std::string str = vgettext2("Changes in the $window since the last save will be lost.", symbols);
		message += "\n\n" + tintegrate::generate_format(str, "red");
	}
	const int res = gui2::show_message(gui().video(), _("Quit"), message, gui2::tmessage::yes_no_buttons);
	if (res != gui2::twindow::CANCEL) {
		do_quit_ = true;
		quit_mode_ = mode;
	}
}

bool mkwin_controller::app_mouse_motion(const int x, const int y, const bool minimap)
{
	if (minimap) {
		return true;
	}

	map_location hex_clicked = gui_->screen_2_loc(x, y);
	last_hex_ = hex_clicked;
	last_unit_ = units_.unit_clicked_on(x, y, last_hex_);

	gui_->highlight_hex(hex_clicked);

	if (cursor::get() != cursor::WAIT) {
		if (current_unit_ && !last_hex_.x && !last_hex_.y) {
			// cursor::set(cursor::ILLEGAL);
			cursor::set(cursor::NORMAL);
			gui_->set_mouseover_hex_overlay(NULL);

		} else if (!last_hex_.x || !last_hex_.y || !map_.on_board(last_hex_)) {
			// cursor::set(cursor::INTERIOR);
			cursor::set(cursor::NORMAL);
			gui_->set_mouseover_hex_overlay(NULL);

		} else {
			// no selected unit or we can't move it
			cursor::set(cursor::NORMAL);
			gui_->resume_mouseover_hex_overlay();
		}
	}

	return true;
}

void mkwin_controller::select_unit(unit* u)
{
	gui_->scroll_to_tile(*u->get_draw_locations().begin(), display::ONSCREEN, true, true);

	selected_hex_ = u->get_location();
	gui_->show_context_menu();
}

void mkwin_controller::unpack_widget_tpl(unit* u)
{
	utils::string_map symbols;
	std::stringstream ss;
	symbols["id"] = tintegrate::generate_format(u->cell().id, "red");
	ss << vgettext2("Once unpake, can not be back to template. Do you want to unpake $id?", symbols);
	int res = gui2::show_message(gui_->video(), "", ss.str(), gui2::tmessage::yes_no_buttons);
	if (res != gui2::twindow::OK) {
		return;
	}

	unit::tparent parent = u->parent();
	gui2::tgrid::tchild cell = u->cell().widget.cell;
	
	std::string tpl_id = unit::extract_from_tpl_widget_id(u->cell().id);
	const config& tpl_cfg = top_config_.find_child("tpl_widget", "id", tpl_id);
	
	config widget_cfg, linked_group_cfg;
	{
		game_config::config_cache& cache = game_config::config_cache::instance();
		game_config::config_cache_transaction main_transaction;

		config cfg;
		cache.get_config(widget_template_cfg(), cfg);

		preproc_map& map = cache.make_copy_map();
		
		preproc_map::const_iterator it = map.find(tpl_cfg["widget"]);
		if (it != map.end()) {
			const preproc_define& define = it->second;
			::read(widget_cfg, define.value);
		}

		it = map.find(tpl_cfg["linked_group"]);
		if (it != map.end()) {
			const preproc_define& define = it->second;
			::read(linked_group_cfg, define.value);
		}
	}

	units_.erase(selected_hex_);

	BOOST_FOREACH (const config& linked, linked_group_cfg.child_range("linked_group")) {
		linked_groups_.push_back(gui2::tlinked_group(linked));
	}

	BOOST_FOREACH (const config::any_child& c, widget_cfg.all_children_range()) {
		const std::string& type = c.key;
		const std::string& definition = c.cfg["definition"].str();
		gui2::tcontrol_definition_ptr ptr;
		if (type != "grid") {
			ptr = mkwin_display::find_widget(*gui_, type, definition, c.cfg["id"].str());
		}
		u = new unit(*this, *gui_, units_, std::make_pair(type, ptr), parent.u, parent.number);
		units_.insert(selected_hex_, u);
		u->from_widget(widget_cfg, true);
	}
	u->cell().widget.cell = cell;
	// modify id. can not be back to template when reload late.
	u->cell().id = tpl_id;

	replace_child_unit(u);
	if (u->has_child()) {
		layout_dirty();
	}
	gui_->show_context_menu();
}

void mkwin_controller::insert_used_widget_tpl(const config& tpl_cfg)
{
	VALIDATE(!tpl_cfg["id"].str().empty(), null_str);
	VALIDATE(!tpl_cfg["widget"].str().empty(), null_str);
	used_widget_tpl_.insert(&tpl_cfg);
}

void mkwin_controller::derive_create(unit* u)
{
	int create_childs = 0;
	if (u->is_grid() || u->is_stack() || u->is_panel() || u->is_scroll_panel()) {
		create_childs = 1;

	} else if (u->is_listbox()) {
		u->insert_listbox_child(default_child_width, default_child_height);
		return;
	} else if (u->is_tree_view()) {
		u->insert_treeview_child();
		return;
	}
	for (int i = 0; i < create_childs; i ++) {
		u->insert_child(default_child_width, default_child_height);
	}
}

void mkwin_controller::replace_child_unit(unit* u)
{
	const unit::tparent& parent = u->parent();
	unit::tchild& child = parent.u? parent.u->child(parent.number): top_;

	const map_location& window_loc = child.window->get_location();
	int pitch = (u->get_location().y - 1 - window_loc.y) * child.cols.size();
	int at = pitch + u->get_location().x - 1 - window_loc.x;
	child.units[at] = u;
}

void mkwin_controller::app_left_mouse_down(const int x, const int y, const bool minimap)
{
	if (minimap) {
		return;
	}

	if (!map_.on_board(last_hex_)) {
		return;
	}
	if (in_theme_top()) {
		int type = last_unit_? dynamic_cast<unit*>(last_unit_)->type(): unit::NONE;
		if (type == unit::WINDOW || type == unit::WIDGET) {
			selected_hex_ = last_unit_->get_location();
		} else {
			selected_hex_ = map_location::null_location;
		}
	} else {
		selected_hex_ = last_hex_;
	}
	
	unit* u = NULL;
	bool valid = false;
	if (last_hex_.x && last_hex_.y) {
		u = units_.find_unit(selected_hex_);
		if (!preview_ && u && gui_->selected_widget() != gui_->spacer && u->is_spacer()) {
			unit::tparent parent = u->parent();
			units_.erase(selected_hex_);

			units_.insert(selected_hex_, new unit(*this, *gui_, units_, gui_->selected_widget(), parent.u, parent.number));
			u = units_.find_unit(selected_hex_);
			derive_create(u);
			
			if (u->is_tpl()) {
				u->cell().id = unit::form_tpl_widget_id(unit::extract_from_widget_tpl(u->widget().first));
			}

			replace_child_unit(u);
			if (u->has_child()) {
				layout_dirty();
			}

			valid = true;
		}
	}

	if (valid) {
		fill_object_list();
	}
	gui_->show_context_menu();
}

void mkwin_controller::app_right_mouse_down(const int x, const int y)
{
	do_right_click();
}

void mkwin_controller::do_right_click()
{
	selected_hex_ = map_location();
	gui_->show_context_menu();
}

void mkwin_controller::post_change_resolution()
{
	set_status();

	unit* u = units_.find_unit(selected_hex_);
	if (u) {
		if (in_theme_top()) {
			gui_->scroll_to_xy(u->get_rect().x, u->get_rect().y, display::ONSCREEN);
		} else {
			gui_->scroll_to_tile(u->get_location(), display::ONSCREEN);
		}
	}

	gui_->show_context_menu();
}

void mkwin_controller::click_widget(const std::string& type, const std::string& definition)
{
	if (type != "grid") {
		gui_->click_widget(type, definition);
	} else {
		gui_->click_grid(type);
	}
}

void mkwin_controller::app_toggle_report(gui2::twidget& widget)
{
	long index = (long)widget.cookie();

	gui2::tmkwin_scene* scene = dynamic_cast<gui2::tmkwin_scene*>(gui_->get_theme());
	if (index == 0) {
		scene->load_widget_page();
	} else if (index == 1) {
		scene->load_object_page(units_);
	}
}

void mkwin_controller::form_linked_groups(const config& res_cfg)
{
	linked_groups_.clear();

	BOOST_FOREACH (const config& linked, res_cfg.child_range("linked_group")) {
		linked_groups_.push_back(gui2::tlinked_group(linked));
	}
}

void mkwin_controller::generate_linked_groups(config& res_cfg) const
{
	config tmp;
	for (std::vector<gui2::tlinked_group>::const_iterator it = linked_groups_.begin(); it != linked_groups_.end(); ++ it) {
		const gui2::tlinked_group& linked = *it;

		tmp.clear();
		tmp["id"] = linked.id;
		if (linked.fixed_width) {
			tmp["fixed_width"] = true;
		}
		if (linked.fixed_height) {
			tmp["fixed_height"] = true;
		}
		res_cfg.add_child("linked_group", tmp);
	}
}

void mkwin_controller::form_context_menu(tmenu2& menu, const config& cfg, const std::string& menu_id)
{
	const std::string& items = cfg[menu_id].str();
	std::vector<std::string> vstr = utils::split(items);
	std::vector<std::string> vstr2;
	size_t flags;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::string& id = *it;
		vstr2 = utils::split(id, '|');
		flags = 0;
		if (vstr2.size() >= 2) {
			flags = gui2::tcontext_menu::decode_flag_str(vstr[1]);
		}
		menu.items.push_back(tmenu2::titem(vstr2[0], NULL, flags & gui2::tcontext_menu::F_HIDE, flags & gui2::tcontext_menu::F_PARAM));
		size_t pos = id.rfind("_m");
		if (id.size() <= 2 || pos != id.size() - 2) {
			continue;
		}
		const std::string sub = id.substr(0, pos);
		menu.items.back().id = sub;
		menu.items.back().submenu = new tmenu2(null_str, sub, &menu);
		tmenu2& menu2 = *menu.items.back().submenu;
		form_context_menu(menu2, cfg, menu2.id);
	}
}

void mkwin_controller::form_context_menus(const config& res_cfg)
{
	context_menus_.clear();

	std::vector<std::string> vstr, vstr2;
	BOOST_FOREACH (const config &cfg, res_cfg.child_range("context_menu")) {
		const std::string& report = cfg["report"].str();
		const std::string& main = cfg["main"].str();
		if (report.empty() || main.empty()) {
			continue;
		}
		context_menus_.push_back(tmenu2(report, "main", NULL));

		tmenu2& menu = context_menus_.back();
		form_context_menu(menu, cfg, menu.id);
	}
	if (context_menus_.empty()) {
		context_menus_.push_back(tmenu2(null_str, "main", NULL));
	}
}

void mkwin_controller::generate_context_menus(config& res_cfg)
{
	for (std::vector<tmenu2>::const_iterator it = context_menus_.begin(); it != context_menus_.end(); ++ it) {
		const tmenu2& menu = *it;
		if (menu.items.empty()) {
			continue;
		}
		config& cfg = res_cfg.add_child("context_menu");
		menu.generate(cfg);
	}
}

void mkwin_controller::form_float_widgets(const config& res_cfg)
{
	float_widgets_.clear();

	std::vector<std::string> vstr, vstr2;
	BOOST_FOREACH (const config &cfg, res_cfg.child_range("float_widget")) {
		std::string type, definition, id;
		BOOST_FOREACH (const config::any_child &c, cfg.all_children_range()) {
			type = c.key;
			definition = c.cfg["definition"].str();
			id = c.cfg["id"].str();
			break;
		}
		if (reserve_float_widget_ids_.find(id) != reserve_float_widget_ids_.end()) {
			continue;
		}

		float_widgets_.push_back(tfloat_widget());
		float_widgets_.back().from(*this, *gui_, units_, cfg, type, definition, id);
	}
}

void mkwin_controller::generate_float_widgets(config& res_cfg)
{
	for (std::vector<tfloat_widget>::const_iterator it = float_widgets_.begin(); it != float_widgets_.end(); ++ it) {
		const tfloat_widget& item = *it;

		config& cfg = res_cfg.add_child("float_widget");
		item.generate(reserve_float_widget_ids_, cfg);
	}

	struct titem {
		titem(const std::string& control, const std::string& definition, const std::string& id)
			: control(control)
			, definition(definition)
			, id(id)
		{}

		std::string control;
		std::string definition;
		std::string id;
	};

	// fake float_widget. include all default float widgets.
	titem item("spacer", "default", "_tpl_common_float_widgets");

	config tooltip_cfg;
	config& label_cfg = tooltip_cfg.add_child(item.control);
	label_cfg["id"] = item.id;
	label_cfg["definition"] = item.definition;
	{
		tfloat_widget tmp;
		tmp.u.reset(new unit(*this, *gui_, units_, std::make_pair(item.control, gui_->find_widget(*gui_, item.control, item.definition, item.id)), nullptr, -1));
		tmp.u->from(tooltip_cfg);

		tmp.generate(reserve_float_widget_ids_, res_cfg);
	}
}

void mkwin_controller::insert_float_widget(const std::string& id)
{
	VALIDATE(!id.empty(), null_str);

	float_widgets_.push_back(tfloat_widget());
	tfloat_widget& tmp = float_widgets_.back();

	const std::string type = "image";
	const std::string definition = "default";
	tmp.u.reset(new unit(*this, *gui_, units_, std::make_pair(type, gui_->find_widget(*gui_, type, definition, id)), nullptr, -1));
	tmp.u->cell().id = id;
}

config mkwin_controller::generate_window_cfg() const
{
	config top, tmp;
	config& window_cfg = top.add_child("window");
	units_.find_unit(map_location(0, 0))->generate(window_cfg);
	config& res_cfg = window_cfg.child("resolution");

	config& top_grid = res_cfg.add_child("grid");

	int rows = (int)top_.rows.size();
	int cols = (int)top_.cols.size();

	config unit_cfg;
	config* row_cfg = NULL;
	int current_y = -1;
	for (int y = 1; y < rows + 1; y ++) {
		for (int x = 1; x < cols + 1; x ++) {
			unit* u = units_.find_unit(map_location(x, y));
			if (u->get_location().y != current_y) {
				row_cfg = &top_grid.add_child("row");
				current_y = u->get_location().y;

				unit* row = units_.find_unit(map_location(0, y));
				row->generate(*row_cfg);
			}
			unit_cfg.clear();
			u->generate(unit_cfg);
			if (y == 1) {
				unit* column = units_.find_unit(map_location(x, 0));
				column->generate(unit_cfg);
			}
			row_cfg->add_child("column", unit_cfg);
		}
	}

	return top;
}

std::vector<unit*> mkwin_controller::form_top_units() const
{
	std::vector<unit*> result;

	if (!current_unit_) {
		if (preview_) {
			VALIDATE(top_.units.empty(), "units in top_ must be empty!");
			result = units_.form_units();

		} else {
			result = top_.units;
		}

	} else {
		result = theme_top_.units;
	}

	return result;
}

void mkwin_controller::reload_map(int w, int h)
{
	map_ = tmap(generate_map_data(w, h, in_theme_top()));
	gui_->reload_map();
	units_.zero_map();
	units_.create_coor_map(map_.w(), map_.h());
}

bool same_parent(const unit* a, const unit* b)
{
	const unit* a_parent = a;
	const unit* b_parent = b;

	while (a_parent->parent().u) {
		a_parent = a_parent->parent().u;
		if (a_parent == b) {
			return true;
		}
	}
	while (b_parent->parent().u) {
		b_parent = b_parent->parent().u;
		if (b_parent == a) {
			return true;
		}
	}
	return false;
}

bool mkwin_controller::can_copy(const unit* u, bool cut) const
{
	if (preview_) {
		return false;
	}
	if (!u) {
		return false;
	}
	if (u == copied_unit_) {
		if (cut_) {
			return !cut;
		} else {
			return cut;
		}
	}
	if (u->type() == unit::WINDOW) {
		return false;
	}
	if (u->is_main_map()) {
		return false;
	}
	return true;
}

bool mkwin_controller::can_paste(const unit* u) const
{
	if (preview_) {
		return false;
	}
	if (!u || !copied_unit_ || u == copied_unit_) {
		return false;
	}
	if (u->type() != copied_unit_->type()) {
		return false;
	}

	unit::tparent this_parent = u->parent();
	unit::tparent that_parent = copied_unit_->parent();

	if (u->type() == unit::ROW) {
		int this_cols = this_parent.u? this_parent.u->child(this_parent.number).cols.size(): top_.cols.size(); 
		int that_cols = that_parent.u? that_parent.u->child(that_parent.number).cols.size(): top_.cols.size();
		if (this_cols != that_cols) {
			return false;
		}
		
	} else if (u->type() == unit::COLUMN) {
		int this_rows = this_parent.u? this_parent.u->child(this_parent.number).rows.size(): top_.rows.size(); 
		int that_rows = that_parent.u? that_parent.u->child(that_parent.number).rows.size(): top_.rows.size();
		if (this_rows != that_rows) {
			return false;
		}

	} else if (!u->is_spacer()) {
		return false;
	}

	return !same_parent(u, copied_unit_);
}

bool mkwin_controller::can_adjust_row(const unit* u) const
{
	const unit::tparent& parent = u->parent();
	if (parent.u) {
		// return !parent.u->is_listbox() || parent.number != 1;
	}
	return true;
}

bool mkwin_controller::can_adjust_column(const unit* u) const
{
	const unit::tparent& parent = u->parent();
	if (parent.u) {
		return !parent.u->is_listbox() || parent.number != 1;
	}
	return true;
}

bool mkwin_controller::can_erase(const unit* u) const
{
	if (preview_) {
		return false;
	}

	if (!u) {
		return false;
	}

	const unit::tparent& parent = u->parent();

	if (u->type() == unit::WINDOW) {
		return parent.u && parent.u->is_grid();
	}
	if (u->type() != unit::WIDGET) {
		return false;
	}
	if (u->is_spacer() || u->is_main_map()) {
		return false;
	}
	if (parent.u) {
		return !parent.u->is_listbox() || parent.number != 1;
	}
	return true;
}

bool mkwin_controller::in_context_menu(const std::string& id) const
{
	using namespace gui2;
	std::pair<std::string, std::string> item = gui2::tcontext_menu::extract_item(id);
	int command = hotkey::get_hotkey(item.first).get_id();

	const unit* u = units_.find_unit(selected_hex_, true);

	switch(command) {
	// idle section
	case tmkwin_scene::HOTKEY_BUILD:
	{
		gui2::tmkwin_scene* scene = dynamic_cast<gui2::tmkwin_scene*>(gui_->get_theme());
		return !selected_hex_.valid() && scene->require_build();
	}

	case tmkwin_scene::HOTKEY_PREVIEW:
	case HOTKEY_ZOOM_IN:
	case HOTKEY_ZOOM_OUT:
		return preview_ || !selected_hex_.valid();
	case HOTKEY_SYSTEM:
		return !selected_hex_.valid() && !preview_;

	case tmkwin_scene::HOTKEY_SETTING: // setting
		return !preview_ && u && (u->type() != unit::WINDOW || (!u->parent().u || u->parent().u->is_stack()));
	case tmkwin_scene::HOTKEY_SPECIAL_SETTING:
		return !preview_ && u && u->has_special_setting();

	case HOTKEY_COPY:
		return can_copy(u, false);
	case HOTKEY_CUT:
		return can_copy(u, true);
	case HOTKEY_PASTE:
		return can_paste(u);

	// unit
	case tmkwin_scene::HOTKEY_ERASE: // erase
		return can_erase(u);

	// window
	case tmkwin_scene::HOTKEY_LINKED_GROUP:
		return u && ((u->type() == unit::WINDOW && !u->parent().u) || u->is_listbox());

	// row
	case tmkwin_scene::HOTKEY_INSERT_TOP:
	case tmkwin_scene::HOTKEY_INSERT_BOTTOM:
	case tmkwin_scene::HOTKEY_ERASE_ROW:
		return !in_theme_top() && u && u->type() == unit::ROW && can_adjust_row(u);

	// column
	case tmkwin_scene::HOTKEY_INSERT_LEFT:
	case tmkwin_scene::HOTKEY_INSERT_RIGHT:
	case tmkwin_scene::HOTKEY_ERASE_COLUMN:
		return !in_theme_top() && u && u->type() == unit::COLUMN && can_adjust_column(u);

	case tmkwin_scene::HOTKEY_INSERT_CHILD: // add a page
		return u && u->is_stack();
	case tmkwin_scene::HOTKEY_ERASE_CHILD: // erase this page
		return u && u->type() == unit::WINDOW && u->parent().u && u->parent().u->is_stack() && u->parent().u->children().size() >= 2;

	case tmkwin_scene::HOTKEY_UNPACK:
		return u && u->is_tpl();

	default:
		return false;
	}

	return false;
}

bool mkwin_controller::actived_context_menu(const std::string& id) const
{
	using namespace gui2;
	std::pair<std::string, std::string> item = gui2::tcontext_menu::extract_item(id);
	int command = hotkey::get_hotkey(item.first).get_id();

	const unit* u = units_.find_unit(selected_hex_, true);
	if (!u) {
		return true;
	}
	const unit::tparent& parent = u->parent();
	const unit::tchild& child = parent.u? parent.u->child(parent.number): top_;

	switch(command) {
	// row
	case tmkwin_scene::HOTKEY_ERASE_ROW:
		return child.rows.size() >= 2;

	// column
	case tmkwin_scene::HOTKEY_ERASE_COLUMN:
		return child.cols.size() >= 2;

	}
	return true;
}

bool mkwin_controller::verify_new_float_widget(const std::string& label, const int at)
{
	const int min_id_len = 1;
	const int max_id_len = 32;

	const std::string id = utils::lowercase(label);
	if (!utils::isvalid_id(id, false, min_id_len, max_id_len)) {
		return false;
	}
	int at2 = 0;
	for (std::vector<tfloat_widget>::const_iterator it = float_widgets_.begin(); it != float_widgets_.end(); ++ it, at2 ++) {
		if (at != at2 && it->u->cell().id == id) {
			return false;
		}
	}
	return true;
}

void mkwin_controller::refresh_title()
{
	std::stringstream ss;
	gui2::tcontrol* label = dynamic_cast<gui2::tcontrol*>(gui_->get_theme_object("title"));
	if (preview_) {
		ss << _("Theme");
	} else {
		ss << _("Dialog");
	}
	ss << " (";

	std::string file = "---";
	if (!file_.empty()) {
		file = file_name(file_);
	}
	ss << tintegrate::generate_format(file, "blue");
	ss << ")";

	label->set_label(ss.str());
}

