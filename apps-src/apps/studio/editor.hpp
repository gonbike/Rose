#ifndef STUDIO_EDITOR_HPP_INCLUDED
#define STUDIO_EDITOR_HPP_INCLUDED

#include "config_cache.hpp"
#include "config.hpp"
#include "version.hpp"
#include "task.hpp"
#include "theme.hpp"

#include <set>

#include <boost/bind.hpp>
#include <boost/function.hpp>

extern const std::string studio_guid;
class display;

class tapp_capabilities
{
public:
	tapp_capabilities(const config& cfg)
		: app(cfg["app"].str())
		, bundle_id(cfg["bundle_id"].str())
		, ble(cfg["ble"].to_bool())
		, healthkit(cfg["healthkit"].to_bool())
		, path_(cfg["path"].str())
	{
		if (path_.empty()) {
			path_ = "..";
		}
	}

	bool operator==(const tapp_capabilities& that) const
	{
		if (app != that.app) return false;
		if (bundle_id.id() != that.bundle_id.id()) return false;
		if ((ble && !that.ble) || (!ble && that.ble)) return false;
		if ((healthkit && !that.healthkit) || (!healthkit && that.healthkit)) return false;
		return true;
	}
	bool operator!=(const tapp_capabilities& that) const { return !operator==(that); }

	void reset(const tapp_capabilities& that)
	{
		app = that.app;
		bundle_id.reset(that.bundle_id.id());
		ble = that.ble;
		healthkit = that.healthkit;
	}

	void generate2(std::stringstream& ss, const std::string& prefix) const;

public:
	std::string app;
	tdomain bundle_id;
	bool ble;
	bool healthkit;
	std::set<std::string> tdomains;

protected:
	std::string path_;
};

bool is_apps_kit(const std::string& res_folder);
bool is_studio_app(const std::string& app);
bool is_private_app(const std::string& app);
bool is_reserve_app(const std::string& app);

bool check_res_folder(const std::string& folder);
bool check_apps_src_folder(const std::string& folder);

namespace apps_sln {

std::map<std::string, std::string> apps_in(const std::string& src2_path);
bool add_project(const std::string& app, const std::string& guid_str);
bool remove_project(const std::string& app);

bool can_new_dialog(const std::string& app);
bool new_dialog(const std::string& app, const std::vector<std::string>& files);
}

class tapp_copier;

class texporter: public ttask
{
public:
	texporter(const config& cfg, void* copier)
		: ttask(cfg)
		, copier_(*(reinterpret_cast<tapp_copier*>(copier)))
	{}

private:
	void app_complete_paths() override;
	bool app_post_handle(display& disp, const tsubtask& subtask, const bool last) override;
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;

	bool generate_android_prj(display& disp) const;
	bool generate_window_prj(display& disp) const;

private:
	tapp_copier& copier_;
};

class tstudio_extra_exporter: public ttask
{
public:
	tstudio_extra_exporter(const config& cfg, void* copier)
		: ttask(cfg)
		, copier_(*(reinterpret_cast<tapp_copier*>(copier)))
	{}

private:
	void app_complete_paths() override;

private:
	tapp_copier& copier_;
};

class tandroid_res: public ttask
{
public:
	tandroid_res(const config& cfg, void* copier)
		: ttask(cfg)
		, copier_(*(reinterpret_cast<tapp_copier*>(copier)))
	{}

private:
	void app_complete_paths() override;
	bool app_can_execute(const tsubtask& subtask, const bool last) override;
	bool app_post_handle(display& disp, const tsubtask& subtask, const bool last) override;
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;

	std::string get_android_res_path();

private:
	tapp_copier& copier_;
};

class tios_kit: public ttask
{
public:
	static const std::string kit_alias;
	static const std::string studio_alias;

	tios_kit(const config& cfg, void* copier)
		: ttask(cfg)
		, copier_(*(reinterpret_cast<tapp_copier*>(copier)))
	{}

private:
	bool app_post_handle(display& disp, const tsubtask& subtask, const bool last) override;
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;

private:
	tapp_copier& copier_;
};

class tapp_copier: public tapp_capabilities
{
public:
	static const std::string windows_prj_alias;
	static const std::string android_prj_alias;
	static const std::string ios_prj_alias;
	static const std::string app_windows_prj_alias;
	static const std::string app_android_prj_alias;
	static const std::string app_ios_prj_alias;

	tapp_copier(const config& cfg, const std::string& app = null_str);

	void app_complete_paths(std::map<std::string, std::string>& paths) const;
	bool generate_ios_prj(display& disp, const ttask& task) const;

public:
	std::unique_ptr<texporter> exporter;
	std::unique_ptr<tstudio_extra_exporter> studio_extra_exporter;
	std::unique_ptr<tandroid_res> android_res_copier;
	std::unique_ptr<tios_kit> ios_kiter;
};

class tnewer: public ttask
{
public:
	static const std::string windows_prj_alias;

	tnewer(const config& cfg, void*)
		: ttask(cfg)
	{}
	void set_app(const std::string& app) { app_ = app; }

private:
	void app_complete_paths() override;
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;

private:
	std::string app_;
};

class timporter: public ttask
{
public:
	timporter(const config& cfg, void*)
		: ttask(cfg)
	{}
	void set_app(const std::string& app, const std::string& res_path, const std::string& src2_path);

private:
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;

private:
	std::string app_;
};

class tremover: public ttask
{
public:
	tremover(const config& cfg, void*)
		: ttask(cfg)
	{}
	void set_app(const std::string& app, const std::set<std::string>& tdomains)
	{ 
		app_ = app; 
		tdomains_ = tdomains;
	}

private:
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;
	bool app_post_handle(display& disp, const tsubtask& subtask, const bool last) override;
	bool did_language(const std::string& dir, const SDL_dirent2* dirent, std::vector<std::string>& languages);

private:
	std::string app_;
	std::set<std::string> tdomains_;
};

class tnew_window: public ttask
{
public:
	tnew_window(const config& cfg)
		: ttask(cfg)
	{}
	void set_app(const std::string& app, const std::string& id);

	virtual std::vector<std::string> sln_files() = 0;
private:
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;

protected:
	std::string app_;
	std::string id_;
};

class tnew_dialog: public tnew_window
{
public:
	tnew_dialog(const config& cfg, void*)
		: tnew_window(cfg)
	{}
	std::vector<std::string> sln_files() override;
};

class tnew_scene: public tnew_window
{
public:
	tnew_scene(const config& cfg, void*)
		: tnew_window(cfg)
		, unit_files_(false)
	{}
	std::vector<std::string> sln_files() override;
	void set_unit_files(bool val) { unit_files_ = val; }

private:
	bool app_can_execute(const tsubtask& subtask, const bool last) override;

private:
	bool unit_files_;
};

class tvalidater: public ttask
{
public:
	static const std::string windows_prj_alias;

	tvalidater(const config& cfg, void*)
		: ttask(cfg)
	{}
	void set_app(const std::string& app) { app_ = app; }

private:
	void app_complete_paths() override;
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;

private:
	std::string app_;
};

class tsave_theme: public ttask
{
public:
	static const std::string windows_prj_alias;

	tsave_theme(const config& cfg, void*)
		: ttask(cfg)
	{}
	void set_theme(const theme::tfields& fields) { fields_ = fields; }

private:
	void app_complete_paths() override;
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;
	bool app_post_handle(display& disp, const tsubtask& subtask, const bool last) override;

private:
	std::string app_;
	theme::tfields fields_;
};

class tremove_theme: public ttask
{
public:
	static const std::string windows_prj_alias;

	tremove_theme(const config& cfg, void*)
		: ttask(cfg)
	{}
	void set_theme(const theme::tfields& fields) { fields_ = fields; }

private:
	void app_complete_paths() override;
	std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) override;

private:
	std::string app_;
	theme::tfields fields_;
};

#endif