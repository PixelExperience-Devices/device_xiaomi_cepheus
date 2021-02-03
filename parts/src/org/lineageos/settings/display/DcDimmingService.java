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

import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.ContentObserver;
import android.os.Binder;
import android.os.IBinder;
import android.os.Handler;
import android.os.SystemClock;
import android.os.UserHandle;
import android.provider.Settings;
import android.util.ArrayMap;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
import java.util.Locale;

import org.lineageos.settings.R;

import android.os.SystemProperties;
import android.util.Log;

import com.android.internal.os.BatteryStatsImpl.SystemClocks;

public class DcDimmingService extends Service {

    private static final String TAG = "DcDimmingService";
    private Context mContext;
    private String mDcDimmingNode;
    private static final String DC_DIMMING_AUTO_MODE = "dc_dimming_auto_mode";
    private static final String DC_DIMMING_BRIGHTNESS = "dc_dimming_brightness";
    private static final String DC_DIMMING_STATE = "dc_dimming_state";
    private static final String START_TIME = "start_time";
    private static final String END_TIME = "end_time";
    public static final int MODE_AUTO_OFF = 0;
    public static final int MODE_AUTO_TIME = 1;
    public static final int MODE_AUTO_BRIGHTNESS = 2;
    public static final int MODE_AUTO_FULL = 3;
    private static final boolean DEBUG = false;

    private final Handler mHandler = new Handler();
    private SettingsObserver mSettingsObserver;
    private int mBrightness, mBrightnessAvg, mBrightnessThreshold, mAutoMode, mHour, mMinute;
    private boolean mDcOn, mScreenOff;
    private long mAvgStartTime;
    private final ArrayMap<Integer, Long> mBrightnessMap = new ArrayMap<>(20);
    private final IBinder mBinder = new LocalBinder();
    private Date timeRunnable, startTime, endTime;
    private Calendar calendar;
    private SimpleDateFormat inputParser;

    @Override
    public void onCreate() {
        super.onCreate();
        mContext = this;

        mDcDimmingNode = getString(R.string.dc_dimming_node);
        mSettingsObserver = new SettingsObserver(mHandler);
        final IntentFilter intentFilter =
                new IntentFilter(Intent.ACTION_SCREEN_OFF);
        intentFilter.addAction(Intent.ACTION_SCREEN_ON);
        registerReceiver(mIntentReceiver, intentFilter);
        mSettingsObserver.observe();
        mSettingsObserver.init();
        inputParser = new SimpleDateFormat("HH:mm", Locale.getDefault());
        Calendar calend = Calendar.getInstance();
        mHour = calend.get(Calendar.HOUR_OF_DAY);
        mMinute = calend.get(Calendar.MINUTE);
        if (getStartTime() == null) {
            setStartTime(String.format("%02d:%02d", mHour, mMinute));
        }
        if (getEndTime() == null) {
            setEndTime(String.format("%02d:%02d", mHour, mMinute));
        }
        try {
            startTime = inputParser.parse(getStartTime());
            endTime = inputParser.parse(getEndTime());
            ;
        } catch (ParseException e) {
        }

        try {
            timeRunnable = inputParser.parse(mHour + ":" + mMinute);
        } catch (ParseException e) {
        }
        updateState(mDcOn);
        if (DEBUG) Log.d(TAG, "DcDimmingService started");
    }

    public class LocalBinder extends Binder {
        public DcDimmingService getService() {
            return DcDimmingService.this;
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        if (DEBUG) Log.d(TAG, "DcDimmingService bound");
        return mBinder;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (DEBUG) Log.d(TAG, "DcDimmingService destroyed");
    }

    private final Runnable mBrightnessRunnable = new Runnable() {
        @Override
        public void run() {
            updateBrightnessAvg();
            if (DEBUG) Log.d(TAG, "DcDimming mBrightnessRunnable mBrightnessAvg:" + mBrightnessAvg + " mBrightnessThreshold:" + mBrightnessThreshold);
            if (!mHandler.hasCallbacks(mBrightnessRunnable)) {
                mHandler.postDelayed(mBrightnessRunnable, 10000);
            }
            updateState(mDcOn);
        }
    };

    private final Runnable mTimeRunnable = new Runnable() {
        @Override
        public void run() {
            calendar = Calendar.getInstance();
            mHour = calendar.get(Calendar.HOUR_OF_DAY);
            mMinute = calendar.get(Calendar.MINUTE);
            if (DEBUG) Log.d(TAG, "DcDimming mTimeRunnable");
            try {
                timeRunnable = inputParser.parse(mHour + ":" + mMinute);
            } catch (ParseException e) {
            }
            if (!mHandler.hasCallbacks(mTimeRunnable)) {
                mHandler.postDelayed(mTimeRunnable, 21000);
            }
            updateState(mDcOn);
        }
    };

    private final BroadcastReceiver mIntentReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (Intent.ACTION_SCREEN_ON.equals(intent.getAction())) {
                if (DEBUG) Log.d(TAG, "DcDimming mIntentReceiver ACTION_SCREEN_ON");
                mScreenOff = false;
                if (!mHandler.hasCallbacks(mBrightnessRunnable)) {
                    mHandler.postDelayed(mBrightnessRunnable, 2500);
                }
                if (!mHandler.hasCallbacks(mTimeRunnable)) {
                    mHandler.postDelayed(mTimeRunnable, 5000);
                }
                mHandler.postDelayed(() -> {
                    updateState(mDcOn);
                }, 300);
            } else if (Intent.ACTION_SCREEN_OFF.equals(intent.getAction())) {
                if (DEBUG) Log.d(TAG, "DcDimming mIntentReceiver ACTION_SCREEN_OFF");
                mScreenOff = true;
                mHandler.removeCallbacks(mBrightnessRunnable);
                mHandler.removeCallbacks(mTimeRunnable);
            }
        }
    };

    public synchronized void updateState(boolean enable) {
        String nodeVal = readNode();
        if (nodeVal == null) {
            return;
        }
        if (mAutoMode != MODE_AUTO_OFF && isDcDimmingOn()) {
            switch (mAutoMode) {
                case MODE_AUTO_TIME:
                    if (!mHandler.hasCallbacks(mTimeRunnable)) {
                        mHandler.postDelayed(mTimeRunnable, 21000);
                    }
                    mDcOn = autoEnableDC();
                    mHandler.removeCallbacks(mBrightnessRunnable);
                    writeNode(mDcOn);
                    break;
                case MODE_AUTO_BRIGHTNESS:
                    if (!mHandler.hasCallbacks(mBrightnessRunnable)) {
                        mHandler.postDelayed(mBrightnessRunnable, 10000);
                    }
                    mDcOn = autoEnableDC();
                    mHandler.removeCallbacks(mTimeRunnable);
                    writeNode(mDcOn);
                    break;
                case MODE_AUTO_FULL:
                    if (!mHandler.hasCallbacks(mTimeRunnable)) {
                        mHandler.postDelayed(mTimeRunnable, 21000);
                    }
                    if (!mHandler.hasCallbacks(mBrightnessRunnable)) {
                        mHandler.postDelayed(mBrightnessRunnable, 10000);
                    }
                    mDcOn = autoEnableDC();
                    writeNode(mDcOn);
                    break;
            }
        } else {
            mHandler.removeCallbacks(mBrightnessRunnable);
            mHandler.removeCallbacks(mTimeRunnable);
            mDcOn = isDcDimmingOn();
            writeNode(mDcOn);
        }
    }

    public void writeNode(boolean enable) {
        if (DEBUG) Log.d(TAG, "DcDimming writeNode enable:" + enable);
        BufferedWriter writer = null;
        try {
            writer = new BufferedWriter(new FileWriter(mDcDimmingNode));
            writer.write(enable ? "1" : "0");
        } catch (FileNotFoundException e) {
            Log.w(TAG, "DcDimming no such file " + mDcDimmingNode + " for writing", e);
        } catch (IOException e) {
            Log.e(TAG, "DcDimming could not write to file " + mDcDimmingNode, e);
        } finally {
            try {
                if (writer != null) {
                    writer.close();
                }
            } catch (IOException e) {
                // Ignored, not much we can do anyway
            }
        }
    }

    private String readNode() {
        String node = "null";
        BufferedReader reader = null;
        try {
            reader = new BufferedReader(new FileReader(mDcDimmingNode), 512);
            node = reader.readLine();
        } catch (FileNotFoundException e) {
            Log.w(TAG, "DcDimming no such file " + mDcDimmingNode + " for reading", e);
        } catch (IOException e) {
            Log.e(TAG, "DcDimming could not read from file " + mDcDimmingNode, e);
        } finally {
            try {
                if (reader != null) {
                    reader.close();
                }
            } catch (IOException e) {
                // Ignored, not much we can do anyway
            }
        }
        return node;
    }

    public boolean autoEnableDC() {
        switch (mAutoMode) {
            case MODE_AUTO_OFF:
                return autoModeManual();
            case MODE_AUTO_TIME:
                return autoModeTime();
            case MODE_AUTO_BRIGHTNESS:
                return autoModeBrightness();
            case MODE_AUTO_FULL:
                return autoModeTimeBrightness();
        }
        return false;
    }

    private boolean autoModeManual() {
        return true;
    }

    private boolean autoModeTime() {
        try {
            startTime = inputParser.parse(getStartTime());
            endTime = inputParser.parse(getEndTime());
            ;
        } catch (ParseException e) {
        }
        if (startTime.before(endTime)) {
            if (startTime.equals(timeRunnable) || startTime.before(timeRunnable) && endTime.after(timeRunnable)) {
                return true;
            } else return false;
        }
        if (startTime.after(endTime)) {
            if ((startTime.equals(timeRunnable) || startTime.before(timeRunnable) && endTime.before(timeRunnable)) || (startTime.equals(timeRunnable) || startTime.after(timeRunnable) && endTime.after(timeRunnable))) {
                return true;
            } else return false;
        }
        return false;
    }

    private boolean autoModeBrightness() {
        return mBrightnessAvg != 0 && mBrightnessAvg <= mBrightnessThreshold;
    }

    private boolean autoModeTimeBrightness() {
        return autoModeTime() && autoModeBrightness();
    }

    public void setAutoMode(int mode) {
        final long ident = Binder.clearCallingIdentity();
        try {
            if (mAutoMode != mode) {
                if (DEBUG) Log.d(TAG, "DcDimming setAutoMode(" + mode + ")");
                mAutoMode = mode;
                Settings.System.putIntForUser(mContext
                                .getContentResolver(),
                        DC_DIMMING_AUTO_MODE, mode,
                        UserHandle.USER_CURRENT);
                updateState(mDcOn);
            }
        } finally {
            Binder.restoreCallingIdentity(ident);
        }
    }

    public void setDcDimming(boolean enable) {
    	if (DEBUG) Log.d(TAG, "DcDimming setDcDimming(" + enable + ")");
        final long ident = Binder.clearCallingIdentity();
        try {
            Settings.System.putIntForUser(mContext.getContentResolver(),
                    DC_DIMMING_STATE, enable ? 1 : 0, UserHandle.USER_CURRENT);
            updateState(enable);
        } finally {
            Binder.restoreCallingIdentity(ident);
        }
    }

    public void setStartTime(String minutes) {
        final long ident = Binder.clearCallingIdentity();
        try {
            Settings.System.putStringForUser(mContext.getContentResolver(),
                    START_TIME, minutes, UserHandle.USER_CURRENT);
        } finally {
            Binder.restoreCallingIdentity(ident);
        }
    }

    public void setEndTime(String minutes) {
        final long ident = Binder.clearCallingIdentity();
        try {
            Settings.System.putStringForUser(mContext.getContentResolver(),
                    END_TIME, minutes, UserHandle.USER_CURRENT);
        } finally {
            Binder.restoreCallingIdentity(ident);
        }
    }

    public String getStartTime() {
        return
                Settings.System.getStringForUser(mContext.getContentResolver(),
                        START_TIME, UserHandle.USER_CURRENT);
    }

    public String getEndTime() {
        return
                Settings.System.getStringForUser(mContext.getContentResolver(),
                        END_TIME, UserHandle.USER_CURRENT);
    }


    public int getAutoMode() {
        return mAutoMode;
    }

    public boolean isDcDimmingOn() {
        return Settings.System.getIntForUser(getContentResolver(),
                DC_DIMMING_STATE, 0,
                UserHandle.USER_CURRENT) == 1;
    }

    public void setBrightnessThreshold(int thresh) {
    	if (DEBUG) Log.d(TAG, "DcDimming setBrightnessThreshold(" + thresh + ")");
        mBrightnessThreshold = thresh;
        if (!mScreenOff) {
            final long ident = Binder.clearCallingIdentity();
            try {
                Settings.System.putIntForUser(mContext.getContentResolver(),
                        DC_DIMMING_BRIGHTNESS,
                        thresh, UserHandle.USER_CURRENT);
                updateState(mDcOn);
            } finally {
                Binder.restoreCallingIdentity(ident);
            }
        }
    }

    private void updateBrightnessAvg() {
        final int size = mBrightnessMap.size();
        long totalTime = SystemClock.uptimeMillis() - mAvgStartTime;
        float tmpFrac = 0.0f;
        for (int i = 0; i < size; i++) {
            int brght = mBrightnessMap.keyAt(i);
            long diffTime = mBrightnessMap.valueAt(i);
            if (brght == mBrightness) {
                diffTime = SystemClock.uptimeMillis() - diffTime;
                mBrightnessMap.put(brght, SystemClock.uptimeMillis());
            }
            tmpFrac += (float) brght * ((float) diffTime / totalTime);
        }
        ArrayList<Integer> c = new ArrayList<>(1);
        c.add(mBrightness);
        mBrightnessMap.retainAll(c);
        mAvgStartTime = SystemClock.uptimeMillis();
        mBrightnessAvg = (int) tmpFrac;
    }

    private class SettingsObserver extends ContentObserver {

        SettingsObserver(Handler handler) {
            super(handler);
        }

        void observe() {
            ContentResolver resolver = getContentResolver();
            resolver.registerContentObserver(Settings.System.getUriFor(
                    Settings.System.SCREEN_BRIGHTNESS), false, this,
                    UserHandle.USER_ALL);
        }

        void init() {
            mBrightness = Settings.System.getIntForUser(getContentResolver(),
                    Settings.System.SCREEN_BRIGHTNESS, 0,
                    UserHandle.USER_CURRENT);
            mBrightnessThreshold = Settings.System.getIntForUser(getContentResolver(),
                    DC_DIMMING_BRIGHTNESS, 0,
                    UserHandle.USER_CURRENT);
            mAutoMode = Settings.System.getIntForUser(getContentResolver(),
                    DC_DIMMING_AUTO_MODE, 0,
                    UserHandle.USER_CURRENT);
            mDcOn = Settings.System.getIntForUser(getContentResolver(),
                    DC_DIMMING_STATE, 0,
                    UserHandle.USER_CURRENT) == 1;
            mAvgStartTime = SystemClock.uptimeMillis();
            mBrightnessMap.put(mBrightness, mAvgStartTime);
        }

        @Override
        public void onChange(boolean selfChange) {
            if (!isDcDimmingOn()) {
                return;
            }
            long currTime = SystemClock.uptimeMillis();
            if (mBrightnessMap.containsKey(mBrightness)) {
                mBrightnessMap.put(mBrightness, currTime - mBrightnessMap.get(mBrightness));
            }
            mBrightness = Settings.System.getIntForUser(getContentResolver(),
                    Settings.System.SCREEN_BRIGHTNESS, 0,
                    UserHandle.USER_CURRENT);
            if (DEBUG) Log.d(TAG, "DcDimming onChange brightness:" + mBrightness);
            mBrightnessMap.put(mBrightness, currTime);
        }
    }
}
