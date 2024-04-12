# FFmpeg custom builder for Android
This is a special script that makes it easy to build [FFmpeg 0.7.1](https://github.com/FFmpeg/FFmpeg/tree/n0.7.1), [FFmpeg 0.11.5](https://github.com/FFmpeg/FFmpeg/tree/n0.11.5), [FFmpeg 3.1.4](https://github.com/FFmpeg/FFmpeg/tree/n3.1.4) and [FFmpeg 4.0.4](https://github.com/FFmpeg/FFmpeg/tree/n4.0.4) for Android. Builds FFmpeg 0.11.5 without problems in Android NDK r8c with Android 2.0 NDK platform (kinda), but builds FFmpeg 3.1.4 in Android NDK r11c as well.

### Building
1. If there is none in-the-box, install the missing packages:
   
   ```sh
   # for Ubuntu/Debian/Linux Mint
   sudo apt-get install gcc g++ yasm gettext autoconf automake cmake git git-core \
                        libass-dev libfreetype6-dev libmp3lame-dev libsdl2-dev libtool \
                        libvdpau-dev libvorbis-dev pkg-config wget zlib1g-dev texinfo
   ```
3. Download Android NDK (`r5`, `r7b` or `r8e`/`r11c`). \
   <sub>See [NDK r8e download page](http://web.archive.org/web/20130629195058/http://developer.android.com/tools/sdk/ndk/index.html#Downloads) or [list of unsupported NDK versions](https://github.com/android/ndk/wiki/Unsupported-Downloads).</sub>
5. Clone repo.
   
   ```sh
   git clone https://github.com/tinelix/ffmpeg-android-builder.git
   cd ffmpeg-android-builder
   ```
7. Change `./build-android-x.y.z.sh` file and `ffmpeg-x.y.z` directories permissions to `0777` (`chmod -R 0777 .`) and run it.

### License
This builder using FFmpeg (modified) source code licensed under LGPLv2.1 or later version with LGPLv3 licensing model as default. Scripts are also licensed under LGPLv3.
