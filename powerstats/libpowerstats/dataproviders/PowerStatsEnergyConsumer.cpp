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

#include <dataproviders/PowerStatsEnergyConsumer.h>

#include <android-base/logging.h>

namespace aidl {
namespace android {
namespace hardware {
namespace power {
namespace stats {

PowerStatsEnergyConsumer::PowerStatsEnergyConsumer(std::shared_ptr<PowerStats> p,
                                                   EnergyConsumerType type, std::string name,
                                                   bool attr)
    : kType(type), kName(name), mPowerStats(p), mWithAttribution(attr) {}

std::unique_ptr<PowerStatsEnergyConsumer> PowerStatsEnergyConsumer::createMeterConsumer(
        std::shared_ptr<PowerStats> p, EnergyConsumerType type, std::string name,
        std::set<std::string> channelNames) {
    return createMeterAndEntityConsumer(p, type, name, channelNames, "", {});
}

std::unique_ptr<PowerStatsEnergyConsumer> PowerStatsEnergyConsumer::createEntityConsumer(
        std::shared_ptr<PowerStats> p, EnergyConsumerType type, std::string name,
        std::string powerEntityName, std::map<std::string, int32_t> stateCoeffs) {
    return createMeterAndEntityConsumer(p, type, name, {}, powerEntityName, stateCoeffs);
}

std::unique_ptr<PowerStatsEnergyConsumer> PowerStatsEnergyConsumer::createMeterAndEntityConsumer(
        std::shared_ptr<PowerStats> p, EnergyConsumerType type, std::string name,
        std::set<std::string> channelNames, std::string powerEntityName,
        std::map<std::string, int32_t> stateCoeffs) {
    auto ret =
            std::unique_ptr<PowerStatsEnergyConsumer>(new PowerStatsEnergyConsumer(p, type, name));

    if (ret->addEnergyMeter(channelNames) && ret->addPowerEntity(powerEntityName, stateCoeffs)) {
        return ret;
    }

    LOG(ERROR) << "Failed to create PowerStatsEnergyConsumer for " << name;
    return nullptr;
}

std::unique_ptr<PowerStatsEnergyConsumer> PowerStatsEnergyConsumer::createMeterAndAttrConsumer(
        std::shared_ptr<PowerStats> p, EnergyConsumerType type, std::string name,
        std::set<std::string> channelNames, std::unordered_map<int32_t, std::string> paths,
        std::map<std::string, int32_t> stateCoeffs) {
    auto ret = std::unique_ptr<PowerStatsEnergyConsumer>(
            new PowerStatsEnergyConsumer(p, type, name, true));

    if (ret->addEnergyMeter(channelNames) && ret->addAttribution(paths, stateCoeffs)) {
        return ret;
    }

    LOG(ERROR) << "Failed to create PowerStatsEnergyConsumer for " << name;
    return nullptr;
}

bool PowerStatsEnergyConsumer::addEnergyMeter(std::set<std::string> channelNames) {
    if (channelNames.empty()) {
        return true;
    }

    std::vector<Channel> channels;
    mPowerStats->getEnergyMeterInfo(&channels);

    for (const auto &c : channels) {
        if (channelNames.count(c.name)) {
            mChannelIds.push_back(c.id);
        }
    }

    return (mChannelIds.size() == channelNames.size());
}

bool PowerStatsEnergyConsumer::addPowerEntity(std::string powerEntityName,
                                              std::map<std::string, int32_t> stateCoeffs) {
    if (powerEntityName.empty() || stateCoeffs.empty()) {
        return true;
    }

    std::vector<PowerEntity> powerEntities;
    mPowerStats->getPowerEntityInfo(&powerEntities);

    for (const auto &p : powerEntities) {
        if (powerEntityName == p.name) {
            mPowerEntityId = p.id;
            for (const auto &s : p.states) {
                if (stateCoeffs.count(s.name)) {
                    mCoefficients.emplace(s.id, stateCoeffs.at(s.name));
                }
            }
            break;
        }
    }

    return (mCoefficients.size() == stateCoeffs.size());
}

bool PowerStatsEnergyConsumer::addAttribution(std::unordered_map<int32_t, std::string> paths,
                                              std::map<std::string, int32_t> stateCoeffs) {
    mAttrInfoPath = paths;

    if (paths.count(UID_TIME_IN_STATE)) {
        mEnergyAttribution = PowerStatsEnergyAttribution();
        AttributionStats attrStats = mEnergyAttribution.getAttributionStats(paths);
        if (attrStats.uidTimeInStats.empty() || attrStats.uidTimeInStateNames.empty()) {
            LOG(ERROR) << "Missing uid_time_in_state";
            return false;
        }

        // stateCoeffs should not blocking energy consumer to return power meter
        // so just handle this in getEnergyConsumed()
        if (stateCoeffs.empty()) {
            return true;
        }

        int32_t stateId = 0;
        for (const auto &stateName : attrStats.uidTimeInStateNames) {
            if (stateCoeffs.count(stateName)) {
                // When uid_time_in_state is not the only type of attribution,
                // should condider to separate the coefficients just for attribution.
                mCoefficients.emplace(stateId, stateCoeffs.at(stateName));
            }
            stateId++;
        }
    }

    return (mCoefficients.size() == stateCoeffs.size());
}

std::optional<EnergyConsumerResult> PowerStatsEnergyConsumer::getEnergyConsumed() {
    int64_t totalEnergyUWs = 0;
    int64_t timestampMs = 0;

    if (!mChannelIds.empty()) {
        std::vector<EnergyMeasurement> measurements;
        if (mPowerStats->readEnergyMeter(mChannelIds, &measurements).isOk()) {
            for (const auto &m : measurements) {
                totalEnergyUWs += m.energyUWs;
                timestampMs = m.timestampMs;
            }
        } else {
            LOG(ERROR) << "Failed to read energy meter";
            return {};
        }
    }

    std::vector<EnergyConsumerAttribution> attribution;
    if (!mCoefficients.empty()) {
        if (mWithAttribution) {
            AttributionStats attrStats = mEnergyAttribution.getAttributionStats(mAttrInfoPath);
            if (attrStats.uidTimeInStats.empty() || attrStats.uidTimeInStateNames.empty()) {
                LOG(ERROR) << "Missing uid_time_in_state";
                return {};
            }

            int64_t totalRelativeEnergyUWs = 0;
            for (const auto &uidTimeInStat : attrStats.uidTimeInStats) {
                int64_t uidEnergyUWs = 0;
                for (int id = 0; id < uidTimeInStat.second.size(); id++) {
                    if (mCoefficients.count(id)) {
                        int64_t d_time_in_state = uidTimeInStat.second.at(id);
                        if (mUidTimeInStateSS.count(uidTimeInStat.first)) {
                            d_time_in_state -= mUidTimeInStateSS.at(uidTimeInStat.first).at(id);
                        }
                        uidEnergyUWs += mCoefficients.at(id) * d_time_in_state;
                    }
                }
                totalRelativeEnergyUWs += uidEnergyUWs;

                EnergyConsumerAttribution attr = {
                    .uid = uidTimeInStat.first,
                    .energyUWs = uidEnergyUWs,
                };
                attribution.emplace_back(attr);
            }

            int64_t d_totalEnergyUWs = totalEnergyUWs - mTotalEnergySS;
            float powerScale = 0;
            if (totalRelativeEnergyUWs != 0) {
                powerScale = static_cast<float>(d_totalEnergyUWs) / totalRelativeEnergyUWs;
            }
            for (auto &attr : attribution) {
                attr.energyUWs = (int64_t)(attr.energyUWs * powerScale) +
                    (mUidEnergySS.count(attr.uid) ? mUidEnergySS.at(attr.uid) : 0);
                mUidEnergySS[attr.uid] = attr.energyUWs;
            }

            mUidTimeInStateSS = attrStats.uidTimeInStats;
            mTotalEnergySS = totalEnergyUWs;
        } else {
            std::vector<StateResidencyResult> results;
            if (mPowerStats->getStateResidency({mPowerEntityId}, &results).isOk()) {
                for (const auto &s : results[0].stateResidencyData) {
                    if (mCoefficients.count(s.id)) {
                        totalEnergyUWs += mCoefficients.at(s.id) * s.totalTimeInStateMs;
                    }
                }
            } else {
                LOG(ERROR) << "Failed to get state residency";
                return {};
            }
        }
    }

    return EnergyConsumerResult{.timestampMs = timestampMs,
                                .energyUWs = totalEnergyUWs,
                                .attribution = attribution};
}

std::string PowerStatsEnergyConsumer::getConsumerName() {
    return kName;
}

}  // namespace stats
}  // namespace power
}  // namespace hardware
}  // namespace android
}  // namespace aidl
