package it.gui.yass;

import android.app.Activity;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.res.Resources;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.EditText;
import android.widget.Button;
import android.widget.Spinner;
import android.widget.TextView;

import java.util.Locale;
import java.util.Timer;
import java.util.TimerTask;

import it.gui.yass.databinding.ActivityMainBinding;

public class MainActivity extends Activity {
    static {
        // Load native library
        System.loadLibrary("native-lib");
    }

    private ActivityMainBinding binding;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        onNativeCreate();

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

        Button stopButton = findViewById(R.id.stopButton);
        stopButton.setEnabled(false);
    }

    @Override
    protected void onDestroy() {
        onNativeDestroy();
        super.onDestroy();
    }

    private native void onNativeCreate();

    private native void onNativeDestroy();

    private native String getServerHost();

    private native int getServerPort();

    private native String getUsername();

    private native String getPassword();

    private native int getCipher();

    private native String[] getCipherStrings();

    enum NativeMachineState {
        STOPPED,
        STOPPING,
        STARTING,
        STARTED,
    }

    private NativeMachineState state = NativeMachineState.STOPPED;

    public void onStartClicked(View view) {
        if (state == NativeMachineState.STOPPED) {
            Button startButton = findViewById(R.id.startButton);
            startButton.setEnabled(false);

            TextView statusTextView = findViewById(R.id.statusTextView);
            statusTextView.setText(R.string.status_starting);
            state = NativeMachineState.STARTING;
            nativeStart();
        }

    }

    public void onStopClicked(View view) {
        if (state == NativeMachineState.STARTED) {
            stopRefreshPoll();

            Button stopButton = findViewById(R.id.stopButton);
            stopButton.setEnabled(false);

            TextView statusTextView = findViewById(R.id.statusTextView);
            statusTextView.setText(R.string.status_stopping);
            state = NativeMachineState.STOPPING;
            nativeStop();
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

    // first connection number, then rx rate, then tx rate

    private native double[] getRealtimeTransferRate();

    private Timer mRefreshTimer;
    private TimerTask mRefreshTimerTask;

    private void startRefreshPoll() {
        mRefreshTimer = new Timer();
        mRefreshTimerTask = new TimerTask() {
            @Override
            public void run() {
                double[] result = getRealtimeTransferRate();
                TextView statusTextView = findViewById(R.id.statusTextView);
                Resources res = getResources();
                statusTextView.setText(String.format(res.getString(R.string.status_started_with_rate), (int) result[0], result[1], result[2]));
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
}
