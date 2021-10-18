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

#include <PowerStatsAidl.h>
#include "PowerStatsEnergyAttribution.h"

#include <utils/RefBase.h>

#include <map>
#include <set>

namespace aidl {
namespace android {
namespace hardware {
namespace power {
namespace stats {

using ::android::sp;

/**
 * PowerStatsEnergyConsumer is an energy consumer that can be represented as
 * EnergyConsumed = SUM_i(E_i) + SUM_j(C_j * T_j)
 * where E_i is the energy of channel i of the EnergyMeter and
 * where C_j is the coefficient (in mW) of state j and T_j is the total time (in ms) in state j
 *
 * Factory functions are provided to create PowerStatsEnergyConsumer of three varieties
 * 1. MeterAndEntityConsumer - number of channels is > 0, and there is at least one C_j != 0
 * 2. MeterConsumer - number of channels is > 0, and all C_j = 0
 * 3. EntityConsumer - number of channels is 0, and there is at least one C_j != 0
 */
class PowerStatsEnergyConsumer : public PowerStats::IEnergyConsumer {
  public:
    static std::unique_ptr<PowerStatsEnergyConsumer> createMeterConsumer(
            std::shared_ptr<PowerStats> p, EnergyConsumerType type, std::string name,
            std::set<std::string> channelNames);
    static std::unique_ptr<PowerStatsEnergyConsumer> createEntityConsumer(
            std::shared_ptr<PowerStats> p, EnergyConsumerType type, std::string name,
            std::string powerEntityName, std::map<std::string, int32_t> stateCoeffs);

    static std::unique_ptr<PowerStatsEnergyConsumer> createMeterAndEntityConsumer(
            std::shared_ptr<PowerStats> p, EnergyConsumerType type, std::string name,
            std::set<std::string> channelNames, std::string powerEntityName,
            std::map<std::string, int32_t> stateCoeffs);

    static std::unique_ptr<PowerStatsEnergyConsumer> createMeterAndAttrConsumer(
            std::shared_ptr<PowerStats> p, EnergyConsumerType type, std::string name,
            std::set<std::string> channelNames, std::unordered_map<int32_t, std::string> paths,
            std::map<std::string, int32_t> stateCoeffs);

    std::pair<EnergyConsumerType, std::string> getInfo() override { return {kType, kName}; }

    std::optional<EnergyConsumerResult> getEnergyConsumed() override;

    std::string getConsumerName() override;

  private:
    PowerStatsEnergyConsumer(std::shared_ptr<PowerStats> p, EnergyConsumerType type,
                             std::string name, bool attr = false);
    bool addEnergyMeter(std::set<std::string> channelNames);
    bool addPowerEntity(std::string powerEntityName, std::map<std::string, int32_t> stateCoeffs);
    bool addAttribution(std::unordered_map<int32_t, std::string> paths,
                        std::map<std::string, int32_t> stateCoeffs);

    const EnergyConsumerType kType;
    const std::string kName;
    std::shared_ptr<PowerStats> mPowerStats;
    std::vector<int32_t> mChannelIds;
    int32_t mPowerEntityId;
    bool mWithAttribution;
    std::unordered_map<int32_t, std::string> mAttrInfoPath;
    PowerStatsEnergyAttribution mEnergyAttribution;
    // Snapshot of each uid's energy, uid_time_in_state and total energy from power meter
    // mUidTimeInStateSS: key = uid, val = {uid_time_in_state}
    // mUidEnergySS:      key = uid, val = {uid's energy(UWs)}
    // mTotalEnergySS:    total energy from power meter
    std::unordered_map<int32_t, std::vector<long>> mUidTimeInStateSS;
    std::unordered_map<int32_t, int64_t> mUidEnergySS;
    int64_t mTotalEnergySS = 0;
    std::map<int32_t, int32_t> mCoefficients;  // key = stateId, val = coefficients (mW)
};

}  // namespace stats
}  // namespace power
}  // namespace hardware
}  // namespace android
}  // namespace aidl
