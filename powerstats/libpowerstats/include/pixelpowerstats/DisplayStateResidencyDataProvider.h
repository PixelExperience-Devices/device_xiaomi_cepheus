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

#ifndef HARDWARE_GOOGLE_PIXEL_POWERSTATS_DISPLAYSTATERESIDENCYDATAPROVIDER_H
#define HARDWARE_GOOGLE_PIXEL_POWERSTATS_DISPLAYSTATERESIDENCYDATAPROVIDER_H
// TODO(b/167628903): Delete this file
#include <pixelpowerstats/PowerStats.h>
#include <utils/Looper.h>
#include <utils/Thread.h>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace android {
namespace hardware {
namespace google {
namespace pixel {
namespace powerstats {

class DisplayStateResidencyDataProvider : public IStateResidencyDataProvider {
  public:
    // id = powerEntityId to be associated with this data provider
    // path = path to the display state file descriptor
    // state = list of states to be tracked
    DisplayStateResidencyDataProvider(uint32_t id, std::string path,
                                      std::vector<std::string> states);
    ~DisplayStateResidencyDataProvider();
    bool getResults(
            std::unordered_map<uint32_t, PowerEntityStateResidencyResult> &results) override;
    std::vector<PowerEntityStateSpace> getStateSpaces() override;

  private:
    // Poll for display state changes
    void pollLoop();
    // Main function to update the stats when display state change is detected
    void updateStats();

    // File descriptor of display state
    int mFd;
    // Path to display state file descriptor
    const std::string mPath;
    // powerEntityId associated with this data provider
    const uint32_t mPowerEntityId;
    // List of states to track indexed by mCurState
    std::vector<std::string> mStates;
    // Lock to protect concurrent read/write to mResidencies and mCurState
    std::mutex mLock;
    // Accumulated display state stats indexed by mCurState
    std::vector<PowerEntityStateResidencyData> mResidencies;
    // Index of current state
    int mCurState;
    // Looper to facilitate polling of display state file desciptor
    sp<Looper> mLooper;

    std::thread mThread;
};

}  // namespace powerstats
}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_PIXEL_POWERSTATS_DISPLAYSTATERESIDENCYDATAPROVIDER_H
