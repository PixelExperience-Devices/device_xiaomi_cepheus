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

#include <unordered_map>

namespace aidl {
namespace android {
namespace hardware {
namespace power {
namespace stats {

class IioEnergyMeterDataProvider : public PowerStats::IEnergyMeterDataProvider {
  public:
    IioEnergyMeterDataProvider(const std::vector<const std::string> &deviceNames,
                               const bool useSelector = false);

    // Methods from PowerStats::IRailEnergyDataProvider
    ndk::ScopedAStatus readEnergyMeter(const std::vector<int32_t> &in_channelIds,
                                       std::vector<EnergyMeasurement> *_aidl_return) override;
    ndk::ScopedAStatus getEnergyMeterInfo(std::vector<Channel> *_aidl_return) override;

  private:
    void findIioEnergyMeterNodes();
    void parseEnabledRails();
    int parseEnergyValue(std::string path);
    int parseEnergyContents(const std::string &contents);

    std::mutex mLock;
    std::unordered_map<std::string, std::string> mDevicePaths;  // key: path, value: device name
    std::unordered_map<std::string, int32_t> mChannelIds;  // key: name, value: id
    std::vector<Channel> mChannelInfos;
    std::vector<EnergyMeasurement> mReading;

    const std::vector<const std::string> kDeviceNames;
    const std::string kDeviceType = "iio:device";
    const std::string kIioRootDir = "/sys/bus/iio/devices/";
    const std::string kNameNode = "/name";
    const std::string kEnabledRailsNode = "/enabled_rails";
    const std::string kEnergyValueNode = "/energy_value";
};

}  // namespace stats
}  // namespace power
}  // namespace hardware
}  // namespace android
}  // namespace aidl
