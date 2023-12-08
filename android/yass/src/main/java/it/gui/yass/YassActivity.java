/*
 * Copyright (C) 2023 by Chilledheart
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */
package it.gui.yass;

import android.app.Activity;
import android.app.NativeActivity;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.inputmethod.InputMethodManager;

public class YassActivity extends NativeActivity {

    static {
        // Load native library
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @SuppressWarnings("unused")
    public void showSoftInput() {
        InputMethodManager inputMethodManager = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
        inputMethodManager.showSoftInput(this.getWindow().getDecorView(), 0);
    }

    @SuppressWarnings("unused")
    public void hideSoftInput() {
        InputMethodManager inputMethodManager = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
        inputMethodManager.hideSoftInputFromWindow(this.getWindow().getDecorView().getWindowToken(), 0);
    }

    // We assume dispatchKeyEvent() of the NativeActivity is actually called for every
    // KeyEvent and not consumed by any View before it reaches here
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_DOWN) {
            notifyUnicodeChar(event.getUnicodeChar(event.getMetaState()));
        }
        return super.dispatchKeyEvent(event);
    }

    @SuppressWarnings("unused")
    public int getIpAddress() {
        WifiManager wm = (WifiManager) getSystemService(Context.WIFI_SERVICE);
        return wm.getConnectionInfo().getIpAddress();
    }

    private String getNativeLibraryDirectory() {
        ApplicationInfo ai = this.getApplicationContext().getApplicationInfo();
        if ((ai.flags & ApplicationInfo.FLAG_UPDATED_SYSTEM_APP) != 0
                || (ai.flags & ApplicationInfo.FLAG_SYSTEM) == 0) {
            return ai.nativeLibraryDir;
        }

        return "/system/lib/";
    }

    private native void notifyUnicodeChar(int unicode);
}
