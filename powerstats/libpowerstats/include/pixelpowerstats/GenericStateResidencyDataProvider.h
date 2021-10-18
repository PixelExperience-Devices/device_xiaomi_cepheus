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

#ifndef HARDWARE_GOOGLE_PIXEL_POWERSTATS_GENERICSTATERESIDENCYDATAPROVIDER_H
#define HARDWARE_GOOGLE_PIXEL_POWERSTATS_GENERICSTATERESIDENCYDATAPROVIDER_H
// TODO(b/167628903): Delete this file
#include <pixelpowerstats/PowerStats.h>

namespace android {
namespace hardware {
namespace google {
namespace pixel {
namespace powerstats {

class StateResidencyConfig {
  public:
    std::string name;
    std::string header;

    bool entryCountSupported;
    std::string entryCountPrefix;
    std::function<uint64_t(uint64_t)> entryCountTransform;

    bool totalTimeSupported;
    std::string totalTimePrefix;
    std::function<uint64_t(uint64_t)> totalTimeTransform;

    bool lastEntrySupported;
    std::string lastEntryPrefix;
    std::function<uint64_t(uint64_t)> lastEntryTransform;
};

class PowerEntityConfig {
  public:
    PowerEntityConfig(const std::vector<StateResidencyConfig> &stateResidencyConfigs);
    PowerEntityConfig(const std::string &header,
                      const std::vector<StateResidencyConfig> &stateResidencyConfigs);
    PowerEntityConfig(const uint32_t start_id, const std::string &header,
                      const std::vector<StateResidencyConfig> &stateResidencyConfigs);
    std::string mHeader;
    std::vector<std::pair<uint32_t, StateResidencyConfig>> mStateResidencyConfigs;
};

class GenericStateResidencyDataProvider : public IStateResidencyDataProvider {
  public:
    GenericStateResidencyDataProvider(std::string path) : mPath(std::move(path)) {}
    ~GenericStateResidencyDataProvider() = default;
    void addEntity(uint32_t id, const PowerEntityConfig &config);
    bool getResults(
            std::unordered_map<uint32_t, PowerEntityStateResidencyResult> &results) override;
    std::vector<PowerEntityStateSpace> getStateSpaces() override;

  private:
    const std::string mPath;
    std::vector<std::pair<uint32_t, PowerEntityConfig>> mPowerEntityConfigs;
};

std::vector<StateResidencyConfig> generateGenericStateResidencyConfigs(
        const StateResidencyConfig &stateConfig,
        const std::vector<std::pair<std::string, std::string>> &stateHeaders);

}  // namespace powerstats
}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_PIXEL_POWERSTATS_GENERICSTATERESIDENCYDATAPROVIDER_H
