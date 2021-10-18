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

#include <dataproviders/IioEnergyMeterDataSelector.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <inttypes.h>

namespace aidl {
namespace android {
namespace hardware {
namespace power {
namespace stats {

void IioEnergyMeterDataSelector::parseConfigData(
        const std::string data,
        std::unordered_map<std::string, std::list<std::string>> *deviceConfigs) {
    std::string deviceName; /* Initialized empty */
    std::list<std::string> deviceConfig;

    /* Parse the configuration from the config file */
    std::istringstream input(data);
    std::string line;
    while (std::getline(input, line)) {
        /* Skip whitespace/tab lines */
        if (line.find_first_not_of("\t ") == std::string::npos) {
            continue;
        }

        /* Look for "[Device Name]" in the line */
        std::vector<std::string> words = ::android::base::Split(line, "][");
        if (words.size() == 3) {
            if (deviceName.empty() == false) {
                /* End of device config - store config in map */
                deviceConfigs->emplace(deviceName, deviceConfig);
                deviceConfig.clear();
            }

            deviceName = words[1];
        } else {
            if (deviceName.empty() == false) {
                /* We current have a device name set, store the line */
                deviceConfig.emplace_back(line);
            } /* Else skip line - no device name is yet associated with config line */
        }
    }

    /* End of file - store config in map */
    if (deviceName.empty() == false) {
        deviceConfigs->emplace(deviceName, deviceConfig);
    }
}

void IioEnergyMeterDataSelector::applyConfigToDevices(
        const std::unordered_map<std::string, std::list<std::string>> &deviceConfigs) {
    for (const auto &devicePathPair : kDevicePaths) {
        std::string devicePath = devicePathPair.first;
        std::string deviceName = devicePathPair.second;

        /* Check if a config exists for this device */
        if (deviceConfigs.count(deviceName) == 1) {
            LOG(INFO) << "Attempting to configure: " << deviceName;
            std::string nodePath = devicePath + kSelectionNode;
            std::list<std::string> config = deviceConfigs.at(deviceName);
            for (const auto &railConfig : config) {
                bool success = ::android::base::WriteStringToFile(railConfig, nodePath);
                if (!success) {
                    LOG(ERROR) << "Failed to write: " << railConfig << " to: " << nodePath;
                } else {
                    LOG(INFO) << "Wrote rail config: " << railConfig;
                }
            }
        }
    }
}

void IioEnergyMeterDataSelector::applyConfigsByAscendingPriority() {
    std::string data;

    /* Parsed in order of initialization. Thus, the last item will have highest priority */
    for (const auto &configPath : kConfigPaths) {
        if (!::android::base::ReadFileToString(configPath, &data)) {
            LOG(DEBUG) << "Could not parse rail config from " << configPath;
            continue;
        }

        /* key: device name, value: list of config lines */
        std::unordered_map<std::string, std::list<std::string>> deviceConfigs;
        parseConfigData(data, &deviceConfigs);
        applyConfigToDevices(deviceConfigs);
    }
}

/**
 * Sends configuration complete to the driver
 */
void IioEnergyMeterDataSelector::sendConfigurationComplete() {
    for (const auto &devicePathPair : kDevicePaths) {
        /* Sends configuration complete to the driver */
        std::string devicePath = devicePathPair.first;
        std::string nodePath = devicePath + kSelectionNode;
        bool success = ::android::base::WriteStringToFile(kSelectionComplete, nodePath);
        if (!success) {
            LOG(ERROR) << "Failed to write: " << kSelectionComplete << " to: " << nodePath;
        }
    }
}

IioEnergyMeterDataSelector::IioEnergyMeterDataSelector(
        const std::unordered_map<std::string, std::string> &devicePaths)
    : kDevicePaths(std::move(devicePaths)) {
    applyConfigsByAscendingPriority();
    sendConfigurationComplete();
}

}  // namespace stats
}  // namespace power
}  // namespace hardware
}  // namespace android
}  // namespace aidl
