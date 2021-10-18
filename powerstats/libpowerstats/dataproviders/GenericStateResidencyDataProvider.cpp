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

#include <android-base/logging.h>
#include <android-base/strings.h>

#include <dataproviders/GenericStateResidencyDataProvider.h>

namespace aidl {
namespace android {
namespace hardware {
namespace power {
namespace stats {

std::vector<GenericStateResidencyDataProvider::StateResidencyConfig>
generateGenericStateResidencyConfigs(
        const GenericStateResidencyDataProvider::StateResidencyConfig &stateConfig,
        const std::vector<std::pair<std::string, std::string>> &stateHeaders) {
    std::vector<GenericStateResidencyDataProvider::StateResidencyConfig> stateResidencyConfigs;
    stateResidencyConfigs.reserve(stateHeaders.size());
    for (auto h : stateHeaders) {
        GenericStateResidencyDataProvider::StateResidencyConfig cfg = {stateConfig};
        cfg.name = h.first;
        cfg.header = h.second;
        stateResidencyConfigs.emplace_back(cfg);
    }
    return stateResidencyConfigs;
}

static bool extractStat(const char *line, const std::string &prefix, uint64_t *stat) {
    char const *prefixStart = strstr(line, prefix.c_str());
    if (prefixStart == nullptr) {
        // Did not find the given prefix
        return false;
    }

    *stat = strtoull(prefixStart + prefix.length(), nullptr, 0);
    return true;
}

static bool parseState(StateResidency *data,
                       const GenericStateResidencyDataProvider::StateResidencyConfig &config,
                       FILE *fp, char **line, size_t *len) {
    size_t numFieldsRead = 0;
    const size_t numFields =
            config.entryCountSupported + config.totalTimeSupported + config.lastEntrySupported;

    while ((numFieldsRead < numFields) && (getline(line, len, fp) != -1)) {
        uint64_t stat = 0;
        // Attempt to extract data from the current line
        if (config.entryCountSupported && extractStat(*line, config.entryCountPrefix, &stat)) {
            data->totalStateEntryCount =
                    config.entryCountTransform ? config.entryCountTransform(stat) : stat;
            ++numFieldsRead;
        } else if (config.totalTimeSupported && extractStat(*line, config.totalTimePrefix, &stat)) {
            data->totalTimeInStateMs =
                    config.totalTimeTransform ? config.totalTimeTransform(stat) : stat;
            ++numFieldsRead;
        } else if (config.lastEntrySupported && extractStat(*line, config.lastEntryPrefix, &stat)) {
            data->lastEntryTimestampMs =
                    config.lastEntryTransform ? config.lastEntryTransform(stat) : stat;
            ++numFieldsRead;
        }
    }

    // End of file was reached and not all state data was parsed. Something
    // went wrong
    if (numFieldsRead != numFields) {
        LOG(ERROR) << "Failed to parse stats for " << config.name;
        return false;
    }

    return true;
}

template <class T, class Func>
static int32_t findNextIndex(const std::vector<T> &collection, FILE *fp, char **line, size_t *len,
                             Func pred) {
    // handling the case when there is no header to look for
    if (pred(collection[0], "")) {
        return 0;
    }

    while (getline(line, len, fp) != -1) {
        for (int32_t i = 0; i < collection.size(); ++i) {
            if (pred(collection[i], *line)) {
                return i;
            }
        }
    }

    return -1;
}

static bool getStateData(std::vector<StateResidency> *result,
                         const std::vector<GenericStateResidencyDataProvider::StateResidencyConfig>
                                 &stateResidencyConfigs,
                         FILE *fp, char **line, size_t *len) {
    size_t numStatesRead = 0;
    size_t numStates = stateResidencyConfigs.size();
    int32_t nextState = -1;
    auto pred = [](auto a, char const *b) {
        // return true if b matches the header contained in a, ignoring whitespace
        return (a.header == ::android::base::Trim(std::string(b)));
    };

    result->reserve(numStates);

    // Search for state headers until we have found them all or can't find anymore
    while ((numStatesRead < numStates) &&
           (nextState = findNextIndex<GenericStateResidencyDataProvider::StateResidencyConfig>(
                    stateResidencyConfigs, fp, line, len, pred)) >= 0) {
        // Found a matching state header. Parse the contents
        StateResidency data = {.id = nextState};
        if (parseState(&data, stateResidencyConfigs[nextState], fp, line, len)) {
            result->emplace_back(data);
            ++numStatesRead;
        } else {
            break;
        }
    }

    // There was a problem parsing and we failed to get data for all of the states
    if (numStatesRead != numStates) {
        return false;
    }

    return true;
}

bool GenericStateResidencyDataProvider::getStateResidencies(
        std::unordered_map<std::string, std::vector<StateResidency>> *residencies) {
    // Using FILE* instead of std::ifstream for performance reasons
    std::unique_ptr<FILE, decltype(&fclose)> fp(fopen(mPath.c_str(), "r"), fclose);
    if (!fp) {
        PLOG(ERROR) << "Failed to open file " << mPath;
        return false;
    }

    size_t len = 0;
    char *line = nullptr;
    size_t numEntitiesRead = 0;
    size_t numEntities = mPowerEntityConfigs.size();
    int32_t nextConfig = -1;
    auto pred = [](auto a, char const *b) {
        // return true if b matches the header contained in a, ignoring whitespace
        return (a.mHeader == ::android::base::Trim(std::string(b)));
    };

    // Search for entity headers until we have found them all or can't find anymore
    while ((numEntitiesRead < numEntities) &&
           (nextConfig = findNextIndex<decltype(mPowerEntityConfigs)::value_type>(
                    mPowerEntityConfigs, fp.get(), &line, &len, pred)) >= 0) {
        // Found a matching header. Retrieve its state data
        std::vector<StateResidency> result;
        if (getStateData(&result, mPowerEntityConfigs[nextConfig].mStateResidencyConfigs, fp.get(),
                         &line, &len)) {
            residencies->emplace(mPowerEntityConfigs[nextConfig].mName, result);
            ++numEntitiesRead;
        } else {
            break;
        }
    }

    free(line);

    // There was a problem gathering state residency data for one or more entities
    if (numEntitiesRead != numEntities) {
        LOG(ERROR) << "Failed to get results for " << mPath;
        return false;
    }

    return true;
}

std::unordered_map<std::string, std::vector<State>> GenericStateResidencyDataProvider::getInfo() {
    std::unordered_map<std::string, std::vector<State>> ret;
    for (const auto &entityConfig : mPowerEntityConfigs) {
        int32_t stateId = 0;
        std::vector<State> stateInfos;
        for (const auto &stateConfig : entityConfig.mStateResidencyConfigs) {
            State stateInfo = {.id = stateId++, .name = stateConfig.name};
            stateInfos.emplace_back(stateInfo);
        }

        ret.emplace(entityConfig.mName, stateInfos);
    }
    return ret;
}

}  // namespace stats
}  // namespace power
}  // namespace hardware
}  // namespace android
}  // namespace aidl
