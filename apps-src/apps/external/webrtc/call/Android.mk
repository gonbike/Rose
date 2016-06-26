SUB_PATH := $(WEBRTC_SUBPATH)/call

LOCAL_SRC_FILES += $(SUB_PATH)/audio_send_stream.cc \
	$(SUB_PATH)/bitrate_allocator.cc \
	$(SUB_PATH)/call.cc \
	$(SUB_PATH)/flexfec_receive_stream_impl.cc \
	$(SUB_PATH)/syncable.cc