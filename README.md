# FFmpeg custom builder for Android
This is a special script that makes it easy to build [FFmpeg 0.8.12](https://github.com/FFmpeg/FFmpeg/tree/n0.8.12), [FFmpeg 3.1.4](https://github.com/FFmpeg/FFmpeg/tree/n3.1.4) and [FFmpeg 4.0.4](https://github.com/FFmpeg/FFmpeg/tree/n4.0.4) for Android. Builds FFmpeg 0.8.12 without problems in Android NDK r8c with Android 2.0 NDK platform (kinda), but builds FFmpeg 3.1.4 in Android NDK r11c as well.

### Building
1. If there is none in-the-box, install the missing packages in your package manager: `gcc` `g++` `yasm` `gettext` `autoconf` `automake` `cmake` `git` `git-core` `libass-dev` `libfreetype6-dev` `libmp3lame-dev` `libsdl2-dev` `libtool` `libvdpau-dev` `libvorbis-dev` `pkg-config` `wget` `zlib1g-dev` `texinfo` for Ubuntu/Debian and their based distributions.
2. Download Android NDK (`r5`, `r7b` or `r8c`/`r11c`): [NDK archived download page](http://web.archive.org/web/20130629195058/http://developer.android.com/tools/sdk/ndk/index.html#Downloads) and [Unsupported NDK Versions](https://github.com/android/ndk/wiki/Unsupported-Downloads)
3. Clone repo.
   `git clone https://github.com/tinelix/ffmpeg-android-builder.git`
4. `cd ffmpeg-android-builder`
5. Change `./build-android-x.y.z.sh` file and `ffmpeg-x.y.z` directories permissions to `0777` (`chmod -R 0777 .`) and run it.

### License
This builder using FFmpeg (modified) source code licensed under LGPLv2.1 or later version with LGPLv3 licensing model as default. Scripts are also licensed under this same license.
