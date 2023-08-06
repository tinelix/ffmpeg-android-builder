# NDK Project Example (Android.mk).
# NOTICE: FFmpeg and FFmpeg custom builder for Android licensed under LGPLv3.0 or later version.

LOCAL_PATH := $(call my-dir)

#declare the prebuilt library

include $(CLEAR_VARS)

ifeq($(TARGET_ARCH_ABI),armeabi)
	ANDROID_ARCH = armv6
ifeq($(TARGET_ARCH_ABI),armeabi-v7a)
	ANDROID_ARCH = armv7
else
	ANDROID_ARCH = armv8
endif

FFMPEG_VERSION := 4.0.4

LOCAL_MODULE := ffmpeg-prebuilt

LOCAL_SRC_FILES := ../ffmpeg/android/${ANDROID_ARCH}/libffmpeg-v${FFMPEG_VERSION}.so

LOCAL_EXPORT_C_INCLUDES := ../ffmpeg/android/${ANDROID_ARCH}/include

LOCAL_EXPORT_LDLIBS := ../ffmpeg/android/${ANDROID_ARCH}/libffmpeg-v${FFMPEG_VERSION}.so

LOCAL_PRELINK_MODULE := true

include $(PREBUILT_SHARED_LIBRARY)
include $(CLEAR_VARS)

LOCAL_ALLOW_UNDEFINED_SYMBOLS=false
LOCAL_MODULE := ffmpeg-player
LOCAL_SRC_FILES := ffmpeg-player.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../ffmpeg/android/${ANDROID_ARCH}/include
LOCAL_SHARED_LIBRARY := ffmpeg-prebuilt
LOCAL_LDLIBS    := -llog -ljnigraphics -lz -lm $(LOCAL_PATH)/../ffmpeg/android/${ANDROID_ARCH}/libffmpeg-v${FFMPEG_VERSION}.so

include $(BUILD_SHARED_LIBRARY)

