/*
 * Copyright (C) 2018 The LineageOS Project
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

package org.lineageos.settings.display;

import android.os.Bundle;
import android.widget.Switch;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragment;

import com.android.settingslib.widget.TopIntroPreference;
import com.android.settingslib.widget.MainSwitchPreference;
import com.android.settingslib.widget.OnMainSwitchChangeListener;

import org.lineageos.settings.R;
import org.lineageos.settings.utils.FileUtils;

public class DcDimmingSettingsFragment extends PreferenceFragment implements
        OnMainSwitchChangeListener {

    private TopIntroPreference mIntroText;
    private MainSwitchPreference mDcDimmingSwitchBar;
    private static final String DC_DIMMING_INTRO_KEY = "dc_dimming_top_intro";
    private static final String DC_DIMMING_KEY = "dc_dimming";
    private static final String DC_DIMMING_NODE = "/sys/devices/platform/soc/soc:qcom,dsi-display-primary/dimlayer_bl";

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        addPreferencesFromResource(R.xml.dcdimming_settings);

        mIntroText = (TopIntroPreference) findPreference(DC_DIMMING_INTRO_KEY);
        mDcDimmingSwitchBar = (MainSwitchPreference) findPreference(DC_DIMMING_KEY);

        if (FileUtils.fileExists(DC_DIMMING_NODE)) {
            mDcDimmingSwitchBar.setEnabled(true);
            mDcDimmingSwitchBar.addOnSwitchChangeListener(this);
        } else {
            mIntroText.setSummary(R.string.dc_dimming_text_not_supported);
            mDcDimmingSwitchBar.setEnabled(false);
        }
    }

    @Override
    public void onSwitchChanged(Switch switchView, boolean isChecked) {
        FileUtils.writeLine(DC_DIMMING_NODE, isChecked ? "1":"0");
    }
}