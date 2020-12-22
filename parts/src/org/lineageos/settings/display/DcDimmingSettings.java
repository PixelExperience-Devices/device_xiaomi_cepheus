/*Copyright (C) 2020 The LineageOS Project

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.*/

package org.lineageos.settings.display;

import static org.lineageos.settings.display.DcDimmingService.MODE_AUTO_OFF;
import static org.lineageos.settings.display.DcDimmingService.MODE_AUTO_TIME;
import static org.lineageos.settings.display.DcDimmingService.MODE_AUTO_BRIGHTNESS;
import static org.lineageos.settings.display.DcDimmingService.MODE_AUTO_FULL;
import static com.android.settingslib.display.BrightnessUtils.GAMMA_SPACE_MAX;
import static com.android.settingslib.display.BrightnessUtils.convertGammaToLinear;
import static com.android.settingslib.display.BrightnessUtils.convertLinearToGamma;
import android.app.TimePickerDialog;
import android.content.ComponentName;
import android.content.Context;
import android.content.ContentResolver;
import android.content.Intent;
import android.content.ServiceConnection;
import android.database.ContentObserver;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.PowerManager;
import android.os.UserHandle;
import android.provider.Settings;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.TextView;
import android.app.ActionBar;
import androidx.preference.PreferenceViewHolder;
import androidx.preference.DropDownPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;
import androidx.preference.PreferenceFragment;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.SwitchPreference;
import android.os.SystemProperties;
import android.widget.TimePicker;
import androidx.preference.PreferenceManager;
import com.android.settingslib.widget.FooterPreference;
import com.android.settingslib.widget.LayoutPreference;
import androidx.preference.PreferenceGroup.OnExpandButtonClickListener;
import org.lineageos.settings.R;
import java.util.Calendar;

public class DcDimmingSettings extends PreferenceFragment
        implements Preference.OnPreferenceChangeListener, SeekBar.OnSeekBarChangeListener {

    private static final String TAG = "DcDimmingSettings";
    private static final String DC_ENABLE = "dc_dimming_enable";
    private static final String DC_BRIGHTNESS = "dc_dimming_brightness";
    private static final String DC_STATE = "dc_dimming_state";
    private static final String DC_TIME = "dc_dimming_time";
    private static final String DC_AUTO_MODE = "dc_dimming_auto_mode";
    private static final String SEEKBAR_PROGRESS = "seek_bar_progress";


    private DcDimmingService mService;
    private Context mContext;
    private LayoutPreference brightnessPreference, mTimePicker;
    private DropDownPreference mDropPreference;
    private SwitchPreference mSwitch;
    private SeekBar mSeekBar;
    private TextView mDcState, mBrightnessValue, mBrightnessTitle, mBrightnessSumm;
    private Button mStartTimeButton, mEndTimeButton;
    private boolean mSwitchChecked, isActivated, auto, manual;
    private boolean mBound = false;
    private Intent intent;
    private int mHour, mMinute, mMinimumBacklight, mMaximumBacklight, mode;
    private String startTime, endTime;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        mContext = getActivity();
        intent = new Intent(mContext, DcDimmingService.class);
        mContext.bindService(intent, mConnection, Context.BIND_AUTO_CREATE);

        setPreferencesFromResource(R.xml.dc_dimming_settings, rootKey);
        final ActionBar actionBar = getActivity().getActionBar();
        actionBar.setDisplayHomeAsUpEnabled(true);

        PowerManager pm = (PowerManager) mContext.getSystemService(Context.POWER_SERVICE);
        mMinimumBacklight = pm.getMinimumScreenBrightnessSetting();
        mMaximumBacklight = pm.getMaximumScreenBrightnessSetting();
        SettingsObserver settingsObserver = new SettingsObserver(new Handler());
        settingsObserver.observe();

        brightnessPreference = findPreference(DC_BRIGHTNESS);
        mSeekBar = brightnessPreference.findViewById(R.id.seekbar);
        mBrightnessValue = brightnessPreference.findViewById(R.id.value);
        mBrightnessTitle = brightnessPreference.findViewById(android.R.id.title);
        mBrightnessSumm = brightnessPreference.findViewById(android.R.id.summary);
        mSeekBar.setOnSeekBarChangeListener(this);
        mSeekBar.setMax(GAMMA_SPACE_MAX / 2 + 1);

        mSwitch = (SwitchPreference) findPreference(DC_ENABLE);
        mSwitch.setOnPreferenceChangeListener(this);

        mTimePicker = findPreference(DC_TIME);
        mStartTimeButton = mTimePicker.findViewById(R.id.time_start_button);
        mEndTimeButton = mTimePicker.findViewById(R.id.time_end_button);
        mStartTimeButton.setOnClickListener(mStartButtonListener);
        mEndTimeButton.setOnClickListener(mEndButtonListener);

        mDropPreference = findPreference(DC_AUTO_MODE);
        mDropPreference.setEntries(new CharSequence[]{
                mContext.getString(R.string.dc_dimming_mode_never),
                mContext.getString(R.string.dc_dimming_mode_time),
                mContext.getString(R.string.dc_dimming_mode_brightness),
                mContext.getString(R.string.dc_dimming_mode_full)
        });
        mDropPreference.setEntryValues(new CharSequence[]{
                String.valueOf(MODE_AUTO_OFF),
                String.valueOf(MODE_AUTO_TIME),
                String.valueOf(MODE_AUTO_BRIGHTNESS),
                String.valueOf(MODE_AUTO_FULL)
        });
        mDropPreference.setOnPreferenceChangeListener(this);

        Preference footerPreference = findPreference(FooterPreference.KEY_FOOTER);
        footerPreference.setTitle(mContext.getResources().getString(R.string.dc_dimming_info));

        mSwitchChecked = !mSwitch.isChecked();
    }

    private ServiceConnection mConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName className,
                                       IBinder service) {
            Log.d(TAG, "DcDimmingService Connected");
            DcDimmingService.LocalBinder binder = (DcDimmingService.LocalBinder) service;
            mService = binder.getService();
            mDropPreference.setValue(String.valueOf(mService.getAutoMode()));
            final int gamma = Settings.System.getIntForUser(mContext.getContentResolver(),
                    SEEKBAR_PROGRESS, 0,
                    UserHandle.USER_CURRENT);
            mSeekBar.setProgress(gamma);
            mBound = true;
            updateSettings();
        }

        @Override
        public void onServiceDisconnected(ComponentName arg0) {
            Log.d(TAG, "DcDimmingService Disconnected");
            mBound = false;
        }
    };

    @Override
    public void onDestroy() {
        super.onDestroy();
        mContext.unbindService(mConnection);
        mBound = false;
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (preference instanceof DropDownPreference) {
            mService.setAutoMode(Integer.parseInt((String) newValue));
        }
        if (preference instanceof SwitchPreference) {
            mSwitchChecked = !mSwitch.isChecked();
            mService.setDcDimming(mSwitchChecked);
            updateSettings();
        }
        return true;
    }

    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        if (fromUser) {
            final int linear = convertGammaToLinear(progress, mMinimumBacklight, mMaximumBacklight);
            final int per = (int) ((float) progress * 100.0f / GAMMA_SPACE_MAX);
            final int min = Math.min(per, linear);
                Settings.System.putIntForUser(mContext
                            .getContentResolver(),
                    SEEKBAR_PROGRESS, progress,
                    UserHandle.USER_CURRENT);
                mService.setBrightnessThreshold(linear);
        }
        updateSettings();
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {
    }

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {
    }

    private final View.OnClickListener mStartButtonListener = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            final Calendar c = Calendar.getInstance();
            mHour = c.get(Calendar.HOUR_OF_DAY);
            mMinute = c.get(Calendar.MINUTE);
            TimePickerDialog timePickerDialog = new TimePickerDialog(mContext,
                    new TimePickerDialog.OnTimeSetListener() {
                        @Override
                        public void onTimeSet(TimePicker view, int hourOfDay,
                                              int minute) {
                            mStartTimeButton.setText(String.format("%02d:%02d", hourOfDay, minute));
                            mService.setStartTime(String.format("%02d:%02d", hourOfDay, minute));
                        }
                    }, mHour, mMinute, true);
            timePickerDialog.show();
            updateSettings();
        }
    };

    private final View.OnClickListener mEndButtonListener = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            final Calendar c = Calendar.getInstance();
            mHour = c.get(Calendar.HOUR_OF_DAY);
            mMinute = c.get(Calendar.MINUTE);
            TimePickerDialog timePickerDialog = new TimePickerDialog(mContext,
                    new TimePickerDialog.OnTimeSetListener() {
                        @Override
                        public void onTimeSet(TimePicker view, int hourOfDay,
                                              int minute) {
                            mEndTimeButton.setText(String.format("%02d:%02d", hourOfDay, minute));
                            mService.setEndTime(String.format("%02d:%02d", hourOfDay, minute));
                        }
                    }, mHour, mMinute, true);
            timePickerDialog.show();
            updateSettings();
        }
    };

    private void updateBrightnessValue(int gamma) {
        int per = (int) ((float) gamma * 100.0f / GAMMA_SPACE_MAX);
        if (!isActivated) {
            mBrightnessValue.setText(R.string.dc_dimming_summary_off);
        } else {
            mBrightnessValue.setText(String.valueOf(per) + "%");
        }
    }

    private void updateSettings() {
        if (!mBound) {
            return;
        }
        mode = mService.getAutoMode();
        isActivated = mService.isDcDimmingOn();
        auto = mService.autoEnableDC();
        manual = mode == MODE_AUTO_OFF;
        startTime = mService.getStartTime();
        endTime = mService.getEndTime();

        updateBrightnessState((mode >= MODE_AUTO_BRIGHTNESS) && isActivated);
        updateTimeState((mode == MODE_AUTO_TIME) || (mode == MODE_AUTO_FULL) && isActivated);
        updateBrightnessValue(mSeekBar.getProgress());
        boolean active = isActivated && (auto || manual);
        mStartTimeButton.setText(startTime);
        mEndTimeButton.setText(endTime);
            }

    private void updateTimeState(boolean enable) {
        mStartTimeButton.setEnabled(enable);
        mEndTimeButton.setEnabled(enable);
    }

    private void updateBrightnessState(boolean enable) {
        mBrightnessValue.setEnabled(enable);
        mBrightnessTitle.setEnabled(enable);
        mBrightnessSumm.setEnabled(enable);
        mSeekBar.setEnabled(enable);
    }

    private class SettingsObserver extends ContentObserver {
        SettingsObserver(Handler handler) {
            super(handler);
        }

        void observe() {
            ContentResolver resolver = mContext.getContentResolver();
            resolver.registerContentObserver(Settings.System.getUriFor(
                    DC_AUTO_MODE), false, this,
                    UserHandle.USER_ALL);
            resolver.registerContentObserver(Settings.System.getUriFor(
                    DC_STATE), false, this,
                    UserHandle.USER_ALL);
        }


        @Override
        public void onChange(boolean selfChange) {
            updateSettings();
        }
    }
}
