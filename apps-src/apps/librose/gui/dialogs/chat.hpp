/* $Id: campaign_difficulty.hpp 49603 2011-05-22 17:56:17Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_CHAT_HPP_INCLUDED
#define GUI_DIALOGS_CHAT_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "lobby.hpp"
#include <integrate.hpp>
#include "gui/widgets/widget.hpp"
#include "thread.hpp"

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/base/nethelpers.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/signalthread.h"
#include "webrtc/base/sigslot.h"

class config;
class display;
class tgroup;

namespace gui2 {

class tlabel;
class tbutton;
class tspacer;
class ttree_view_node;
class ttoggle_button;
class tscroll_label;
class ttext_box;
class tscroll_text_box;
class tstack;
class treport;
class ttree_view;
class tlistbox;
class tstack;
class tgrid;
class ttrack;

class tchat_: public tdialog, public tlobby::thandler, public webrtc::PeerConnectionObserver, public webrtc::CreateSessionDescriptionObserver, public sigslot::has_slots<>, public rtc::MessageHandler
{
public:
	static std::string err_encode_str;

	enum {CONTACT_HOME, FIND_LAYER, CHANNEL_LAYER, VRENDERER_LAYER, MSG_LAYER};
	enum {NOTIFY_LAYER, PERSION_LAYER};

	struct tcookie 
	{
		tcookie(size_t signature = 0, ttree_view_node* node = NULL, const std::string& nick = null_str, int id = tlobby_channel::npos, bool channel = true)
			: signature(signature)
			, node(node)
			, nick(nick)
			, id(id)
			, channel(channel)
			, unread(0)
			, online(false)
			, away(false)
		{}

		bool valid_user() const { return !channel; }

		size_t signature;
		ttree_view_node* node;
		std::string nick;
		int id;
		bool channel;
		int unread;
		bool online;
		bool away;
	};

	enum {f_none, f_min, f_netdiag = f_min, f_copy, f_reply, f_face, f_part, 
		f_part_friend, f_explore, f_max = f_explore};
	enum {ft_none = 0x1, ft_person = 0x2, ft_channel = 0x4, ft_chan_person = 0x8};
	struct tfunc 
	{
		tfunc(int id);

		int id;
		int type;
		std::string image;
		std::string tooltip;
	};

	struct tsession
	{
		static int logs_per_page;
		tsession(chat_logs::treceiver& receiver);

		int current_logs(std::vector<chat_logs::tlog>& logs) const;
		int pages() const;
		bool can_previous() const;
		bool can_next() const;
		const chat_logs::tlog& log(int at) const;

		chat_logs::treceiver* receiver;
		std::vector<chat_logs::tlog> history;
		int current_page;
	};

	tchat_(display& disp, int chat_page);
	virtual ~tchat_();

	virtual void handle_status(int at, tsock::ttype type) override;
	virtual bool handle_raw(int at, tsock::ttype type, const char* param[]) override;

	void pre_create_renderer();
	void post_create_renderer();

	void did_draw_vrenderer(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn, bool force);
	void did_control_drag_detect(ttrack& widget, const tpoint& first, const tpoint& last);
	void did_drag_coordinate(ttrack& widget, const tpoint& first, const tpoint& last);
	ttrack* vrenderer_track() const { return vrenderer_track_; }
	uint8_t* pixels(bool remote) const { return remote? remote_pixels_: local_pixels_; }

protected:
	/** Inherited from tdialog. */
	void pre_show(twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window) override;

	void process_message(const std::string& chann, const std::string& from, const std::string& text);
	void process_userlist(const std::string& chan, const std::string& names);
	void process_userlist_end(const std::string& chan);
	void process_uparted(const std::string& chan);
	void process_part(const std::string& chan, const std::string& nick, const std::string& reason);
	void process_join(const std::string& chan, const std::string& nick);
	void process_whois(const std::string& chan, const std::string& nick, bool online, bool away);
	void process_quit(const std::string& nick);
	void process_chanlist_start();
	void process_chanlist(const std::string& chan, int users, const std::string& topic);
	void process_chanlist_end();
	void process_online(const char* nicks);
	void process_offline(const char* nicks);
	void process_forbid_join(const std::string& chan, const std::string& reason);

	void process_network_status(bool connected);

	virtual void desire_swap_page(twindow& window, int page, bool open) {}

	void contact_person_toggled(twidget& widget);
	void contact_channel_toggled(ttree_view& view, ttree_view_node& node);
	void simulate_cookie_toggled(bool person, int cid, int id, bool channel);

	void netdiag(twindow& window);
	void send(bool& handled, bool& halt);
	void find(twindow& window);
	void join_channel(twindow& window);
	void insert_face(twindow& window, int index);
	void signal_handler_sdl_key_down(bool& halt
		, bool& handled
		, const SDL_Keycode key
		, SDL_Keymod modifier
		, const Uint16 unicode);

	void chat_setup(twindow& window, bool caller);

	void notify_did_click(tlistbox& list, twidget& widget);
	void notify_did_double_click(tlistbox& list, twidget& widget);
	void find_chan_toggled(twindow& window, tlistbox& list);

	void channel2_toggled(twidget& widget);
	void refresh_channel2_toolbox(const tlobby_user& user);
	void chat_to(twindow& window);
	void join_friend(twindow& window);

	std::vector<tcookie>& ready_branch(bool person, const tlobby_channel& channel);
	void update_node_internal(const std::vector<tcookie>& cookies, const tcookie& cookie);
	std::pair<std::vector<tcookie>*, tcookie* > contact_find(bool person, size_t signature);
	std::pair<std::vector<tcookie>*, tcookie* > contact_find(bool person, int cid, int id, bool channel);
	std::pair<const std::vector<tcookie>*, const tcookie* > contact_find(bool person, int cid, int id, bool channel) const;

	// std::string format_log_str(const tsession& sess, std::vector<tintegrate::tlocator>& locator) const;
	void format_log_2_listbox(tlistbox& list, const tsession& sess, int cursel) const;

	void chat_2_scroll_label(tlistbox& list, const tsession& sess, int cursel = twidget::npos);
	void ready_face(twindow& window);

	void swap_page(twindow& window, int page, bool swap);

	void did_item_changed_report(treport& report, ttoggle_button& widget);
	void did_item_click_report(treport& report, tbutton& widget);

	tsession& get_session(chat_logs::treceiver& receiver, bool allow_create = false);
	void switch_session(bool person, std::vector<tcookie>& branch, tcookie& cookie);

	bool gui_ready() const;
	void generate_channel_tree(tlobby_channel& channel);

private:
	void switch_to_home(twindow& window);
	void switch_to_find(twindow& window);
	void switch_to_channel(twindow& window);
	void switch_to_msg(twindow& window);
	void switch_to_video(twindow& window);
	void leave_from_video(twindow& window);

	void reload_toolbar(twindow& window);
	void refresh_toolbar(int type, int id);

	void reload_catalog(twindow& window);
	void contact_switch_to_notify(twindow& window);
	void contact_switch_to_person(twindow& window);
	void user_to_title(const tcookie& cookie) const;
	void insert_user(bool person, std::vector<tcookie>& branch, const tlobby_user& user);
	void erase_user(bool person, std::vector<tcookie>& branch, int uid);
	void clear_branch(bool person, int type);
	void remove_branch(bool person, int type);
	void handle_multiline(const char* nick, const char* chan, const char* text);
	void previous_page(twindow& window);
	void next_page(twindow& window);

	void enter_inputing(bool enter);
	void refresh_vrenderer_status(twindow& window) const;

	void visible_float_widgets(bool visible);
	//
	// webrtc
	//
	bool InitializePeerConnection();

	void ConnectToPeer(const std::string& peer_nick, bool caller, const std::string& offer);
	std::unique_ptr<cricket::VideoCapturer> OpenVideoCaptureDevice();
	bool CreatePeerConnection(bool dtls);
	void DeletePeerConnection();
	void AddStreams();

	//
	// PeerConnectionObserver implementation.
	//

	void OnSignalingChange(
		webrtc::PeerConnectionInterface::SignalingState new_state) override;
	void OnAddStream(
		rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
	void OnRemoveStream(
		rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
	void OnDataChannel(
		rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override {}
	void OnRenegotiationNeeded() override {}
	void OnIceConnectionChange(
		webrtc::PeerConnectionInterface::IceConnectionState new_state) override; // {}
	void OnIceGatheringChange(
		webrtc::PeerConnectionInterface::IceGatheringState new_state) override; // {}
	void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
	void OnIceConnectionReceivingChange(bool receiving) override; // {}

	// CreateSessionDescriptionObserver implementation.
	virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
	virtual void OnFailure(const std::string& error) override;

	virtual void StartLocalRenderer(webrtc::VideoTrackInterface* local_video);
	virtual void StopLocalRenderer();
	virtual void StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video);
	virtual void StopRemoteRenderer();

	//
	// PeerConnectionClientObserver implementation.
	//
	virtual void OnServerConnectionFailure();

	void Connect(const std::string& server, int port);

	void DoConnect();
	void Close();
	void InitSocketSignals();
	bool ConnectControlSocket();
	void OnConnect(rtc::AsyncSocket* socket);
	void msg_2_signaling_server(rtc::AsyncSocket* socket, const std::string& msg);

	// Returns true if the whole response has been read.
	const char* read_2_buffer(rtc::AsyncSocket* socket, bool first, size_t* content_length);

	void OnRead(rtc::AsyncSocket* socket);
	void OnClose(rtc::AsyncSocket* socket, int err);

	void OnResolveResult(rtc::AsyncResolverInterface* resolver);

	void StartLogin(const std::string& server, int port);

	threading::mutex& get_mutex(bool remote) { return remote? remote_mutex_: local_mutex_; }
	void set_renderer_texture_size(bool remote, int width, int height);
	
	int AddRef() const override;
	int Release() const override;

	void resize_recv_data(int size);
	void resize_send_data(int size);

	enum {
		MSG_SIGNALING_CLOSE
	};
	void OnMessage(rtc::Message* msg) override;

protected:
	size_t signature_;
	std::map<int, std::vector<tcookie> > person_cookies_;
	std::map<int, std::vector<tcookie> > channel_cookies_;
	std::vector<tcookie> channel2_branch_;
	config gamelist_;
	std::vector<std::string> users_;

	int current_page_;
	tstack* page_panel_;
	std::vector<std::string> chans_;
	treport* catalog_;
	treport* toolbar_;
	std::vector<tfunc> funcs_;
	std::vector<tsession> sessions_;
	tsession* current_session_;
	bool inputing_;
	bool swap_resultion_;
	bool portrait_;

private:
	display& disp_;
	twindow* window_;
	tstack* panel_;
	tgrid* contact_layer_;
	tgrid* find_layer_;
	tgrid* channel_layer_;
	tgrid* vrenderer_layer_;
	tgrid* msg_layer_;
	tlistbox* notify_list_;
	ttree_view* person_tree_;
	ttree_view* channel2_tree_;
	tbutton* previous_page_;
	tbutton* next_page_;
	tlabel* pagenum_;
	tlistbox* history_;
	tscroll_text_box* input_;
	ttext_box* input_tb_;
	tspacer* input_scale_;
	tbutton* send_;
	tbutton* find_;
	tbutton* switch_to_chat_find_;
	tbutton* switch_to_chat_channel_;
	tbutton* join_channel_;
	tlistbox* chanlist_;
	tbutton* chat_to_;
	tbutton* join_friend_;
	int current_ft_;

	ttrack* vrenderer_track_;

	bool in_find_chan_;
	int cond_min_users_;
	std::vector<std::string> list_chans_;

	int src_pos_;
	int chat_page_;
	bool relay_only_;

	//
	// webrtc
	//
	int peer_id_;
	bool loopback_;
	rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
	rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
	// PeerConnectionClient* client_;
	std::deque<std::string*> pending_messages_;
	std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> > active_streams_;
	std::string server_;

	enum State {
		NOT_CONNECTED,
		RESOLVING,
		SIGNING_IN,
		CONNECTED,
		SIGNING_OUT_WAITING,
		SIGNING_OUT,
	};

	rtc::SocketAddress server_address_;
	rtc::AsyncResolver* resolver_;
	std::unique_ptr<rtc::AsyncSocket> control_socket_;
	std::string onconnect_data_;
	std::string notification_data_;
	std::string my_nick_;
	std::string peer_nick_;
	State state_;
	int my_id_;

	bool deconstructed_;

	class VideoRenderer : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
	public:
		VideoRenderer(tchat_* chat, int width, int height, webrtc::VideoTrackInterface* track_to_render, bool remote);
		virtual ~VideoRenderer();

		// VideoSinkInterface implementation
		void OnFrame(const webrtc::VideoFrame& frame) override;

		bool dirty() const { return dirty_; }

	protected:

		enum {
			SET_SIZE,
			RENDER_FRAME,
		};

		tchat_* chat_;
		int bytesperpixel_;
		rtc::scoped_refptr<webrtc::VideoTrackInterface> rendered_track_;
		bool remote_;
		bool dirty_;
	};
	std::unique_ptr<VideoRenderer> local_renderer_;
	std::unique_ptr<VideoRenderer> remote_renderer_;
	std::unique_ptr<rtc::Thread> _networkThread;
	std::unique_ptr<rtc::Thread> _workerThread;
	std::unique_ptr<rtc::Thread> _signalingThread;

	std::unique_ptr<tdisable_idle_lock> disable_idle_lock_;
	std::string preferred_codec_;

	threading::mutex remote_mutex_;
	threading::mutex local_mutex_;
	tpoint local_render_size_;
	tpoint original_local_offset_;
	tpoint current_local_offset_;

	texture remote_tex_;
	texture local_tex_;
	tpoint remote_size_;
	tpoint local_size_;
	uint8_t* remote_pixels_;
	uint8_t* local_pixels_;

	mutable volatile int ref_count_;

	char* send_data_;
	int send_data_size_;
	int send_data_vsize_;

	int last_msg_should_size_;
	webrtc::PeerConnectionInterface::IceConnectionState connection_state_;
	webrtc::PeerConnectionInterface::IceGatheringState gathering_state_;

	char* recv_data_;
	int recv_data_size_;
	int recv_data_vsize_;
};

class tchat2: public tchat_
{
public:
	explicit tchat2(display& disp);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	enum {CHAT_PAGE, CHATING_PAGE, NONE_PAGE};
	
	void handle_status(int at, tsock::ttype type);

private:
	display& disp_;
};


}


#endif /* ! GUI_DIALOGS_CAMPAIGN_DIFFICULTY_HPP_INCLUDED */
