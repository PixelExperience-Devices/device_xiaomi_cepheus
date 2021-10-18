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

#include "include/PowerStatsAidl.h"
#include <aidl/android/hardware/power/stats/BnPowerStats.h>

#include <android-base/chrono_utils.h>
#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>

#include <inttypes.h>
#include <chrono>
#include <numeric>
#include <string>

namespace aidl {
namespace android {
namespace hardware {
namespace power {
namespace stats {

void PowerStats::addStateResidencyDataProvider(std::unique_ptr<IStateResidencyDataProvider> p) {
    if (!p) {
        return;
    }

    int32_t id = mPowerEntityInfos.size();
    auto info = p->getInfo();

    size_t index = mStateResidencyDataProviders.size();
    mStateResidencyDataProviders.emplace_back(std::move(p));

    for (const auto &[entityName, states] : info) {
        PowerEntity i = {
                .id = id++,
                .name = entityName,
                .states = states,
        };
        mPowerEntityInfos.emplace_back(i);
        mStateResidencyDataProviderIndex.emplace_back(index);
    }
}

ndk::ScopedAStatus PowerStats::getPowerEntityInfo(std::vector<PowerEntity> *_aidl_return) {
    *_aidl_return = mPowerEntityInfos;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PowerStats::getStateResidency(const std::vector<int32_t> &in_powerEntityIds,
                                                 std::vector<StateResidencyResult> *_aidl_return) {
    if (mPowerEntityInfos.empty()) {
        return ndk::ScopedAStatus::ok();
    }

    // If in_powerEntityIds is empty then return data for all supported entities
    if (in_powerEntityIds.empty()) {
        std::vector<int32_t> v(mPowerEntityInfos.size());
        std::iota(std::begin(v), std::end(v), 0);
        return getStateResidency(v, _aidl_return);
    }

    std::unordered_map<std::string, std::vector<StateResidency>> stateResidencies;

    for (const int32_t id : in_powerEntityIds) {
        // check for invalid ids
        if (id < 0 || id >= mPowerEntityInfos.size()) {
            return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
        }

        // Check to see if we already have data for the given id
        std::string powerEntityName = mPowerEntityInfos[id].name;
        if (stateResidencies.find(powerEntityName) == stateResidencies.end()) {
            mStateResidencyDataProviders.at(mStateResidencyDataProviderIndex.at(id))
                    ->getStateResidencies(&stateResidencies);
        }

        // Append results if we have them
        auto stateResidency = stateResidencies.find(powerEntityName);
        if (stateResidency != stateResidencies.end()) {
            StateResidencyResult res = {
                    .id = id,
                    .stateResidencyData = stateResidency->second,
            };
            _aidl_return->emplace_back(res);
        } else {
            // Failed to get results for the given id.
            LOG(ERROR) << "Failed to get results for " << powerEntityName;
        }
    }

    return ndk::ScopedAStatus::ok();
}

void PowerStats::addEnergyConsumer(std::unique_ptr<IEnergyConsumer> p) {
    if (!p) {
        return;
    }

    std::pair<EnergyConsumerType, std::string> info = p->getInfo();

    int32_t count = count_if(mEnergyConsumerInfos.begin(), mEnergyConsumerInfos.end(),
                             [&info](const EnergyConsumer &c) { return info.first == c.type; });
    int32_t id = mEnergyConsumers.size();
    mEnergyConsumerInfos.emplace_back(
            EnergyConsumer{.id = id, .ordinal = count, .type = info.first, .name = info.second});
    mEnergyConsumers.emplace_back(std::move(p));
}

ndk::ScopedAStatus PowerStats::getEnergyConsumerInfo(std::vector<EnergyConsumer> *_aidl_return) {
    *_aidl_return = mEnergyConsumerInfos;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PowerStats::getEnergyConsumed(const std::vector<int32_t> &in_energyConsumerIds,
                                                 std::vector<EnergyConsumerResult> *_aidl_return) {
    if (mEnergyConsumers.empty()) {
        return ndk::ScopedAStatus::ok();
    }

    // If in_powerEntityIds is empty then return data for all supported energy consumers
    if (in_energyConsumerIds.empty()) {
        std::vector<int32_t> v(mEnergyConsumerInfos.size());
        std::iota(std::begin(v), std::end(v), 0);
        return getEnergyConsumed(v, _aidl_return);
    }

    for (const auto id : in_energyConsumerIds) {
        // check for invalid ids
        if (id < 0 || id >= mEnergyConsumers.size()) {
            return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
        }

        auto resopt = mEnergyConsumers[id]->getEnergyConsumed();
        if (resopt) {
            EnergyConsumerResult res = resopt.value();
            res.id = id;
            _aidl_return->emplace_back(res);
        } else {
            // Failed to get results for the given id.
            LOG(ERROR) << "Failed to get results for " << mEnergyConsumerInfos[id].name;
        }
    }

    return ndk::ScopedAStatus::ok();
}

void PowerStats::setEnergyMeterDataProvider(std::unique_ptr<IEnergyMeterDataProvider> p) {
    mEnergyMeterDataProvider = std::move(p);
}

ndk::ScopedAStatus PowerStats::getEnergyMeterInfo(std::vector<Channel> *_aidl_return) {
    if (!mEnergyMeterDataProvider) {
        return ndk::ScopedAStatus::ok();
    }
    return mEnergyMeterDataProvider->getEnergyMeterInfo(_aidl_return);
}

ndk::ScopedAStatus PowerStats::readEnergyMeter(const std::vector<int32_t> &in_channelIds,
                                               std::vector<EnergyMeasurement> *_aidl_return) {
    if (!mEnergyMeterDataProvider) {
        return ndk::ScopedAStatus::ok();
    }
    return mEnergyMeterDataProvider->readEnergyMeter(in_channelIds, _aidl_return);
}

void PowerStats::getEntityStateNames(
        std::unordered_map<int32_t, std::string> *entityNames,
        std::unordered_map<int32_t, std::unordered_map<int32_t, std::string>> *stateNames) {
    std::vector<PowerEntity> infos;
    getPowerEntityInfo(&infos);

    for (const auto &info : infos) {
        entityNames->emplace(info.id, info.name);
        stateNames->emplace(info.id, std::unordered_map<int32_t, std::string>());
        auto &entityStateNames = stateNames->at(info.id);
        for (const auto &state : info.states) {
            entityStateNames.emplace(state.id, state.name);
        }
    }
}

void PowerStats::getChannelNames(std::unordered_map<int32_t, std::string> *channelNames) {
    std::vector<Channel> infos;
    getEnergyMeterInfo(&infos);

    for (const auto &info : infos) {
        channelNames->emplace(info.id, "[" + info.name + "]:" + info.subsystem);
    }
}

void PowerStats::dumpEnergyMeter(std::ostringstream &oss, bool delta) {
    const char *headerFormat = "  %32s   %18s\n";
    const char *dataFormat = "  %32s   %14.2f mWs\n";
    const char *headerFormatDelta = "  %32s   %18s (%14s)\n";
    const char *dataFormatDelta = "  %32s   %14.2f mWs (%14.2f)\n";

    std::unordered_map<int32_t, std::string> channelNames;
    getChannelNames(&channelNames);

    oss << "\n============= PowerStats HAL 2.0 energy meter ==============\n";

    std::vector<EnergyMeasurement> energyData;
    readEnergyMeter({}, &energyData);

    if (delta) {
        static std::vector<EnergyMeasurement> prevEnergyData;
        ::android::base::boot_clock::time_point curTime = ::android::base::boot_clock::now();
        static ::android::base::boot_clock::time_point prevTime = curTime;

        oss << "Elapsed time: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(curTime - prevTime).count()
            << " ms\n";

        oss << ::android::base::StringPrintf(headerFormatDelta, "Channel", "Cumulative Energy",
                                             "Delta   ");

        std::unordered_map<int32_t, int64_t> prevEnergyDataMap;
        for (const auto &data : prevEnergyData) {
            prevEnergyDataMap.emplace(data.id, data.energyUWs);
        }

        for (const auto &data : energyData) {
            auto prevEnergyDataIt = prevEnergyDataMap.find(data.id);
            int64_t deltaEnergy = 0;
            if (prevEnergyDataIt != prevEnergyDataMap.end()) {
                deltaEnergy = data.energyUWs - prevEnergyDataIt->second;
            }

            oss << ::android::base::StringPrintf(dataFormatDelta, channelNames.at(data.id).c_str(),
                                                 static_cast<float>(data.energyUWs) / 1000.0,
                                                 static_cast<float>(deltaEnergy) / 1000.0);
        }

        prevEnergyData = energyData;
        prevTime = curTime;
    } else {
        oss << ::android::base::StringPrintf(headerFormat, "Channel", "Cumulative Energy");

        for (const auto &data : energyData) {
            oss << ::android::base::StringPrintf(dataFormat, channelNames.at(data.id).c_str(),
                                                 static_cast<float>(data.energyUWs) / 1000.0);
        }
    }

    oss << "========== End of PowerStats HAL 2.0 energy meter ==========\n";
}

void PowerStats::dumpStateResidency(std::ostringstream &oss, bool delta) {
    const char *headerFormat = "  %16s   %18s   %16s   %15s   %17s\n";
    const char *dataFormat =
            "  %16s   %18s   %13" PRIu64 " ms   %15" PRIu64 "   %14" PRIu64 " ms\n";
    const char *headerFormatDelta = "  %16s   %18s   %16s (%14s)   %15s (%16s)   %17s (%14s)\n";
    const char *dataFormatDelta = "  %16s   %18s   %13" PRIu64 " ms (%14" PRId64 ")   %15" PRIu64
                                  " (%16" PRId64 ")   %14" PRIu64 " ms (%14" PRId64 ")\n";

    // Construct maps to entity and state names
    std::unordered_map<int32_t, std::string> entityNames;
    std::unordered_map<int32_t, std::unordered_map<int32_t, std::string>> stateNames;
    getEntityStateNames(&entityNames, &stateNames);

    oss << "\n============= PowerStats HAL 2.0 state residencies ==============\n";

    std::vector<StateResidencyResult> results;
    getStateResidency({}, &results);

    if (delta) {
        static std::vector<StateResidencyResult> prevResults;
        ::android::base::boot_clock::time_point curTime = ::android::base::boot_clock::now();
        static ::android::base::boot_clock::time_point prevTime = curTime;

        oss << "Elapsed time: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(curTime - prevTime).count()
            << " ms\n";

        oss << ::android::base::StringPrintf(headerFormatDelta, "Entity", "State", "Total time",
                                             "Delta   ", "Total entries", "Delta   ",
                                             "Last entry tstamp", "Delta ");

        // Process prevResults into a 2-tier lookup table for easy reference
        std::unordered_map<int32_t, std::unordered_map<int32_t, StateResidency>> prevResultsMap;
        for (const auto &prevResult : prevResults) {
            prevResultsMap.emplace(prevResult.id, std::unordered_map<int32_t, StateResidency>());
            for (auto stateResidency : prevResult.stateResidencyData) {
                prevResultsMap.at(prevResult.id).emplace(stateResidency.id, stateResidency);
            }
        }

        // Iterate over the new result data (one "result" per entity)
        for (const auto &result : results) {
            const char *entityName = entityNames.at(result.id).c_str();

            // Look up previous result data for the same entity
            auto prevEntityResultIt = prevResultsMap.find(result.id);

            // Iterate over individual states within the current entity's new result
            for (const auto &stateResidency : result.stateResidencyData) {
                const char *stateName = stateNames.at(result.id).at(stateResidency.id).c_str();

                // If a previous result was found for the same entity, see if that
                // result also contains data for the current state
                int64_t deltaTotalTime = 0;
                int64_t deltaTotalCount = 0;
                int64_t deltaTimestamp = 0;
                if (prevEntityResultIt != prevResultsMap.end()) {
                    auto prevStateResidencyIt = prevEntityResultIt->second.find(stateResidency.id);
                    // If a previous result was found for the current entity and state, calculate
                    // the deltas and display them along with new result
                    if (prevStateResidencyIt != prevEntityResultIt->second.end()) {
                        deltaTotalTime = stateResidency.totalTimeInStateMs -
                                         prevStateResidencyIt->second.totalTimeInStateMs;
                        deltaTotalCount = stateResidency.totalStateEntryCount -
                                          prevStateResidencyIt->second.totalStateEntryCount;
                        deltaTimestamp = stateResidency.lastEntryTimestampMs -
                                         prevStateResidencyIt->second.lastEntryTimestampMs;
                    }
                }

                oss << ::android::base::StringPrintf(
                        dataFormatDelta, entityName, stateName, stateResidency.totalTimeInStateMs,
                        deltaTotalTime, stateResidency.totalStateEntryCount, deltaTotalCount,
                        stateResidency.lastEntryTimestampMs, deltaTimestamp);
            }
        }

        prevResults = results;
        prevTime = curTime;
    } else {
        oss << ::android::base::StringPrintf(headerFormat, "Entity", "State", "Total time",
                                             "Total entries", "Last entry tstamp");
        for (const auto &result : results) {
            for (const auto &stateResidency : result.stateResidencyData) {
                oss << ::android::base::StringPrintf(
                        dataFormat, entityNames.at(result.id).c_str(),
                        stateNames.at(result.id).at(stateResidency.id).c_str(),
                        stateResidency.totalTimeInStateMs, stateResidency.totalStateEntryCount,
                        stateResidency.lastEntryTimestampMs);
            }
        }
    }

    oss << "========== End of PowerStats HAL 2.0 state residencies ==========\n";
}

void PowerStats::dumpEnergyConsumer(std::ostringstream &oss, bool delta) {
    (void)delta;

    std::vector<EnergyConsumerResult> results;
    getEnergyConsumed({}, &results);

    oss << "\n============= PowerStats HAL 2.0 energy consumers ==============\n";

    for (const auto &result : results) {
        oss << ::android::base::StringPrintf("%-12s : %14.2f mWs\n",
                                             mEnergyConsumers[result.id]->getConsumerName().c_str(),
                                             static_cast<float>(result.energyUWs) / 1000.0);
        for (auto &attr : result.attribution) {
            oss << ::android::base::StringPrintf("  %10d - %14.2f mWs\n", attr.uid,
                                                 static_cast<float>(attr.energyUWs) / 1000.0);
        }
    }

    oss << "========== End of PowerStats HAL 2.0 energy consumers ==========\n";
}

binder_status_t PowerStats::dump(int fd, const char **args, uint32_t numArgs) {
    std::ostringstream oss;
    bool delta = (numArgs == 1) && (std::string(args[0]) == "delta");

    // Generate debug output for state residency
    dumpStateResidency(oss, delta);

    // Generate debug output for energy consumer
    dumpEnergyConsumer(oss, delta);

    // Generate debug output energy meter
    dumpEnergyMeter(oss, delta);

    ::android::base::WriteStringToFd(oss.str(), fd);
    fsync(fd);
    return STATUS_OK;
}

}  // namespace stats
}  // namespace power
}  // namespace hardware
}  // namespace android
}  // namespace aidl
