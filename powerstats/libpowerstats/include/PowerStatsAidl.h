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

#include <aidl/android/hardware/power/stats/BnPowerStats.h>

#include <optional>
#include <unordered_map>

namespace aidl {
namespace android {
namespace hardware {
namespace power {
namespace stats {

class PowerStats : public BnPowerStats {
  public:
    class IStateResidencyDataProvider {
      public:
        virtual ~IStateResidencyDataProvider() = default;
        virtual bool getStateResidencies(
                std::unordered_map<std::string, std::vector<StateResidency>> *residencies) = 0;
        virtual std::unordered_map<std::string, std::vector<State>> getInfo() = 0;
    };

    class IEnergyConsumer {
      public:
        virtual ~IEnergyConsumer() = default;
        virtual std::pair<EnergyConsumerType, std::string> getInfo() = 0;
        virtual std::optional<EnergyConsumerResult> getEnergyConsumed() = 0;
        virtual std::string getConsumerName() = 0;
    };

    class IEnergyMeterDataProvider {
      public:
        virtual ~IEnergyMeterDataProvider() = default;
        virtual ndk::ScopedAStatus readEnergyMeter(
                const std::vector<int32_t> &in_channelIds,
                std::vector<EnergyMeasurement> *_aidl_return) = 0;
        virtual ndk::ScopedAStatus getEnergyMeterInfo(std::vector<Channel> *_aidl_return) = 0;
    };

    PowerStats() = default;
    void addStateResidencyDataProvider(std::unique_ptr<IStateResidencyDataProvider> p);
    void addEnergyConsumer(std::unique_ptr<IEnergyConsumer> p);
    void setEnergyMeterDataProvider(std::unique_ptr<IEnergyMeterDataProvider> p);

    // Methods from aidl::android::hardware::power::stats::IPowerStats
    ndk::ScopedAStatus getPowerEntityInfo(std::vector<PowerEntity> *_aidl_return) override;
    ndk::ScopedAStatus getStateResidency(const std::vector<int32_t> &in_powerEntityIds,
                                         std::vector<StateResidencyResult> *_aidl_return) override;
    ndk::ScopedAStatus getEnergyConsumerInfo(std::vector<EnergyConsumer> *_aidl_return) override;
    ndk::ScopedAStatus getEnergyConsumed(const std::vector<int32_t> &in_energyConsumerIds,
                                         std::vector<EnergyConsumerResult> *_aidl_return) override;
    ndk::ScopedAStatus getEnergyMeterInfo(std::vector<Channel> *_aidl_return) override;
    ndk::ScopedAStatus readEnergyMeter(const std::vector<int32_t> &in_channelIds,
                                       std::vector<EnergyMeasurement> *_aidl_return) override;
    binder_status_t dump(int fd, const char **args, uint32_t numArgs) override;

  private:
    void getEntityStateNames(
            std::unordered_map<int32_t, std::string> *entityNames,
            std::unordered_map<int32_t, std::unordered_map<int32_t, std::string>> *stateNames);
    void getChannelNames(std::unordered_map<int32_t, std::string> *channelNames);
    void dumpStateResidency(std::ostringstream &oss, bool delta);
    void dumpStateResidencyDelta(std::ostringstream &oss,
                                 const std::vector<StateResidencyResult> &results);
    void dumpStateResidencyOneShot(std::ostringstream &oss,
                                   const std::vector<StateResidencyResult> &results);
    void dumpEnergyConsumer(std::ostringstream &oss, bool delta);
    void dumpEnergyMeter(std::ostringstream &oss, bool delta);

    std::vector<std::unique_ptr<IStateResidencyDataProvider>> mStateResidencyDataProviders;
    std::vector<PowerEntity> mPowerEntityInfos;
    /* Index that maps each power entity id to an entry in mStateResidencyDataProviders */
    std::vector<size_t> mStateResidencyDataProviderIndex;

    std::vector<std::unique_ptr<IEnergyConsumer>> mEnergyConsumers;
    std::vector<EnergyConsumer> mEnergyConsumerInfos;

    std::unique_ptr<IEnergyMeterDataProvider> mEnergyMeterDataProvider;
};

}  // namespace stats
}  // namespace power
}  // namespace hardware
}  // namespace android
}  // namespace aidl
