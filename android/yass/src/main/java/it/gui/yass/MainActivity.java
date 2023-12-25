// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */
package it.gui.yass;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.res.Resources;
import android.net.ConnectivityManager;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TextView;

import java.io.IOException;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Locale;
import java.util.Timer;
import java.util.TimerTask;

import it.gui.yass.databinding.ActivityMainBinding;

public class MainActivity extends Activity {
    static {
        // Load native library
        System.loadLibrary("native-lib");
        System.loadLibrary("tun2proxy");
    }

    private NativeMachineState state = NativeMachineState.STOPPED;
    private Timer mRefreshTimer;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        it.gui.yass.databinding.ActivityMainBinding binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        onNativeCreate();

        loadSettingsFromNative();
    }

    @Override
    protected void onDestroy() {
        stopRefreshPoll();
        tun2ProxyStop();
        onNativeDestroy();
        super.onDestroy();
    }

    private native void onNativeCreate();

    private native void onNativeDestroy();

    private native String getServerHost();

    private native void setServerHost(String serverHost);

    private native int getServerPort();

    private native void setServerPort(int serverPort);

    private native String getUsername();

    private native void setUsername(String username);

    private native String getPassword();

    private native void setPassword(String password);

    private native int getCipher();

    private native void setCipher(int cipher_idx);

    private native String[] getCipherStrings();

    private native int getTimeout();

    private native void setTimeout(int timeout);

    private void loadSettingsFromNative() {
        EditText serverHostEditText = findViewById(R.id.serverHostEditText);
        serverHostEditText.setText(getServerHost());
        EditText serverPortEditText = findViewById(R.id.serverPortEditText);
        serverPortEditText.setText(String.format(getLocale(), "%d", getServerPort()));
        EditText usernameEditText = findViewById(R.id.usernameEditText);
        usernameEditText.setText(getUsername());
        EditText passwordEditText = findViewById(R.id.passwordEditText);
        passwordEditText.setText(getPassword());

        Spinner cipherSpinner = findViewById(R.id.cipherSpinner);
        ArrayAdapter<String> adapter = new ArrayAdapter<>(this,
                android.R.layout.simple_spinner_item, getCipherStrings());
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        cipherSpinner.setAdapter(adapter);
        cipherSpinner.setSelection(getCipher());

        EditText timeoutEditText = findViewById(R.id.timeoutEditText);
        timeoutEditText.setText(String.format(getLocale(), "%d", getTimeout()));

        TextView currentIpTextView = findViewById(R.id.currentIpTextView);

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
        Resources res = getResources();
        currentIpTextView.setText(String.format(res.getString(R.string.status_current_ip_address), currentIp));

        Button stopButton = findViewById(R.id.stopButton);
        stopButton.setEnabled(false);
    }

    private void saveSettingsIntoNative() {
        EditText serverHostEditText = findViewById(R.id.serverHostEditText);
        setServerHost(serverHostEditText.getText().toString());

        EditText serverPortEditText = findViewById(R.id.serverPortEditText);
        setServerPort(Integer.parseInt(serverPortEditText.getText().toString()));
        EditText usernameEditText = findViewById(R.id.usernameEditText);
        setUsername(usernameEditText.getText().toString());
        EditText passwordEditText = findViewById(R.id.passwordEditText);
        setPassword(passwordEditText.getText().toString());

        Spinner cipherSpinner = findViewById(R.id.cipherSpinner);
        setCipher(cipherSpinner.getSelectedItemPosition());

        EditText timeoutEditText = findViewById(R.id.timeoutEditText);
        setTimeout(Integer.parseInt(timeoutEditText.getText().toString()));
    }

    private final YassVpnService vpnService = new YassVpnService();

    private ParcelFileDescriptor tunFd = null;

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == VPN_SERVICE_CODE && resultCode == RESULT_OK) {
            onStartVpn();
            return;
        }
        super.onActivityResult(requestCode, resultCode, data);
    }

    private final int VPN_SERVICE_CODE = 10000;
    private Thread tun2proxyThread;
    private void onStartVpn() {
        tunFd = vpnService.connect(getApplicationContext());
        if (tunFd == null) {
            return;
        }
        tun2proxyThread = new Thread(){
            public void run() {
                tun2ProxyStart("socks5://127.0.0.1:3000", tunFd.getFd(), vpnService.DEFAULT_MTU, true, true);
            }
        };
        tun2proxyThread.start();

        Button startButton = findViewById(R.id.startButton);
        startButton.setEnabled(false);

        TextView statusTextView = findViewById(R.id.statusTextView);
        statusTextView.setText(R.string.status_starting);
        state = NativeMachineState.STARTING;
        nativeStart();
    }

    public void onStartClicked(View view) {
        if (state == NativeMachineState.STOPPED) {
            saveSettingsIntoNative();

            Intent intent = YassVpnService.prepare(getApplicationContext());
            if (intent == null) {
                onActivityResult(VPN_SERVICE_CODE, RESULT_OK, null);
            } else {
                startActivityForResult(intent, VPN_SERVICE_CODE);
            }
        }
    }

    private void onStopVpn() {
        try {
            tunFd.close();
        } catch (IOException e) {
            //nop
        }
        tunFd = null;

        tun2ProxyStop();
        try {
            tun2proxyThread.join();
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
        nativeStop();

        Button stopButton = findViewById(R.id.stopButton);
        stopButton.setEnabled(false);

        TextView statusTextView = findViewById(R.id.statusTextView);
        statusTextView.setText(R.string.status_stopping);
        state = NativeMachineState.STOPPING;
    }
    public void onStopClicked(View view) {
        if (state == NativeMachineState.STARTED) {
            stopRefreshPoll();
            onStopVpn();
        }
    }

    private native void nativeStart();

    @SuppressWarnings("unused")
    private void onNativeStarted(int error_code) {
        this.runOnUiThread(new Runnable() {
            public void run() {
                if (error_code != 0) {
                    state = NativeMachineState.STOPPED;
                    Button startButton = findViewById(R.id.startButton);
                    startButton.setEnabled(true);

                    TextView statusTextView = findViewById(R.id.statusTextView);
                    Resources res = getResources();
                    statusTextView.setText(String.format(res.getString(R.string.status_started_with_error), error_code));
                } else {
                    state = NativeMachineState.STARTED;
                    Button stopButton = findViewById(R.id.stopButton);
                    stopButton.setEnabled(true);

                    TextView statusTextView = findViewById(R.id.statusTextView);
                    statusTextView.setText(R.string.status_started);

                    startRefreshPoll();
                }
            }
        });
    }

    private native void nativeStop();

    @SuppressWarnings("unused")
    private void onNativeStopped() {
        this.runOnUiThread(new Runnable() {
            public void run() {
                state = NativeMachineState.STOPPED;
                Button startButton = findViewById(R.id.startButton);
                startButton.setEnabled(true);

                TextView statusTextView = findViewById(R.id.statusTextView);
                Resources res = getResources();
                statusTextView.setText(String.format(res.getString(R.string.status_stopped), getServerHost(), getServerPort()));
            }
        });
    }

    private native int tun2ProxyStart(String proxy_url, int tun_fd, int tun_mtu, boolean verbose, boolean dns_over_tcp);
    private native int tun2ProxyStop();

    // first connection number, then rx rate, then tx rate

    private native long[] getRealtimeTransferRate();

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

    private void startRefreshPoll() {
        mRefreshTimer = new Timer();
        TimerTask mRefreshTimerTask = new TimerTask() {
            @Override
            public void run() {
                long[] result = getRealtimeTransferRate();
                TextView statusTextView = findViewById(R.id.statusTextView);
                Resources res = getResources();
                statusTextView.setText(String.format(res.getString(R.string.status_started_with_rate),
                        result[0],
                        humanReadableByteCountBin(result[1]),
                        humanReadableByteCountBin(result[2])));
            }
        };
        mRefreshTimer.schedule(mRefreshTimerTask, 0, 1000L);
    }

    private void stopRefreshPoll() {
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
