# FFmpeg custom builder for Android
This is a universal script that makes it easy to build [FFmpeg 0.7.1](https://github.com/FFmpeg/FFmpeg/tree/n0.7.1), [FFmpeg 0.11.5](https://github.com/FFmpeg/FFmpeg/tree/n0.11.5), [FFmpeg 3.1.4](https://github.com/FFmpeg/FFmpeg/tree/n3.1.4) and [FFmpeg 4.0.4](https://github.com/FFmpeg/FFmpeg/tree/n4.0.4) for Android.

### Building
1. If there is none in-the-box, install the missing packages:
   
   ```sh
   # for Ubuntu/Debian/Linux Mint
   # Windows and macOS currently not supported
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

### Compatibility table

```
-------------------------------------------------------------------------------------------------------------------|
| FFmpeg version  | NDK version | Supported ABIs   | Supported Android versions   | Tested in Linux distros        |
|-----------------|-------------|------------------|------------------------------|--------------------------------|
| 4.0.4           | r11c        | armeabi          | Android 2.0 and above        | Debian 8.11.0                  |
|                 |             | armeabi-v7a      |                              |                                |
|                 |             | x86-eabi         |                              |                                |
|                 |             | arm64-v8a        |                              |                                |
|-----------------|-------------|------------------|------------------------------|--------------------------------|
| 3.1.4           | r11c        | armeabi          | Android 2.0 and above        |                                |
|                 |             | armeabi-v7a      |                              |                                |
|                 |             | x86-eabi         |                              |                                |
|                 |             | arm64-v8a        |                              |                                |
|-----------------|-------------|------------------|------------------------------|--------------------------------|
| 0.11.5          | r8e         | armeabi          | Android 2.0 and above        |                                |
|                 |             | armeabi-v7a      |                              |                                |
|-----------------|-------------|------------------|------------------------------|--------------------------------|
| 0.7.1           | r8e         | armeabi          | Android 2.0 and above        |                                |
|                 |             | armeabi-v7a      |                              |                                |
|                 |             | x86              |                              |                                |
----------------------------------------------------------------------------------|--------------------------------|
```

### License
This builder using FFmpeg (modified) source code licensed under [LGPLv2.1 or later version](https://github.com/tinelix/ffmpeg-android-builder/blob/main/COPYING.FFMPEG.LGPLv2.1) with [LGPLv3 licensing model](https://github.com/tinelix/ffmpeg-android-builder/blob/main/COPYING.FFMPEG.LGPLv3) as default. Scripts are also licensed under [LGPLv3](https://github.com/tinelix/ffmpeg-android-builder/blob/main/COPYING.BUILDER).
