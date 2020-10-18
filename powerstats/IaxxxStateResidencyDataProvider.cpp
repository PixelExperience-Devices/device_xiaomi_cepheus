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

#include "IaxxxStateResidencyDataProvider.h"

#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <utility>

#include <linux/mfd/adnc/iaxxx-module.h>

namespace android {
namespace hardware {
namespace google {
namespace pixel {
namespace powerstats {

IaxxxStateResidencyDataProvider::IaxxxStateResidencyDataProvider(uint32_t id)
    : mPath("/dev/iaxxx-module-celldrv"), mPowerEntityId(id) {}

bool IaxxxStateResidencyDataProvider::getResults(
        std::unordered_map<uint32_t, PowerEntityStateResidencyResult> &results) {
    android::base::unique_fd devNode(open(mPath.c_str(), O_RDWR));
    if (devNode.get() < 0) {
        PLOG(ERROR) << __func__ << ":Failed to open file " << mPath;
        return false;
    }

    struct iaxxx_pwr_stats pwrStatsCount;
    int err = ioctl(devNode.get(), IAXXX_POWER_STATS_COUNT, &pwrStatsCount);
    if (err != 0) {
        PLOG(ERROR) << __func__ << "Failed to retrieve stats from " << mPath;
        return false;
    }

    PowerEntityStateResidencyResult result = {.powerEntityId = mPowerEntityId};
    hidl_vec<PowerEntityStateResidencyData> stateResidencyData;
    stateResidencyData.resize(NUM_MPLL_CLK_FREQ + 1);  // Each of the MPLL frequencies and sleep

    // Populate stats for each MPLL frequency
    for (uint32_t stateId = MPLL_CLK_3000; stateId != NUM_MPLL_CLK_FREQ; stateId++) {
        stateResidencyData[stateId] = PowerEntityStateResidencyData{
                .powerEntityStateId = stateId,
                .totalTimeInStateMs = pwrStatsCount.mpllCumulativeDur[stateId],
                .totalStateEntryCount = pwrStatsCount.mpll_cumulative_cnts[stateId],
                .lastEntryTimestampMs = pwrStatsCount.mpllTimeStamp[stateId]};
    }

    // Populate stats for Sleep mode
    stateResidencyData[NUM_MPLL_CLK_FREQ] = PowerEntityStateResidencyData{
            .powerEntityStateId = NUM_MPLL_CLK_FREQ,
            .totalTimeInStateMs = pwrStatsCount.sleepModeCumulativeDur,
            .totalStateEntryCount = 0,  // Sleep entry count is not available
            .lastEntryTimestampMs = pwrStatsCount.sleepModeTimeStamp};

    result.stateResidencyData = stateResidencyData;
    results.insert(std::make_pair(mPowerEntityId, result));
    return true;
}

std::vector<PowerEntityStateSpace> IaxxxStateResidencyDataProvider::getStateSpaces() {
    std::vector<PowerEntityStateSpace> stateSpace = {{.powerEntityId = mPowerEntityId}};
    hidl_vec<PowerEntityStateInfo> states;
    states.resize(NUM_MPLL_CLK_FREQ + 1);  // Each of the MPLL frequencies and sleep
    for (uint32_t stateId = MPLL_CLK_3000; stateId <= NUM_MPLL_CLK_FREQ; stateId++) {
        states[stateId] = PowerEntityStateInfo{
                .powerEntityStateId = stateId,
                .powerEntityStateName = static_cast<std::string>(mStateNames[stateId])};
    }
    stateSpace[0].states = states;

    return stateSpace;
}

}  // namespace powerstats
}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
