SUB_PATH := $(WEBRTC_SUBPATH)/api

LOCAL_SRC_FILES += $(SUB_PATH)/audio_codecs/audio_decoder.cc \
	$(SUB_PATH)/audio_codecs/audio_format.cc \
	$(SUB_PATH)/audio_codecs/builtin_audio_decoder_factory.cc \
	$(SUB_PATH)/mediaconstraintsinterface.cc \
	$(SUB_PATH)/mediastreaminterface.cc \
	$(SUB_PATH)/mediatypes.cc \
	$(SUB_PATH)/statstypes.cc \
	$(SUB_PATH)/video/i420_buffer.cc \
	$(SUB_PATH)/video/video_frame.cc