#define GETTEXT_DOMAIN "studio-lib"

#include "base_instance.hpp"
#include "preferences_display.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/chat.hpp"
#include "gui/dialogs/rose.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/design.hpp"
#include "gui/widgets/window.hpp"
#include "game_end_exceptions.hpp"
#include "wml_exception.hpp"
#include "gettext.hpp"
#include "hotkeys.hpp"
#include "formula_string_utils.hpp"
#include "version.hpp"
#include "mkwin_controller.hpp"
#include "help.hpp"

#include <errno.h>
#include <iostream>

#include <boost/foreach.hpp>

class game_instance: public base_instance
{
public:
	game_instance(int argc, char** argv);

	void fill_anim_tags(std::map<const std::string, int>& tags);

	void start_mkwin(const std::map<std::string, std::string>& app_tdomains);

private:
	void app_load_app_config(const config& cfg) override;
};

game_instance::game_instance(int argc, char** argv)
	: base_instance(argc, argv)
{
	teditor_ editor_(game_config::path);
	editor_.make_system_bins_exist();
}

void game_instance::fill_anim_tags(std::map<const std::string, int>& tags)
{
	// although don't use below animation, but pass program verify, define them still.
}

void game_instance::start_mkwin(const std::map<std::string, std::string>& app_tdomains)
{
	display_lock lock(disp());
	hotkey::scope_changer changer(game_config(), "hotkey_mkwin");

	display::initial_zoom = 64 * gui2::twidget::hdpi_scale;
	mkwin_controller mkwin(game_config(), video_, app_tdomains);
	mkwin.initialize();
	mkwin.main_loop();
}

void game_instance::app_load_app_config(const config& cfg)
{
	game_config::logo_png = cfg["log_png"].str();
	game_config::version = game_config::rose_version;
	game_config::wesnoth_version = version_info(game_config::version);

	game_config::absolute_path = game_config::path + "/absolute";
	game_config::apps_src_path = directory_name2(game_config::path) + "/apps-src";
	if (!check_apps_src_folder(game_config::apps_src_path)) {
		game_config::apps_src_path.clear();
	}
}

/**
 * Setups the game environment and enters
 * the titlescreen or game loops.
 */
static int do_gameloop(int argc, char** argv)
{
	instance_manager<game_instance> manager(argc, argv, "studio", _("Rose Studio"), "#rose", true);
	game_instance& game = manager.get();

	try {
		std::map<std::string, std::string> app_tdomains;
		for (;;) {
			game.loadscreen_manager().reset();

			gui2::trose::tresult res = gui2::trose::NOTHING;

			const font::floating_label_context label_manager;

			cursor::set(cursor::NORMAL);

			if (res == gui2::trose::NOTHING) {
				// load/reload hero_map from file
				gui2::trose dlg(game.disp(), group.leader());
				dlg.show(game.disp().video());
				res = static_cast<gui2::trose::tresult>(dlg.get_retval());
				app_tdomains = dlg.get_app_tdomains();
			}

			if (res == gui2::trose::QUIT_GAME) {
				posix_print("do_gameloop, received QUIT_GAME, will exit!\n");
				return 0;

			} else if (res == gui2::trose::EDIT_DIALOG) {
				game.start_mkwin(app_tdomains);

			} else if (res == gui2::trose::DESIGN) {
				gui2::tdesign dlg(game.disp());
				dlg.show(game.disp().video());

			} else if (res == gui2::trose::CHANGE_LANGUAGE) {
				if (game.change_language()) {
					t_string::reset_translations();
					image::flush_cache();
				}

			} else if (res == gui2::trose::MESSAGE) {
				gui2::tchat2 dlg(game.disp());
				// rtc::scoped_refptr<gui2::tchat2> dlg(new rtc::RefCountedObject<gui2::tchat2>(game.disp()));

				dlg.show(game.disp().video());
			
			} else if (res == gui2::trose::EDIT_PREFERENCES) {
				preferences::show_preferences_dialog(game.disp(), true);

			}
		}

	} catch (twml_exception& e) {
		e.show(game.disp());

	} catch (CVideo::quit&) {
		//just means the game should quit
		posix_print("SDL_main, catched CVideo::quit\n");

	} catch (game_logic::formula_error& e) {
		gui2::show_error_message(game.disp().video(), e.what());
	} 

	return 0;
}

void test()
{
	for (int abs_delta = 10; abs_delta < 70; abs_delta ++) {
		int ret =  1 + (abs_delta - 10) / 6; // ===> [1, 10]
		posix_print("abs_delta: %i, -----> ret: %i\n", abs_delta, ret);
	}
}

int main(int argc, char** argv)
{
/*
	test();
	return 1;
*/
	try {
		do_gameloop(argc, argv);
	} catch (twml_exception& e) {
		// this exception is generated when create instance.
		posix_print_mb("%s\n", e.user_message.c_str());
	}

	return 0;
}
