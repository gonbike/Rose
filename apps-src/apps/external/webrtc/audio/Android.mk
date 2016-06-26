SUB_PATH := $(WEBRTC_SUBPATH)/audio

LOCAL_SRC_FILES += $(SUB_PATH)/utility/audio_frame_operations.cc \
	$(SUB_PATH)/audio_receive_stream.cc \
	$(SUB_PATH)/audio_send_stream.cc \
	$(SUB_PATH)/audio_state.cc \
	$(SUB_PATH)/audio_transport_proxy.cc