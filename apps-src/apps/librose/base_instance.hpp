/* $Id: editor_display.hpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
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

#ifndef LIBROSE_BASE_INSTANCE_HPP_INCLUDED
#define LIBROSE_BASE_INSTANCE_HPP_INCLUDED

#include "rose_config.hpp"
#include "display.hpp"
#include "hero.hpp"
#include "preferences.hpp"
#include "sound.hpp"
#include "filesystem.hpp"
#include "serialization/preprocessor.hpp"
#include "config_cache.hpp"
#include "cursor.hpp"
#include "loadscreen.hpp"
#include "lobby.hpp"

#include "webrtc/base/thread.h"
#include "webrtc/base/physicalsocketserver.h"

#define INVALID_UINT32_ID		0
class animation;

namespace gui2 {
class tchat_;
}

namespace rtc {
// SDLThread. Automatically pumps wakeup and IO messages.

class SDLThread : public Thread
{
public:
	SDLThread()
		: ss_()
	{
		set_socketserver(&ss_);
	}
	virtual ~SDLThread()
	{
		// Stop();
		set_socketserver(NULL);
	}
	void pump()
	{
		Message msg;
		size_t max_msgs = std::max<size_t>(1, size());
		// require detect io signal, so io_process of Get use true.
		for (; max_msgs > 0 && Get(&msg, 0, true); --max_msgs) {
			Dispatch(&msg);
		}
	}

private:
	PhysicalSocketServer ss_;
};

}

class base_instance
{
public:
	static void prefix_create(const std::string& _app, const std::string& title, const std::string& channel, bool landscape);

	base_instance(int argc, char** argv, int sample_rate = 44100, size_t sound_buffer_size = 4096);
	virtual ~base_instance();

	void initialize();
	virtual void close();
	virtual tlobby* create_lobby();

	loadscreen::global_loadscreen_manager& loadscreen_manager() const { return *loadscreen_manager_; }

	bool init_language();
	bool init_video();
	virtual bool init_config(const bool force);

	/** Gets the underlying screen object. */
	CVideo& video() { return video_; }

	display& disp();
	const config& game_config() const { return game_config_; }
	const config& core_cfg() const { return core_cfg_; }
	void set_core_cfg(const config& cfg) { core_cfg_ = cfg; }
	bool is_loading() { return false; }

	bool change_language();
	virtual int show_preferences_dialog(display& disp, bool first);

	virtual void regenerate_heros(hero_map& heros, bool allow_empty);
	hero_map& heros() { return heros_; }

	virtual void fill_anim_tags(std::map<const std::string, int>& tags) {};
	virtual void fill_anim(int at, const std::string& id, bool area, bool tpl, const config& cfg);

	void pre_create_renderer();
	void post_create_renderer();
	void clear_textures();

	void clear_anims();
	const std::multimap<std::string, const config>& utype_anim_tpls() const { return utype_anim_tpls_; }
	const animation* anim(int at) const;

	void init_locale();
	void handle_app_event(Uint32 type);

	bool foreground() const { return foreground_; }
	bool terminating() const { return terminating_; }

	void theme_switch_to(const std::string& app, const std::string& id);

	uint32_t get_callback_id() const;
	uint32_t background_connect(const boost::function<bool (uint32_t ticks)>& callback);
	void handle_background();

	void set_chat(gui2::tchat_* chat) {	chat_ = chat; }
	gui2::tchat_* chat() const { return chat_; }

	rtc::SDLThread& sdl_thread() { return sdl_thread_; }
	void pump();

protected:
	virtual void app_load_app_config(const config& cfg) {}
	virtual void load_game_cfg(const bool force);

private:
	virtual void app_init_locale(const std::string& intl_dir) {}

	virtual void app_terminating() {}
	virtual void app_willenterbackground() {}
	virtual void app_didenterbackground() {}
	virtual void app_willenterforeground() {}
	virtual void app_didenterforeground() {}
	virtual void app_lowmemory() {}

	virtual void app_pre_create_renderer() {}
	virtual void app_post_create_renderer() {}

	void background_disconnect(const uint32_t id);
	void background_disconnect_th(const uint32_t id);

protected:
	surface icon_;
	CVideo video_;
	hero_map heros_;

	int force_bpp_;
	const font::manager font_manager_;
	const preferences::base_manager prefs_manager_;
	const image::manager image_manager_;
	sound::music_thinker music_thinker_;
	binary_paths_manager paths_manager_;
	const cursor::manager* cursor_manager_; // it should create after video-subsystem.
	loadscreen::global_loadscreen_manager* loadscreen_manager_; // it should create after video-subsystem.
	const gui2::event::tmanager* gui2_event_manager_; // it should create after gui2-subsystem.

	util::scoped_ptr<display> disp_;

	config game_config_;
	
	config core_cfg_; // remove [terrain_type] children's data.bin.

	preproc_map old_defines_map_;
	game_config::config_cache& cache_;

	bool foreground_;
	bool terminating_;
	std::map<int, animation*> anims_;
	std::multimap<std::string, const config> utype_anim_tpls_;

	boost::scoped_ptr<sound::tpersist_xmit_audio_lock> background_persist_xmit_audio_;
	int background_callback_id_;
	std::map<uint32_t, boost::function<bool (uint32_t ticks)> > background_callbacks_;

	gui2::tchat_* chat_;
	rtc::SDLThread sdl_thread_;
};

extern base_instance* instance;

// C++ Don't call virtual function during class construct function.
template<class T>
class instance_manager
{
public:
	instance_manager(int argc, char** argv, const std::string& app, const std::string& title, const std::string& channel, bool landscape)
	{
		// if exception generated when construct, system don't call destructor.
		try {
			base_instance::prefix_create(app, title, channel, landscape);
			instance = new T(argc, argv);
			instance->initialize();

		} catch(...) {
			if (instance) {
				delete instance;
				instance = NULL;
			}
			throw;
		}
	}
	
	~instance_manager()
	{
		if (instance) {
			delete instance;
			instance = NULL;
		}
	}
	T& get() { return *(dynamic_cast<T*>(instance)); }
};

#endif
