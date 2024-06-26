#!/bin/bash
#
#  Copyright © 2023-2024 Dmitry Tretyakov (aka. Tinelix)
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

echo "FFmpeg custom builder for Android"
echo "Copyright (c) Dmitry Tretyakov (aka. Tinelix), 2023-2024"
echo "Licensed under LGPLv3 or later version.";
echo "";

FFMPEG_INPUT_ARCH="ARCH";

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

if [ ! -d "ffmpeg-1.1.12" ]; then
  echo "[ERROR] FFmpeg 1.1.12 source code not found. Please download it from https://github.com/ffmpeg/ffmpeg.";
  exit 1
fi

FFMPEG_VERSION="1.1.12"

echo "Creating output directories...";
chmod -R 0777 .
mkdir -p ffmpeg-1.1.12/android
mkdir -p ffmpeg-1.1.12/android/armeabi
mkdir -p ffmpeg-1.1.12/android/armeabi-v7a
mkdir -p ffmpeg-1.1.12/android/arm64-v8a
mkdir -p ffmpeg-1.1.12/android/x86

echo "Configuring FFmpeg v${FFMPEG_VERSION} build...";

FFMPEG_BUILD_PLATFORM="linux";

if [ $FFMPEG_INPUT_ARCH == "armv6" ]; then
	FFMPEG_TARGET_ARCH="armv6";
	FFMPEG_TARGET_CPU="armv6";
	ANDROID_TARGET_ARCH="armeabi";
	ANDROID_TOOLCHAIN_CPUABI="arm";
	ANDROID_TARGET_API=5;
elif [ $FFMPEG_INPUT_ARCH == "armv7" ]; then
	FFMPEG_TARGET_ARCH="arm";
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
else
	echo "Canceling...";
	exit 1;
fi;

FFMPEG_CFLAGS="-std=c99 -O3 -Wall -pipe -fPIC -fasm \
		-finline-limit=300 -ffast-math \
		-fno-strict-aliasing -Werror=strict-aliasing \
		-Wno-psabi \
		-DANDROID -DHAVE_SYS_UIO_H=1 -DNDEBUG"

FFMPEG_TARGET_OS="linux"


if [ $FFMPEG_INPUT_ARCH == "x86" ]; then
	FFMPEG_CFLAGS+=" -O2 -march=i686 -m32"
	ANDROID_NDK_SYSROOT="${ANDROID_NDK_HOME}/platforms/android-${ANDROID_TARGET_API}/arch-x86"
else
	ANDROID_NDK_SYSROOT="${ANDROID_NDK_HOME}/platforms/android-${ANDROID_TARGET_API}/arch-${ANDROID_TOOLCHAIN_CPUABI}"
fi;

if [ -z "$ANDROID_NDK_HOME" ]; then # requires NDK r10e+
	echo "[ERROR] NDK (requires r7b-r10e) not installed or ANDROID_NDK_HOME variable is not defined. Quiting...";
	exit 1
else
	if [[ $FFMPEG_INPUT_ARCH == "armv7" || $FFMPEG_INPUT_ARCH == "armv6" ]]; then
		FFMPEG_CPU_FLAGS="--enable-armv5te --enable-neon"
		if [ $NDK_RELEASE == "r8e" ]; then
			ANDROID_TOOLCHAIN_ROOT="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi-4.4.3/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86_64"
			ANDROID_NDK_TOOLCHAINS="${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi-"
			ANDROID_NDK_GCC="${ANDROID_TOOLCHAIN_ROOT}/lib/gcc/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi/4.4.3"
		elif [ $NDK_RELEASE == "r5b" ]; then
			ANDROID_TOOLCHAIN_ROOT="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TOOLCHAIN_CPUABI}-eabi-4.4.0/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86"
			ANDROID_NDK_TOOLCHAINS="${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_CPUABI}-eabi-"
			ANDROID_NDK_GCC="${ANDROID_TOOLCHAIN_ROOT}/lib/gcc/${ANDROID_TOOLCHAIN_CPUABI}-eabi/4.4.0/"
		elif [[ $NDK_RELEASE == "r6" || $NDK_RELEASE == "r7" ]]; then
			ANDROID_TOOLCHAIN_ROOT="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi-4.4.3/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86"
			ANDROID_NDK_TOOLCHAINS="${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi-"
			ANDROID_NDK_GCC="${ANDROID_TOOLCHAIN_ROOT}/lib/gcc/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi/4.4.3"
		else
			ANDROID_TOOLCHAIN_ROOT="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi-4.9/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86_64"
			ANDROID_NDK_TOOLCHAINS="${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi-"
			ANDROID_NDK_GCC="${ANDROID_TOOLCHAIN_ROOT}/lib/gcc/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-androideabi/4.9"
		fi;
    else
        FFMPEG_CPU_FLAGS="--disable-asm"
        OPTIMIZE_CFLAGS="-m32"
        ANDROID_NDK_SYSROOT="${ANDROID_NDK_HOME}/platforms/android-${ANDROID_TARGET_API}/arch-${ANDROID_TARGET_ARCH}"
        if [ $NDK_RELEASE == "r8e" ]; then
			ANDROID_TOOLCHAIN_ROOT="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TARGET_ARCH}-4.4.3/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86_64"
			ANDROID_NDK_TOOLCHAINS="${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-android-"
			ANDROID_NDK_GCC="${ANDROID_TOOLCHAIN_ROOT}/lib/gcc/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-android/4.4.3"
		elif [[ $NDK_RELEASE == "r6" || $NDK_RELEASE == "r7" ]]; then
			ANDROID_TOOLCHAIN_ROOT="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TARGET_ARCH}-4.4.3/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86"
			ANDROID_NDK_TOOLCHAINS="${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_CPUABI}-android-${FFMPEG_BUILD_PLATFORM}-"
			ANDROID_NDK_GCC="$/lib/gcc/${ANDROID_TOOLCHAIN_CPUABI}-android-${FFMPEG_BUILD_PLATFORM}/4.4.3"
		elif [ $NDK_RELEASE == "r5b" ]; then
			ANDROID_TOOLCHAIN_ROOT="${ANDROID_NDK_HOME}/toolchains/${ANDROID_TARGET_ARCH}-4.2.1/prebuilt/${FFMPEG_BUILD_PLATFORM}-x86"
			ANDROID_NDK_TOOLCHAINS="${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_CPUABI}-android-${FFMPEG_BUILD_PLATFORM}-"
			ANDROID_NDK_GCC="${ANDROID_TOOLCHAIN_ROOT}/lib/gcc/${ANDROID_TOOLCHAIN_CPUABI}-${FFMPEG_BUILD_PLATFORM}-android/4.2.1"
		fi;
	fi;
fi

FFMPEG_FLAGS="--target-os=${FFMPEG_TARGET_OS} \
		--prefix=./android/${ANDROID_TARGET_ARCH} \
		--disable-everything \
		--enable-cross-compile \
		--arch=${FFMPEG_TARGET_ARCH} \
		--cc=${ANDROID_NDK_TOOLCHAINS}gcc \
		--cross-prefix=${ANDROID_NDK_TOOLCHAINS} \
		--nm=${ANDROID_NDK_TOOLCHAINS}nm \
		--sysroot=${ANDROID_NDK_SYSROOT} \
		--disable-gpl \
		--enable-version3 \
		--disable-nonfree \
		--enable-avcodec \
		--enable-avformat \
		--enable-avutil \
		--enable-swscale \
		--enable-avfilter \
		--disable-programs \
		--disable-ffmpeg \
		--disable-ffplay \
		--disable-ffprobe \
		--disable-doc \
		--disable-htmlpages \
		--disable-dxva2 \
		--disable-vaapi \
		--disable-vdpau \
		--disable-encoders \
		--disable-decoders \
		--disable-demuxers \
		--disable-parsers \
		--disable-muxers \
		--disable-filters \
		--disable-debug \
		--enable-network \
		--enable-protocol=file,http,async,tcp,tls \
		--enable-parser=h263,h264,vp8,flac,aac,aac_latm,vorbis,mp3,ogg,theora \
		--enable-demuxer=flv,mp3,mp4,ogg,ogv,mov,h264,data \
		--enable-decoder=mp3,aac,aac_latm,vp8,h263,h264,theora,flac,vorbis \
		--enable-encoder=libmp3lame,vorbis,aac \
		--enable-muxer=mp4,ogg,mp3 \
		--enable-small \
		--enable-inline-asm \
		--enable-optimizations"

cd ffmpeg-1.1.12

if [ $FFMPEG_INPUT_ARCH != "x86" ]; then
	FFMPEG_FLAGS+=" --enable-yasm \
		--enable-asm"
else
	FFMPEG_FLAGS+=" --disable-asm"
fi;

# Build workaround special for FFmpeg 3.1.x
rm -rf ./compat/strtod.o
rm -rf ./compat/strtod.d

if [ -f "dos2unix" ]; then
    dos2unix ./configure
    dos2unix ./fake-pkg-config
fi;

if ./configure $FFMPEG_FLAGS --extra-cflags="$FFMPEG_CFLAGS"; then
	echo;
	echo "Build starts in 15 seconds. Wait or press CTRL+Z for cancel.";
	sleep 15s;
	echo;
	echo "Building FFmpeg for ${ANDROID_TARGET_ARCH}...";
else
	echo;
	echo "Build configuration error."
	echo;
	exit 1;
fi;

if make clean && make -j8 && make install ; then
	echo;
	echo "Linking FFmpeg libraries...";
	if ${ANDROID_NDK_TOOLCHAINS}ld \
		-rpath-link=${ANDROID_NDK_SYSROOT}/usr/lib \
		-L${ANDROID_NDK_SYSROOT}/usr/lib \
		-L./android/${ANDROID_TARGET_ARCH}/lib \
		-soname libffmpeg.so -shared -nostdlib -Bsymbolic --whole-archive --no-undefined -o \
		./android/${ANDROID_TARGET_ARCH}/libffmpeg.so \
		libavcodec/libavcodec.a \
		libavfilter/libavfilter.a \
		libswresample/libswresample.a \
		libavformat/libavformat.a \
		libavutil/libavutil.a \
		libswscale/libswscale.a \
		-lc -lm -lz -ldl -llog --dynamic-linker=/system/bin/linker \
		${ANDROID_NDK_GCC}/libgcc.a;
	then
		echo;
		echo "FFmpeg successfully builded!";
		echo;
		echo "Copy *.so file to '[app module]/src/main/jniLibs' of your Android project."
		echo "*.so file and headers placed in './ffmpeg/android/${ANDROID_TARGET_ARCH}' directory."
		echo;
	else
		echo;
		echo "ERROR: Unfortunately, you can't build FFmpeg.";
		echo;
		exit 1;
	fi;
else
	echo;
	echo "ERROR: Unfortunately, you can't build FFmpeg.";
	echo;
	exit 1;
fi;
