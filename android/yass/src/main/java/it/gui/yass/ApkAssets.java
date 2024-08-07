// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */
package it.gui.yass;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.content.res.AssetManager;
import android.util.Log;

import java.io.IOException;

/**
 * A utility class to retrieve references to uncompressed assets insides the apk. A reference is
 * defined as tuple (file descriptor, offset, size) enabling direct mapping without deflation.
 * This can be used even within the renderer process, since it just dup's the apk's fd.
 */
public class ApkAssets {
    private static final String TAG = "ApkAssets";

    // This isn't thread safe, but that's ok because it's only used for debugging.
    // Note reference operations are atomic so there is no security issue.
    private static String sLastError;

    @SuppressWarnings("unused")
    public static long[] open(Context context, String fileName) {
        sLastError = null;
        AssetFileDescriptor afd = null;
        try {
            AssetManager manager = context.getAssets();
            afd = manager.openNonAssetFd(fileName);
            return new long[]{
                    afd.getParcelFileDescriptor().detachFd(), afd.getStartOffset(), afd.getLength()
            };
        } catch (IOException e) {
            sLastError = "Error while loading asset " + fileName + " from " + fileName + ": " + e;
            // As a general rule there's no point logging here because the caller should handle
            // receiving an fd of -1 sensibly, and the log message is either mirrored later, or
            // unwanted (in the case where a missing file is expected), or wanted but will be
            // ignored, as most non-fatal logs are.
            // It makes sense to log here when the file exists, but is unable to be opened as an fd
            // because (for example) it is unexpectedly compressed in an apk. In that case, the log
            // message might save someone some time working out what has gone wrong.
            // For that reason, we only suppress the message when the exception message doesn't look
            // informative (Android framework passes the filename as the message on actual file not
            // found, and the empty string also wouldn't give any useful information for debugging).
            if (!e.getMessage().equals("") && !e.getMessage().equals(fileName)) {
                Log.e(TAG, sLastError);
            }
            return new long[]{-1, -1, -1};
        } finally {
            try {
                if (afd != null) {
                    afd.close();
                }
            } catch (IOException e2) {
                Log.e(TAG, "Unable to close AssetFileDescriptor", e2);
            }
        }
    }

    @SuppressWarnings("unused")
    private static String takeLastErrorString() {
        String rv = sLastError;
        sLastError = null;
        return rv;
    }
}

