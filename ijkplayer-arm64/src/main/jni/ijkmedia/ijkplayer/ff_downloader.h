//
// Created by Alimin on 2024/5/3.
//
#include "libavutil/log.h"
#include "libavformat/avformat.h"
#include "ff_cmdutils.h"

typedef struct _FFDownloader {
    const char *filename;
    const char *saveDir;
    const char *saveName;
    pthread_t thread_t;
    int req_abort;
    float progress;
} FFDownloader;

FFDownloader *ff_create_downloader(const char *filename, const char *saveDir, const char *saveName);

int ff_free_downloader(FFDownloader *downloader);

int ff_start_download(FFDownloader *downloader);

int ff_stop_download(FFDownloader *downloader);