/* $Id: text_box.cpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
/*
   Copyright (C) 2008 - 2012 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/widgets/text_box.hpp"

#include "font.hpp"
#include "gui/auxiliary/widget_definition/text_box.hpp"
#include "gui/auxiliary/window_builder/text_box.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/menu.hpp"
#include "filesystem.hpp"
#include "gettext.hpp"
#include "rose_config.hpp"
#include "theme.hpp"

#include "display.hpp"
#include <boost/bind.hpp>

namespace gui2 {

SDL_Rect dbg_start_rect{0, 0, 0, 0};
SDL_Rect dbg_end_rect{0, 0, 0, 0};

REGISTER_WIDGET(text_box)

ttext_box::ttext_box(bool multi_line)
	: tcontrol(COUNT)
	, multi_line_(multi_line)
	, state_(ENABLED)
	, selection_start_(::create_rect(0, 0, 0, 0))
	, selection_end_(::create_rect(twidget::npos, twidget::npos, 0, 0))
	, maximum_length_(std::string::npos)
	, integrate_default_color_(0)
	, src_pos_(-1)
	, hide_cursor_(true)
	, forbid_hide_ticks_(0)
	, text_x_offset_(0)
	, text_y_offset_(0)
	, text_height_(0)
	, selectioning_(false)
	, first_coordinate_(construct_null_coordinate())
	, selection_threshold_(600)
	, start_selection_ticks_(0)
	, xpos_(0)
{
	text_editable_ = true;

	connect_signal<event::SDL_KEY_DOWN>(boost::bind(
			&ttext_box::signal_handler_sdl_key_down, this, _2, _3, _5, _6, _7));

	connect_signal<event::SDL_TEXT_INPUT>(boost::bind(
			&ttext_box::signal_handler_sdl_text_input, this, _2, _3, _5));

	connect_signal<event::RECEIVE_KEYBOARD_FOCUS>(boost::bind(
			&ttext_box::signal_handler_receive_keyboard_focus, this, _2));
	connect_signal<event::LOSE_KEYBOARD_FOCUS>(boost::bind(
			&ttext_box::signal_handler_lose_keyboard_focus, this, _2));

	set_wants_mouse_left_double_click();

	connect_signal<event::MOUSE_MOTION>(boost::bind(
				&ttext_box::signal_handler_mouse_motion, this, _2, _3, _5, _6));
	connect_signal<event::LEFT_BUTTON_DOWN>(boost::bind(
				&ttext_box::signal_handler_left_button_down, this, _2, _3, _5));
	connect_signal<event::MOUSE_LEAVE>(boost::bind(
				&ttext_box::signal_handler_mouse_leave, this, _2, _3, _5));
	connect_signal<event::LEFT_BUTTON_DOUBLE_CLICK>(boost::bind(&ttext_box
				::signal_handler_left_button_double_click, this, _2, _4));

	connect_signal<event::RIGHT_BUTTON_UP>(boost::bind(&ttext_box::signal_handler_right_button_click, this, _4, _5));
}

void ttext_box::set_maximum_chars(const size_t maximum_chars)
{
	maximum_length_ = maximum_chars;
	set_label(label_);
}

void ttext_box::set_label(const std::string& text)
{
	if (text == label_) {
		return;
	}

	if (text.empty()) {
		goto_start_of_data();
	}

	std::string tmp_text;
	bool use_tmp_text = false;
	if (maximum_length_ != std::string::npos) {
		tmp_text = tintegrate::drop_escape(text);
		size_t size = 0;
		size_t byte_count = 0;
		utils::utf8_iterator it(tmp_text);
		for (; it != utils::utf8_iterator::end(tmp_text); ++ it) {
			if (++ size > maximum_length_) {
				break;
			}
			byte_count += it.substr().second - it.substr().first;
		}
		if (byte_count < tmp_text.size()) {
			use_tmp_text = true;
			tmp_text.erase(byte_count);
			tmp_text = tintegrate::stuff_escape(tmp_text);
		}
	}
	const std::string& result_text = use_tmp_text? tmp_text: text;

	if (result_text != label_ || use_tmp_text) {
		// tcontrol::set_label will call update_canvas, 
		// but did_text_changed_ maybe change some canvas parameter, and require call update_canvas again
		// so don't call tcontrol::set_label directly.
		// tcontrol::set_label(result_text);
		
		label_ = result_text;
		label_size_.second.x = 0;
		// update_canvas();
		// set_dirty();
		calculate_integrate();

		if (did_text_changed_) {
			did_text_changed_(*this);
		}
		
		// default to put the cursor at the end of the buffer.
		update_canvas();
		set_dirty();
	}
}

void ttext_box::set_value(const std::string& label) 
{ 
	set_label(tintegrate::stuff_escape(label)); 
}

std::string ttext_box::get_value() const
{ 
	return tintegrate::drop_escape(label_);
}

void ttext_box::popup_new_window()
{
	show_magnifier(false);
	show_edit_button(false);
}


void ttext_box::did_edit_click(twindow& window, const int id)
{
	if (id == float_widget_select) {
		VALIDATE(selection_start_.x != twidget::npos && selection_end_.x == twidget::npos && text_height_ > 0, null_str);
		selection_end_ = integrate_->next_editable_at(selection_start_);

		set_cursor(selection_end_, true);
	} else if (id == float_widget_copy) {
		copy_selection();

	} else if (id == float_widget_paste) {
		paste_selection();
	}
	show_edit_button(false);
}

void ttext_box::show_magnifier(const bool show)
{
	if (!game_config::mobile) {
		return;
	}

	const SDL_Rect& cursor_rect = selection_end_.x >= 0? selection_end_: selection_start_;

	twindow* window = get_window();
	const std::string magnifier_id = "_tpl_magnifier";
	tfloat_widget* item = window->find_float_widget(magnifier_id);

	if (show) {
		if (label_.empty()) {
			return;
		}
		int cursor_x_offset;
		surface surf = integrate_->magnifier_surf(cursor_rect, cursor_x_offset);
		if (surf) {
			tcontrol& widget = *item->widget.get();
			const tpoint best_size(surf->w + widget.config()->text_extra_width, surf->h + widget.config()->text_extra_height);

			int x, y;
			SDL_GetMouseState(&x, &y);
			const int vertical_gap = 32 * twidget::hdpi_scale;
			if (y - best_size.y - vertical_gap >= window->get_y()) {
				// top
				const int gap = 32 * twidget::hdpi_scale;
				if (x < window->get_x() + gap) {
					x = window->get_x() + gap;
				} else if (x > window->get_x() + (int)window->get_width() - gap) {
					x = window->get_x() + window->get_width() - gap;
				}
				x -= cursor_x_offset;
				y -= best_size.y + vertical_gap;
			} else {
				// left
				const int vertical_gap = 4 * twidget::hdpi_scale;
				const int left_gap = 40 * twidget::hdpi_scale;
				x -= best_size.x + left_gap;
				y = window->get_y() + vertical_gap;
			}
			item->set_ref_widget(this, tpoint(x, y));

			item->widget->set_blits(image::tblit(surf, 0, 0));
			item->widget->set_layout_size(best_size);
			item->set_visible(true);
		}
	} else {
		item->set_ref_widget(this, null_point);
		item->set_visible(false);
	}
}

void ttext_box::show_edit_button(const bool show)
{
	if (!game_config::mobile) {
		return;
	}

	const std::string select_id = "_tpl_edit_select";
	tfloat_widget* select = get_window()->find_float_widget(select_id);

	const std::string copy_id = "_tpl_edit_copy";
	tfloat_widget* copy = get_window()->find_float_widget(copy_id);

	const std::string paste_id = "_tpl_edit_paste";
	tfloat_widget* paste = get_window()->find_float_widget(paste_id);

	twindow* window = get_window();
	if (show) {
		int first_width = 0, total_width = 0;
		if (selection_end_.x == twidget::npos) {
			if (!label_.empty()) {
				total_width += select->widget->get_best_size().x;
			}
		} else {
			total_width += copy->widget->get_best_size().x;
		}
		first_width = total_width;
		if (SDL_HasClipboardText()) {
			// paste
			total_width = paste->widget->get_best_size().x;
		}
		const int min_edge_gap = 4 * twidget::hdpi_scale;
		VALIDATE((int)window->get_width() >= min_edge_gap + total_width, null_str);

		int x, y;
		SDL_GetMouseState(&x, &y);
		if (x < min_edge_gap + total_width) {
			x = min_edge_gap + total_width;
		} else if (x + total_width + min_edge_gap > (int)window->get_width()) {
			x = window->get_width() - total_width - min_edge_gap;
		}
		if (selection_end_.x == twidget::npos) {
			// select
			if (!label_.empty()) {
				select->set_ref_widget(this, tpoint(x, y));
				select->set_visible(true);
			}
		} else {
			// copy
			copy->set_ref_widget(this, tpoint(x, y));
			copy->set_visible(true);
			
		}
		if (SDL_HasClipboardText()) {
			// paste
			x += first_width;
			paste->set_ref_widget(this, tpoint(x, y));
			paste->set_visible(true);
		}
		window->set_did_edit_click(boost::bind(&ttext_box::did_edit_click, this, _1, _2));

	} else {
		select->set_visible(false);

		copy->set_visible(false);

		paste->set_visible(false);
		window->set_did_edit_click(NULL);
	}
}

void ttext_box::set_cursor(const SDL_Rect& offset, const bool select)
{
	if (select || selection_end_.x != twidget::npos) {
		if (offset == selection_start_) {
			selection_end_ = ::create_rect(twidget::npos, twidget::npos, 0, 0);
		} else {
			selection_end_ = offset;
		}
	} else {
		selection_start_ = offset;
	}

	if (selectioning_) {
		show_magnifier(true);
	}

	if (did_cursor_moved_) {
		did_cursor_moved_(*this);
	}
	update_canvas();
	set_dirty();
}

void ttext_box::goto_start_of_data(const bool select)
{ 
	if (!select && selection_end_.x != twidget::npos) {
		clear_selection();
	}
	SDL_Rect offset = integrate_->holden_rect(0, 0);
	set_cursor(offset, select); 
}

void ttext_box::goto_end_of_data(const bool select)
{ 
	if (!select && selection_end_.x != twidget::npos) {
		clear_selection();
	}
	SDL_Rect end = integrate_->editable_at(get_width(), get_height());
	set_cursor(end, select);
}

void ttext_box::select_all() 
{ 
	selection_start_ = integrate_->holden_rect(0, 0);
	goto_end_of_data(true); 
}

void ttext_box::insert_str(const std::string& str)
{
	if (str.empty()) {
		return;
	}

	delete_selection();

	if (maximum_length_ != std::string::npos && integrate_->at_end(selection_start_.x, selection_start_.y)) {
		std::string tmp_text = tintegrate::drop_escape(label_);
		if (utils::utf8str_len(tmp_text) >= maximum_length_) {
			return;
		}
	}
	std::string str2;
	if (str.size() == 1 && str[0] == '\n' && (label_.empty() || (label_[label_.size() - 1] != '\n' && integrate_->at_end(selection_start_.x, selection_start_.y)))) {
		str2 = "\n\n";
	} else {
		str2 = tintegrate::stuff_escape(str);
	}

	if (label().empty() && !integrate_->empty()) {
		// exist placeholder
		integrate_->clear();
	}

	SDL_Rect new_start;
	const std::string& text = integrate_->insert_str(true, selection_start_.x, selection_start_.y, str2, new_start);
	if (!multi_line_ && selection_end_.y == twidget::npos && selection_start_.x != new_start.x) {
		const int text_maximum_width = tcontrol::get_text_maximum_width();
		if (new_start.x + xpos_ > text_maximum_width) {
			xpos_ = -1 * (new_start.x - text_maximum_width);
		}
	}
	// after delete, start maybe in end outer.
	selection_start_ = new_start;

	set_label(text);

	set_cursor(selection_start_, false);
}

void ttext_box::insert_img(const std::string& str)
{
	if (str.empty()) {
		return;
	}

	delete_selection();

	if (maximum_length_ != std::string::npos && integrate_->at_end(selection_start_.x, selection_start_.y)) {
		std::string tmp_text = tintegrate::drop_escape(label_);
		if (utils::utf8str_len(tmp_text) >= maximum_length_) {
			return;
		}
	}
	
	std::string str2 = tintegrate::generate_img(str);
	SDL_Rect new_start;
	const std::string& text = integrate_->insert_str(false, selection_start_.x, selection_start_.y, str2, new_start);
	// after delete, start maybe in end outer.
	selection_start_ = new_start;

	set_label(text);

	set_cursor(selection_start_, false);
}

int ttext_box::get_src_pos() const
{
	if (!integrate_) {
		return 0;
	}
	return integrate_->calculate_src_pos(selection_start_.x, selection_start_.y);
}

void ttext_box::set_src_pos(int pos)
{
	src_pos_ = pos;
	if (src_pos_ != -1 && integrate_) {
		selection_start_ = integrate_->calculate_cursor(src_pos_);
		src_pos_ = -1;
		set_cursor(selection_start_, false);
	}
}

void ttext_box::copy_selection()
{
	if (!exist_selection()) {
		return;
	}

	SDL_Rect start, end;
	normalize_start_end(start, end);

	std::string text = integrate_->handle_selection(start.x, start.y, end.x, end.y, NULL);
	if (!text.empty()) {
		SDL_SetClipboardText(text.c_str());
	}
}

void ttext_box::paste_selection()
{
	if (!SDL_HasClipboardText()) {
		return;
	}
	char* text = SDL_GetClipboardText();
	insert_str(text);
	SDL_free(text);
}

void ttext_box::cut_selection()
{
	if (!exist_selection()) {
		return;
	}

	SDL_Rect start, end;
	normalize_start_end(start, end);

	std::string text = integrate_->handle_selection(start.x, start.y, end.x, end.y, NULL);
	if (!text.empty()) {
		SDL_SetClipboardText(text.c_str());
	}
	delete_selection();
}

void ttext_box::normalize_start_end(SDL_Rect& start, SDL_Rect& end) const
{
	bool normal = selection_end_.x < 0 || 
		(selection_end_.y > selection_start_.y || (selection_end_.y == selection_start_.y && selection_end_.x >= selection_start_.x));

	start = normal? selection_start_: selection_end_;
	end = normal? selection_end_: selection_start_;
}

void ttext_box::calculate_integrate()
{
	if (integrate_) {
		delete integrate_;
		integrate_ = nullptr;
	}
	const int max = get_text_maximum_width();
	if (max > 0) {
		uint32_t color = integrate_default_color_;
		if (!color) {
			color = theme::text_color_from_index(text_color_tpl_, theme::normal);
		}

		bool alternated = false;
		if (label_.empty() && !placeholder_.empty()) {
			alternated = true;
			label_ = placeholder_;
			color = theme::text_color_from_index(text_color_tpl_, theme::placeholder);
		}
		// before place, w_ = 0. it indicate not ready.
		integrate_ = new tintegrate(label_, get_text_maximum_width(), -1, get_text_font_size(), uint32_to_color(color), text_editable_);
		if (!locator_.empty()) {
			integrate_->fill_locator_rect(locator_, true);
		}

		if (alternated) {
			label_.clear();
		}
	}

	if (integrate_) {
		if (src_pos_ == -1) {
			selection_start_ = integrate_->editable_at(selection_start_.x, selection_start_.y);
		} else {
			selection_start_ = integrate_->calculate_cursor(src_pos_);
			src_pos_ = -1;
		}
		if (exist_selection()) {
			selection_end_ = integrate_->editable_at(selection_end_.x, selection_end_.y);
		}
	}
}

void ttext_box::clear_selection()
{
	VALIDATE(selection_end_.x != twidget::npos, null_str);
	selection_end_.x = selection_end_.y = twidget::npos;
	selection_end_.w = selection_end_.h = 0;
}

void ttext_box::set_state(const tstate state)
{
	if (state_ == DISABLED && state == FOCUSSED) {
		return;
	}

	if (!cursor_timer_.valid() && state == FOCUSSED) {
		cursor_timer_.reset(add_timer(500, boost::bind(&ttext_box::cursor_timer_handler, this)));
	}

	if (state != state_) {
		state_ = state;
		set_dirty();
	}
}

void ttext_box::handle_key_left_arrow(SDL_Keymod modifier, bool& handled)
{
	/** @todo implement the ctrl key. */

	handled = true;

	if (label_.empty()) {
		return;
	}
	if (!selection_start_.x && !selection_start_.y) {
		return;
	}

	SDL_Rect new_start;
	integrate_->handle_char(false, selection_start_.x, selection_start_.y, true, new_start);
	if (!multi_line_ && selection_end_.y == twidget::npos && selection_start_.x != new_start.x) {
		tpoint text_size = get_best_text_size(INT_MAX);
		const int text_maximum_width = tcontrol::get_text_maximum_width();
		if (text_size.x > text_maximum_width) {
			if (new_start.x < -1 * xpos_) {
				xpos_ = -1 * new_start.x;
			}
		}
	}
	set_cursor(new_start, (modifier & KMOD_SHIFT) != 0);
}

void ttext_box::handle_key_right_arrow(SDL_Keymod modifier, bool& handled)
{
	/** @todo implement the ctrl key. */

	handled = true;
	if (label_.empty()) {
		return;
	}

	SDL_Rect new_start;
	integrate_->handle_char(false, selection_start_.x, selection_start_.y, false, new_start);
	if (!multi_line_ && selection_end_.y == twidget::npos && selection_start_.x != new_start.x) {
		const int text_maximum_width = tcontrol::get_text_maximum_width();
		if (new_start.x + xpos_ > text_maximum_width) {
			xpos_ = -1 * (new_start.x - text_maximum_width);
		}
	}
	set_cursor(new_start, (modifier & KMOD_SHIFT) != 0);
}

void ttext_box::handle_key_up_arrow(SDL_Keymod modifier, bool& handled)
{
	/** @todo implement the ctrl key. */
	handled = true;

	if (label_.empty()) {
		return;
	}
	if (!selection_start_.y) {
		return;
	}

	SDL_Rect new_selection_start = integrate_->key_arrow(selection_start_.x, selection_start_.y, true);
	if (!SDL_RectEmpty(&new_selection_start)) {
		set_cursor(new_selection_start, (modifier & KMOD_SHIFT) != 0);
	}
}

void ttext_box::handle_key_down_arrow(SDL_Keymod modifier, bool& handled)
{
	/** @todo implement the ctrl key. */
	handled = true;

	if (label_.empty()) {
		return;
	}

	SDL_Rect new_selection_start = integrate_->key_arrow(selection_start_.x, selection_start_.y, false);
	if (!SDL_RectEmpty(&new_selection_start)) {
		set_cursor(new_selection_start, (modifier & KMOD_SHIFT) != 0);
	}
}
void ttext_box::handle_key_home(SDL_Keymod modifier, bool& handled)
{
	handled = true;
	if (modifier & KMOD_CTRL) {
		goto_start_of_data((modifier & KMOD_SHIFT) != 0);
	} else {
		goto_start_of_line((modifier & KMOD_SHIFT) != 0);
	}
}

void ttext_box::handle_key_end(SDL_Keymod modifier, bool& handled)
{
	handled = true;
	if(modifier & KMOD_CTRL) {
		goto_end_of_data((modifier & KMOD_SHIFT) != 0);
	} else {
		goto_end_of_line((modifier & KMOD_SHIFT) != 0);
	}
}

void ttext_box::handle_key_backspace(SDL_Keymod /*modifier*/, bool& handled)
{
	handled = true;
	if (exist_selection()) {
		delete_selection();
	} else if (selection_start_.x || selection_start_.y){
		delete_char(true);
	}
	fire2(event::NOTIFY_MODIFIED, *this);
}

void ttext_box::handle_key_delete(SDL_Keymod /*modifier*/, bool& handled)
{
	handled = true;
	if (exist_selection()) {
		delete_selection();
	} else if (!label_.empty()) {
		delete_char(false);
	}
	fire2(event::NOTIFY_MODIFIED, *this);
}

Uint16 shift_character(SDL_Keymod modifier, Uint16 c)
{
	if (c == '\r') {
		return '\n';
	}
	int shifted = !!(modifier & KMOD_SHIFT);
	int capslock = !!(modifier & KMOD_CAPS);
	if (!(shifted ^ capslock)) {
		return c;
	}

	if (c >= 'a' && c <= 'z') {
		return 'A' + (c - 'a');
	} else if (c >= 0x30 && c <= 0x39) {
		if (c == 0x30) {
			return 0x29;
		} else if (c == 0x31) {
			return 0x21;
		} else if (c == 0x32) {
			return 0x40;
		} else if (c == 0x33) {
			return 0x23;
		} else if (c == 0x34) {
			return 0x24;
		} else if (c == 0x35) {
			return 0x25;
		} else if (c == 0x36) {
			return 0x5e;
		} else if (c == 0x37) {
			return 0x26;
		} else if (c == 0x38) {
			return 0x2a;
		} else {
			return 0x28;
		}
	} else if (c == '`') {
		return 0x7e;
	} else if (c == '-') {
		return 0x5f;
	} else if (c == '=') {
		return 0x2b;
	} else if (c == '[') {
		return 0x7b;
	} else if (c == ']') {
		return 0x7d;
	} else if (c == '\\') {
		return 0x7c;
	} else if (c == ';') {
		return 0x3a;
	} else if (c == '\'') {
		return 0x22;
	} else if (c == ',') {
		return 0x3c;
	} else if (c == '.') {
		return 0x3e;
	} else if (c == '/') {
		return 0x3f;
	}
	return c;
}

void ttext_box::handle_key_default(
		bool& handled, SDL_Keycode key, SDL_Keymod modifier, Uint16)
{
#if defined(_WIN32)
	if (!(modifier & KMOD_SHIFT) || key != '\r') {
		return;
	}
#endif

	if (key == '\r' || (key >= 32 && key < 127)) {
		handled = true;

		// sdl.dll can shift 'a' to 'z', other don't.
		int modified = shift_character(modifier, key);
		std::string str(1, modified);
		insert_str(str);
		fire2(event::NOTIFY_MODIFIED, *this);
	}
}

void ttext_box::signal_handler_sdl_key_down(const event::tevent event
		, bool& handled
		, const SDL_Keycode key
		, SDL_Keymod modifier
		, const Uint16 unicode)
{
// For copy/paste we use a different key on the MAC. Other ctrl modifiers won't
// be modifed seems not to be required when I read the comment in
// widgets/textbox.cpp:516. Would be nice if somebody on a MAC would test it.
#ifdef __APPLE__
	const unsigned copypaste_modifier = SDLK_LGUI |SDLK_RGUI;
#else
	const unsigned copypaste_modifier = KMOD_CTRL;
#endif

	switch(key) {

		case SDLK_LEFT :
			handle_key_left_arrow(modifier, handled);
			break;

		case SDLK_RIGHT :
			handle_key_right_arrow(modifier, handled);
			break;

		case SDLK_UP :
			handle_key_up_arrow(modifier, handled);
			break;

		case SDLK_DOWN :
			handle_key_down_arrow(modifier, handled);
			break;

		case SDLK_PAGEUP :
			handle_key_page_up(modifier, handled);
			break;

		case SDLK_PAGEDOWN :
			handle_key_page_down(modifier, handled);
			break;

		case SDLK_a :
			if(!(modifier & KMOD_CTRL)) {
				handle_key_default(handled, key, modifier, unicode);
				break;
			}

			// If ctrl-a is used for home drop the control modifier
			modifier = static_cast<SDL_Keymod>(modifier &~ KMOD_CTRL);
			/* FALL DOWN */

		case SDLK_HOME :
			handle_key_home(modifier, handled);
			break;

		case SDLK_e :
			if(!(modifier & KMOD_CTRL)) {
				handle_key_default(handled, key, modifier, unicode);
				break;
			}

			// If ctrl-e is used for end drop the control modifier
			modifier = static_cast<SDL_Keymod>(modifier &~ KMOD_CTRL);
			/* FALL DOWN */

		case SDLK_END :
			handle_key_end(modifier, handled);
			break;

		case SDLK_BACKSPACE :
			handle_key_backspace(modifier, handled);
			break;

		case SDLK_u :
			if(modifier & KMOD_CTRL) {
				handle_key_clear_line(modifier, handled);
			} else {
				handle_key_default(handled, key, modifier, unicode);
			}
			break;

		case SDLK_DELETE :
#if defined(__APPLE__) && TARGET_OS_IPHONE
			handle_key_backspace(modifier, handled);
#else
			handle_key_delete(modifier, handled);
#endif
			break;

#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
        case SDLK_RETURN:
            SDL_StopTextInput();
            break;
#endif
            
		case SDLK_c :
			if(!(modifier & copypaste_modifier)) {
				handle_key_default(handled, key, modifier, unicode);
				break;
			}

			// atm we don't care whether there is something to copy or paste
			// if nothing is there we still don't want to be chained.
			copy_selection();
			handled = true;
			break;

		case SDLK_x :
			if(!(modifier & copypaste_modifier)) {
				handle_key_default(handled, key, modifier, unicode);
				break;
			}

			copy_selection();
			delete_selection();
			handled = true;
			break;

		case SDLK_v :
			if(!(modifier & copypaste_modifier)) {
				handle_key_default(handled, key, modifier, unicode);
				break;
			}

			paste_selection();
			handled = true;
			break;

		default :
			handle_key_default(handled, key, modifier, unicode);

	}

	hide_cursor_ = false;
	forbid_hide_ticks_ = SDL_GetTicks() + 200;
	cursor_timer_handler();
}

void ttext_box::signal_handler_sdl_text_input(const event::tevent event
		, bool& handled
		, const char* text)
{
	// for windows, ascii char will send WM_KEYDOWN and WM_CHAR,
	// WM_KEYDOWN result to call signal_handler_sdl_key_down,
	// WM_CHAR result to call signal_handler_sdl_text_input,
	// so ascii will generate two input. 
	const std::string str = text;
	bool inserted = false;
	utils::utf8_iterator ch(str);
	for (utils::utf8_iterator end = utils::utf8_iterator::end(str); ch != end; ++ch) {
/*
		if (*ch <= 0x7f) {
#if defined(ANDROID) || defined(_WIN32)
			// on Android, some ASCII is receive by textinput insteal keydown.
			// but some controller char is by keydown, rt, del, etc.
#else
			continue;
#endif
		}
*/
		if (*ch == '\r') {
			insert_str("\n");
		} else {
			insert_str(utils::wchar_to_string(*ch));
		}
		inserted = true;
	}
	if (inserted) {
		fire2(event::NOTIFY_MODIFIED, *this);
	}

	handled = true;
}

void ttext_box::signal_handler_receive_keyboard_focus(const event::tevent event)
{
	set_state(FOCUSSED);
}

void ttext_box::signal_handler_lose_keyboard_focus(const event::tevent event)
{
	set_state(ENABLED);

	show_edit_button(false);
	if (selection_end_.x != twidget::npos) {
		clear_selection();

		hide_cursor_ = true;
		update_canvas();
		set_dirty();
	}
	cursor_timer_.reset();
}

void ttext_box::set_visible_area(const SDL_Rect& area)
{
	tcontrol::set_visible_area(area);
	BOOST_FOREACH(tcanvas& tmp, canvas()) {
		tmp.set_variable("visible_y", variant((area.y - y_) / twidget::hdpi_scale));
		tmp.set_variable("visible_height", variant(area.h / twidget::hdpi_scale));
	}
}

void ttext_box::cancel_start_selection()
{ 
	VALIDATE(start_selection_ticks_ != 0, null_str);
	start_selection_ticks_ = 0; 
}

void ttext_box::cursor_timer_handler()
{
	uint32_t now = SDL_GetTicks();

	if (start_selection_ticks_ && now >= start_selection_ticks_) {
		VALIDATE(!selectioning_, null_str);
		start_selection_ticks_ = 0;
		selectioning_ = true;

		if (selection_end_.x != twidget::npos) {
			clear_selection();

			tpoint mouse(first_coordinate_.x - get_x(), first_coordinate_.y - get_y());
			mouse.x -= xpos_;
			SDL_Rect offset = integrate_->editable_at(mouse.x - text_x_offset_, mouse.y - text_y_offset_);
			set_cursor(offset, false);
			return;
		}

		if (!label_.empty()) {
			show_magnifier(true);
		} else {
			show_edit_button(true);
		}
	}

	if (state_ != FOCUSSED) {
		hide_cursor_ = true;
	} else if (selectioning_ || selection_end_.x != twidget::npos) {
		hide_cursor_ = false;
	} else if (now > forbid_hide_ticks_) {
		hide_cursor_ = !hide_cursor_;
	}

		const uint32_t cursor_color = calculate_cursor_color();

		BOOST_FOREACH(tcanvas& tmp, canvas()) {
			tmp.set_variable("cursor_height", variant(hide_cursor_? 0: text_height_  / twidget::hdpi_scale));
			tmp.set_variable("cursor_color", variant(cursor_color));
		}

	set_dirty();
}

uint32_t ttext_box::calculate_cursor_color() const
{
	uint32_t cursor_color = 0xff000000;
	if (selectioning_ || selection_end_.x != twidget::npos) {
		if (is_null_coordinate(first_coordinate_)) {
			cursor_color = 0xffe4af0a;
		} else {
			cursor_color = 0xffff0000;
		}
		cursor_color = 0xffe4af0a;
	}

	return cursor_color;
}

unsigned ttext_box::cursor_height() const 
{ 
	return text_height_;
}

int ttext_box::get_text_maximum_width() const
{
	const int width = multi_line_? w_: INT_MAX;
	return width - config_->text_extra_width;
}

void ttext_box::update_canvas()
{
	boost::intrusive_ptr<const ttext_box_definition::tresolution> conf =
		boost::dynamic_pointer_cast
		<const ttext_box_definition::tresolution>(config());

	text_height_ = font::get_max_height(get_text_font_size());

	// normally, text_extra_width is used by calculated, to ttext_box, also used prefix pixels.
	text_x_offset_ = conf->text_extra_width / 2;
	text_y_offset_ = conf->text_extra_height / 2;

	/***** Gather the info *****/
	const int max_width = get_text_maximum_width();

	// Set the cursor info.
	SDL_Rect start, end;
	normalize_start_end(start, end);

	int cursor_offset_y = 0;
	const SDL_Rect& cursor_rect = selection_end_.x >= 0? selection_end_: selection_start_;

	if (!integrate_ || integrate_->align_bottom()) {
		cursor_offset_y = cursor_rect.y + (cursor_rect.h - (int)text_height_);
	} else {
		cursor_offset_y = cursor_rect.y + (cursor_rect.h - (int)text_height_) / 2;
	}
	
	if (cursor_offset_y < 0) {
		cursor_offset_y = 0;
	}

	const int selection_width_bonus = 2 * twidget::hdpi_scale; // i think, it is fix for bug. require fixed in future.
	int selection_width_i = 0;
	int selection_height_i = start.h;
	if (end.x != twidget::npos) {
		if (start.y == end.y) {
			selection_width_i = end.x - start.x + selection_width_bonus;
		} else {
			VALIDATE(multi_line_, null_str);
			selection_width_i = max_width - start.x; 
		}
	}

	int selection_height_ii = 0;

	int selection_width_iii = 0;
	int selection_height_iii = 0;
	if (end.x >= 0) {
		if (start.y != end.y) {
			selection_height_ii = end.y - start.y - selection_height_i;

			selection_width_iii = end.x + selection_width_bonus;

			selection_height_iii = end.h;
		}
	}

	uint32_t cursor_color = calculate_cursor_color();

	const SDL_Rect dirty_rect = get_dirty_rect();
	/***** Set in all canvases *****/

	const std::string label2 = label().empty()? placeholder_: label();

	BOOST_FOREACH(tcanvas& tmp, canvas()) {

		tmp.set_variable("text", variant(label2));
		tmp.set_variable("text_x_offset", variant(text_x_offset_ / twidget::hdpi_scale));
		tmp.set_variable("text_y_offset", variant(text_y_offset_ / twidget::hdpi_scale));
		tmp.set_variable("text_maximum_width", variant(max_width / twidget::hdpi_scale));
		tmp.set_variable("text_editable", variant(get_text_editable()));

		tmp.set_variable("cursor_height", variant(hide_cursor_? 0: text_height_/ twidget::hdpi_scale));
		tmp.set_variable("end_height", variant(selection_end_.x != twidget::npos? selection_height_i / twidget::hdpi_scale: 0));

		tmp.set_variable("xpos", variant(xpos_ / twidget::hdpi_scale));

		tmp.set_variable("selection_offset_x_i", variant(start.x / twidget::hdpi_scale));
		tmp.set_variable("selection_offset_y_i", variant(start.y / twidget::hdpi_scale));
		tmp.set_variable("selection_width_i", variant(selection_width_i / twidget::hdpi_scale));
		tmp.set_variable("selection_height_i", variant(selection_height_i / twidget::hdpi_scale));

		tmp.set_variable("selection_height_ii", variant(selection_height_ii / twidget::hdpi_scale));

		tmp.set_variable("selection_width_iii", variant(selection_width_iii / twidget::hdpi_scale));
		tmp.set_variable("selection_height_iii", variant(selection_height_iii / twidget::hdpi_scale));

		tmp.set_variable("visible_y", variant((dirty_rect.y - y_) / twidget::hdpi_scale));
		tmp.set_variable("visible_height", variant(dirty_rect.h / twidget::hdpi_scale));

		tmp.set_variable("drag_offset_x_ii", variant(end.x / twidget::hdpi_scale));
		tmp.set_variable("drag_offset_y_ii", variant(end.y / twidget::hdpi_scale));
		tmp.set_variable("cursor_color", variant(cursor_color));
	}
}

void ttext_box::adjust_xpos_when_delete(const SDL_Rect& new_start)
{
	if (multi_line_ || selection_end_.y != twidget::npos) {
		return;
	}
	if (label_.empty()) {
		return;
	}

	tpoint text_size = get_best_text_size(INT_MAX);
	const int text_maximum_width = tcontrol::get_text_maximum_width();
	if (text_size.x > text_maximum_width) {
		if (new_start.x < -1 * xpos_) {
			const int bonus = new_start.x > text_maximum_width / 4? text_maximum_width / 4: new_start.x;
			xpos_ = -1 * (new_start.x - bonus);
		}
		if (text_size.x + xpos_ < text_maximum_width + config_->text_extra_width / 2) {
			xpos_ = -1 * (text_size.x - text_maximum_width - config_->text_extra_width / 2);
		}
	} else if (xpos_) {
		xpos_ = 0;
	}
}

void ttext_box::delete_char(const bool backspace)
{
	if (label_.empty()) {
		return;
	}

	SDL_Rect start, end;
	normalize_start_end(start, end);

	SDL_Rect new_start;
	const std::string& text = integrate_->handle_char(true, start.x, start.y, backspace, new_start);
	if (text != label()) {
		adjust_xpos_when_delete(new_start);

		selection_start_ = new_start;
		set_label(text);
		// repoint
		set_cursor(selection_start_, false);
	}
}

void ttext_box::delete_selection()
{
	if (!exist_selection()) {
		return;
	}

	SDL_Rect start, end;
	normalize_start_end(start, end);

	SDL_Rect new_start;
	const std::string& text = integrate_->handle_selection(start.x, start.y, end.x, end.y, &new_start);
	// after delete, start maybe in end outer.
	selection_start_ = new_start;
	clear_selection();
	set_label(text);

	adjust_xpos_when_delete(new_start);
	set_cursor(selection_start_, false);
}

void ttext_box::handle_mouse_selection(const tpoint& coordinate, const bool down)
{
	if (!down && label_.empty()) {
        return;
    }

	hide_cursor_ = false;
	forbid_hide_ticks_ = SDL_GetTicks() + 200;
	cursor_timer_handler();

	tpoint mouse(coordinate.x - get_x(), coordinate.y - get_y());
	mouse.x -= xpos_;
	// FIXME we don't test for overflow in width
	if (mouse.x < static_cast<int>(text_x_offset_) || mouse.y < static_cast<int>(text_y_offset_)) {
		return;
	}

	SDL_Rect offset = empty_rect;
	if (!label_.empty()) {
		offset = integrate_->editable_at(mouse.x - text_x_offset_, mouse.y - text_y_offset_);
	}
	if (!down) {
		if (SDL_RectEmpty(&offset)) {
			return;
		} else if (selection_end_.x != twidget::npos && selection_start_ == offset) {
			return;
		}
	}

	if (down && selection_end_.x != twidget::npos) {
		const int expand_x_size = 24 * twidget::hdpi_scale;
		const int expand_y_size = 24 * twidget::hdpi_scale;
		SDL_Rect start_rect {selection_start_.x - expand_x_size, selection_start_.y - expand_y_size, 2 * expand_x_size, selection_start_.h + 2 * expand_y_size};
		SDL_Rect end_rect {selection_end_.x - expand_x_size, selection_end_.y - expand_y_size, 2 * expand_x_size, selection_end_.h + 2 * expand_y_size};

		start_rect.x += x_ + text_x_offset_;
		start_rect.y += y_ + text_y_offset_;
		
		end_rect.x += x_ + text_x_offset_;
		end_rect.y += y_ + text_y_offset_;

		// gap of start.y and end.y maybe very small.
		if (start_rect.y < end_rect.y) {
			if (start_rect.y + start_rect.h >= end_rect.y + expand_y_size) {
				start_rect.h = end_rect.y + expand_y_size - start_rect.y;
			}
		} else if (end_rect.y < start_rect.y) {
			if (end_rect.y + end_rect.h >= start_rect.y + expand_y_size) {
				end_rect.h = start_rect.y + expand_y_size - end_rect.y;
			}

		} else {
			// boat start and end are in same line.
			VALIDATE(start_rect.x != end_rect.x, null_str);

			if (start_rect.x < end_rect.x) {
				// start, end
				const int intersect = start_rect.x + start_rect.w - (end_rect.x + expand_x_size);
				if (intersect > 0) {
					start_rect.w -= intersect;

					const int intersect2 = start_rect.x + start_rect.w - end_rect.x;
					if (intersect2 > 0) {
						end_rect.x += intersect2;
						end_rect.w -= intersect2;
					}
				}
			} else {
				// end, start
				const int intersect = end_rect.x + end_rect.w - (start_rect.x + expand_x_size);
				if (intersect > 0) {
					end_rect.w -= intersect;

					const int intersect2 = end_rect.x + end_rect.w - start_rect.x;
					if (intersect2 > 0) {
						start_rect.x += intersect2;
						start_rect.w -= intersect2;
					}
				}
			}
		}

		start_rect.x += xpos_;
		end_rect.x += xpos_;

		const SDL_Rect rect = get_rect();
		SDL_IntersectRect(&start_rect, &rect, &start_rect);
		SDL_IntersectRect(&end_rect, &rect, &end_rect);

		// dbg_start_rect = start_rect;
		// dbg_end_rect = end_rect;

		bool expand_selection = false;
		if (point_in_rect(coordinate.x, coordinate.y, start_rect)) {
			expand_selection = true;
			// swip start and end
			SDL_Rect tmp = selection_start_;
			selection_start_ = selection_end_;
			selection_end_ = tmp;

		} else if (point_in_rect(coordinate.x, coordinate.y, end_rect)) {
			expand_selection = true;
		}

		if (expand_selection) {
			VALIDATE(!selectioning_, null_str);
			selectioning_ = true;
			first_coordinate_ = coordinate;

			set_cursor(offset, true);
			return;

		} else {
			// don't clear selection immediate. wait start_selection_ticks_. 
			// selection_end_ = ::create_rect(twidget::npos, twidget::npos, 0, 0);

		}
	}

	if (!SDL_RectEmpty(&offset) && (!down || selection_end_.x == twidget::npos)) {
		set_cursor(offset, game_config::mobile? false: !down);
	}
	if (down) {
		VALIDATE(!start_selection_ticks_, null_str);
		start_selection_ticks_ = SDL_GetTicks() + selection_threshold_;
		first_coordinate_ = coordinate;
	}
}

void ttext_box::handle_key_clear_line(SDL_Keymod /*modifier*/, bool& handled)
{
	handled = true;

	set_label("");
}

const std::string& ttext_box::get_control_type() const
{
	static const std::string type = "text_box";
	return type;
}

void ttext_box::signal_handler_mouse_motion(const event::tevent event, bool& handled, const tpoint& coordinate, const tpoint& coordinate2)
{
	if (is_null_coordinate(first_coordinate_)) {
		return;
	}

	if (selectioning_) {
		handle_mouse_selection(coordinate, false);

	} else {
		if (start_selection_ticks_) {
			const int abs_diff_x = abs(coordinate.x - first_coordinate_.x);
			const int abs_diff_y = abs(coordinate.y - first_coordinate_.y);
			const int clear_click_threshold_ = 2 * hdpi_scale;
			if (abs_diff_x >= clear_click_threshold_ || abs_diff_x >= clear_click_threshold_) {
				cancel_start_selection();
			}
		}
		if (!multi_line_ && !label_.empty()) {
			xpos_ = xpos_ + coordinate2.x;
			update_canvas();
			set_dirty();
		}
	}

	handled = true;
}

void ttext_box::signal_handler_left_button_down(const event::tevent event, bool& handled, const tpoint& coordinate)
{
	/*
	 * Copied from the base class see how we can do inheritance with the new
	 * system...
	 */
	get_window()->keyboard_capture(this);
	get_window()->mouse_capture(true);

	show_edit_button(false);

	VALIDATE(is_null_coordinate(first_coordinate_), null_str);
	handle_mouse_selection(coordinate, true);

	handled = true;

	if (game_config::mobile) {
		SDL_StartTextInput();
	}
}

void ttext_box::signal_handler_mouse_leave(const event::tevent event, bool& handled, const tpoint& coordinate)
{
	if (selectioning_) {
		selectioning_ = false;

		if (!label_.empty()) {
			show_magnifier(false);
			show_edit_button(true);
		}
	} else if (selection_end_.x != twidget::npos && start_selection_ticks_) {
		clear_selection();

		tpoint mouse(first_coordinate_.x - get_x(), first_coordinate_.y - get_y());
		mouse.x -= xpos_;
		SDL_Rect offset = integrate_->editable_at(mouse.x - text_x_offset_, mouse.y - text_y_offset_);
		set_cursor(offset, false);

	} else if (!multi_line_ && !label_.empty() && !is_null_coordinate(first_coordinate_)) {
		bool require_correct = false;
		if (xpos_ > 0) {
			xpos_ = 0;
			require_correct = true;
		} else if (xpos_ < 0) {
			tpoint text_size = get_best_text_size(INT_MAX);
			const int text_maximum_width = tcontrol::get_text_maximum_width();
			if (text_size.x <= text_maximum_width) {
				xpos_ = 0;
				require_correct = true;
			} else if (text_size.x + xpos_ < text_maximum_width) {
				xpos_ = -1 * (text_size.x - text_maximum_width);
				require_correct = true;
			}
		}
		if (require_correct) {
			update_canvas();
			set_dirty();
		}
	}

	dbg_start_rect = dbg_end_rect = empty_rect;

	start_selection_ticks_ = 0;
	set_null_coordinate(first_coordinate_);

	handled = true;
}

void ttext_box::signal_handler_left_button_double_click(const event::tevent event, bool& halt)
{
	return;

	select_all();

	halt = true;
}

void ttext_box::signal_handler_right_button_click(bool& halt, const tpoint& coordinate)
{
	std::vector<gui2::tmenu::titem> items;
	
	if (selection_end_.x != twidget::npos) {
		items.push_back(gui2::tmenu::titem(float_widget_copy, std::string(_("Copy"))));
		items.push_back(gui2::tmenu::titem(float_widget_cut, std::string(_("Cut"))));
	}
	if (SDL_HasClipboardText()) {
		// paste
		items.push_back(gui2::tmenu::titem(float_widget_paste, std::string(_("Paste"))));
	}

	if (items.empty()) {
		return;
	}

	int selected;
	{
		display& disp = *display::get_singleton();
		gui2::tmenu dlg(disp, items, twidget::npos);
		dlg.show(disp.video(), coordinate.x, coordinate.y);
		int retval = dlg.get_retval();
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}
		// absolute_draw();
		selected = dlg.selected_val();
	}

	if (selected == float_widget_copy) {
		copy_selection();

	} else if (selected == float_widget_paste) {
		paste_selection();

	} else if (selected == float_widget_cut) {
		cut_selection();
	}
}

} //namespace gui2
