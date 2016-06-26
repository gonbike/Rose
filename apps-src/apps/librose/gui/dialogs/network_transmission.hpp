/* $Id$ */
/*
   Copyright (C) 2011 by Sergey Popov <loonycyborg@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_NETWORK_RECEIVE_HPP_INCLUDED
#define GUI_DIALOGS_NETWORK_RECEIVE_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gui/widgets/control.hpp"

namespace gui2 {

class tprogress_
{
public:
	static const int finish_precent;
	virtual void set_percent(const int percent) = 0;
	virtual void cancel_task() = 0;
	virtual bool is_ok() const = 0;
};

class tbutton;

/**
 * Dialog that tracks network transmissions
 *
 * It shows upload/download progress and allows the user
 * to cancel the transmission.
 */
class tnetwork_transmission : public tdialog, public tprogress_
{
public:
	tnetwork_transmission(const std::string& title, const std::string& subtitle, int hidden_ms = 4);

	void set_subtitle(const std::string&);

	void set_percent(const int percent) override;
	void cancel_task() override;
	bool is_ok() const override;

protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

private:
	void cancel(twindow& window);

private:
	/** The title for the dialog. */
	std::string title_;

	/**
	 * The subtitle for the dialog.
	 *
	 * This field commenly shows the action in progress eg connecting,
	 * uploading, downloading etc..
	 */
	std::string subtitle_;

	twindow* window_;
	// bool track_upload_;
	tbutton* cancel_;

	Uint32 start_ticks_;
	Uint32 hidden_ticks_;
};

} // namespace gui2

#endif

