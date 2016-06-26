// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a stub config.h for libSRTP. It doesn't define anything besides
// version number strings because the build is configured by libsrtp.gyp.
#ifndef _LIBSRTP_CONFIG_H
#define _LIBSRTP_CONFIG_H

#define PACKAGE_STRING "libsrtp2 2.0.0-pre"
#define PACKAGE_VERSION "2.0.0-pre"

#if defined(_MSC_VER) && !defined(__cplusplus)
  // Microsoft provides "inline" only for C++ code
  #define inline __inline
#endif

#define HAVE_CONFIG_H
#define HAVE_STDLIB_H
// #define HAVE_STRING_H

#define OPENSSL
#define CPU_CISC
#define TESTAPP_SOURCE

// BoringSSL doesn't support AES-192, cipher will be disabled
#define SRTP_NO_AES192

#ifdef _WIN32
	#define HAVE_BYTESWAP_METHODS_H
	#define HAVE_WINSOCK2_H
#elif __APPLE__
	#define HAVE_NETINET_IN_H
#elif ANDROID
	#define HAVE_NETINET_IN_H
#endif

#endif
