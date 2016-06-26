SUB_PATH := $(WEBRTC_SUBPATH)/voice_engine

LOCAL_SRC_FILES += $(SUB_PATH)/channel.cc \
	$(SUB_PATH)/channel_manager.cc \
	$(SUB_PATH)/channel_proxy.cc \
	$(SUB_PATH)/coder.cc \
	$(SUB_PATH)/file_player.cc \
	$(SUB_PATH)/file_recorder.cc \
	$(SUB_PATH)/level_indicator.cc \
	$(SUB_PATH)/monitor_module.cc \
	$(SUB_PATH)/output_mixer.cc \
	$(SUB_PATH)/shared_data.cc \
	$(SUB_PATH)/statistics.cc \
	$(SUB_PATH)/transmit_mixer.cc \
	$(SUB_PATH)/utility.cc \
	$(SUB_PATH)/voe_audio_processing_impl.cc \
	$(SUB_PATH)/voe_base_impl.cc \
	$(SUB_PATH)/voe_codec_impl.cc \
	$(SUB_PATH)/voe_file_impl.cc \
	$(SUB_PATH)/voe_hardware_impl.cc \
	$(SUB_PATH)/voe_neteq_stats_impl.cc \
	$(SUB_PATH)/voe_network_impl.cc \
	$(SUB_PATH)/voe_rtp_rtcp_impl.cc \
	$(SUB_PATH)/voe_volume_control_impl.cc \
	$(SUB_PATH)/voice_engine_impl.cc