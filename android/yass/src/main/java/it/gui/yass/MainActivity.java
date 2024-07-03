// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */
package it.gui.yass;

import android.app.AlertDialog;
import android.app.Dialog;
import android.app.UiModeManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.net.ConnectivityManager;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.ParcelFileDescriptor;
import android.preference.PreferenceManager;
import android.util.Log;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.app.AppCompatDelegate;

import java.io.IOException;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.HashMap;
import java.util.Locale;
import java.util.Timer;
import java.util.TimerTask;

import it.gui.yass.databinding.ActivityMainBinding;

public class MainActivity extends AppCompatActivity {
    private static final String TAG = "MainActivity";
    private static final String YASS_TAG = "yass";
    private static final String TUN2PROXY_TAG = "tun2proxy";
    public static MainActivity self;

    static {
        // Load native library
        System.loadLibrary("yass");
    }

    private final YassVpnService vpnService = new YassVpnService();
    private final int VPN_REQUEST_CODE = 10000;
    private final int SETTINGS_REQUEST_CODE = 10001;
    private final Handler handler = new Handler();
    private NativeMachineState state = NativeMachineState.STOPPED;
    private Timer mRefreshTimer;
    private int nativeLocalPort = 0;
    private long tun2proxyPtr = 0;
    private Thread tun2proxyThread;

    private static String humanReadableByteCountBin(long bytes) {
        if (bytes < 1024) {
            return String.format("%d B/s", bytes);
        }
        long value = bytes;
        String ci = "KMGTPE";
        int cPos = 0;
        for (int i = 40; i >= 0 && bytes > 0xfffccccccccccccL >> i; i -= 10) {
            value >>= 10;
            ++cPos;
        }
        return String.format("%5.2f %c/s", value / 1024.0, ci.charAt(cPos));
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        self = this;
        it.gui.yass.databinding.ActivityMainBinding binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        getSupportActionBar().hide();

        onNativeCreate();
        loadSettingsFromNative();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            UiModeManager manager = (UiModeManager) getSystemService(UI_MODE_SERVICE);
            manager.setApplicationNightMode(UiModeManager.MODE_NIGHT_AUTO);
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM);
        } else {
            AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_NO);
        }
        Log.i(TAG, "onCreate done");
    }

    @Override
    protected void onDestroy() {
        if (state == NativeMachineState.STARTED || state == NativeMachineState.STARTING) {
            onStopVpn();
        }
        onNativeDestroy();
        super.onDestroy();
        self = null;
        Log.i(TAG, "onDestroy done");
    }

    private native void onNativeCreate();

    private native void onNativeDestroy();

    private void loadSettingsFromNative() {
        EditText serverHostEditText = findViewById(R.id.serverHostEditText);
        serverHostEditText.setText(YassUtils.getServerHost());
        EditText serverSNIEditText = findViewById(R.id.serverSNIEditText);
        serverSNIEditText.setText(YassUtils.getServerSNI());
        EditText serverPortEditText = findViewById(R.id.serverPortEditText);
        serverPortEditText.setText(String.format(getLocale(), "%d", YassUtils.getServerPort()));
        EditText usernameEditText = findViewById(R.id.usernameEditText);
        usernameEditText.setText(YassUtils.getUsername());
        EditText passwordEditText = findViewById(R.id.passwordEditText);
        passwordEditText.setText(YassUtils.getPassword());

        Spinner cipherSpinner = findViewById(R.id.cipherSpinner);
        ArrayAdapter<String> adapter = new ArrayAdapter<>(this,
                android.R.layout.simple_spinner_item, YassUtils.getCipherStrings());
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        cipherSpinner.setAdapter(adapter);
        cipherSpinner.setSelection(YassUtils.getCipher());

        EditText dohUrlEditText = findViewById(R.id.dohUrlEditText);
        dohUrlEditText.setText(String.format(getLocale(), "%s", YassUtils.getDoHUrl()));

        EditText dotHostEditText = findViewById(R.id.dotHostEditText);
        dotHostEditText.setText(String.format(getLocale(), "%s", YassUtils.getDoTHost()));

        EditText limitRateEditText = findViewById(R.id.limitRateEditText);
        limitRateEditText.setText(String.format(getLocale(), "%s", YassUtils.getLimitRate()));

        EditText timeoutEditText = findViewById(R.id.timeoutEditText);
        timeoutEditText.setText(String.format(getLocale(), "%d", YassUtils.getTimeout()));

        loadSettingsFromNativeCurrentIp();

        HashMap preferences = (HashMap) PreferenceManager.getDefaultSharedPreferences(getApplicationContext()).getAll();

        preferences.forEach((key, value) -> {
            Log.d("Preferences", String.format("Loading Preferences: %s -> %s", key, value));
            if (key.equals(YassSettingsFragment.EnablePostQuantumKyberPreferenceKey)) {
                YassUtils.setEnablePostQuantumKyber((Boolean) value);
            }
        });

        Button stopButton = findViewById(R.id.stopButton);
        stopButton.setEnabled(false);
    }

    private void loadSettingsFromNativeCurrentIp() {
        String currentIp = "?";
        try {
            byte[] ipAddress = ByteBuffer.wrap(new byte[4])
                    .order(ByteOrder.nativeOrder())
                    .putInt(getIpAddress())
                    .array();
            InetAddress addr = InetAddress.getByAddress(ipAddress);
            currentIp = addr.toString();
        } catch (UnknownHostException e) {
            // nop
        }

        TextView currentIpTextView = findViewById(R.id.currentIpTextView);
        currentIpTextView.setText(String.format(getString(R.string.status_current_ip_address), currentIp, nativeLocalPort));
    }

    private String saveSettingsIntoNative() {
        EditText serverHostEditText = findViewById(R.id.serverHostEditText);
        EditText serverSNIEditText = findViewById(R.id.serverSNIEditText);
        EditText serverPortEditText = findViewById(R.id.serverPortEditText);
        EditText usernameEditText = findViewById(R.id.usernameEditText);
        EditText passwordEditText = findViewById(R.id.passwordEditText);
        Spinner cipherSpinner = findViewById(R.id.cipherSpinner);
        EditText dohUrlEditText = findViewById(R.id.dohUrlEditText);
        EditText dotHostEditText = findViewById(R.id.dotHostEditText);
        EditText limitRateEditText = findViewById(R.id.limitRateEditText);
        EditText timeoutEditText = findViewById(R.id.timeoutEditText);

        return YassUtils.saveConfig(serverHostEditText.getText().toString(),
                serverSNIEditText.getText().toString(),
                serverPortEditText.getText().toString(),
                usernameEditText.getText().toString(),
                passwordEditText.getText().toString(),
                cipherSpinner.getSelectedItemPosition(),
                dohUrlEditText.getText().toString(),
                dotHostEditText.getText().toString(),
                limitRateEditText.getText().toString(),
                timeoutEditText.getText().toString());
    }

    public void onStartClicked(View view) {
        if (state == NativeMachineState.STOPPED) {
            String error_msg = saveSettingsIntoNative();
            if (error_msg != null) {
                onNativeStartFailedOnUIThread(error_msg, false);
                return;
            }

            Button startButton = findViewById(R.id.startButton);
            startButton.setEnabled(false);

            TextView statusTextView = findViewById(R.id.statusTextView);
            statusTextView.setText(R.string.status_starting);
            state = NativeMachineState.STARTING;
            nativeStart();
        }
    }

    private native void nativeStart();

    private void onNativeStartedOnUIThread(int local_port) {
        nativeLocalPort = local_port;
        Intent intent = YassVpnService.prepare(getApplicationContext());

        if (intent == null) {
            onActivityResult(VPN_REQUEST_CODE, RESULT_OK, null);
        } else {
            startActivityForResult(intent, VPN_REQUEST_CODE);
        }
    }

    private void onNativeStartFailedOnUIThread(String error_msg, boolean stop_native) {
        if (stop_native) {
            nativeStop();
        }

        state = NativeMachineState.STOPPED;
        Button startButton = findViewById(R.id.startButton);
        startButton.setEnabled(true);

        TextView statusTextView = findViewById(R.id.statusTextView);
        statusTextView.setText(String.format(getString(R.string.status_started_with_error_msg), error_msg));

        Dialog alertDialog = new AlertDialog.Builder(this)
                .setTitle(getString(R.string.status_start_failed))
                .setMessage(String.format(getString(R.string.status_started_with_error_msg), error_msg))
                .create();
        alertDialog.show();
    }

    @SuppressWarnings("unused")
    private void onNativeStarted(String error_msg, int local_port) {
        if (error_msg != null) {
            Log.e(YASS_TAG, String.format("yass thr start failed: %s", error_msg));
        } else {
            Log.v(YASS_TAG, String.format("yass thr started with port %d", local_port));
        }
        this.runOnUiThread(new Runnable() {
            public void run() {
                if (error_msg != null) {
                    onNativeStartFailedOnUIThread(error_msg, false);
                } else {
                    onNativeStartedOnUIThread(local_port);
                }
            }
        });
    }

    private native void nativeStop();

    @SuppressWarnings("unused")
    private void onNativeStopped() {
        Log.v(YASS_TAG, "yass thr stopped");
        this.runOnUiThread(new Runnable() {
            public void run() {
                state = NativeMachineState.STOPPED;
                Button startButton = findViewById(R.id.startButton);
                startButton.setEnabled(true);

                TextView statusTextView = findViewById(R.id.statusTextView);
                statusTextView.setText(String.format(getString(R.string.status_stopped), YassUtils.getServerHost(), YassUtils.getServerPort()));
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == SETTINGS_REQUEST_CODE) {
            return;
        }
        if (requestCode == VPN_REQUEST_CODE && resultCode == RESULT_OK) {
            onStartVpn();
            return;
        }
        if (requestCode == VPN_REQUEST_CODE) {
            Log.e(TAG, "vpn service intent not allowed");
            onNativeStartFailedOnUIThread(String.format(getString(R.string.status_started_with_error_msg), "No permission to create VPN service"), true);
            return;
        }
        super.onActivityResult(requestCode, resultCode, data);
    }

    public void onStartVpn() {
        ParcelFileDescriptor tunFd = vpnService.connect(getString(R.string.app_name), getApplicationContext(), nativeLocalPort);
        if (tunFd == null) {
            Log.e(TAG, "Unable to run create tunFd");
            onNativeStartFailedOnUIThread(String.format(getString(R.string.status_started_with_error_msg), "Unable to run create tunFd"), true);
            return;
        }

        int log_level = 0;
        if (Log.isLoggable(TUN2PROXY_TAG, Log.VERBOSE)) {
            log_level = 5;
        } else if (Log.isLoggable(TUN2PROXY_TAG, Log.DEBUG)) {
            log_level = 4;
        } else if (Log.isLoggable(TUN2PROXY_TAG, Log.INFO)) {
            log_level = 3;
        } else if (Log.isLoggable(TUN2PROXY_TAG, Log.WARN)) {
            log_level = 2;
        } else if (Log.isLoggable(TUN2PROXY_TAG, Log.ERROR)) {
            log_level = 1;
        }

        String proxyUrl = String.format(Locale.getDefault(), "socks5://127.0.0.1:%d", nativeLocalPort);
        tun2proxyPtr = tun2ProxyInit(proxyUrl, tunFd.getFd(), YassVpnService.DEFAULT_MTU, log_level, true);
        if (tun2proxyPtr == 0) {
            Log.e(TUN2PROXY_TAG, "Unable to run tun2ProxyInit");
            vpnService.stopSelf();
            try {
                tunFd.close();
            } catch (IOException e) {
                // nop
            }
            onNativeStartFailedOnUIThread(String.format(getString(R.string.status_started_with_error_msg), "Unable to run tun2ProxyInit"), true);
            return;
        }
        Log.v(TUN2PROXY_TAG, String.format("Init with proxy url: %s", proxyUrl));
        tun2proxyThread = new Thread() {
            public void run() {
                Log.v(TUN2PROXY_TAG, "tun2proxy thr started");
                int ret = tun2ProxyRun(tun2proxyPtr);
                if (ret != 0) {
                    // TODO should we handle this error?
                    Log.e(TUN2PROXY_TAG, String.format("Unable to run tun2ProxyRun: %d", ret));
                }
                try {
                    tunFd.close();
                } catch (IOException e) {
                    // nop
                }
                Log.v(TUN2PROXY_TAG, "tun2proxy thr stopped");
            }
        };
        tun2proxyThread.setName("tun2proxy thr");
        tun2proxyThread.start();

        state = NativeMachineState.STARTED;
        Button stopButton = findViewById(R.id.stopButton);
        stopButton.setEnabled(true);

        TextView statusTextView = findViewById(R.id.statusTextView);
        statusTextView.setText(R.string.status_started);

        loadSettingsFromNativeCurrentIp();

        startRefreshPoll();
    }

    public void onStopVpn() {
        stopRefreshPoll();
        vpnService.stopSelf();
        int ret = tun2ProxyShutdown(tun2proxyPtr);
        if (ret != 0) {
            Log.e(TUN2PROXY_TAG, String.format("Unable to run tun2ProxyShutdown: %d", ret));
        }

        try {
            tun2proxyThread.join();
        } catch (InterruptedException e) {
            // nop
        }
        tun2ProxyDestroy(tun2proxyPtr);
        tun2proxyPtr = 0;
        tun2proxyThread = null;
        nativeStop();

        Button stopButton = findViewById(R.id.stopButton);
        stopButton.setEnabled(false);

        nativeLocalPort = 0;
        loadSettingsFromNativeCurrentIp();

        TextView statusTextView = findViewById(R.id.statusTextView);
        statusTextView.setText(R.string.status_stopping);
        state = NativeMachineState.STOPPING;
    }

    public void onStopClicked(View view) {
        if (state == NativeMachineState.STARTED) {
            onStopVpn();
        }
    }

    public void onOptionsClicked(View view) {
        // opening a new intent to open settings activity.
        Intent intent = new Intent(MainActivity.this, SettingsActivity.class);
        startActivityForResult(intent, SETTINGS_REQUEST_CODE);
    }

    private native long tun2ProxyInit(String proxy_url, int tun_fd, int tun_mtu, int log_level, boolean dns_over_tcp);

    private native int tun2ProxyRun(long context);

    private native int tun2ProxyShutdown(long context);

    private native void tun2ProxyDestroy(long context);


    //
    // first connection number, then rx rate, then tx rate
    //
    private native long[] getRealtimeTransferRate();

    private void startRefreshPoll() {
        mRefreshTimer = new Timer();
        Log.v(TAG, "refresh polling timer started");
        TimerTask mRefreshTimerTask = new TimerTask() {
            @Override
            public void run() {
                handler.post(new Runnable() {
                    @Override
                    public void run() {
                        if (state != NativeMachineState.STARTED) {
                            return;
                        }
                        long[] result = getRealtimeTransferRate();
                        TextView statusTextView = findViewById(R.id.statusTextView);
                        statusTextView.setText(String.format(getString(R.string.status_started_with_rate),
                                result[0],
                                humanReadableByteCountBin(result[1]),
                                humanReadableByteCountBin(result[2])));
                    }
                });

            }
        };
        mRefreshTimer.schedule(mRefreshTimerTask, 0, 1000L);
    }

    private void stopRefreshPoll() {
        Log.v(TAG, "refresh polling timer stopped");
        if (mRefreshTimer != null) {
            mRefreshTimer.cancel();
            mRefreshTimer.purge();
        }
    }

    @SuppressWarnings("unused")
    public ConnectivityManager getConnectivityManager() {
        return (ConnectivityManager) getSystemService(Context.CONNECTIVITY_SERVICE);
    }

    @SuppressWarnings("unused")
    public int getIpAddress() {
        WifiManager wm = (WifiManager) getSystemService(Context.WIFI_SERVICE);
        return wm.getConnectionInfo().getIpAddress();
    }

    @SuppressWarnings("unused")
    private String getNativeLibraryDirectory() {
        ApplicationInfo ai = this.getApplicationContext().getApplicationInfo();
        if ((ai.flags & ApplicationInfo.FLAG_UPDATED_SYSTEM_APP) != 0
                || (ai.flags & ApplicationInfo.FLAG_SYSTEM) == 0) {
            return ai.nativeLibraryDir;
        }

        return "/system/lib/";
    }

    @SuppressWarnings("unused")
    private String getCacheLibraryDirectory() {
        return this.getApplicationContext().getCacheDir().toString();
    }

    @SuppressWarnings("unused")
    private String getDataLibraryDirectory() {
        return this.getApplicationContext().getDir("data", MODE_PRIVATE).toString();
    }

    @SuppressWarnings("unused")
    private String getCurrentLocale() {
        return this.getApplicationContext().getResources().getConfiguration().getLocales().get(0).toString();
    }

    private Locale getLocale() {
        return this.getApplicationContext().getResources().getConfiguration().getLocales().get(0);
    }

    @SuppressWarnings("unused")
    public long[] openApkAssets(String fileName) {
        return ApkAssets.open(this.getApplicationContext(), fileName);
    }

    enum NativeMachineState {
        STOPPED,
        STOPPING,
        STARTING,
        STARTED,
    }
}
