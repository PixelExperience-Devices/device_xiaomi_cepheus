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
#ifndef HARDWARE_GOOGLE_PIXEL_POWERSTATS_IAXXXSTATERESIDENCYDATAPROVIDER_H
#define HARDWARE_GOOGLE_PIXEL_POWERSTATS_IAXXXSTATERESIDENCYDATAPROVIDER_H

#include <pixelpowerstats/PowerStats.h>

#include <unordered_map>

#include <linux/mfd/adnc/iaxxx-module.h>

namespace android {
namespace hardware {
namespace google {
namespace pixel {
namespace powerstats {

class IaxxxStateResidencyDataProvider : public IStateResidencyDataProvider {
  public:
    IaxxxStateResidencyDataProvider(uint32_t id);
    ~IaxxxStateResidencyDataProvider() = default;
    bool getResults(
            std::unordered_map<uint32_t, PowerEntityStateResidencyResult> &results) override;
    std::vector<PowerEntityStateSpace> getStateSpaces() override;

  private:
    const std::string mPath;
    const uint32_t mPowerEntityId;
    static constexpr std::string_view mStateNames[] = {
        "MPLL_3MHz",  "MPLL_5MHz",  "MPLL_6MHz",   "MPLL_8MHz",  "MPLL_10MHz", "MPLL_15MHz",
        "MPLL_30MHz", "MPLL_35MHz", "MPLL_40MHz",  "MPLL_45MHz", "MPLL_50MHz", "MPLL_55MHz",
        "MPLL_60MHz", "MPLL_80MHz", "MPLL_120MHz", "Sleep"};
    static_assert(NUM_MPLL_CLK_FREQ + 1 == sizeof(mStateNames) / sizeof(*mStateNames),
                  "mStateNames must have an entry for each of the MPLL frequencies and sleep");
};

}  // namespace powerstats
}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_PIXEL_POWERSTATS_IAXXXSTATERESIDENCYDATAPROVIDER_H
