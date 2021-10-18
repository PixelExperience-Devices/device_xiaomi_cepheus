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

#include <list>
#include <string>
#include <unordered_map>
#include <vector>

namespace aidl {
namespace android {
namespace hardware {
namespace power {
namespace stats {

/**
 * This class provides the necessary functionality for selection of energy meter
 * based on file configurations.
 */
class IioEnergyMeterDataSelector {
  public:
    IioEnergyMeterDataSelector(const std::unordered_map<std::string, std::string> &devicePaths);

  private:
    void parseConfigData(const std::string data,
                         std::unordered_map<std::string, std::list<std::string>> *deviceConfigs);
    void applyConfigToDevices(
            const std::unordered_map<std::string, std::list<std::string>> &deviceConfigs);
    void applyConfigsByAscendingPriority();
    void sendConfigurationComplete();

    const std::unordered_map<std::string, std::string> kDevicePaths;
    const std::string kSelectionNode = "/enabled_rails";
    const std::string kSelectionComplete = "CONFIG_COMPLETE";

    /* Order matters (ascending priority), see applyConfigsByAscendingPriority() */
    const std::vector<const std::string> kConfigPaths = {
            "/data/vendor/powerstats/odpm_config",
    };
};

}  // namespace stats
}  // namespace power
}  // namespace hardware
}  // namespace android
}  // namespace aidl
