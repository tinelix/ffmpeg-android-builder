#!/bin/bash
#  Build FFmpeg 0.11.5 for legacy Android devices
#
#  Copyright © 2023 Dmitry Tretyakov (aka. Tinelix)
#
#  This program is free software: you can redistribute it and/or modify it under the terms of
#  the GNU Lesser General Public License as published by the Free Software Foundation, either
#  version 3 of the License, or (at your option) any later version.
#  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
#  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#  See the GNU Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public License along with this
#  program. If not, see https://www.gnu.org/licenses/.
#
#  Source code: https://github.com/tinelix/ffmpeg-android-builder

FFMPEG_VERSION="$(cat ./ffmpeg-0.11.5/RELEASE)"

echo "FFmpeg custom builder for Android"
echo "Copyright (c) Dmitry Tretyakov (aka. Tinelix), 2023"
echo "Licensed under LGPLv3 or later version.";
echo "";

FFMPEG_INPUT_ARCH="ARCH";
NDK_RELEASE="NDK_REL";

if [[ -z $1 ]];
then
    read -p "Specify architecture [armv6, armv7, x86]: " FFMPEG_INPUT_ARCH
else
    FFMPEG_INPUT_ARCH=$1
fi

if [[ -z $2 ]];
then
    read -p "Specify NDK release [r7, r8e, r10e]: " NDK_RELEASE
else
    NDK_RELEASE=$2
fi

if [ ! -d "ffmpeg-0.11.5" ]; then
  echo "[ERROR] FFmpeg 0.11.5 source code not found. Please download it from https://github.com/ffmpeg/ffmpeg.";
  exit 1
fi

echo "Creating output directories...";
chmod -R 0777 .
mkdir -p ffmpeg-0.11.5/android
mkdir -p ffmpeg-0.11.5/android/armeabi
mkdir -p ffmpeg-0.11.5/android/armeabi-v7a
mkdir -p ffmpeg-0.11.5/android/x86

cd ffmpeg-0.11.5

echo "Configuring FFmpeg v$FFMPEG_VERSION build for $FFMPEG_INPUT_ARCH...";

FFMPEG_BUILD_PLATFORM="linux";
FFMPEG_CPU_FLAGS=""

if [ $FFMPEG_INPUT_ARCH == "armv6" ]; then
	FFMPEG_TARGET_ARCH="armv6";
	FFMPEG_TARGET_CPU="armv6";
	ANDROID_TARGET_ARCH="armeabi";
	ANDROID_TOOLCHAIN_CPUABI="arm";
	ANDROID_TARGET_API=5;
elif [ $FFMPEG_INPUT_ARCH == "armv7" ]; then
	FFMPEG_TARGET_ARCH="armv7";
	FFMPEG_TARGET_CPU="armv7-a";
	ANDROID_TARGET_ARCH="armeabi-v7a";
	ANDROID_TOOLCHAIN_CPUABI="arm";
	ANDROID_TARGET_API=5;
elif [ $FFMPEG_INPUT_ARCH == "x86" ]; then
	FFMPEG_TARGET_ARCH="x86";
	FFMPEG_TARGET_CPU="x86";
	ANDROID_TARGET_ARCH="x86";
	ANDROID_TOOLCHAIN_CPUABI="i686";
	ANDROID_TARGET_API=9;
elif [ $FFMPEG_INPUT_ARCH == "armv8a" ]; then
	echo "ARMv8a not supported for legacy Android versions. Canceling...";
	exit 1;
else
	echo "Canceling...";
	exit 1;
fi;

FFMPEG_CFLAGS="-O3 -fpic -DANDROID -DHAVE_SYS_UIO_H=1 -fasm -fno-strict-aliasing -finline-limit=300 -Wno-psabi -fno-short-enums -I${ANDROID_NDK_HOME}/platforms/android-${ANDROID_TARGET_API}/arch-arm/usr/include"
ANDROID_NDK_SYSROOT="${ANDROID_NDK_HOME}/platforms/android-${ANDROID_TARGET_API}/arch-${ANDROID_TOOLCHAIN_CPUABI}"
OPTIMIZE_CFLAGS="-mfloat-abi=softfp -mfpu=vfpv3-d16 -marm -march=${FFMPEG_TARGET_CPU}"


if [ -z "$ANDROID_NDK_HOME" ]; then # requires NDK r7b-r10e
	echo "[ERROR] NDK (requires r7b-r10e) not installed or ANDROID_NDK_HOME variable is not defined. Quiting...";
	exit 1
else
	if [ $FFMPEG_INPUT_ARCH == "armv8a" ]; then
		ANDROID_NDK_TOOLCHAINS="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-android-4.9/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86_64/bin/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-android"
		ANDROID_NDK_GCC="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-android-4.9/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86_64/lib/gcc/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-android/4.9"
	elif [[ $FFMPEG_INPUT_ARCH == "armv7" || $FFMPEG_INPUT_ARCH == "armv6" ]]; then
        FFMPEG_CPU_FLAGS="--enable-armv5te --enable-neon"
		if [ $NDK_RELEASE == "r8e" ]; then
			ANDROID_NDK_TOOLCHAINS="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi-4.4.3/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86_64/bin/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi"
			ANDROID_NDK_GCC="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi-4.4.3/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86_64/lib/gcc/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi/4.4.3"
        elif [ $NDK_RELEASE == "r5b" ]; then
            ANDROID_NDK_TOOLCHAINS="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TOOLCHAIN_CPUABI}-eabi-4.4.0/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86/bin/${ANDROID_TOOLCHAIN_CPUABI}-eabi"
			ANDROID_NDK_GCC="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TOOLCHAIN_CPUABI}-eabi-4.4.0/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86/lib/gcc/${ANDROID_TOOLCHAIN_CPUABI}-eabi/4.4.0/"
        elif [ $NDK_RELEASE == "r6" ]; then
			ANDROID_NDK_TOOLCHAINS="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi-4.4.3/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86/bin/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi"
			ANDROID_NDK_GCC="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi-4.4.3/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86/lib/gcc/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi/4.4.3"
        elif [ $NDK_RELEASE == "r7" ]; then
			ANDROID_NDK_TOOLCHAINS="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi-4.4.3/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86/bin/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi"
			ANDROID_NDK_GCC="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi-4.4.3/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86/lib/gcc/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi/4.4.3"
		else
			ANDROID_NDK_TOOLCHAINS="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi-4.9/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86_64/bin/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi"
			ANDROID_NDK_GCC="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi-4.9/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86_64/lib/gcc/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi/4.9"
		fi;
    else
        FFMPEG_CPU_FLAGS="--disable-asm"
        OPTIMIZE_CFLAGS="-m32"
        ANDROID_NDK_SYSROOT="${ANDROID_NDK_HOME}/platforms/android-${ANDROID_TARGET_API}/arch-${ANDROID_TARGET_ARCH}"
        if [ $NDK_RELEASE == "r8e" ]; then
			ANDROID_NDK_TOOLCHAINS="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TARGET_ARCH}-4.4.3/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86_64/bin/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-android"
			ANDROID_NDK_GCC="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TARGET_ARCH}-4.4.3/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86_64/lib/gcc/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-android/4.4.3"
        elif [ $NDK_RELEASE == "r7" ]; then
			ANDROID_NDK_TOOLCHAINS="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TARGET_ARCH}-4.4.3/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86/bin/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-android"
			ANDROID_NDK_GCC="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TARGET_ARCH}-4.4.3/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86/lib/gcc/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-android/4.4.3"
        elif [ $NDK_RELEASE == "r6" ]; then
			ANDROID_NDK_TOOLCHAINS="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TARGET_ARCH}-4.4.3/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86/bin/${ANDROID_TOOLCHAIN_CPUABI}-android-${FFMPEG_BUILD_PLATFORM}"
			ANDROID_NDK_GCC="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TARGET_ARCH}-4.4.3/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86/lib/gcc/${ANDROID_TOOLCHAIN_CPUABI}-android-${FFMPEG_BUILD_PLATFORM}/4.4.3"
        elif [ $NDK_RELEASE == "r5b" ]; then
            ANDROID_NDK_TOOLCHAINS="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TARGET_ARCH}-4.2.1/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86/bin/${ANDROID_TOOLCHAIN_CPUABI}-android-${FFMPEG_BUILD_PLATFORM}"
			ANDROID_NDK_GCC="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TARGET_ARCH}-4.2.1/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86/lib/gcc/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-android/4.2.1"
		else
			ANDROID_NDK_TOOLCHAINS="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TARGET_ARCH}-4.9/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86_64/bin/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-android"
			ANDROID_NDK_GCC="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TARGET_ARCH}-4.9/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86_64/lib/gcc/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-android/4.9"
		fi;
	fi;
fi

FFMPEG_FLAGS="--prefix=./android/$ANDROID_TARGET_ARCH
    --cross-prefix=$ANDROID_NDK_TOOLCHAINS-
    --enable-cross-compile
    --target-os=linux
    --arch=arm
    --disable-shared
    --enable-static
    --disable-gpl
    --enable-version3
    --disable-nonfree
    --disable-doc
    --disable-ffmpeg
    --disable-ffplay
    --disable-ffprobe
    --disable-ffserver
    --disable-avdevice
    --disable-avfilter
    --disable-postproc
    --disable-encoders
    --disable-decoders
    --disable-demuxers
    --disable-parsers
    --disable-muxers
    --disable-filters
    --disable-bsfs
    --enable-network
    --disable-protocol=udp,gopher,rtmp,rtp,srtp
    --enable-parser=h263,h264,theora,vp8,flac,aac,aac_latm,vorbis,mp3,mpeg4video
    --enable-demuxer=flv,mp3,ogv,ogg,data,mp4
    --enable-decoder=mp3,aac,vp8,h263,h264,theora,flac,vorbis,aac,aac_latm,mpeg4video
    --enable-encoder=libmp3lame,vorbis,h264,aac
    --enable-muxer=mp4,ogg,mp3
    --disable-symver
    --disable-debug
    --disable-stripping
    --enable-small"

if [ $FFMPEG_INPUT_ARCH == "x86" ]; then
    ./configure $FFMPEG_FLAGS --extra-ldflags="-Wl,-rpath-link=$ANDROID_NDK_SYSROOT/usr/lib -L$ANDROID_NDK_SYSROOT/usr/lib -nostdlib -lm -ldl -llog" --extra-cflags="-I$ANDROID_NDK_SYSROOT/usr/include " $FFMPEG_CPU_FLAGS
else
    ./configure $FFMPEG_FLAGS --extra-ldflags="-Wl,-rpath-link=$ANDROID_NDK_SYSROOT/usr/lib -L$ANDROID_NDK_SYSROOT/usr/lib -nostdlib -lc -lm -ldl -llog" --extra-cflags="$FFMPEG_CFLAGS" $FFMPEG_CPU_FLAGS
fi;
sed -i 's/HAVE_LRINT 0/HAVE_LRINT 1/g' config.h
sed -i 's/HAVE_LRINTF 0/HAVE_LRINTF 1/g' config.h
sed -i 's/HAVE_ROUND 0/HAVE_ROUND 1/g' config.h
sed -i 's/HAVE_ROUNDF 0/HAVE_ROUNDF 1/g' config.h
sed -i 's/HAVE_TRUNC 0/HAVE_TRUNC 1/g' config.h
sed -i 's/HAVE_TRUNCF 0/HAVE_TRUNCF 1/g' config.h
echo;
echo "Build starts in 15 seconds. Wait or press CTRL+Z for cancel.";
sleep 15s;
echo;
echo "Building FFmpeg for ${ANDROID_TARGET_ARCH}...";
make clean
make  -j4 install
echo;
echo "Linking FFmpeg libraries...";
$ANDROID_NDK_TOOLCHAINS-ar d libavcodec/libavcodec.a inverse.o
$ANDROID_NDK_TOOLCHAINS-ld -rpath-link=$ANDROID_NDK_SYSROOT/usr/lib -L$ANDROID_NDK_SYSROOT/usr/lib  -soname libffmpeg.so -shared -nostdlib  -Bsymbolic --whole-archive --no-undefined -o ./android/$ANDROID_TARGET_ARCH/libffmpeg.so libavcodec/libavcodec.a libavformat/libavformat.a libavutil/libavutil.a libswscale/libswscale.a -lc -lm -lz -ldl -llog --dynamic-linker=/system/bin/linker $ANDROID_NDK_GCC/libgcc.a
echo;
echo "FFmpeg successfully builded!";
echo;
echo "Copy *.so file to '[app module]/src/main/jniLibs' of your Android project."
echo "*.so file and headers placed in './ffmpeg/android/${ANDROID_TARGET_ARCH}' directory."
echo;
