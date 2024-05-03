//
// Created by Alimin on 2024/5/3.
//

#include "ff_downloader.h"

FFDownloader *ff_create_downloader(const char *filename, const char *saveDir) {
    FFDownloader *downloader = av_malloc(sizeof(FFDownloader));
    memset(downloader, 0, sizeof(FFDownloader));
    downloader->filename = filename;
    downloader->saveDir = saveDir;
    av_log(NULL, AV_LOG_INFO, "%s: filename=%s, saveDir=%s\n", __func__, filename, saveDir);
    return downloader;
}

int ff_free_downloader(FFDownloader *downloader) {
    av_free(downloader);
    return 0;
}

static void *thread_func(void *arg) {
    FFDownloader *downloader = arg;
    av_log(NULL, AV_LOG_INFO, "%s: thread start.\n", __func__);
    int ret = 0;
    int st_index[AVMEDIA_TYPE_NB];
    int os_index[AVMEDIA_TYPE_NB];
    AVPacket pkt1, *pkt = &pkt1;
    AVFormatContext *ic = avformat_alloc_context();
    AVFormatContext *oc = NULL;
    if (!ic) {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate context.\n");
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    ret = avformat_open_input(&ic, downloader->filename, NULL, NULL);
    if (ret < 0) {
        print_error(downloader->filename, ret);
        goto fail;
    }
    av_format_inject_global_side_data(ic);

    char path[10240];
    sprintf(path, "%s/%s", downloader->saveDir, "111.mp4");
    ret = avformat_alloc_output_context2(&oc, NULL, NULL, path);
    if (ret < 0 || !oc) {
        av_log(NULL, AV_LOG_FATAL, "Could open output context.\n");
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    for (int i = 0; i < ic->nb_streams; i++) {
        AVStream *st = ic->streams[i];
        enum AVMediaType type = st->codecpar->codec_type;
        st_index[type] = i;
    }
    st_index[AVMEDIA_TYPE_VIDEO] =
            av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO,
                                st_index[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);
    st_index[AVMEDIA_TYPE_AUDIO] =
            av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO,
                                st_index[AVMEDIA_TYPE_AUDIO],
                                st_index[AVMEDIA_TYPE_VIDEO],
                                NULL, 0);
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        AVStream *stream = avformat_new_stream(oc, NULL);
//        avcodec_copy_context(ic->streams[st_index[AVMEDIA_TYPE_VIDEO]]->codec, stream->codec);
        ret = avcodec_parameters_from_context(stream->codecpar, ic->streams[st_index[AVMEDIA_TYPE_VIDEO]]->codec);
        os_index[AVMEDIA_TYPE_VIDEO] = stream->index;
    }
    if (st_index[AVMEDIA_TYPE_AUDIO] > 0) {
        AVStream *stream = avformat_new_stream(oc, NULL);
//        avcodec_copy_context(ic->streams[st_index[AVMEDIA_TYPE_AUDIO]]->codec, stream->codec);
        ret = avcodec_parameters_from_context(stream->codecpar, ic->streams[st_index[AVMEDIA_TYPE_AUDIO]]->codec);
        os_index[AVMEDIA_TYPE_AUDIO] = stream->index;
    }
    if (oc && !(oc->flags & AVFMT_NOFILE)) {
        ret = avio_open(&oc->pb, oc->filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            print_error(downloader->filename, ret);
            goto fail;
        }
    }

    ret = avformat_write_header(oc, NULL);
    if (ret < 0) {
        print_error(downloader->filename, ret);
        goto fail;
    }

    pkt->flags = 0;
    while ((ret = av_read_frame(ic, pkt)) != AVERROR_EOF) {
        if (downloader->req_abort /* || downloader->progress > 0.01f */) {
            av_log(NULL, AV_LOG_WARNING, "%s: req_abort\n", __func__);
            break;
        }
        int64_t duration = av_rescale_q(ic->streams[pkt->stream_index]->duration, ic->streams[pkt->stream_index]->time_base, AV_TIME_BASE_Q);
        int64_t pts = av_rescale_q(pkt->pts, ic->streams[pkt->stream_index]->time_base, AV_TIME_BASE_Q);
        if (pkt->stream_index == st_index[AVMEDIA_TYPE_VIDEO]) {
            downloader->progress = pts * 1.0f / duration;
        }
        av_log(NULL, AV_LOG_INFO, "%s: av_read_frame. ret=%d, %" PRId64 "/%" PRId64 ", progress=%0.2f\n",
               __func__, ret, pts, duration, downloader->progress * 100);
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