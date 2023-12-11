package it.gui.yass;

import android.app.Activity;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.widget.ArrayAdapter;
import android.widget.EditText;
import android.widget.Button;
import android.widget.Spinner;

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
        serverPortEditText.setText(Integer.toString(getServerPort()));
        EditText usernameEditText = findViewById(R.id.usernameEditText);
        usernameEditText.setText(getUsername());
        EditText passwordEditText = findViewById(R.id.passwordEditText);
        passwordEditText.setText(getPassword());

        Spinner cipherSpinner = findViewById(R.id.cipherSpinner);
        ArrayAdapter<String> adapter = new ArrayAdapter<String>(this,
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

    @SuppressWarnings("unused")
    public long[] openApkAssets(String fileName) {
        return ApkAssets.open(this.getApplicationContext(), fileName);
    }
}
