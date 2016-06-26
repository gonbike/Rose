SUB_PATH := $(WEBRTC_SUBPATH)/p2p

LOCAL_SRC_FILES += $(SUB_PATH)/base/asyncstuntcpsocket.cc \
	$(SUB_PATH)/base/basicpacketsocketfactory.cc \
	$(SUB_PATH)/base/dtlstransportchannel.cc \
	$(SUB_PATH)/base/jseptransport.cc \
	$(SUB_PATH)/base/p2pconstants.cc \
	$(SUB_PATH)/base/p2ptransportchannel.cc \
	$(SUB_PATH)/base/port.cc \
	$(SUB_PATH)/base/portallocator.cc \
	$(SUB_PATH)/base/pseudotcp.cc \
	$(SUB_PATH)/base/relayport.cc \
	$(SUB_PATH)/base/relayserver.cc \
	$(SUB_PATH)/base/session.cc \
	$(SUB_PATH)/base/sessiondescription.cc \
	$(SUB_PATH)/base/stun.cc \
	$(SUB_PATH)/base/stunport.cc \
	$(SUB_PATH)/base/stunrequest.cc \
	$(SUB_PATH)/base/stunserver.cc \
	$(SUB_PATH)/base/tcpport.cc \
	$(SUB_PATH)/base/transportcontroller.cc \
	$(SUB_PATH)/base/transportdescription.cc \
	$(SUB_PATH)/base/transportdescriptionfactory.cc \
	$(SUB_PATH)/base/turnport.cc \
	$(SUB_PATH)/base/turnserver.cc \
	$(SUB_PATH)/base/udptransport.cc \
	$(SUB_PATH)/client/basicportallocator.cc \
	$(SUB_PATH)/client/socketmonitor.cc