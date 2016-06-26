#define GETTEXT_DOMAIN "rose-lib"

#include "control.hpp"

#include "font.hpp"
#include "formula_string_utils.hpp"
#include "gui/auxiliary/event/message.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/auxiliary/window_builder/control.hpp"
#include "marked-up_text.hpp"
#include "display.hpp"
#include "hotkeys.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#include <iomanip>

#include "posix2.h"

namespace gui2 {

void tfloat_widget::set_visible(bool visible)
{
	twindow::tinvalidate_layout_blocker blocker(window);
	widget->set_visible(visible? twidget::VISIBLE: twidget::INVISIBLE);
}

tfloat_widget::~tfloat_widget()
{
	// when destruct, parent of widget must be window.
	// widget->set_parent(&window);
	VALIDATE(widget->parent() == &window, null_str);
}

void tfloat_widget::set_ref_widget(tcontrol* widget, const tpoint& mouse)
{
	bool dirty = false;
	if (!widget) {
		VALIDATE(is_null_coordinate(mouse), null_str);
	}
	if (widget != ref_widget_) {
		if (ref_widget_) {
			ref_widget_->associate_float_widget(*this, false);
		}
		ref_widget_ = widget;
		dirty = true;
	}
	if (mouse != mouse_) {
		mouse_ = mouse;
		dirty = true;
	}
	if (dirty) {
		need_layout = true;
	}
}

void tfloat_widget::set_ref(const std::string& ref)
{
	if (ref == ref_) {
		return;
	}
	if (!ref_.empty()) {
		tcontrol* ref_widget = dynamic_cast<tcontrol*>(window.find(ref, false));
		if (ref_widget) {
			ref_widget->associate_float_widget(*this, false);
		}
	}
	ref_ = ref;
	need_layout = true;
}

tcontrol::tcontrol(const unsigned canvas_count)
	: label_()
	, label_size_(std::make_pair(0, tpoint(0, 0)))
	, text_editable_(false)
	, post_anims_()
	, integrate_(NULL)
	, tooltip_()
	, canvas_(canvas_count)
	, config_(NULL)
	, text_maximum_width_(0)
	, size_is_max_(false)
	, calculate_text_box_size_(false)
	, text_font_size_(0)
	, text_color_tpl_(0)
	, best_width_("")
	, best_height_("")
{
	connect_signal<event::SHOW_TOOLTIP>(boost::bind(
			  &tcontrol::signal_handler_show_tooltip
			, this
			, _2
			, _3
			, _5));

	connect_signal<event::NOTIFY_REMOVE_TOOLTIP>(boost::bind(
			  &tcontrol::signal_handler_notify_remove_tooltip
			, this
			, _2
			, _3));
}

tcontrol::~tcontrol()
{
	while (!post_anims_.empty()) {
		erase_animation(post_anims_.front());
	}
	if (integrate_) {
		delete integrate_;
		integrate_ = NULL;
	}
}

bool tcontrol::disable_click_dismiss() const
{
	return get_visible() == twidget::VISIBLE && get_active();
}

tpoint tcontrol::get_config_min_size() const
{
	tpoint result(config_->min_width, config_->min_height);
	return result;
}

void tcontrol::layout_init(bool linked_group_only)
{
	// Inherited.
	twidget::layout_init(linked_group_only);
	if (!linked_group_only) {
		text_maximum_width_ = 0;
	}
}

tpoint tcontrol::get_best_text_size(const int maximum_width) const
{
	VALIDATE(!label_.empty(), null_str);

	if (!label_size_.second.x || (maximum_width != label_size_.first)) {
		// Try with the minimum wanted size.
		label_size_.first = maximum_width;

		label_size_.second = font::get_rendered_text_size(label_, maximum_width, get_text_font_size(), font::NORMAL_COLOR, text_editable_);
	}

	const tpoint& size = label_size_.second;
	VALIDATE(size.x <= maximum_width, null_str);

	return size;
}

tpoint tcontrol::best_size_from_builder() const
{
	tpoint result(npos, npos);
	const twindow* window = NULL;

	if (best_width_.has_formula2()) {
		window = get_window();
		result.x = best_width_(window->variables()) * twidget::hdpi_scale;
		if (result.x < 0) {
			result.x = 0;
		}
	}
	if (best_height_.has_formula2()) {
		if (!window) {
			window = get_window();
		}
		result.y = best_height_(window->variables()) * twidget::hdpi_scale;
		if (result.y < 0) {
			result.y = 0;
		}
	}
	return result;
}

tpoint tcontrol::calculate_best_size() const
{
	VALIDATE(config_, null_str);

	if (restrict_width_ && (clear_restrict_width_cell_size || !text_maximum_width_)) {
		return tpoint(0, 0);
	}

	// if has width/height field, use them. or calculate.
	const tpoint cfg_size = best_size_from_builder();
	if (cfg_size.x != npos && cfg_size.y != npos && !size_is_max_) {
		return cfg_size;
	}

	// calculate text size.
	tpoint text_size(0, 0);
	if (config_->label_is_text) {
		if ((!text_editable_ || calculate_text_box_size_) && !label_.empty()) {
			int maximum_width = settings::screen_width - config_->text_extra_width;
				
			if (text_maximum_width_ && maximum_width > text_maximum_width_) {
				maximum_width = text_maximum_width_;
			}
			text_size = get_best_text_size(maximum_width);
		}

	} else {
		VALIDATE(!text_editable_, null_str);
		text_size = mini_get_best_text_size();
	}

	// text size must >= minimum size. 
	const tpoint minimum = get_config_min_size();
	if (minimum.x > 0 && text_size.x < minimum.x) {
		text_size.x = minimum.x;
	}
	if (minimum.y > 0 && text_size.y < minimum.y) {
		text_size.y = minimum.y;
	}

	tpoint result(text_size.x + config_->text_extra_width, text_size.y + config_->text_extra_height);
	if (!size_is_max_) {
		if (cfg_size.x != npos) {
			result.x = cfg_size.x;
		}
		if (cfg_size.y != npos) {
			result.y = cfg_size.y;
		}
	} else {
		if (cfg_size.x != npos && result.x >= cfg_size.x) {
			result.x = cfg_size.x;
		}
		if (cfg_size.y != npos && result.y >= cfg_size.y) {
			result.y = cfg_size.y;
		}
	}

	return result;
}

void tcontrol::calculate_integrate()
{
	VALIDATE(!text_editable_ && !integrate_, null_str);
}

void tcontrol::refresh_locator_anim(std::vector<tintegrate::tlocator>& locator)
{
	if (!text_editable_) {
		return;
	}
	locator_.clear();
	if (integrate_) {
		integrate_->fill_locator_rect(locator, true);
	} else {
		locator_ = locator;
	}
}

void tcontrol::set_blits(const std::vector<image::tblit>& blits)
{ 
	blits_ = blits;
	set_dirty();
}

void tcontrol::set_blits(const image::tblit& blit)
{
	blits_.clear();
	blits_.push_back(blit);
	set_dirty();
}

void tcontrol::place(const tpoint& origin, const tpoint& size)
{
	if (restrict_width_) {
		VALIDATE(text_maximum_width_ >= size.x - config_->text_extra_width, null_str);
	}

	SDL_Rect previous_rect = ::create_rect(x_, y_, w_, h_);

	// resize canvasses
	BOOST_FOREACH(tcanvas& canvas, canvas_) {
		canvas.set_width(size.x);
		canvas.set_height(size.y);
	}

	// inherited
	twidget::place(origin, size);

	calculate_integrate();

	// update the state of the canvas after the sizes have been set.
	update_canvas();

	if (::create_rect(x_, y_, w_, h_) != previous_rect) {
		for (std::set<tfloat_widget*>::const_iterator it = associate_float_widgets_.begin(); it != associate_float_widgets_.end(); ++ it) {
			tfloat_widget& item = **it;
			item.need_layout = true;
		}
	}
}

void tcontrol::set_definition(const std::string& definition)
{
	VALIDATE(!config(), null_str);

	set_config(get_control(get_control_type(), definition));

	VALIDATE(canvas().size() == config()->state.size(), null_str);
	for (size_t i = 0; i < canvas().size(); ++i) {
		canvas(i) = config()->state[i].canvas;
		canvas(i).start_animation();
	}

	const std::string border = mini_default_border();
	set_border(border);
	set_icon(null_str);

	update_canvas();

	load_config_extra();

	VALIDATE(config(), null_str);
}

void tcontrol::set_best_size(const std::string& width, const std::string& height)
{
	best_width_ = tformula<unsigned>(width);
	best_height_ = tformula<unsigned>(height);
}

void tcontrol::set_label(const std::string& label)
{
	if (label == label_) {
		return;
	}

	label_ = label;
	label_size_.second.x = 0;
	update_canvas();
	set_dirty();

	calculate_integrate();
}

void tcontrol::set_text_editable(bool editable)
{
	if (editable == text_editable_) {
		return;
	}

	text_editable_ = editable;
	update_canvas();
	set_dirty();
}

void tcontrol::update_canvas()
{
	// set label in canvases
	BOOST_FOREACH(tcanvas& canvas, canvas_) {
		canvas.set_variable("text", variant(label_));
	}
}

int tcontrol::get_text_maximum_width() const
{
	return get_width() - config_->text_extra_width;
}

void tcontrol::set_text_maximum_width(int maximum)
{
	if (restrict_width_) {
		text_maximum_width_ = maximum - config_->text_extra_width;
	}
}

void tcontrol::clear_texture()
{
	BOOST_FOREACH(tcanvas& canvas, canvas_) {
		canvas.clear_texture();
	}
}

bool tcontrol::exist_anim()
{
	if (!post_anims_.empty()) {
		return true;
	}
	if (integrate_ && integrate_->exist_anim()) {
		return true;
	}
	return !canvas_.empty() && canvas_[get_state()].exist_anim();
}

int tcontrol::insert_animation(const ::config& cfg)
{
	int id = start_cycle_float_anim(*display::get_singleton(), cfg);
	if (id != INVALID_ANIM_ID) {
		std::vector<int>& anims = post_anims_;
		anims.push_back(id);
	}
	return id;
}

void tcontrol::erase_animation(int id)
{
	bool found = false;
	std::vector<int>::iterator find = std::find(post_anims_.begin(), post_anims_.end(), id);
	if (find != post_anims_.end()) {
		post_anims_.erase(find);
		found = true;
	}
	if (found) {
		display& disp = *display::get_singleton();
		disp.erase_area_anim(id);
		set_dirty();
	}
}

void tcontrol::set_canvas_variable(const std::string& name, const variant& value)
{
	BOOST_FOREACH(tcanvas& canvas, canvas_) {
		canvas.set_variable(name, value);
	}
	set_dirty();
}

int tcontrol::get_text_font_size() const
{
	int ret = text_font_size_? text_font_size_: font::CFG_SIZE_DEFAULT;
	if (ret >= font_min_relative_size) {
		ret = font_size_from_relative(ret);
	}
	return ret * twidget::hdpi_scale;
}

class tshare_canvas_integrate_lock
{
public:
	tshare_canvas_integrate_lock(tintegrate* integrate)
		: integrate_(share_canvas_integrate)
	{
		share_canvas_integrate = integrate;
	}
	~tshare_canvas_integrate_lock()
	{
		share_canvas_integrate = integrate_;
	}

private:
	tintegrate* integrate_;
};

void tcontrol::impl_draw_background(
		  texture& frame_buffer
		, int x_offset
		, int y_offset)
{
	tshare_canvas_integrate_lock lock2(integrate_);

	canvas(get_state()).blit(*this, frame_buffer, calculate_blitting_rectangle(x_offset, y_offset), get_dirty(), post_anims_);
}

texture tcontrol::get_canvas_tex()
{
	VALIDATE(get_dirty() || get_redraw(), null_str);

	return canvas(get_state()).get_canvas_tex(*this, post_anims_);
}

void tcontrol::associate_float_widget(tfloat_widget& item, bool associate)
{
	if (associate) {
		associate_float_widgets_.insert(&item);
	} else {
		std::set<tfloat_widget*>::iterator it = associate_float_widgets_.find(&item);
		if (it != associate_float_widgets_.end()) {
			associate_float_widgets_.erase(it);
		}
	}
}

void tcontrol::signal_handler_show_tooltip(
		  const event::tevent event
		, bool& handled
		, const tpoint& location)
{
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	return;
#endif

	if (!tooltip_.empty()) {
		std::string tip = tooltip_;
		event::tmessage_show_tooltip message(tip, *this, location);
		handled = fire(event::MESSAGE_SHOW_TOOLTIP, *this, message);
	}
}

void tcontrol::signal_handler_notify_remove_tooltip(
		  const event::tevent event
		, bool& handled)
{
	/*
	 * This makes the class know the tip code rather intimately. An
	 * alternative is to add a message to the window to remove the tip.
	 * Might be done later.
	 */
	get_window()->remove_tooltip();
	// tip::remove();

	handled = true;
}

} // namespace gui2

