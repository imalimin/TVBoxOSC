package com.github.tvbox.osc.server;

import android.content.Context;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import tv.danmaku.ijk.media.player.IjkMediaPlayer;

public enum DownloadManager {
    INSTANCE;
    public static final ExecutorService spThreadPool = Executors.newSingleThreadExecutor();
    private final Map<String, Long> map = new HashMap<>();

    public int startDownload(Context context, String url) {
        if (url == null || url.isEmpty()) {
            return -1;
        }
        String dir = context.getExternalCacheDir().getAbsolutePath();
        synchronized (map) {
            if (!map.containsKey(url)) {
                long handle = IjkMediaPlayer.native_startDownload(url, dir);
                map.put(url, handle);
                return 0;
            } else {
                return 1;
            }
        }
    }

    public int stopDownload(String url) {
        synchronized (map) {
            if (map.containsKey(url)) {
                IjkMediaPlayer.native_stopDownload(map.get(url));
                map.remove(url);
            }
        }
        return -1;
    }

    public float getProgress(String url) {
        synchronized (map) {
            if (map.containsKey(url)) {
                return IjkMediaPlayer.native_getDownloadProgress(map.get(url));
            }
        }
        return 0;
    }
}
