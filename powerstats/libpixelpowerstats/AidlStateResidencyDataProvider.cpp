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
#define LOG_TAG "libpixelpowerstats"

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <pixelpowerstats/AidlStateResidencyDataProvider.h>
#include <time.h>
using android::base::StringPrintf;

static const uint64_t MAX_LATENCY_US = 2000;

namespace android {
namespace hardware {
namespace google {
namespace pixel {
namespace powerstats {

void AidlStateResidencyDataProvider::addEntity(uint32_t id, std::string entityName,
                                               std::vector<std::string> stateNames) {
    std::lock_guard<std::mutex> lock(mLock);
    // Create a new entry in the map of power entities
    mEntityInfos.emplace(entityName, StateSpace{.powerEntityId = id, .stateInfos = {}});

    // Create an entry for each state and assign an Id.
    uint32_t stateId = 0;
    auto &stateInfos = mEntityInfos.at(entityName).stateInfos;
    for (auto stateName : stateNames) {
        stateInfos.emplace(stateName, stateId++);
    }
}

binderStatus AidlStateResidencyDataProvider::unregisterCallbackInternal(
        const sp<IBinder> &callback) {
    if (callback == nullptr) {
        // Callback pointer is null. Return an error.
        return binderStatus::fromExceptionCode(binderStatus::EX_NULL_POINTER, "callback is null");
    }

    bool removed = false;
    std::lock_guard<std::mutex> lock(mLock);

    // Iterate over collection of callbacks and remove the one that matches
    for (auto it = mCallbacks.begin(); it != mCallbacks.end();) {
        if (asBinder(it->second) == callback) {
            LOG(INFO) << "Unregistering callback for " << it->first;
            it = mCallbacks.erase(it);
            removed = true;
        } else {
            it++;
        }
    }
    (void)callback->unlinkToDeath(this);  // ignore errors
    return removed ? binderStatus::ok()
                   : binderStatus::fromExceptionCode(binderStatus::EX_ILLEGAL_ARGUMENT,
                            "callback not found");
}

void AidlStateResidencyDataProvider::binderDied(const wp<IBinder> &who) {
    binderStatus status = unregisterCallbackInternal(who.promote());
    if (!status.isOk()) {
        LOG(ERROR) << __func__ << "failed to unregister callback " << status.toString8();
    }
}

binderStatus AidlStateResidencyDataProvider::unregisterCallback(
        const sp<IPixelPowerStatsCallback> &callback) {
    return unregisterCallbackInternal(asBinder(callback));
}

binderStatus AidlStateResidencyDataProvider::registerCallback(
        const std::string &entityName, const sp<IPixelPowerStatsCallback> &callback) {
    LOG(INFO) << "Registering callback for " << entityName;
    if (callback == nullptr) {
        // Callback pointer is null. Return an error.
        LOG(ERROR) << __func__ << ": "
                   << "Invalid callback. Callback is null";
        return binderStatus::fromExceptionCode(
                binderStatus::EX_NULL_POINTER, "Invalid callback. Callback is null");
    }

    std::lock_guard<std::mutex> lock(mLock);
    if (mEntityInfos.find(entityName) == mEntityInfos.end()) {
        // Could not find the entity associated with this callback. Return an error.
        LOG(ERROR) << __func__ << ": "
                   << "Invalid entity";
        return binderStatus::fromExceptionCode(binderStatus::EX_ILLEGAL_ARGUMENT, "Invalid entity");
    }

    mCallbacks.emplace(entityName, callback);

    // death recipient
    auto linkRet = asBinder(callback)->linkToDeath(this, 0u /* cookie */);
    if (linkRet != android::OK) {
        LOG(WARNING) << __func__ << "Cannot link to death: " << linkRet;
        // ignore the error
    }

    return binderStatus::ok();
}

static binderStatus getStatsTimed(
        const std::pair<std::string, sp<IPixelPowerStatsCallback>> &cb,
        std::vector<android::vendor::powerstats::StateResidencyData> &stats) {
    struct timespec then;
    struct timespec now;

    clock_gettime(CLOCK_BOOTTIME, &then);
    binderStatus status = cb.second->getStats(&stats);
    clock_gettime(CLOCK_BOOTTIME, &now);

    uint64_t time_elapsed_us =
            ((now.tv_sec - then.tv_sec) * 1000000) + ((now.tv_nsec - then.tv_nsec) / 1000);
    if (time_elapsed_us > MAX_LATENCY_US) {
        LOG(WARNING) << "getStats for " << cb.first << " exceeded time allowed: " << time_elapsed_us
                     << "us";
    }
    return status;
}

bool AidlStateResidencyDataProvider::buildResult(
        std::string entityName,
        const std::vector<android::vendor::powerstats::StateResidencyData> &stats,
        PowerEntityStateResidencyResult &result) {
    auto infosEntry = mEntityInfos.find(entityName);
    if (infosEntry == mEntityInfos.end()) {
        LOG(ERROR) << __func__ << " failed: " << entityName << " is not registered.";
        return false;
    }
    auto stateSpace = infosEntry->second;
    result.powerEntityId = stateSpace.powerEntityId;
    size_t numStates = stateSpace.stateInfos.size();
    result.stateResidencyData.resize(numStates);
    size_t numStatesFound = 0;
    for (auto stat = stats.begin(); (numStatesFound < numStates) && (stat != stats.end()); stat++) {
        auto stateInfosEntry = stateSpace.stateInfos.find(stat->state);

        if (stateInfosEntry != stateSpace.stateInfos.end()) {
            PowerEntityStateResidencyData &data = result.stateResidencyData[numStatesFound++];
            data.powerEntityStateId = stateInfosEntry->second;
            data.totalTimeInStateMs = static_cast<uint64_t>(stat->totalTimeInStateMs);
            data.totalStateEntryCount = static_cast<uint64_t>(stat->totalStateEntryCount);
            data.lastEntryTimestampMs = static_cast<uint64_t>(stat->lastEntryTimestampMs);
        } else {
            LOG(WARNING) << "getStats for " << entityName << " returned data for unknown state "
                         << stat->state;
        }
    }

    return (numStatesFound == numStates);
}

bool AidlStateResidencyDataProvider::getResults(
        std::unordered_map<uint32_t, PowerEntityStateResidencyResult> &results) {
    std::lock_guard<std::mutex> lock(mLock);
    // TODO (b/126260512): return cached results if time elapsed isn't large
    size_t numResultsFound = 0;
    size_t numResults = mEntityInfos.size();
    for (auto cb : mCallbacks) {
        std::vector<android::vendor::powerstats::StateResidencyData> stats;

        // Get stats for the current callback
        binderStatus status = getStatsTimed(cb, stats);
        if (!status.isOk()) {
            LOG(ERROR) << "getStats for " << cb.first << " failed: " << status.toString8();
        }

        PowerEntityStateResidencyResult result;
        if (buildResult(cb.first, stats, result)) {
            results.emplace(result.powerEntityId, result);
            numResultsFound++;
        } else {
            LOG(ERROR) << "State residency data missing for " << cb.first;
        }
    }
    bool ret = (numResultsFound == numResults);

    // TODO (b/126260512): Cache results of the call, the return value, and the timestamp.
    return ret;
}

std::vector<PowerEntityStateSpace> AidlStateResidencyDataProvider::getStateSpaces() {
    std::lock_guard<std::mutex> lock(mLock);
    std::vector<PowerEntityStateSpace> stateSpaces;
    stateSpaces.reserve(mEntityInfos.size());

    // Return state space information for every entity for which this is configured to provide data
    for (auto info : mEntityInfos) {
        PowerEntityStateSpace statespace = {
                .powerEntityId = info.second.powerEntityId, .states = {}};
        statespace.states.resize(info.second.stateInfos.size());
        size_t i = 0;
        for (auto state : info.second.stateInfos) {
            statespace.states[i++] = {
                    .powerEntityStateId = state.second, .powerEntityStateName = state.first};
        }
        stateSpaces.emplace_back(statespace);
    }
    return stateSpaces;
}

}  // namespace powerstats
}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
