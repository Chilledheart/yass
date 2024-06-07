package it.gui.yass;

import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.util.Log;

import androidx.annotation.Nullable;

// TODO use PreferenceFragementCompact once use minimum sdk version 28
// https://developer.android.com/develop/ui/views/components/settings#java
public class YassSettingsFragment extends PreferenceFragment {
    public static String EnablePostQuantumKyberPreferenceKey = "enable_post_quantum_kyber";

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // below line is used to add preference
        // fragment from our xml folder.
        addPreferencesFromResource(R.xml.preferences);

        Preference enablePostQuantumKyberPref = findPreference(EnablePostQuantumKyberPreferenceKey);

        if (enablePostQuantumKyberPref != null) {
            enablePostQuantumKyberPref.setOnPreferenceChangeListener((preference, newValue) -> {
                Log.d("Preferences", String.format("Setting Preferences: %s -> %s", EnablePostQuantumKyberPreferenceKey, newValue));
                YassUtils.setEnablePostQuantumKyber((Boolean) newValue);
                return true; // Return true if the event is handled.
            });
        }
    }
}