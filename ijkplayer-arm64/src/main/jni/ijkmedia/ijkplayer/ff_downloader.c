//
// Created by Alimin on 2024/5/3.
//

#include "ff_downloader.h"
#include "stdio.h"

FFDownloader *ff_create_downloader(const char *filename, const char *saveDir, const char *saveName) {
    FFDownloader *downloader = av_malloc(sizeof(FFDownloader));
    memset(downloader, 0, sizeof(FFDownloader));
    downloader->filename = filename;
    downloader->saveDir = saveDir;
    downloader->saveName = saveName;
    av_log(NULL, AV_LOG_INFO, "%s: filename=%s, saveDir=%s, saveName=%s\n", __func__, filename, saveDir, saveName);
    return downloader;
}

int ff_free_downloader(FFDownloader *downloader) {
    av_free(downloader);
    return 0;
}

static AVBSFContext *createFilter(const char *name, AVCodecParameters *codecpar) {
    const AVBitStreamFilter *bsfilter = av_bsf_get_by_name(name);
    AVBSFContext *bsf_ctx = NULL;
    if (bsfilter) {
        av_bsf_alloc(bsfilter, &bsf_ctx); //AVBSFContext;
        avcodec_parameters_copy(bsf_ctx->par_in, codecpar);
        av_bsf_init(bsf_ctx);
    }
    return bsf_ctx;
}

static void copy_stream(AVFormatContext *ic, AVFormatContext *oc, enum AVMediaType type, int *st_index, int *os_index) {
    if (st_index[type] >= 0) {
        AVStream *in_stream = ic->streams[st_index[type]];
        AVStream *stream = avformat_new_stream(oc, NULL);
        int ret = avcodec_parameters_copy(stream->codecpar, in_stream->codecpar);
        av_log(NULL, AV_LOG_INFO, "Copy codecpar ret=%d. index=%d, type=%d.\n", ret, in_stream->index, type);
        os_index[type] = stream->index;
    }
}

static void *thread_func(void *arg) {
    FFDownloader *downloader = arg;
    av_log(NULL, AV_LOG_INFO, "%s: thread start.\n", __func__);
    int ret = 0;
    int is_index[AVMEDIA_TYPE_NB];
    int os_index[AVMEDIA_TYPE_NB];
    AVPacket pkt1, *pkt = &pkt1;
    AVFormatContext *ic = avformat_alloc_context();
    AVFormatContext *oc = NULL;
    if (!ic) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate context.\n");
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    ret = avformat_open_input(&ic, downloader->filename, NULL, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not open int.\n");
        print_error(downloader->filename, ret);
        goto fail;
    }
    av_format_inject_global_side_data(ic);

    char *suffix = "mp4";
    if (strstr(ic->iformat->name, "hls")) {
        suffix = "ts";
    }
    char path[10240];
    sprintf(path, "%s/%s.%s", downloader->saveDir, downloader->saveName, suffix);
    ret = remove(path);
    av_log(NULL, AV_LOG_INFO, "Save path=%s, remove check ret=%d\n", path, ret);
    ret = avformat_alloc_output_context2(&oc, NULL, NULL, path);
    if (ret < 0 || !oc) {
        av_log(NULL, AV_LOG_ERROR, "Could open output context.\n");
        goto fail;
    }
    if (true) {
        int orig_nb_streams = ic->nb_streams;
        do {
            if (av_stristart(downloader->filename, "data:", NULL) && orig_nb_streams > 0) {
                int i = 0;
                for (i = 0; i < orig_nb_streams; i++) {
                    if (!ic->streams[i] || !ic->streams[i]->codecpar || ic->streams[i]->codecpar->profile == FF_PROFILE_UNKNOWN) {
                        break;
                    }
                }

                if (i == orig_nb_streams) {
                    break;
                }
            }
            ret = avformat_find_stream_info(ic, NULL);
        } while(0);

        if (ret < 0) {
            av_log(NULL, AV_LOG_WARNING,"%s: could not find codec parameters\n", downloader->filename);
            goto fail;
        }
    }

    av_dump_format(ic, 0, downloader->filename, 0);

    for (int i = 0; i < ic->nb_streams; i++) {
        AVStream *st = ic->streams[i];
        enum AVMediaType type = st->codecpar->codec_type;
        is_index[type] = i;
    }
    copy_stream(ic, oc, AVMEDIA_TYPE_VIDEO, is_index, os_index);
    copy_stream(ic, oc, AVMEDIA_TYPE_AUDIO, is_index, os_index);
//    AVBSFContext *bsf_ctx = createFilter("h264_mp4toannexb", ic->streams[is_index[AVMEDIA_TYPE_VIDEO]]);
    if (oc && !(oc->flags & AVFMT_NOFILE)) {
        ret = avio_open(&oc->pb, oc->filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not open output io.\n");
            print_error(downloader->filename, ret);
            goto fail;
        }
    }

    ret = avformat_write_header(oc, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not write header.\n");
        print_error(downloader->filename, ret);
        goto fail;
    }

    pkt->flags = 0;
    while ((ret = av_read_frame(ic, pkt)) != AVERROR_EOF) {
        if (downloader->req_abort /* || downloader->progress > 0.01f */) {
            av_log(NULL, AV_LOG_WARNING, "%s: req_abort\n", __func__);
            break;
        }
        int64_t duration = ic->duration;
        if (duration <= 0) {
            duration = av_rescale_q(ic->streams[pkt->stream_index]->duration, ic->streams[pkt->stream_index]->time_base, AV_TIME_BASE_Q);
        }
        int64_t pts = av_rescale_q(pkt->pts, ic->streams[pkt->stream_index]->time_base, AV_TIME_BASE_Q);
        if (pkt->stream_index == is_index[AVMEDIA_TYPE_VIDEO]) {
            downloader->progress = pts * 1.0f / duration;
        }
        av_log(NULL, AV_LOG_DEBUG, "%s: av_read_frame. ret=%d, flags=%d, %" PRId64 "/%" PRId64 ", progress=%0.2f\n",
               __func__, ret, pkt->flags, pts, duration, downloader->progress * 100);
        int type = ic->streams[pkt->stream_index]->codecpar->codec_type;
        int stream_index = os_index[type];
        if (stream_index >= 0) {
            pkt->stream_index = stream_index;
            ret = av_interleaved_write_frame(oc, pkt);
            if (ret != 0) {
                av_log(NULL, AV_LOG_WARNING, "%s: write frame error. ret=%d, type=%d, pts%" PRId64 "\n", __func__, ret, type, pts);
            }
        }
        av_packet_unref(pkt);
    }
    if (ret != 0 && ret != AVERROR_EOF) {
        print_error(downloader->filename, ret);
    }
    av_log(NULL, AV_LOG_INFO, "%s: write finish. ret=%d\n", __func__, ret);
    av_packet_unref(pkt);
    av_write_trailer(oc);

    fail:
    if (ic) {
        avformat_close_input(&ic);
        avformat_free_context(ic);
    }
    if (oc) {
        if (!(oc->flags & AVFMT_NOFILE)) {
            ret = avio_closep(&oc->pb);
        }
        avformat_free_context(oc);
    }
    av_log(NULL, AV_LOG_INFO, "%s: thread finish. ret=%d\n", __func__, ret);
    return NULL;
}

int ff_start_download(FFDownloader *downloader) {
    int ret = pthread_create(&downloader->thread_t, NULL, thread_func, downloader);
    av_log(NULL, AV_LOG_INFO, "%s: start thread ret=%d\n", __func__, ret);
    return ret;
}

int ff_stop_download(FFDownloader *downloader) {
    downloader->req_abort = 1;
    pthread_join(downloader->thread_t, NULL);
    return 0;
}