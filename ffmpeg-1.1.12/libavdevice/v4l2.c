/*
 * Copyright (c) 2000,2001 Fabrice Bellard
 * Copyright (c) 2006 Luca Abeni
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * Video4Linux2 grab interface
 *
 * Part of this file is based on the V4L2 video capture example
 * (http://v4l2spec.bytesex.org/v4l2spec/capture.c)
 *
 * Thanks to Michael Niedermayer for providing the mapping between
 * V4L2_PIX_FMT_* and AV_PIX_FMT_*
 */

#undef __STRICT_ANSI__ //workaround due to broken kernel headers
#include "config.h"
#include "libavformat/internal.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#if HAVE_SYS_VIDEOIO_H
#include <sys/videoio.h>
#else
#if HAVE_ASM_TYPES_H
#include <asm/types.h>
#endif
#include <linux/videodev2.h>
#endif
#include "libavutil/avassert.h"
#include "libavutil/imgutils.h"
#include "libavutil/log.h"
#include "libavutil/opt.h"
#include "avdevice.h"
#include "timefilter.h"
#include "libavutil/parseutils.h"
#include "libavutil/pixdesc.h"
#include "libavutil/avstring.h"

#if CONFIG_LIBV4L2
#include <libv4l2.h>
#else
#define v4l2_open   open
#define v4l2_close  close
#define v4l2_dup    dup
#define v4l2_ioctl  ioctl
#define v4l2_read   read
#define v4l2_mmap   mmap
#define v4l2_munmap munmap
#endif

static const int desired_video_buffers = 256;

#define V4L_ALLFORMATS  3
#define V4L_RAWFORMATS  1
#define V4L_COMPFORMATS 2

/**
 * Return timestamps to the user exactly as returned by the kernel
 */
#define V4L_TS_DEFAULT  0
/**
 * Autodetect the kind of timestamps returned by the kernel and convert to
 * absolute (wall clock) timestamps.
 */
#define V4L_TS_ABS      1
/**
 * Assume kernel timestamps are from the monotonic clock and convert to
 * absolute timestamps.
 */
#define V4L_TS_MONO2ABS 2

/**
 * Once the kind of timestamps returned by the kernel have been detected,
 * the value of the timefilter (NULL or not) determines whether a conversion
 * takes place.
 */
#define V4L_TS_CONVERT_READY V4L_TS_DEFAULT

struct video_data {
    AVClass *class;
    int fd;
    int frame_format; /* V4L2_PIX_FMT_* */
    int width, height;
    int frame_size;
    int interlaced;
    int top_field_first;
    int ts_mode;
    TimeFilter *timefilter;
    int64_t last_time_m;

    int buffers;
    void **buf_start;
    unsigned int *buf_len;
    char *standard;
    int channel;
    char *pixel_format; /**< Set by a private option. */
    int list_format;    /**< Set by a private option. */
    char *framerate;    /**< Set by a private option. */
};

struct buff_data {
    int index;
    int fd;
};

struct fmt_map {
    enum AVPixelFormat ff_fmt;
    enum AVCodecID codec_id;
    uint32_t v4l2_fmt;
};

static struct fmt_map fmt_conversion_table[] = {
    //ff_fmt           codec_id           v4l2_fmt
    { AV_PIX_FMT_YUV420P, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YUV420  },
    { AV_PIX_FMT_YUV420P, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YVU420  },
    { AV_PIX_FMT_YUV422P, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YUV422P },
    { AV_PIX_FMT_YUYV422, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YUYV    },
    { AV_PIX_FMT_UYVY422, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_UYVY    },
    { AV_PIX_FMT_YUV411P, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YUV411P },
    { AV_PIX_FMT_YUV410P, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YUV410  },
    { AV_PIX_FMT_RGB555LE,AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_RGB555  },
    { AV_PIX_FMT_RGB555BE,AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_RGB555X },
    { AV_PIX_FMT_RGB565LE,AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_RGB565  },
    { AV_PIX_FMT_RGB565BE,AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_RGB565X },
    { AV_PIX_FMT_BGR24,   AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_BGR24   },
    { AV_PIX_FMT_RGB24,   AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_RGB24   },
    { AV_PIX_FMT_BGR0,    AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_BGR32   },
    { AV_PIX_FMT_0RGB,    AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_RGB32   },
    { AV_PIX_FMT_GRAY8,   AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_GREY    },
    { AV_PIX_FMT_NV12,    AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_NV12    },
    { AV_PIX_FMT_NONE,    AV_CODEC_ID_MJPEG,    V4L2_PIX_FMT_MJPEG   },
    { AV_PIX_FMT_NONE,    AV_CODEC_ID_MJPEG,    V4L2_PIX_FMT_JPEG    },
#ifdef V4L2_PIX_FMT_H264
    { AV_PIX_FMT_NONE,    AV_CODEC_ID_H264,     V4L2_PIX_FMT_H264    },
#endif
#ifdef V4L2_PIX_FMT_CPIA1
    { AV_PIX_FMT_NONE,    AV_CODEC_ID_CPIA,     V4L2_PIX_FMT_CPIA1   },
#endif
};

static int device_open(AVFormatContext *ctx)
{
    struct v4l2_capability cap;
    int fd;
    int res, err;
    int flags = O_RDWR;

    if (ctx->flags & AVFMT_FLAG_NONBLOCK) {
        flags |= O_NONBLOCK;
    }

    fd = v4l2_open(ctx->filename, flags, 0);
    if (fd < 0) {
        err = errno;

        av_log(ctx, AV_LOG_ERROR, "Cannot open video device %s : %s\n",
               ctx->filename, strerror(err));

        return AVERROR(err);
    }

    res = v4l2_ioctl(fd, VIDIOC_QUERYCAP, &cap);
    if (res < 0) {
        err = errno;
        av_log(ctx, AV_LOG_ERROR, "ioctl(VIDIOC_QUERYCAP): %s\n",
               strerror(err));

        goto fail;
    }

    av_log(ctx, AV_LOG_VERBOSE, "[%d]Capabilities: %x\n",
           fd, cap.capabilities);

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        av_log(ctx, AV_LOG_ERROR, "Not a video capture device.\n");
        err = ENODEV;

        goto fail;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        av_log(ctx, AV_LOG_ERROR,
               "The device does not support the streaming I/O method.\n");
        err = ENOSYS;

        goto fail;
    }

    return fd;

fail:
    v4l2_close(fd);
    return AVERROR(err);
}

static int device_init(AVFormatContext *ctx, int *width, int *height,
                       uint32_t pix_fmt)
{
    struct video_data *s = ctx->priv_data;
    int fd = s->fd;
    struct v4l2_format fmt = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };
    struct v4l2_pix_format *pix = &fmt.fmt.pix;

    int res;

    pix->width = *width;
    pix->height = *height;
    pix->pixelformat = pix_fmt;
    pix->field = V4L2_FIELD_ANY;

    res = v4l2_ioctl(fd, VIDIOC_S_FMT, &fmt);

    if ((*width != fmt.fmt.pix.width) || (*height != fmt.fmt.pix.height)) {
        av_log(ctx, AV_LOG_INFO,
               "The V4L2 driver changed the video from %dx%d to %dx%d\n",
               *width, *height, fmt.fmt.pix.width, fmt.fmt.pix.height);
        *width = fmt.fmt.pix.width;
        *height = fmt.fmt.pix.height;
    }

    if (pix_fmt != fmt.fmt.pix.pixelformat) {
        av_log(ctx, AV_LOG_DEBUG,
               "The V4L2 driver changed the pixel format "
               "from 0x%08X to 0x%08X\n",
               pix_fmt, fmt.fmt.pix.pixelformat);
        res = -1;
    }

    if (fmt.fmt.pix.field == V4L2_FIELD_INTERLACED) {
        av_log(ctx, AV_LOG_DEBUG, "The V4L2 driver using the interlaced mode");
        s->interlaced = 1;
    }

    return res;
}

static int first_field(int fd)
{
    int res;
    v4l2_std_id std;

    res = v4l2_ioctl(fd, VIDIOC_G_STD, &std);
    if (res < 0) {
        return 0;
    }
    if (std & V4L2_STD_NTSC) {
        return 0;
    }

    return 1;
}

static uint32_t fmt_ff2v4l(enum AVPixelFormat pix_fmt, enum AVCodecID codec_id)
{
    int i;

    for (i = 0; i < FF_ARRAY_ELEMS(fmt_conversion_table); i++) {
        if ((codec_id == AV_CODEC_ID_NONE ||
             fmt_conversion_table[i].codec_id == codec_id) &&
            (pix_fmt == AV_PIX_FMT_NONE ||
             fmt_conversion_table[i].ff_fmt == pix_fmt)) {
            return fmt_conversion_table[i].v4l2_fmt;
        }
    }

    return 0;
}

static enum AVPixelFormat fmt_v4l2ff(uint32_t v4l2_fmt, enum AVCodecID codec_id)
{
    int i;

    for (i = 0; i < FF_ARRAY_ELEMS(fmt_conversion_table); i++) {
        if (fmt_conversion_table[i].v4l2_fmt == v4l2_fmt &&
            fmt_conversion_table[i].codec_id == codec_id) {
            return fmt_conversion_table[i].ff_fmt;
        }
    }

    return AV_PIX_FMT_NONE;
}

static enum AVCodecID fmt_v4l2codec(uint32_t v4l2_fmt)
{
    int i;

    for (i = 0; i < FF_ARRAY_ELEMS(fmt_conversion_table); i++) {
        if (fmt_conversion_table[i].v4l2_fmt == v4l2_fmt) {
            return fmt_conversion_table[i].codec_id;
        }
    }

    return AV_CODEC_ID_NONE;
}

#if HAVE_STRUCT_V4L2_FRMIVALENUM_DISCRETE
static void list_framesizes(AVFormatContext *ctx, int fd, uint32_t pixelformat)
{
    struct v4l2_frmsizeenum vfse = { .pixel_format = pixelformat };

    while(!ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &vfse)) {
        switch (vfse.type) {
        case V4L2_FRMSIZE_TYPE_DISCRETE:
            av_log(ctx, AV_LOG_INFO, " %ux%u",
                   vfse.discrete.width, vfse.discrete.height);
        break;
        case V4L2_FRMSIZE_TYPE_CONTINUOUS:
        case V4L2_FRMSIZE_TYPE_STEPWISE:
            av_log(ctx, AV_LOG_INFO, " {%u-%u, %u}x{%u-%u, %u}",
                   vfse.stepwise.min_width,
                   vfse.stepwise.max_width,
                   vfse.stepwise.step_width,
                   vfse.stepwise.min_height,
                   vfse.stepwise.max_height,
                   vfse.stepwise.step_height);
        }
        vfse.index++;
    }
}
#endif

static void list_formats(AVFormatContext *ctx, int fd, int type)
{
    struct v4l2_fmtdesc vfd = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };

    while(!ioctl(fd, VIDIOC_ENUM_FMT, &vfd)) {
        enum AVCodecID codec_id = fmt_v4l2codec(vfd.pixelformat);
        enum AVPixelFormat pix_fmt = fmt_v4l2ff(vfd.pixelformat, codec_id);

        vfd.index++;

        if (!(vfd.flags & V4L2_FMT_FLAG_COMPRESSED) &&
            type & V4L_RAWFORMATS) {
            const char *fmt_name = av_get_pix_fmt_name(pix_fmt);
            av_log(ctx, AV_LOG_INFO, "R : %9s : %20s :",
                   fmt_name ? fmt_name : "Unsupported",
                   vfd.description);
        } else if (vfd.flags & V4L2_FMT_FLAG_COMPRESSED &&
                   type & V4L_COMPFORMATS) {
            AVCodec *codec = avcodec_find_encoder(codec_id);
            av_log(ctx, AV_LOG_INFO, "C : %9s : %20s :",
                   codec ? codec->name : "Unsupported",
                   vfd.description);
        } else {
            continue;
        }

#ifdef V4L2_FMT_FLAG_EMULATED
        if (vfd.flags & V4L2_FMT_FLAG_EMULATED) {
            av_log(ctx, AV_LOG_WARNING, "%s", "Emulated");
            continue;
        }
#endif
#if HAVE_STRUCT_V4L2_FRMIVALENUM_DISCRETE
        list_framesizes(ctx, fd, vfd.pixelformat);
#endif
        av_log(ctx, AV_LOG_INFO, "\n");
    }
}

static int mmap_init(AVFormatContext *ctx)
{
    int i, res;
    struct video_data *s = ctx->priv_data;
    struct v4l2_requestbuffers req = {
        .type   = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .count  = desired_video_buffers,
        .memory = V4L2_MEMORY_MMAP
    };

    res = v4l2_ioctl(s->fd, VIDIOC_REQBUFS, &req);
    if (res < 0) {
        if (errno == EINVAL) {
            av_log(ctx, AV_LOG_ERROR, "Device does not support mmap\n");
        } else {
            av_log(ctx, AV_LOG_ERROR, "ioctl(VIDIOC_REQBUFS)\n");
        }
        return AVERROR(errno);
    }

    if (req.count < 2) {
        av_log(ctx, AV_LOG_ERROR, "Insufficient buffer memory\n");
        return AVERROR(ENOMEM);
    }
    s->buffers = req.count;
    s->buf_start = av_malloc(sizeof(void *) * s->buffers);
    if (s->buf_start == NULL) {
        av_log(ctx, AV_LOG_ERROR, "Cannot allocate buffer pointers\n");
        return AVERROR(ENOMEM);
    }
    s->buf_len = av_malloc(sizeof(unsigned int) * s->buffers);
    if (s->buf_len == NULL) {
        av_log(ctx, AV_LOG_ERROR, "Cannot allocate buffer sizes\n");
        av_free(s->buf_start);
        return AVERROR(ENOMEM);
    }

    for (i = 0; i < req.count; i++) {
        struct v4l2_buffer buf = {
            .type   = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .index  = i,
            .memory = V4L2_MEMORY_MMAP
        };
        res = v4l2_ioctl(s->fd, VIDIOC_QUERYBUF, &buf);
        if (res < 0) {
            av_log(ctx, AV_LOG_ERROR, "ioctl(VIDIOC_QUERYBUF)\n");
            return AVERROR(errno);
        }

        s->buf_len[i] = buf.length;
        if (s->frame_size > 0 && s->buf_len[i] < s->frame_size) {
            av_log(ctx, AV_LOG_ERROR,
                   "Buffer len [%d] = %d != %d\n",
                   i, s->buf_len[i], s->frame_size);

            return -1;
        }
        s->buf_start[i] = v4l2_mmap(NULL, buf.length,
                               PROT_READ | PROT_WRITE, MAP_SHARED,
                               s->fd, buf.m.offset);

        if (s->buf_start[i] == MAP_FAILED) {
            av_log(ctx, AV_LOG_ERROR, "mmap: %s\n", strerror(errno));
            return AVERROR(errno);
        }
    }

    return 0;
}

static void mmap_release_buffer(AVPacket *pkt)
{
    struct v4l2_buffer buf = { 0 };
    int res, fd;
    struct buff_data *buf_descriptor = pkt->priv;

    if (pkt->data == NULL)
        return;

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = buf_descriptor->index;
    fd = buf_descriptor->fd;
    av_free(buf_descriptor);

    res = v4l2_ioctl(fd, VIDIOC_QBUF, &buf);
    if (res < 0)
        av_log(NULL, AV_LOG_ERROR, "ioctl(VIDIOC_QBUF): %s\n",
               strerror(errno));

    pkt->data = NULL;
    pkt->size = 0;
}

#if HAVE_CLOCK_GETTIME && defined(CLOCK_MONOTONIC)
static int64_t av_gettime_monotonic(void)
{
    struct timespec tv;

    clock_gettime(CLOCK_MONOTONIC, &tv);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_nsec / 1000;
}
#endif

static int init_convert_timestamp(AVFormatContext *ctx, int64_t ts)
{
    struct video_data *s = ctx->priv_data;
    int64_t now;

    now = av_gettime();
    if (s->ts_mode == V4L_TS_ABS &&
        ts <= now + 1 * AV_TIME_BASE && ts >= now - 10 * AV_TIME_BASE) {
        av_log(ctx, AV_LOG_INFO, "Detected absolute timestamps\n");
        s->ts_mode = V4L_TS_CONVERT_READY;
        return 0;
    }
#if HAVE_CLOCK_GETTIME && defined(CLOCK_MONOTONIC)
    if (ctx->streams[0]->codec->time_base.den) {
        now = av_gettime_monotonic();
        if (s->ts_mode == V4L_TS_MONO2ABS ||
            (ts <= now + 1 * AV_TIME_BASE && ts >= now - 10 * AV_TIME_BASE)) {
            int64_t period = av_rescale_q(1, ctx->streams[0]->codec->time_base,
                                         AV_TIME_BASE_Q);
            av_log(ctx, AV_LOG_INFO, "Detected monotonic timestamps, converting\n");
            /* microseconds instead of seconds, MHz instead of Hz */
            s->timefilter = ff_timefilter_new(1, period, 1.0E-6);
            s->ts_mode = V4L_TS_CONVERT_READY;
            return 0;
        }
    }
#endif
    av_log(ctx, AV_LOG_ERROR, "Unknown timestamps\n");
    return AVERROR(EIO);
}

static int convert_timestamp(AVFormatContext *ctx, int64_t *ts)
{
    struct video_data *s = ctx->priv_data;

    if (s->ts_mode) {
        int r = init_convert_timestamp(ctx, *ts);
        if (r < 0)
            return r;
    }
#if HAVE_CLOCK_GETTIME && defined(CLOCK_MONOTONIC)
    if (s->timefilter) {
        int64_t nowa = av_gettime();
        int64_t nowm = av_gettime_monotonic();
        ff_timefilter_update(s->timefilter, nowa, nowm - s->last_time_m);
        s->last_time_m = nowm;
        *ts = ff_timefilter_eval(s->timefilter, *ts - nowm);
    }
#endif
    return 0;
}

static int mmap_read_frame(AVFormatContext *ctx, AVPacket *pkt)
{
    struct video_data *s = ctx->priv_data;
    struct v4l2_buffer buf = {
        .type   = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP
    };
    struct buff_data *buf_descriptor;
    int res;

    /* FIXME: Some special treatment might be needed in case of loss of signal... */
    while ((res = v4l2_ioctl(s->fd, VIDIOC_DQBUF, &buf)) < 0 && (errno == EINTR));
    if (res < 0) {
        if (errno == EAGAIN) {
            pkt->size = 0;
            return AVERROR(EAGAIN);
        }
        av_log(ctx, AV_LOG_ERROR, "ioctl(VIDIOC_DQBUF): %s\n",
               strerror(errno));

        return AVERROR(errno);
    }
    av_assert0(buf.index < s->buffers);

    /* CPIA is a compressed format and we don't know the exact number of bytes
     * used by a frame, so set it here as the driver announces it.
     */
    if (ctx->video_codec_id == AV_CODEC_ID_CPIA)
        s->frame_size = buf.bytesused;

    if (s->frame_size > 0 && buf.bytesused != s->frame_size) {
        av_log(ctx, AV_LOG_ERROR,
               "The v4l2 frame is %d bytes, but %d bytes are expected\n",
               buf.bytesused, s->frame_size);

        return AVERROR_INVALIDDATA;
    }

    /* Image is at s->buff_start[buf.index] */
    pkt->data= s->buf_start[buf.index];
    pkt->size = buf.bytesused;
    pkt->pts = buf.timestamp.tv_sec * INT64_C(1000000) + buf.timestamp.tv_usec;
    res = convert_timestamp(ctx, &pkt->pts);
    if (res < 0)
        return res;
    pkt->destruct = mmap_release_buffer;
    buf_descriptor = av_malloc(sizeof(struct buff_data));
    if (buf_descriptor == NULL) {
        /* Something went wrong... Since av_malloc() failed, we cannot even
         * allocate a buffer for memcopying into it
         */
        av_log(ctx, AV_LOG_ERROR, "Failed to allocate a buffer descriptor\n");
        res = v4l2_ioctl(s->fd, VIDIOC_QBUF, &buf);

        return AVERROR(ENOMEM);
    }
    buf_descriptor->fd = s->fd;
    buf_descriptor->index = buf.index;
    pkt->priv = buf_descriptor;

    return s->buf_len[buf.index];
}

static int mmap_start(AVFormatContext *ctx)
{
    struct video_data *s = ctx->priv_data;
    enum v4l2_buf_type type;
    int i, res;

    for (i = 0; i < s->buffers; i++) {
        struct v4l2_buffer buf = {
            .type   = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .index  = i,
            .memory = V4L2_MEMORY_MMAP
        };

        res = v4l2_ioctl(s->fd, VIDIOC_QBUF, &buf);
        if (res < 0) {
            av_log(ctx, AV_LOG_ERROR, "ioctl(VIDIOC_QBUF): %s\n",
                   strerror(errno));

            return AVERROR(errno);
        }
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    res = v4l2_ioctl(s->fd, VIDIOC_STREAMON, &type);
    if (res < 0) {
        av_log(ctx, AV_LOG_ERROR, "ioctl(VIDIOC_STREAMON): %s\n",
               strerror(errno));

        return AVERROR(errno);
    }

    return 0;
}

static void mmap_close(struct video_data *s)
{
    enum v4l2_buf_type type;
    int i;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    /* We do not check for the result, because we could
     * not do anything about it anyway...
     */
    v4l2_ioctl(s->fd, VIDIOC_STREAMOFF, &type);
    for (i = 0; i < s->buffers; i++) {
        v4l2_munmap(s->buf_start[i], s->buf_len[i]);
    }
    av_free(s->buf_start);
    av_free(s->buf_len);
}

static int v4l2_set_parameters(AVFormatContext *s1)
{
    struct video_data *s = s1->priv_data;
    struct v4l2_input input = { 0 };
    struct v4l2_standard standard = { 0 };
    struct v4l2_streamparm streamparm = { 0 };
    struct v4l2_fract *tpf = &streamparm.parm.capture.timeperframe;
    AVRational framerate_q = { 0 };
    int i, ret;

    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (s->framerate &&
        (ret = av_parse_video_rate(&framerate_q, s->framerate)) < 0) {
        av_log(s1, AV_LOG_ERROR, "Could not parse framerate '%s'.\n",
               s->framerate);
        return ret;
    }

    /* set tv video input */
    input.index = s->channel;
    if (v4l2_ioctl(s->fd, VIDIOC_ENUMINPUT, &input) < 0) {
        av_log(s1, AV_LOG_ERROR, "The V4L2 driver ioctl enum input failed:\n");
        return AVERROR(EIO);
    }

    av_log(s1, AV_LOG_DEBUG, "The V4L2 driver set input_id: %d, input: %s\n",
            s->channel, input.name);
    if (v4l2_ioctl(s->fd, VIDIOC_S_INPUT, &input.index) < 0) {
        av_log(s1, AV_LOG_ERROR,
               "The V4L2 driver ioctl set input(%d) failed\n",
                s->channel);
        return AVERROR(EIO);
    }

    if (s->standard) {
        av_log(s1, AV_LOG_DEBUG, "The V4L2 driver set standard: %s\n",
               s->standard);
        /* set tv standard */
        for(i=0;;i++) {
            standard.index = i;
            ret = v4l2_ioctl(s->fd, VIDIOC_ENUMSTD, &standard);
            if (ret < 0 || !av_strcasecmp(standard.name, s->standard))
                break;
        }
        if (ret < 0) {
            av_log(s1, AV_LOG_ERROR, "Unknown standard '%s'\n", s->standard);
            return ret;
        }

        av_log(s1, AV_LOG_DEBUG,
               "The V4L2 driver set standard: %s, id: %"PRIu64"\n",
               s->standard, (uint64_t)standard.id);
        if (v4l2_ioctl(s->fd, VIDIOC_S_STD, &standard.id) < 0) {
            av_log(s1, AV_LOG_ERROR,
                   "The V4L2 driver ioctl set standard(%s) failed\n",
                   s->standard);
            return AVERROR(EIO);
        }
    }

    if (framerate_q.num && framerate_q.den) {
        av_log(s1, AV_LOG_DEBUG, "Setting time per frame to %d/%d\n",
               framerate_q.den, framerate_q.num);
        tpf->numerator   = framerate_q.den;
        tpf->denominator = framerate_q.num;

        if (v4l2_ioctl(s->fd, VIDIOC_S_PARM, &streamparm) != 0) {
            av_log(s1, AV_LOG_ERROR,
                   "ioctl set time per frame(%d/%d) failed\n",
                   framerate_q.den, framerate_q.num);
            return AVERROR(EIO);
        }

        if (framerate_q.num != tpf->denominator ||
            framerate_q.den != tpf->numerator) {
            av_log(s1, AV_LOG_INFO,
                   "The driver changed the time per frame from "
                   "%d/%d to %d/%d\n",
                   framerate_q.den, framerate_q.num,
                   tpf->numerator, tpf->denominator);
        }
    } else {
        if (v4l2_ioctl(s->fd, VIDIOC_G_PARM, &streamparm) != 0) {
            av_log(s1, AV_LOG_ERROR, "ioctl(VIDIOC_G_PARM): %s\n",
                   strerror(errno));
            return AVERROR(errno);
        }
    }
    if (tpf->denominator > 0 && tpf->numerator > 0) {
        s1->streams[0]->codec->time_base.den = tpf->denominator;
        s1->streams[0]->codec->time_base.num = tpf->numerator;
    } else
        av_log(s1, AV_LOG_WARNING, "Time per frame unknown\n");

    return 0;
}

static uint32_t device_try_init(AVFormatContext *s1,
                                enum AVPixelFormat pix_fmt,
                                int *width,
                                int *height,
                                enum AVCodecID *codec_id)
{
    uint32_t desired_format = fmt_ff2v4l(pix_fmt, s1->video_codec_id);

    if (desired_format == 0 ||
        device_init(s1, width, height, desired_format) < 0) {
        int i;

        desired_format = 0;
        for (i = 0; i<FF_ARRAY_ELEMS(fmt_conversion_table); i++) {
            if (s1->video_codec_id == AV_CODEC_ID_NONE ||
                fmt_conversion_table[i].codec_id == s1->video_codec_id) {
                desired_format = fmt_conversion_table[i].v4l2_fmt;
                if (device_init(s1, width, height, desired_format) >= 0) {
                    break;
                }
                desired_format = 0;
            }
        }
    }

    if (desired_format != 0) {
        *codec_id = fmt_v4l2codec(desired_format);
        av_assert0(*codec_id != AV_CODEC_ID_NONE);
    }

    return desired_format;
}

static int v4l2_read_header(AVFormatContext *s1)
{
    struct video_data *s = s1->priv_data;
    AVStream *st;
    int res = 0;
    uint32_t desired_format;
    enum AVCodecID codec_id = AV_CODEC_ID_NONE;
    enum AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;

    st = avformat_new_stream(s1, NULL);
    if (!st) {
        res = AVERROR(ENOMEM);
        goto out;
    }

    s->fd = device_open(s1);
    if (s->fd < 0) {
        res = s->fd;
        goto out;
    }

    if (s->list_format) {
        list_formats(s1, s->fd, s->list_format);
        res = AVERROR_EXIT;
        goto out;
    }

    avpriv_set_pts_info(st, 64, 1, 1000000); /* 64 bits pts in us */

    if (s->pixel_format) {
        AVCodec *codec = avcodec_find_decoder_by_name(s->pixel_format);

        if (codec)
            s1->video_codec_id = codec->id;

        pix_fmt = av_get_pix_fmt(s->pixel_format);

        if (pix_fmt == AV_PIX_FMT_NONE && !codec) {
            av_log(s1, AV_LOG_ERROR, "No such input format: %s.\n",
                   s->pixel_format);

            res = AVERROR(EINVAL);
            goto out;
        }
    }

    if (!s->width && !s->height) {
        struct v4l2_format fmt;

        av_log(s1, AV_LOG_VERBOSE,
               "Querying the device for the current frame size\n");
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (v4l2_ioctl(s->fd, VIDIOC_G_FMT, &fmt) < 0) {
            av_log(s1, AV_LOG_ERROR, "ioctl(VIDIOC_G_FMT): %s\n",
                   strerror(errno));
            res = AVERROR(errno);
            goto out;
        }

        s->width  = fmt.fmt.pix.width;
        s->height = fmt.fmt.pix.height;
        av_log(s1, AV_LOG_VERBOSE,
               "Setting frame size to %dx%d\n", s->width, s->height);
    }

    desired_format = device_try_init(s1, pix_fmt, &s->width, &s->height,
                                     &codec_id);

    /* If no pixel_format was specified, the codec_id was not known up
     * until now. Set video_codec_id in the context, as codec_id will
     * not be available outside this function
     */
    if (codec_id != AV_CODEC_ID_NONE && s1->video_codec_id == AV_CODEC_ID_NONE)
        s1->video_codec_id = codec_id;

    if (desired_format == 0) {
        av_log(s1, AV_LOG_ERROR, "Cannot find a proper format for "
               "codec_id %d, pix_fmt %d.\n", s1->video_codec_id, pix_fmt);
        v4l2_close(s->fd);

        res = AVERROR(EIO);
        goto out;
    }
    if ((res = av_image_check_size(s->width, s->height, 0, s1)) < 0)
        goto out;

    s->frame_format = desired_format;

    if ((res = v4l2_set_parameters(s1)) < 0)
        goto out;

    st->codec->pix_fmt = fmt_v4l2ff(desired_format, codec_id);
    s->frame_size =
        avpicture_get_size(st->codec->pix_fmt, s->width, s->height);

    if ((res = mmap_init(s1)) ||
        (res = mmap_start(s1)) < 0) {
        v4l2_close(s->fd);
        goto out;
    }

    s->top_field_first = first_field(s->fd);

    st->codec->codec_type = AVMEDIA_TYPE_VIDEO;
    st->codec->codec_id = codec_id;
    if (codec_id == AV_CODEC_ID_RAWVIDEO)
        st->codec->codec_tag =
            avcodec_pix_fmt_to_codec_tag(st->codec->pix_fmt);
    else if (codec_id == AV_CODEC_ID_H264) {
        st->need_parsing = AVSTREAM_PARSE_HEADERS;
    }
    if (desired_format == V4L2_PIX_FMT_YVU420)
        st->codec->codec_tag = MKTAG('Y', 'V', '1', '2');
    st->codec->width = s->width;
    st->codec->height = s->height;
    if (st->codec->time_base.num)
        st->codec->bit_rate = s->frame_size * 1/av_q2d(st->codec->time_base) * 8;

out:
    return res;
}

static int v4l2_read_packet(AVFormatContext *s1, AVPacket *pkt)
{
    struct video_data *s = s1->priv_data;
    AVFrame *frame = s1->streams[0]->codec->coded_frame;
    int res;

    av_init_packet(pkt);
    if ((res = mmap_read_frame(s1, pkt)) < 0) {
        return res;
    }

    if (frame && s->interlaced) {
        frame->interlaced_frame = 1;
        frame->top_field_first = s->top_field_first;
    }

    return pkt->size;
}

static int v4l2_read_close(AVFormatContext *s1)
{
    struct video_data *s = s1->priv_data;

    mmap_close(s);

    v4l2_close(s->fd);
    return 0;
}

#define OFFSET(x) offsetof(struct video_data, x)
#define DEC AV_OPT_FLAG_DECODING_PARAM

static const AVOption options[] = {
    { "standard",     "TV standard, used only by analog frame grabber",            OFFSET(standard),     AV_OPT_TYPE_STRING, {.str = NULL }, 0, 0,       DEC },
    { "channel",      "TV channel, used only by frame grabber",                    OFFSET(channel),      AV_OPT_TYPE_INT,    {.i64 = 0 },    0, INT_MAX, DEC },
    { "video_size",   "A string describing frame size, such as 640x480 or hd720.", OFFSET(width),        AV_OPT_TYPE_IMAGE_SIZE, {.str = NULL},  0, 0,       DEC },
    { "pixel_format", "Preferred pixel format",                                    OFFSET(pixel_format), AV_OPT_TYPE_STRING, {.str = NULL},  0, 0,       DEC },
    { "input_format", "Preferred pixel format (for raw video) or codec name",      OFFSET(pixel_format), AV_OPT_TYPE_STRING, {.str = NULL},  0, 0,       DEC },
    { "framerate",    "",                                                          OFFSET(framerate),    AV_OPT_TYPE_STRING, {.str = NULL},  0, 0,       DEC },
    { "list_formats", "List available formats and exit",                           OFFSET(list_format),  AV_OPT_TYPE_INT,    {.i64 = 0 },  0, INT_MAX, DEC, "list_formats" },
    { "all",          "Show all available formats",                                OFFSET(list_format),  AV_OPT_TYPE_CONST,  {.i64 = V4L_ALLFORMATS  },    0, INT_MAX, DEC, "list_formats" },
    { "raw",          "Show only non-compressed formats",                          OFFSET(list_format),  AV_OPT_TYPE_CONST,  {.i64 = V4L_RAWFORMATS  },    0, INT_MAX, DEC, "list_formats" },
    { "compressed",   "Show only compressed formats",                              OFFSET(list_format),  AV_OPT_TYPE_CONST,  {.i64 = V4L_COMPFORMATS },    0, INT_MAX, DEC, "list_formats" },
    { "timestamps",   "Kind of timestamps for grabbed frames",                     OFFSET(ts_mode),      AV_OPT_TYPE_INT,    {.i64 = 0 }, 0, 2, DEC, "timestamps" },
    { "default",      "Use timestamps from the kernel",                            OFFSET(ts_mode),      AV_OPT_TYPE_CONST,  {.i64 = V4L_TS_DEFAULT  }, 0, 2, DEC, "timestamps" },
    { "abs",          "Use absolute timestamps (wall clock)",                      OFFSET(ts_mode),      AV_OPT_TYPE_CONST,  {.i64 = V4L_TS_ABS      }, 0, 2, DEC, "timestamps" },
    { "mono2abs",     "Force conversion from monotonic to absolute timestamps",    OFFSET(ts_mode),      AV_OPT_TYPE_CONST,  {.i64 = V4L_TS_MONO2ABS }, 0, 2, DEC, "timestamps" },
    { "ts",           "Kind of timestamps for grabbed frames",                     OFFSET(ts_mode),      AV_OPT_TYPE_INT,    {.i64 = 0 }, 0, 2, DEC, "timestamps" },
    { NULL },
};

static const AVClass v4l2_class = {
    .class_name = "V4L2 indev",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVInputFormat ff_v4l2_demuxer = {
    .name           = "video4linux2,v4l2",
    .long_name      = NULL_IF_CONFIG_SMALL("Video4Linux2 device grab"),
    .priv_data_size = sizeof(struct video_data),
    .read_header    = v4l2_read_header,
    .read_packet    = v4l2_read_packet,
    .read_close     = v4l2_read_close,
    .flags          = AVFMT_NOFILE,
    .priv_class     = &v4l2_class,
};
