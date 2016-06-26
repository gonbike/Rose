LOCAL_PATH := $(call my-dir)/../../../..

include $(CLEAR_VARS)

LOCAL_MODULE := main

ORIGINAL_PATH := LOCAL_PATH
SDL_PATH := ../SDL/SDL2-2.0.5

LOCAL_CFLAGS := -DWEBRTC_LINUX -DWEBRTC_ANDROID -DWEBRTC_POSIX -DWEBRTC_NS_FIXED -DWEBRTC_APM_DEBUG_DUMP=0 -DWEBRTC_INTELLIGIBILITY_ENHANCER=0 \
-DSSL_USE_OPENSSL -DHAVE_OPENSSL_SSL_H -DFEATURE_ENABLE_SSL -DWEBRTC_CODEC_ISACFX -DEXPAT_RELATIVE_PATH -DFEATURE_ENABLE_ -DVOICEMAIL \
-DFEATURE_ENABLE_PSTN -DHAVE_SRTP -DSRTP_RELATIVE_PATH -DHAVE_SCTP -DHAVE_WEBRTC_VIDEO -DHAVE_WEBRTC_VOICE \
-DWEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE -D_HAS_EXCEPTIONS=0 -DBORINGSSL_IMPLEMENTATION -DBORINGSSL_NO_STATIC_INITIALIZER \
-DOPENSSL_SMALL -DOPENSSL_NO_ASM -DWEBRTC_THREAD_RR

LOCAL_CFLAGS += -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -DWEBRTC_HAS_NEON -DWEBRTC_ARCH_ARM -DWEBRTC_ARCH_ARM_V7
LOCAL_CFLAGS += -DWEBRTC_BUILD_LIBEVENT
LOCAL_CFLAGS += -mfpu=neon

LOCAL_CPPFLAGS += -Wno-inconsistent-missing-override

LOCAL_CPP_EXTENSION := .cxx .cpp .cc

LOCAL_C_INCLUDES := $(LOCAL_PATH)/external \
	$(LOCAL_PATH)/external/expat \
	$(LOCAL_PATH)/external/boost \
	$(LOCAL_PATH)/external/bzip2 \
	$(LOCAL_PATH)/external/zlib \
	$(LOCAL_PATH)/external/boringssl/include \
    $(LOCAL_PATH)/external/expat/lib \
    $(LOCAL_PATH)/external/usrsctplib \
    $(LOCAL_PATH)/external/third_party/libyuv/include \
    $(LOCAL_PATH)/external/third_party/libsrtp/include \
    $(LOCAL_PATH)/external/third_party/libsrtp/crypto/include \
    $(LOCAL_PATH)/external/base/third_party/libevent/android \
    $(LOCAL_PATH)/external/webrtc/common_audio/signal_processing/include \
    $(LOCAL_PATH)/external/webrtc/modules/audio_coding/codecs/isac/main/include \
	$(LOCAL_PATH)/../linker/include/SDL2 \
	$(LOCAL_PATH)/../linker/include/SDL2_image \
	$(LOCAL_PATH)/../linker/include/SDL2_mixer \
	$(LOCAL_PATH)/../linker/include/SDL2_ttf \
	$(LOCAL_PATH)/../linker/include/libvpx \
	$(LOCAL_PATH)/librose \
	$(LOCAL_PATH)/studio

LOCAL_SRC_FILES := $(SDL_PATH)/src/main/android/SDL_android_main.c \
	$(subst $(LOCAL_PATH)/,, \
	$(wildcard $(LOCAL_PATH)/studio/*.c) \
	$(wildcard $(LOCAL_PATH)/studio/*.cpp) \
	$(wildcard $(LOCAL_PATH)/studio/gui/dialogs/*.c) \
	$(wildcard $(LOCAL_PATH)/studio/gui/dialogs/*.cpp) \
	$(wildcard $(LOCAL_PATH)/librose/*.c) \
	$(wildcard $(LOCAL_PATH)/librose/*.cpp) \
	$(wildcard $(LOCAL_PATH)/librose/gui/auxiliary/*.c) \
	$(wildcard $(LOCAL_PATH)/librose/gui/auxiliary/*.cpp) \
	$(wildcard $(LOCAL_PATH)/librose/gui/auxiliary/event/*.c) \
	$(wildcard $(LOCAL_PATH)/librose/gui/auxiliary/event/*.cpp) \
	$(wildcard $(LOCAL_PATH)/librose/gui/auxiliary/iterator/*.c) \
	$(wildcard $(LOCAL_PATH)/librose/gui/auxiliary/iterator/*.cpp) \
	$(wildcard $(LOCAL_PATH)/librose/gui/auxiliary/widget_Definition/*.c) \
	$(wildcard $(LOCAL_PATH)/librose/gui/auxiliary/widget_Definition/*.cpp) \
	$(wildcard $(LOCAL_PATH)/librose/gui/auxiliary/window_Builder/*.c) \
	$(wildcard $(LOCAL_PATH)/librose/gui/auxiliary/window_Builder/*.cpp) \
	$(wildcard $(LOCAL_PATH)/librose/gui/dialogs/*.c) \
	$(wildcard $(LOCAL_PATH)/librose/gui/dialogs/*.cpp) \
	$(wildcard $(LOCAL_PATH)/librose/gui/lib/types/*.c) \
	$(wildcard $(LOCAL_PATH)/librose/gui/lib/types/*.cpp) \
	$(wildcard $(LOCAL_PATH)/librose/gui/widgets/*.c) \
	$(wildcard $(LOCAL_PATH)/librose/gui/widgets/*.cpp) \
	$(wildcard $(LOCAL_PATH)/librose/plot/*.c) \
	$(wildcard $(LOCAL_PATH)/librose/plot/*.cpp) \
	$(wildcard $(LOCAL_PATH)/librose/serialization/*.c) \
	$(wildcard $(LOCAL_PATH)/librose/serialization/*.cpp) \
	$(wildcard $(LOCAL_PATH)/external/boost/libs/iostreams/src/*.c) \
	$(wildcard $(LOCAL_PATH)/external/boost/libs/iostreams/src/*.cpp) \
	$(wildcard $(LOCAL_PATH)/external/boost/libs/regex/src/*.c) \
	$(wildcard $(LOCAL_PATH)/external/boost/libs/regex/src/*.cpp) \
	$(wildcard $(LOCAL_PATH)/external/gettext/gettext-runtime/intl/*.c) \
	$(wildcard $(LOCAL_PATH)/external/gettext/gettext-runtime/intl/*.cpp) \
	$(wildcard $(LOCAL_PATH)/external/libiconv/lib/*.c) \
	$(wildcard $(LOCAL_PATH)/external/libiconv/lib/*.cpp) \
	$(wildcard $(LOCAL_PATH)/external/bzip2/*.c) \
	$(wildcard $(LOCAL_PATH)/external/bzip2/*.cpp) \
	$(wildcard $(LOCAL_PATH)/external/zlib/*.c) \
	$(wildcard $(LOCAL_PATH)/external/zlib/*.cpp))


include $(LOCAL_PATH)/external/boringssl/Android.mk
include $(LOCAL_PATH)/external/expat/Android.mk
include $(LOCAL_PATH)/external/usrsctplib/Android.mk
include $(LOCAL_PATH)/external/base/third_party/libevent/Android.mk
include $(LOCAL_PATH)/external/third_party/libyuv/Android.mk
include $(LOCAL_PATH)/external/third_party/libsrtp/Android.mk

WEBRTC_SUBPATH := external/webrtc
include $(LOCAL_PATH)/$(WEBRTC_SUBPATH)/Android.mk

LOCAL_LDLIBS := -llog -lOpenSLES -lSDL2 -lSDL2_image -lSDL2_mixer -lSDL2_ttf -lvpx
LOCAL_SHORT_COMMANDS := true

include $(BUILD_SHARED_LIBRARY)