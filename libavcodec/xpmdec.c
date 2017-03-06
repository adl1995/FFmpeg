/*
 * XPM image format
 *
 * Copyright (c) 2012 Paul B Mahol
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

#include "libavutil/parseutils.h"
#include "avcodec.h"
#include "internal.h"


typedef struct XPMContext {
    AVFrame  *frame;
    /*AVFrame  picture;*/
    uint8_t  *pixels;
    int      pixels_size;
} XPMDecContext;

static av_cold int xpm_decode_init(AVCodecContext *avctx)
{
    XPMDecContext *x = avctx->priv_data;
    /*frame = ff_get_video_buffer;*/

    /*avcodec_get_frame_defaults(&x->frame);*/
    avctx->coded_frame = &x->frame;
    return 0;
}

static int ascii2index(const uint8_t *cpixel, int cpp)
{
    const uint8_t *p = cpixel;
    int n = 0, m = 1, i;

    for (i = 0; i < cpp; i++) {
        if (*p < ' ' || *p > '~')
            return AVERROR_INVALIDDATA;
        n += (*p++ - ' ') * m;
        m *= 94;
    }
    return n;
}

static int parse_str_int(const uint8_t *p, int len, const uint8_t *key)
{
    const uint8_t *end = p + len;

    for(; p<end - strlen(key); p++) {
        if (!memcmp(p, key, strlen(key)))
            break;
    }
    p += strlen(key);
    if (p >= end)
        return INT_MIN;

    for(; p<end; p++) {
        char *eptr;
        int64_t ret = strtol(p, &eptr, 10);
        if ((const uint8_t *)eptr != p)
            return ret;
    }
    return INT_MIN;
}

static int xpm_decode_frame(AVCodecContext *avctx, void *data,
                            int *data_size, AVPacket *avpkt)
{
    printf("Testing XPM decoder");

    AVFrame *p = data;
    /*AVFrame *p = avctx->coded_frame;*/
    XPMDecContext *x = avctx->priv_data;
    const uint8_t *end, *ptr = avpkt->data;
    int ncolors, cpp, ret, i, j;
    uint8_t *dst, rgba[4];
    
    end = avpkt->data + avpkt->size;
    if (memcmp(ptr, "/* XPM */", 9)) {
        av_log(avctx, AV_LOG_ERROR, "missing signature\n");
        return AVERROR_INVALIDDATA;
    }

    /*  
    string parsed_data;
    for (int i = 0; i < sizeof(ptr)/sizeof(uint8_t); ++i)
    {
        if (memcmp(ptr, "/", i) && memcmp(ptr, "*", i+1))
            parsed_data += data[i];
    }*/

    ptr += strcspn(ptr, "\"");
    if (sscanf(ptr, "\"%u %u %u %u\",",
               &avctx->width, &avctx->height, &ncolors, &cpp) != 4) {
        av_log(avctx, AV_LOG_ERROR, "missing image parameters\n");
        return AVERROR_INVALIDDATA;
    }
    
    if ((ret = ff_get_buffer(avctx, p, 0)) < 0)
        return ret;

    if (ncolors <= 0 || cpp <= 0) {
        av_log(avctx, AV_LOG_ERROR, "invalid number of colors or chars per pixel\n");
        return AVERROR_INVALIDDATA;
    }
    if (ncolors > 256) {
        av_log(avctx, AV_LOG_ERROR, "unsupported number of colors\n");
        return AVERROR_PATCHWELCOME;
    }

    if (cpp != 1 && cpp != 2) {
        av_log(avctx, AV_LOG_ERROR, "unsupported number of chars per pixel\n");
        return AVERROR_PATCHWELCOME;
    }

    avctx->pix_fmt = AV_PIX_FMT_PAL8;

/*    if (frame->data[0])
        avctx->release_buffer(avctx, p);
*/
/*    frame->reference = 0;
    if ((ret = avctx->get_buffer(avctx, p)) < 0)
        return ret;
*/

    av_fast_padded_malloc(&x->pixels, &x->pixels_size, cpp == 2 ? 94 * 94 : 94);
    if (!x->pixels)
        return AVERROR(ENOMEM);

    ptr += strcspn(ptr, ",") + 1;
    for (i = 0; i < ncolors; i++) {
        const uint8_t *index;
        int len;

        ptr += strcspn(ptr, "\"") + 1;
        if (ptr + cpp > end)
            return AVERROR_INVALIDDATA;
        index = ptr;
        ptr += cpp;

        ptr = strstr(ptr, "c ");
        if (ptr)
            ptr += 2;
        else
            return AVERROR_INVALIDDATA;

        len = strcspn(ptr, "\" ");
        if ((ret = av_parse_color(rgba, ptr, len, avctx)) < 0)
            return ret;

        p->data[1] = rgba[0];
        p->data[2] = rgba[1];
        p->data[3] = rgba[2];
        p->data[4] = rgba[3];

        /**(*(frame->data[1] + i * 4    )) = rgba[2];
        *(*(frame->data[1] + i * 4 + 1)) = rgba[1];
        *(*(frame->data[1] + i * 4 + 2)) = rgba[0];
        *(*(frame->data[1] + i * 4 + 3)) = rgba[3];*/

        if ((ret = ascii2index(index, cpp)) < 0)
            return ret;
        x->pixels[ret] = i;

        ptr += strcspn(ptr, ",") + 1;
    }

    for (i = 0; i < avctx->height; i++) {
        dst = x->frame->data[0] + i * x->frame->linesize[0];
        ptr += strcspn(ptr, "\"") + 1;
        for (j = 0; j < avctx->width; j++) {
            if (ptr + cpp > end)
                return AVERROR_INVALIDDATA;
            *dst++ = x->pixels[ascii2index(ptr, cpp)];
            ptr += cpp;
        }
        ptr += strcspn(ptr, ",") + 1;
    }


    p->key_frame = 1;
    p->pict_type = AV_PICTURE_TYPE_I;

    *data_size       = sizeof(AVFrame);
    /**(AVFrame *)data = *p;*/

    return avpkt->size;
}

static av_cold int xpm_decode_close(AVCodecContext *avctx)
{
    XPMDecContext *x = avctx->priv_data;

/*    if (avctx->coded_frame->data[0])
        avctx->release_buffer(avctx, avctx->coded_frame);
*/
    av_freep(&x->pixels);

    return 0;
}

AVCodec ff_xpm_decoder = {
    .name           = "xpm",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_XPM,
    .priv_data_size = sizeof(XPMDecContext),
    .init           = xpm_decode_init,
    .close          = xpm_decode_close,
    .decode         = xpm_decode_frame,
    .capabilities   = CODEC_CAP_DR1,
    .long_name      = NULL_IF_CONFIG_SMALL("XPM (X PixMap) image"),
};
