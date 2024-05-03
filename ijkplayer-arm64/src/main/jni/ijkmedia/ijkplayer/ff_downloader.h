//
// Created by Alimin on 2024/5/3.
//
#include "libavutil/log.h"
#include "libavformat/avformat.h"
#include "ff_cmdutils.h"

typedef struct _FFDownloader {
    const char *filename;
    pthread_t thread_t;
    int req_abort;
    float progress;
} FFDownloader;

FFDownloader *ff_create_downloader(const char *filename);

int ff_start_download(FFDownloader *downloader);