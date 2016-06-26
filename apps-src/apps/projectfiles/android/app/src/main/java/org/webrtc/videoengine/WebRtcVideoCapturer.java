/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.videoengine;

import android.content.Context;

import android.util.Log;

import org.libsdl.app.SDLActivity;
import org.webrtc.AudioSource;
import org.webrtc.AudioTrack;
import org.webrtc.Camera1Enumerator;
import org.webrtc.Camera2Enumerator;

import org.webrtc.CameraEnumerator;
import org.webrtc.EglBase;
import org.webrtc.IceCandidate;
import org.webrtc.Logging;

import org.webrtc.VideoCapturer;
import org.webrtc.VideoSource;

/**
 * Peer connection client implementation.
 *
 * <p>All public methods are routed to local looper thread.
 * All PeerConnectionEvents callbacks are invoked from the same looper thread.
 * This class is a singleton.
 */

public class WebRtcVideoCapturer implements VideoCapturer.CapturerObserver {
  private static final String TAG = "WebRtcVideoCapture";
  private static final String[] MANDATORY_PERMISSIONS = {"android.permission.MODIFY_AUDIO_SETTINGS",
          "android.permission.RECORD_AUDIO", "android.permission.INTERNET", "android.permission.CAMERA"};
  private final boolean useCamera2;
  private final long nativeVideoCapturer;

  private EglBase rootEglBase;
  private EglBase localEglbase;

  private Context context;
  private VideoSource videoSource;
  private int videoWidth;
  private int videoHeight;
  private int videoFps;
  // private CameraVideoCapturer videoCapturer;
  private VideoCapturer videoCapturer;

  WebRtcVideoCapturer(Context context, long nativeVideoCapturer) {
    Logging.d(TAG, "WebRtcVideoCapture ctor");
    this.context = context;
    this.nativeVideoCapturer = nativeVideoCapturer;
    useCamera2 = Camera2Enumerator.isSupported(context);
  }

  private void dispose() {
    Logging.d(TAG, "WebRtcVideoCapture ~ctor");
    if (videoCapturer == null) {
      return;
    }
    closeInternal();
  }

  private boolean init() {
    Logging.d(TAG, "WebRtcVideoCapture init");
    return SDLActivity.checkWebrtcPermission(context, MANDATORY_PERMISSIONS);
/*
    // Check for mandatory permissions.
    for (String permission : MANDATORY_PERMISSIONS) {
      if (ActivityCompat.checkSelfPermission(this.context, permission) != PackageManager.PERMISSION_GRANTED) {
        Logging.d(TAG, "Permission " + permission + " is not granted");
        // ActivityCompat.requestPermissions(SDLActivity.this, new String[]{Manifest.permission.CAMERA}, REQUEST_CAMERA_PERMISSION);

        // setResult(RESULT_CANCELED);
        // finish();
        return false;
      }
    }
    return true;
*/
  }

  private boolean isCommunicationModeEnabled() {
    Logging.d(TAG, "WebRtcVideoCapture isCommunicationModeEnabled");
    return true;
  }

  private boolean captureToTexture() {
    return true;
  }

  private void reportError(final String errorMessage) {
    Log.e(TAG, "WebRtcVideoCapture error: " + errorMessage);
/*    executor.execute(new Runnable() {
      @Override
      public void run() {
        if (!isError) {
          events.onPeerConnectionError(errorMessage);
          isError = true;
        }
      }
    });
*/
  }

  private VideoCapturer createCameraCapturer(CameraEnumerator enumerator) {
    final String[] deviceNames = enumerator.getDeviceNames();
/*
    // First, try to find front facing camera
    Logging.d(TAG, "Looking for front facing cameras.");
    for (String deviceName : deviceNames) {
      if (enumerator.isFrontFacing(deviceName)) {
        Logging.d(TAG, "Creating front facing camera capturer.");
        VideoCapturer videoCapturer = enumerator.createCapturer(deviceName, null);

        if (videoCapturer != null) {
          return videoCapturer;
        }
      }
    }
*/
    // Front facing camera not found, try something else
    Logging.d(TAG, "Looking for other cameras.");
    for (String deviceName : deviceNames) {
      if (!enumerator.isFrontFacing(deviceName)) {
        Logging.d(TAG, "Creating other camera capturer.");
        VideoCapturer videoCapturer = enumerator.createCapturer(deviceName, null);

        if (videoCapturer != null) {
          return videoCapturer;
        }
      }
    }

    return null;
  }

  private boolean startCapture() {
    if (useCamera2) {
      if (!captureToTexture()) {
        reportError("Camera2 only supports capturing to texture. Either disable Camera2 or enable capturing to texture in the options.");
        return false;
      }
      Logging.d(TAG, "Creating capturer using camera2 API.");
      videoCapturer = createCameraCapturer(new Camera2Enumerator(context));
    } else {
      Logging.d(TAG, "Creating capturer using camera1 API.");
      videoCapturer = createCameraCapturer(new Camera1Enumerator(captureToTexture()));
    }

    if (videoCapturer == null) {
      reportError("Failed to open camera");
      return false;
    }

    // videoSource = factory.createVideoSource(videoCapturer);
    rootEglBase = EglBase.create();;
    localEglbase = EglBase.create(rootEglBase.getEglBaseContext());
    final EglBase.Context eglContext = localEglbase.getEglBaseContext();
    nativeCreateSurfaceTextureHelper(nativeVideoCapturer, eglContext);

    // VideoCapturer.CapturerObserver capturerObserver = new VideoCapturer.AndroidVideoTrackSourceObserver(nativeVideoCapturer);
    // nativeInitializeVideoCapturer(nativeVideoCapturer, videoCapturer, capturerObserver);
    nativeInitializeVideoCapturer(nativeVideoCapturer, videoCapturer, this);
    // videoSource = new VideoSource(nativeVideoCapturer);

    // videoWidth = 1280;
    // videoHeight = 720;
    videoWidth = 640;
    videoHeight = 480;
    videoFps = 30;
    videoCapturer.startCapture(videoWidth, videoHeight, videoFps);

    Log.d(TAG, "Peer connection created.");
    return true;
  }

  private void closeInternal() {
    // localEglbase.release();
    // rootEglBase.release();
/*
    if (factory != null && peerConnectionParameters.aecDump) {
      factory.stopAecDump();
    }
    Log.d(TAG, "Closing peer connection.");
    statsTimer.cancel();
    if (peerConnection != null) {
      peerConnection.dispose();
      peerConnection = null;
    }
    Log.d(TAG, "Closing audio source.");
    if (audioSource != null) {
      audioSource.dispose();
      audioSource = null;
    }
*/
    Log.d(TAG, "Stopping capture.");
    if (videoCapturer != null) {
      try {
        videoCapturer.stopCapture();
      } catch(InterruptedException e) {
        throw new RuntimeException(e);
      }
      videoCapturer.dispose();
      videoCapturer = null;
    }

    if (localEglbase != null) {
      localEglbase.release();
    }
    if (rootEglBase != null) {
      rootEglBase.release();
    }
/*
    Log.d(TAG, "Closing video source.");
    if (videoSource != null) {
      videoSource.dispose();
      videoSource = null;
    }
    Log.d(TAG, "Closing peer connection factory.");
    if (factory != null) {
      factory.dispose();
      factory = null;
    }
    options = null;
    Log.d(TAG, "Closing peer connection done.");
    events.onPeerConnectionClosed();
    PeerConnectionFactory.stopInternalTracingCapture();
    PeerConnectionFactory.shutdownInternalTracer();
*/
  }

  @Override
  public void onCapturerStarted(boolean success) {
    nativeCapturerStarted(nativeVideoCapturer, success);
  }

  @Override
  public void onCapturerStopped() {
    nativeCapturerStopped(nativeVideoCapturer);
  }

  @Override
  public void onByteBufferFrameCaptured(byte[] data, int width, int height,
                                        int rotation, long timeStamp) {
    nativeOnByteBufferFrameCaptured(nativeVideoCapturer, data, data.length, width, height, rotation, timeStamp);
  }

  @Override
  public void onTextureFrameCaptured(
          int width, int height, int oesTextureId, float[] transformMatrix, int rotation,
          long timestamp) {
    nativeOnTextureFrameCaptured(nativeVideoCapturer, width, height, oesTextureId, transformMatrix,
            rotation, timestamp);
  }


  private native void nativeCapturerStarted(long nativeSource,
          boolean success);
  private native void nativeCapturerStopped(long nativeSource);
  private native void nativeOnByteBufferFrameCaptured(long nativeSource,
          byte[] data, int length, int width, int height, int rotation, long timeStamp);
  private native void nativeOnTextureFrameCaptured(
          long nativeSource, int width, int height,
          int oesTextureId, float[] transformMatrix, int rotation, long timestamp);
  // private native void nativeOnOutputFormatRequest(long nativeSource,
  //        int width, int height, int framerate);


  // Delivers a captured frame in a texture with id |oesTextureId|. Called on a Java thread
  // owned by VideoCapturer.
  private native void nativeCreateSurfaceTextureHelper(
          long nativeSource, EglBase.Context eglContext);

  private native void nativeInitializeVideoCapturer(
          long nativeSource, VideoCapturer j_video_capturer, VideoCapturer.CapturerObserver j_frame_observer);
}
