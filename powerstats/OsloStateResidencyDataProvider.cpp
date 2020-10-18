/*
 * Copyright (C) 2019 The Android Open Source Project
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
#define LOG_TAG "libpixelpowerstats"

#include "OsloStateResidencyDataProvider.h"

#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <utility>

#include <linux/mfd/adnc/iaxxx-module.h>
#include "tests/oslo_iaxxx_sensor_control.h"

namespace android {
namespace hardware {
namespace google {
namespace pixel {
namespace powerstats {

OsloStateResidencyDataProvider::OsloStateResidencyDataProvider(uint32_t id)
        : mPath("/dev/iaxxx-module-celldrv"), mPowerEntityId(id) {}

bool OsloStateResidencyDataProvider::getResults(
        std::unordered_map<uint32_t, PowerEntityStateResidencyResult> &results) {
    android::base::unique_fd devNode(open(mPath.c_str(), O_RDWR));
    if (devNode.get() < 0) {
        PLOG(ERROR) << __func__ << ":Failed to open file " << mPath;
        return false;
    }

    int err = 0;
    struct iaxxx_sensor_param sp = {
        .inst_id = 0,
        .param_id = SENSOR_PARAM_DUMP_STATS,
        .param_val = 1,
        .block_id = 0,
    };

    err = ioctl(devNode.get(), MODULE_SENSOR_SET_PARAM, (unsigned long)&sp);
    if (err) {
        PLOG(ERROR) << __func__ << ": MODULE_SENSOR_SET_PARAM IOCTL failed";
        return false;
    }

    struct iaxxx_sensor_mode_stats stats[SENSOR_NUM_MODE];
    err = ioctl(devNode.get(), IAXXX_SENSOR_MODE_STATS, (unsigned long)stats);
    if (err) {
        PLOG(ERROR) << __func__ << ": IAXXX_SENSOR_MODE_STATS IOCTL failed";
        return false;
    }

    PowerEntityStateResidencyResult result = {
        .powerEntityId = mPowerEntityId,
        .stateResidencyData = {
            {.powerEntityStateId = SENSOR_MODE_OFF,
             .totalTimeInStateMs = stats[SENSOR_MODE_OFF].totalTimeSpentMs,
             .totalStateEntryCount = stats[SENSOR_MODE_OFF].totalNumEntries,
             .lastEntryTimestampMs = stats[SENSOR_MODE_OFF].lastEntryTimeStampMs},
            {.powerEntityStateId = SENSOR_MODE_ENTRANCE,
             .totalTimeInStateMs = stats[SENSOR_MODE_ENTRANCE].totalTimeSpentMs,
             .totalStateEntryCount = stats[SENSOR_MODE_ENTRANCE].totalNumEntries,
             .lastEntryTimestampMs = stats[SENSOR_MODE_ENTRANCE].lastEntryTimeStampMs},
            {.powerEntityStateId = SENSOR_MODE_INTERACTIVE,
             .totalTimeInStateMs = stats[SENSOR_MODE_INTERACTIVE].totalTimeSpentMs,
             .totalStateEntryCount = stats[SENSOR_MODE_INTERACTIVE].totalNumEntries,
             .lastEntryTimestampMs = stats[SENSOR_MODE_INTERACTIVE].lastEntryTimeStampMs}}};

    results.insert(std::make_pair(mPowerEntityId, result));
    return true;
}

std::vector<PowerEntityStateSpace> OsloStateResidencyDataProvider::getStateSpaces() {
    return {{.powerEntityId = mPowerEntityId,
             .states = {
                 {.powerEntityStateId = SENSOR_MODE_OFF, .powerEntityStateName = "Off"},
                 {.powerEntityStateId = SENSOR_MODE_ENTRANCE, .powerEntityStateName = "Entrance"},
                 {.powerEntityStateId = SENSOR_MODE_INTERACTIVE,
                  .powerEntityStateName = "Interactive"}}}};
}

}  // namespace powerstats
}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
