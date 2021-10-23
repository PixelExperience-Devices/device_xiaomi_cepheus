/*
 * Copyright (C) 2018,2020 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.lineageos.settings.dirac;

import android.app.ActionBar;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CompoundButton;
import android.widget.Switch;
import android.widget.TextView;

import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.Preference.OnPreferenceChangeListener;
import androidx.preference.PreferenceFragment;
import androidx.preference.SwitchPreference;

import com.android.settingslib.widget.MainSwitchPreference;
import com.android.settingslib.widget.OnMainSwitchChangeListener;

import org.lineageos.settings.R;

public class DiracSettingsFragment extends PreferenceFragment 
        implements OnPreferenceChangeListener, OnMainSwitchChangeListener {

    private static final String PREF_HEADSET = "dirac_headset_pref";
    private static final String PREF_HIFI = "dirac_hifi_pref";
    private static final String PREF_PRESET = "dirac_preset_pref";

    private TextView mTextView;
    private MainSwitchPreference mSwitchBar;

    private ListPreference mHeadsetType;
    private ListPreference mPreset;
    private SwitchPreference mHifi;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        addPreferencesFromResource(R.xml.dirac_settings);
    
        DiracUtils.initialize(getActivity());
        boolean enhancerEnabled = DiracUtils.isDiracEnabled();

        mSwitchBar = (MainSwitchPreference) findPreference("dirac_enable");
        mSwitchBar.addOnSwitchChangeListener(this);
        mSwitchBar.setChecked(enhancerEnabled);

        mHeadsetType = (ListPreference) findPreference(PREF_HEADSET);
        mHeadsetType.setOnPreferenceChangeListener(this);

        mPreset = (ListPreference) findPreference(PREF_PRESET);
        mPreset.setOnPreferenceChangeListener(this);

        mHifi = (SwitchPreference) findPreference(PREF_HIFI);
        mHifi.setOnPreferenceChangeListener(this);

        boolean hifiEnabled = DiracUtils.getHifiMode();
        mHeadsetType.setEnabled(!hifiEnabled && enhancerEnabled);
        mPreset.setEnabled(!hifiEnabled && enhancerEnabled);
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        switch (preference.getKey()) {
            case PREF_HEADSET:
                DiracUtils.setHeadsetType(Integer.parseInt(newValue.toString()));
                return true;
            case PREF_HIFI:
                DiracUtils.setHifiMode((Boolean) newValue ? 1 : 0);
                if (DiracUtils.isDiracEnabled()) {
                    mHeadsetType.setEnabled(!(Boolean) newValue);
                    mPreset.setEnabled(!(Boolean) newValue);
                }
                return true;
            case PREF_PRESET:
                DiracUtils.setLevel((String) newValue);
                return true;
            default:
                return false;
        }
    }

    @Override
    public void onSwitchChanged(Switch switchView, boolean isChecked) {
        DiracUtils.setMusic(isChecked);

        if (!DiracUtils.getHifiMode()) {
            mHeadsetType.setEnabled(isChecked);
            mPreset.setEnabled(isChecked);
        }
    }
}