/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include <dataproviders/PixelStateResidencyDataProvider.h>

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android/binder_status.h>

namespace aidl {
namespace android {
namespace hardware {
namespace power {
namespace stats {

PixelStateResidencyDataProvider::PixelStateResidencyDataProvider()
    : mProviderService(ndk::SharedRefBase::make<ProviderService>(this)) {}

void PixelStateResidencyDataProvider::addEntity(std::string name, std::vector<State> states) {
    std::lock_guard<std::mutex> lock(mLock);

    mEntries.emplace_back(name, states);
}

void PixelStateResidencyDataProvider::start() {
    binder_status_t status =
            AServiceManager_addService(mProviderService->asBinder().get(), kInstance.c_str());
    if (status != STATUS_OK) {
        LOG(ERROR) << "Failed to start " << kInstance;
    }
}

::ndk::ScopedAStatus PixelStateResidencyDataProvider::getStateResidenciesTimed(
        const Entry &entry, std::vector<StateResidency> *residency) {
    const uint64_t MAX_LATENCY_US = 2000;

    if (!entry.mCallback) {
        LOG(ERROR) << "callback for " << entry.mName << " is not registered";
        return ndk::ScopedAStatus::fromStatus(STATUS_UNEXPECTED_NULL);
    }

    struct timespec then;
    struct timespec now;

    clock_gettime(CLOCK_BOOTTIME, &then);
    ::ndk::ScopedAStatus status = entry.mCallback->getStateResidency(residency);
    clock_gettime(CLOCK_BOOTTIME, &now);

    uint64_t timeElapsedUs =
            ((now.tv_sec - then.tv_sec) * 1000000) + ((now.tv_nsec - then.tv_nsec) / 1000);
    if (timeElapsedUs > MAX_LATENCY_US) {
        LOG(WARNING) << "getStateResidency latency for " << entry.mName
                     << " exceeded time allowed: " << timeElapsedUs << "us";
    }

    return status;
}

bool PixelStateResidencyDataProvider::getStateResidencies(
        std::unordered_map<std::string, std::vector<StateResidency>> *residencies) {
    std::lock_guard<std::mutex> lock(mLock);

    size_t numResultsFound = 0;
    size_t numResults = mEntries.size();
    for (auto &entry : mEntries) {
        std::vector<StateResidency> residency;
        ::ndk::ScopedAStatus status = getStateResidenciesTimed(entry, &residency);

        if (!status.isOk()) {
            LOG(ERROR) << "getStateResidency for " << entry.mName << " failed";

            if (status.getStatus() == STATUS_DEAD_OBJECT) {
                LOG(ERROR) << "Unregistering dead callback for " << entry.mName;
                entry.mCallback = nullptr;
            }
        }
        if (!residency.empty()) {
            residencies->emplace(entry.mName, residency);
            numResultsFound++;
        }
    }

    return (numResultsFound == numResults);
}

std::unordered_map<std::string, std::vector<State>> PixelStateResidencyDataProvider::getInfo() {
    std::lock_guard<std::mutex> lock(mLock);

    std::unordered_map<std::string, std::vector<State>> ret;
    for (const auto &entry : mEntries) {
        ret.emplace(entry.mName, entry.mStates);
    }

    return ret;
}

::ndk::ScopedAStatus PixelStateResidencyDataProvider::registerCallback(
        const std::string &in_entityName,
        const std::shared_ptr<IPixelStateResidencyCallback> &in_cb) {
    std::lock_guard<std::mutex> lock(mLock);

    if (!in_cb) {
        return ndk::ScopedAStatus::fromStatus(STATUS_UNEXPECTED_NULL);
    }

    auto toRegister =
            std::find_if(mEntries.begin(), mEntries.end(),
                         [&in_entityName](const auto &it) { return it.mName == in_entityName; });

    if (toRegister == mEntries.end()) {
        LOG(ERROR) << __func__ << " Invalid entityName: " << in_entityName;
        return ::ndk::ScopedAStatus::fromStatus(STATUS_BAD_VALUE);
    }

    toRegister->mCallback = in_cb;

    LOG(INFO) << __func__ << ": Registered " << in_entityName;
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus PixelStateResidencyDataProvider::unregisterCallback(
        const std::shared_ptr<IPixelStateResidencyCallback> &in_cb) {
    std::lock_guard<std::mutex> lock(mLock);

    if (!in_cb) {
        return ndk::ScopedAStatus::fromStatus(STATUS_UNEXPECTED_NULL);
    }

    auto toRemove = std::find_if(mEntries.begin(), mEntries.end(), [&in_cb](const auto &it) {
        return it.mCallback->asBinder().get() == in_cb->asBinder().get();
    });

    if (toRemove == mEntries.end()) {
        return ndk::ScopedAStatus::fromStatus(STATUS_BAD_VALUE);
    }

    toRemove->mCallback = nullptr;

    return ::ndk::ScopedAStatus::ok();
}

}  // namespace stats
}  // namespace power
}  // namespace hardware
}  // namespace android
}  // namespace aidl
