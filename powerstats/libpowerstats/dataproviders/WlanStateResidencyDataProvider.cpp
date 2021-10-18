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

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/strings.h>

#include <dataproviders/WlanStateResidencyDataProvider.h>

namespace aidl {
namespace android {
namespace hardware {
namespace power {
namespace stats {

enum {
    ACTIVE_ID = 0,
    DEEPSLEEP_ID = 1,
};

static bool extractStat(const char *line, const std::string &prefix, uint64_t *stat) {
    char const *prefixStart = strstr(line, prefix.c_str());
    if (prefixStart == nullptr) {
        // Did not find the given prefix
        return false;
    }

    *stat = strtoull(prefixStart + prefix.length(), nullptr, 0);
    return true;
}

bool WlanStateResidencyDataProvider::getStateResidencies(
        std::unordered_map<std::string, std::vector<StateResidency>> *residencies) {
    std::vector<StateResidency> result = {{.id = ACTIVE_ID}, {.id = DEEPSLEEP_ID}};

    std::string wlanDriverStatus = ::android::base::GetProperty("wlan.driver.status", "unloaded");
    if (wlanDriverStatus != "ok") {
        LOG(ERROR) << ": wlan is " << wlanDriverStatus;
        // Return 0s for Wlan stats, because the driver is unloaded
        residencies->emplace(mName, result);
        return true;
    }

    // Using FILE* instead of std::ifstream for performance reasons (b/122253123)
    std::unique_ptr<FILE, decltype(&fclose)> fp(fopen(mPath.c_str(), "r"), fclose);
    if (!fp) {
        PLOG(ERROR) << ":Failed to open file " << mPath;
        return false;
    }
    size_t numFieldsRead = 0;
    const size_t numFields = 4;
    size_t len = 0;
    char *line = nullptr;

    while ((numFieldsRead < numFields) && (getline(&line, &len, fp.get()) != -1)) {
        uint64_t stat = 0;
        if (extractStat(line, "cumulative_sleep_time_ms:", &stat)) {
            result[1].totalTimeInStateMs = stat;
            ++numFieldsRead;
        } else if (extractStat(line, "cumulative_total_on_time_ms:", &stat)) {
            result[0].totalTimeInStateMs = stat;
            ++numFieldsRead;
        } else if (extractStat(line, "deep_sleep_enter_counter:", &stat)) {
            result[0].totalStateEntryCount = stat;
            result[1].totalStateEntryCount = stat;
            ++numFieldsRead;
        } else if (extractStat(line, "last_deep_sleep_enter_tstamp_ms:", &stat)) {
            result[1].lastEntryTimestampMs = stat;
            ++numFieldsRead;
        }
    }

    free(line);

    // End of file was reached and not all state data was parsed. Something
    // went wrong
    if (numFieldsRead != numFields) {
        LOG(ERROR) << __func__ << ": failed to parse stats for wlan";
        return false;
    }

    residencies->emplace(mName, result);

    return true;
}

std::unordered_map<std::string, std::vector<State>> WlanStateResidencyDataProvider::getInfo() {
    std::unordered_map<std::string, std::vector<State>> ret = {
            {mName,
             {{.id = ACTIVE_ID, .name = "Active"}, {.id = DEEPSLEEP_ID, .name = "Deep-Sleep"}}},
    };
    return ret;
}

}  // namespace stats
}  // namespace power
}  // namespace hardware
}  // namespace android
}  // namespace aidl
