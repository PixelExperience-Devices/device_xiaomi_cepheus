/*
 * Copyright (C) 2020 The Android Open Source Project
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

#pragma once

#include <map>
#include <unordered_map>
#include <vector>
#include <string>

enum AttributionType {
    /* Parsing uid_time_in_state like following format.
     * (see /proc/uid_time_in_state)
     * uid: state_name_0, state_name_1, ..
     * uid_0: time_in_state_0, time_in_state_1, ..
     * uid_1: time_in_state_0, time_in_state_1, ..
     */
    UID_TIME_IN_STATE,
};

// Declaring different return values for each type of attributions
struct AttributionStats {
    /* Members for UID_TIME_IN_STATE
     * uidTimeInStats: key = uid, val = {uid_time_in_state}
     * uidTimeInStateNames: state_name_0, state_name_1, ..
     */
    std::unordered_map<int32_t, std::vector<long>> uidTimeInStats;
    std::vector<std::string> uidTimeInStateNames;
};

namespace aidl {
namespace android {
namespace hardware {
namespace power {
namespace stats {

class PowerStatsEnergyAttribution {
  public:
    PowerStatsEnergyAttribution() = default;
    ~PowerStatsEnergyAttribution() = default;
    AttributionStats getAttributionStats(std::unordered_map<int32_t, std::string> paths);
private:
    bool readUidTimeInState(AttributionStats *attrStats, std::string path);
};

}  // namespace stats
}  // namespace power
}  // namespace hardware
}  // namespace android
}  // namespace aidl
