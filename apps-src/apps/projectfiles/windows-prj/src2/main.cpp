/* Require Rose v1.0.5 or above. $ */

#define GETTEXT_DOMAIN "studio-lib"

#include "base_instance.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/chat.hpp"
#include "gui/widgets/window.hpp"
#include "game_end_exceptions.hpp"
#include "wml_exception.hpp"
#include "gettext.hpp"
#include "loadscreen.hpp"
#include "formula_string_utils.hpp"
#include "help.hpp"


class game_instance: public base_instance
{
public:
	game_instance(int argc, char** argv);
};

game_instance::game_instance(int argc, char** argv)
	: base_instance(argc, argv)
{
}

/**
 * Setups the game environment and enters
 * the titlescreen or game loops.
 */
static int do_gameloop(int argc, char** argv)
{
	instance_manager<game_instance> manager(argc, argv, "hello", _("Hello World"), "#rose", true);
	game_instance& game = manager.get();

	try {
		game.loadscreen_manager().reset();

		const font::floating_label_context label_manager;

		cursor::set(cursor::NORMAL);

		gui2::tchat2 dlg(game.disp());
		dlg.show(game.disp().video());

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

int main(int argc, char** argv)
{
	try {
		do_gameloop(argc, argv);
	} catch (twml_exception& e) {
		// this exception is generated when create instance.
		posix_print_mb("%s\n", e.user_message.c_str());
	}

	return 0;
}