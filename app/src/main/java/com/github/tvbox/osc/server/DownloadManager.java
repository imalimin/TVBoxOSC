package com.github.tvbox.osc.server;

import java.io.File;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import tv.danmaku.ijk.media.player.IjkMediaPlayer;

public enum DownloadManager {
    INSTANCE;
    private final Map<String, Long> map = new HashMap<>();

    public int startDownload(String url, String name, int index, String channel) {
        if (url == null || url.isEmpty()) {
            return -1;
        }
        File dir = new File("/sdcard/Download/" + name);
        if (!dir.exists()) {
            dir.mkdirs();
        }
        synchronized (map) {
            if (!map.containsKey(url)) {
                long handle = IjkMediaPlayer.native_startDownload(url, dir.getAbsolutePath(), String.format("%s_%03d_%s", name, index + 1, channel));
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
