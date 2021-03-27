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

#ifndef HARDWARE_GOOGLE_PIXEL_POWERSTATS_AIDLSTATERESIDENCYDATAPROVIDER_H
#define HARDWARE_GOOGLE_PIXEL_POWERSTATS_AIDLSTATERESIDENCYDATAPROVIDER_H

#include <android/vendor/powerstats/BnPixelPowerStatsCallback.h>
#include <android/vendor/powerstats/BnPixelPowerStatsProvider.h>
#include <pixelpowerstats/PowerStats.h>

namespace android {
namespace hardware {
namespace google {
namespace pixel {
namespace powerstats {

using android::sp;
using android::vendor::powerstats::BnPixelPowerStatsProvider;
using android::vendor::powerstats::IPixelPowerStatsCallback;
using binderStatus = android::binder::Status;
using ::android::hidl::base::V1_0::IBase;

class AidlStateResidencyDataProvider : public IStateResidencyDataProvider,
                                       public BnPixelPowerStatsProvider,
                                       public android::IBinder::DeathRecipient {
  public:
    AidlStateResidencyDataProvider() = default;
    ~AidlStateResidencyDataProvider() = default;
    void addEntity(uint32_t id, std::string entityName, std::vector<std::string> stateNames);

    // From IStateResidencyDataProvider
    bool getResults(std::unordered_map<uint32_t, PowerEntityStateResidencyResult> &results) override;
    std::vector<PowerEntityStateSpace> getStateSpaces() override;

    // From BnPixelPowerStatsProvider
    binderStatus registerCallback(const std::string &entityName,
                                  const sp<IPixelPowerStatsCallback> &callback) override;
    binderStatus unregisterCallback(const sp<IPixelPowerStatsCallback> &callback) override;

    // From IBinder::DeathRecipient
    void binderDied(const wp<android::IBinder> &who) override;

  private:
    struct StateSpace {
        uint32_t powerEntityId;
        std::unordered_map<std::string, uint32_t> stateInfos;
    };
    bool buildResult(std::string entityName,
                     const std::vector<android::vendor::powerstats::StateResidencyData> &stats,
                     PowerEntityStateResidencyResult &result);
    binderStatus unregisterCallbackInternal(const sp<IBinder> &callback);
    std::unordered_map<std::string, StateSpace> mEntityInfos;
    std::unordered_map<std::string, sp<IPixelPowerStatsCallback>> mCallbacks;
    std::mutex mLock;
};

}  // namespace powerstats
}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_PIXEL_POWERSTATS_AIDLSTATERESIDENCYDATAPROVIDER_H