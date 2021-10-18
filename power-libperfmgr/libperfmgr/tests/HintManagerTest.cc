/*
 * Copyright (C) 2017 The Android Open Source Project
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
 * See the License for the specic language governing permissions and
 * limitations under the License.
 */

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <thread>

#include "perfmgr/FileNode.h"
#include "perfmgr/HintManager.h"
#include "perfmgr/PropertyNode.h"

namespace android {
namespace perfmgr {

using std::literals::chrono_literals::operator""ms;

constexpr auto kSLEEP_TOLERANCE_MS = 50ms;

constexpr char kJSON_RAW[] = R"(
{
    "Nodes": [
        {
            "Name": "CPUCluster0MinFreq",
            "Path": "/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq",
            "Values": [
                "1512000",
                "1134000",
                "384000"
            ],
            "DefaultIndex": 2,
            "ResetOnInit": true
        },
        {
            "Name": "CPUCluster1MinFreq",
            "Path": "/sys/devices/system/cpu/cpu4/cpufreq/scaling_min_freq",
            "Values": [
                "1512000",
                "1134000",
                "384000"
            ],
            "HoldFd": true
        },
        {
            "Name": "ModeProperty",
            "Path": "vendor.pwhal.mode",
            "Values": [
                "HIGH",
                "LOW",
                "NONE"
            ],
            "Type": "Property"
        }
    ],
    "Actions": [
        {
            "PowerHint": "INTERACTION",
            "Node": "CPUCluster1MinFreq",
            "Value": "1134000",
            "Duration": 800
        },
        {
            "PowerHint": "INTERACTION",
            "Node": "ModeProperty",
            "Value": "LOW",
            "Duration": 800
        },
        {
            "PowerHint": "LAUNCH",
            "Node": "CPUCluster0MinFreq",
            "Value": "1134000",
            "Duration": 500
        },
        {
            "PowerHint": "LAUNCH",
            "Node": "ModeProperty",
            "Value": "HIGH",
            "Duration": 500
        },
        {
            "PowerHint": "LAUNCH",
            "Node": "CPUCluster1MinFreq",
            "Value": "1512000",
            "Duration": 2000
        },
        {
            "PowerHint": "MASK_LAUNCH_MODE",
            "Type": "MaskHint",
            "Value": "LAUNCH"
        },
        {
            "PowerHint": "END_LAUNCH_MODE",
            "Type": "EndHint",
            "Value": "LAUNCH"
        },
        {
            "PowerHint": "DO_LAUNCH_MODE",
            "Type": "DoHint",
            "Value": "LAUNCH"
        }
    ]
}
)";

class HintManagerTest : public ::testing::Test, public HintManager {
  protected:
    HintManagerTest() : HintManager(nullptr, std::unordered_map<std::string, Hint>{}) {
        android::base::SetMinimumLogSeverity(android::base::VERBOSE);
        prop_ = "vendor.pwhal.mode";
    }

    virtual void SetUp() {
        // Set up 3 dummy nodes
        std::unique_ptr<TemporaryFile> tf = std::make_unique<TemporaryFile>();
        nodes_.emplace_back(new FileNode(
            "n0", tf->path, {{"n0_value0"}, {"n0_value1"}, {"n0_value2"}}, 2,
            false));
        files_.emplace_back(std::move(tf));
        tf = std::make_unique<TemporaryFile>();
        nodes_.emplace_back(new FileNode(
            "n1", tf->path, {{"n1_value0"}, {"n1_value1"}, {"n1_value2"}}, 2,
            true));
        files_.emplace_back(std::move(tf));
        nodes_.emplace_back(new PropertyNode(
            "n2", prop_, {{"n2_value0"}, {"n2_value1"}, {"n2_value2"}}, 2,
            true));
        nm_ = new NodeLooperThread(std::move(nodes_));
        // Set up dummy actions
        // "INTERACTION"
        // Node0, value1, 800ms
        // Node1, value1, forever
        // Node2, value1, 800ms
        // "LAUNCH"
        // Node0, value0, forever
        // Node1, value0, 400ms
        // Node2, value0, 400ms
        actions_["INTERACTION"].node_actions =
                std::vector<NodeAction>{{0, 1, 800ms}, {1, 1, 0ms}, {2, 1, 800ms}};
        actions_["LAUNCH"].node_actions =
                std::vector<NodeAction>{{0, 0, 0ms}, {1, 0, 400ms}, {2, 0, 400ms}};

        // Prepare dummy files to replace the nodes' path in example json_doc
        files_.emplace_back(std::make_unique<TemporaryFile>());
        files_.emplace_back(std::make_unique<TemporaryFile>());
        // replace file path
        json_doc_ = kJSON_RAW;
        std::string from =
            "/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq";
        size_t start_pos = json_doc_.find(from);
        json_doc_.replace(start_pos, from.length(), files_[0 + 2]->path);
        from = "/sys/devices/system/cpu/cpu4/cpufreq/scaling_min_freq";
        start_pos = json_doc_.find(from);
        json_doc_.replace(start_pos, from.length(), files_[1 + 2]->path);
        EXPECT_TRUE(android::base::SetProperty(prop_, ""))
            << "failed to clear property";
    }

    virtual void TearDown() {
        actions_.clear();
        nodes_.clear();
        files_.clear();
        nm_ = nullptr;
    }
    sp<NodeLooperThread> nm_;
    std::unordered_map<std::string, Hint> actions_;
    std::vector<std::unique_ptr<Node>> nodes_;
    std::vector<std::unique_ptr<TemporaryFile>> files_;
    std::string json_doc_;
    std::string prop_;
};

static inline void _VerifyPropertyValue(const std::string& path,
                                        const std::string& value) {
    std::string s = android::base::GetProperty(path, "");
    EXPECT_EQ(value, s);
}

static inline void _VerifyPathValue(const std::string& path,
                                    const std::string& value) {
    std::string s;
    EXPECT_TRUE(android::base::ReadFileToString(path, &s)) << strerror(errno);
    EXPECT_EQ(value, s);
}

static inline void _VerifyStats(const HintStats &stats, uint32_t count, uint64_t duration_min,
                                uint64_t duration_max) {
    EXPECT_EQ(stats.count, count);
    EXPECT_GE(stats.duration_ms, duration_min);
    EXPECT_LT(stats.duration_ms, duration_max);
}

// Test GetHints
TEST_F(HintManagerTest, GetHintsTest) {
    HintManager hm(nm_, actions_);
    EXPECT_TRUE(hm.Start());
    std::vector<std::string> hints = hm.GetHints();
    EXPECT_TRUE(hm.IsRunning());
    EXPECT_EQ(2u, hints.size());
    EXPECT_NE(std::find(hints.begin(), hints.end(), "INTERACTION"), hints.end());
    EXPECT_NE(std::find(hints.begin(), hints.end(), "LAUNCH"), hints.end());
}

// Test GetHintStats
TEST_F(HintManagerTest, GetHintStatsTest) {
    auto hm = std::make_unique<HintManager>(nm_, actions_);
    EXPECT_TRUE(InitHintStatus(hm));
    EXPECT_TRUE(hm->Start());
    HintStats launch_stats(hm->GetHintStats("LAUNCH"));
    EXPECT_EQ(0, launch_stats.count);
    EXPECT_EQ(0, launch_stats.duration_ms);
    HintStats interaction_stats(hm->GetHintStats("INTERACTION"));
    EXPECT_EQ(0, interaction_stats.count);
    EXPECT_EQ(0, interaction_stats.duration_ms);
}

// Test initialization of default values
TEST_F(HintManagerTest, HintInitDefaultTest) {
    HintManager hm(nm_, actions_);
    EXPECT_TRUE(hm.Start());
    std::this_thread::sleep_for(kSLEEP_TOLERANCE_MS);
    EXPECT_TRUE(hm.IsRunning());
    _VerifyPathValue(files_[0]->path, "");
    _VerifyPathValue(files_[1]->path, "n1_value2");
    _VerifyPropertyValue(prop_, "n2_value2");
}

// Test IsHintSupported
TEST_F(HintManagerTest, HintSupportedTest) {
    HintManager hm(nm_, actions_);
    EXPECT_TRUE(hm.IsHintSupported("INTERACTION"));
    EXPECT_TRUE(hm.IsHintSupported("LAUNCH"));
    EXPECT_FALSE(hm.IsHintSupported("NO_SUCH_HINT"));
}

// Test DumpToFd
TEST_F(HintManagerTest, DumpToFdTest) {
    auto hm = std::make_unique<HintManager>(nm_, actions_);
    EXPECT_TRUE(InitHintStatus(hm));
    TemporaryFile dumptf;
    hm->DumpToFd(dumptf.fd);
    fsync(dumptf.fd);
    std::ostringstream dump_buf;
    dump_buf << "========== Begin perfmgr nodes ==========\nNode Name\tNode "
                "Path\tCurrent Index\tCurrent Value\nn0\t"
             << files_[0]->path << "\t2\t\nn1\t" << files_[1]->path
             << "\t2\t\nn2\tvendor.pwhal.mode\t2\t\n==========  End perfmgr "
                "nodes  ==========\n========== Begin perfmgr stats ==========\n"
                "Hint Name\tCounts\tDuration\nINTERACTION\t0\t0\nLAUNCH\t0\t0\n"
                "==========  End perfmgr stats  ==========\n";
    _VerifyPathValue(dumptf.path, dump_buf.str());
    TemporaryFile dumptf_started;
    EXPECT_TRUE(hm->Start());
    std::this_thread::sleep_for(kSLEEP_TOLERANCE_MS);
    EXPECT_TRUE(hm->IsRunning());
    hm->DumpToFd(dumptf_started.fd);
    fsync(dumptf_started.fd);
    dump_buf.str("");
    dump_buf.clear();
    dump_buf << "========== Begin perfmgr nodes ==========\nNode Name\tNode "
                "Path\tCurrent Index\tCurrent Value\nn0\t"
             << files_[0]->path << "\t2\t\nn1\t" << files_[1]->path
             << "\t2\tn1_value2\nn2\tvendor.pwhal.mode\t2\tn2_value2\n========="
                "=  End perfmgr nodes  ==========\n========== Begin perfmgr "
                "stats ==========\nHint Name\tCounts\tDuration\nINTERACTION\t0\t"
                "0\nLAUNCH\t0\t0\n==========  End perfmgr stats  ==========\n";
    _VerifyPathValue(dumptf_started.path, dump_buf.str());
}

// Test hint/cancel/expire with dummy actions
TEST_F(HintManagerTest, HintTest) {
    auto hm = std::make_unique<HintManager>(nm_, actions_);
    EXPECT_TRUE(InitHintStatus(hm));
    EXPECT_TRUE(hm->Start());
    EXPECT_TRUE(hm->IsRunning());
    EXPECT_TRUE(hm->DoHint("INTERACTION"));
    std::this_thread::sleep_for(kSLEEP_TOLERANCE_MS);
    _VerifyPathValue(files_[0]->path, "n0_value1");
    _VerifyPathValue(files_[1]->path, "n1_value1");
    _VerifyPropertyValue(prop_, "n2_value1");
    // this won't change the expire time of INTERACTION hint
    EXPECT_TRUE(hm->DoHint("INTERACTION", 200ms));
    // now place new hint
    EXPECT_TRUE(hm->DoHint("LAUNCH"));
    std::this_thread::sleep_for(kSLEEP_TOLERANCE_MS);
    _VerifyPathValue(files_[0]->path, "n0_value0");
    _VerifyPathValue(files_[1]->path, "n1_value0");
    _VerifyPropertyValue(prop_, "n2_value0");
    EXPECT_TRUE(hm->DoHint("LAUNCH", 500ms));
    // "LAUNCH" node1 not expired
    std::this_thread::sleep_for(400ms);
    _VerifyPathValue(files_[0]->path, "n0_value0");
    _VerifyPathValue(files_[1]->path, "n1_value0");
    _VerifyPropertyValue(prop_, "n2_value0");
    // "LAUNCH" node1 expired
    std::this_thread::sleep_for(100ms + kSLEEP_TOLERANCE_MS);
    _VerifyPathValue(files_[0]->path, "n0_value0");
    _VerifyPathValue(files_[1]->path, "n1_value1");
    _VerifyPropertyValue(prop_, "n2_value1");
    EXPECT_TRUE(hm->EndHint("LAUNCH"));
    std::this_thread::sleep_for(kSLEEP_TOLERANCE_MS);
    // "LAUNCH" canceled
    _VerifyPathValue(files_[0]->path, "n0_value1");
    _VerifyPathValue(files_[1]->path, "n1_value1");
    _VerifyPropertyValue(prop_, "n2_value1");
    std::this_thread::sleep_for(200ms);
    // "INTERACTION" node0 expired
    _VerifyPathValue(files_[0]->path, "n0_value2");
    _VerifyPathValue(files_[1]->path, "n1_value1");
    _VerifyPropertyValue(prop_, "n2_value2");
    EXPECT_TRUE(hm->EndHint("INTERACTION"));
    std::this_thread::sleep_for(kSLEEP_TOLERANCE_MS);
    // "INTERACTION" canceled
    _VerifyPathValue(files_[0]->path, "n0_value2");
    _VerifyPathValue(files_[1]->path, "n1_value2");
    _VerifyPropertyValue(prop_, "n2_value2");
}

// Test collecting stats with simple actions
TEST_F(HintManagerTest, HintStatsTest) {
    auto hm = std::make_unique<HintManager>(nm_, actions_);
    EXPECT_TRUE(InitHintStatus(hm));
    EXPECT_TRUE(hm->Start());
    EXPECT_TRUE(hm->IsRunning());
    EXPECT_TRUE(hm->DoHint("INTERACTION"));
    std::this_thread::sleep_for(kSLEEP_TOLERANCE_MS);
    _VerifyPathValue(files_[0]->path, "n0_value1");
    _VerifyPathValue(files_[1]->path, "n1_value1");
    _VerifyPropertyValue(prop_, "n2_value1");
    // now place "LAUNCH" hint with timeout of 500ms
    EXPECT_TRUE(hm->DoHint("LAUNCH", 500ms));
    std::this_thread::sleep_for(kSLEEP_TOLERANCE_MS);
    _VerifyPathValue(files_[0]->path, "n0_value0");
    _VerifyPathValue(files_[1]->path, "n1_value0");
    _VerifyPropertyValue(prop_, "n2_value0");
    // "LAUNCH" expired
    std::this_thread::sleep_for(500ms + kSLEEP_TOLERANCE_MS);
    _VerifyPathValue(files_[0]->path, "n0_value1");
    _VerifyPathValue(files_[1]->path, "n1_value1");
    _VerifyPropertyValue(prop_, "n2_value1");
    HintStats launch_stats(hm->GetHintStats("LAUNCH"));
    // Since duration is recorded at the next DoHint,
    // duration should be 0.
    _VerifyStats(launch_stats, 1, 0, 100);
    std::this_thread::sleep_for(100ms + kSLEEP_TOLERANCE_MS);
    EXPECT_TRUE(hm->EndHint("INTERACTION"));
    std::this_thread::sleep_for(kSLEEP_TOLERANCE_MS);
    // "INTERACTION" canceled
    _VerifyPathValue(files_[0]->path, "n0_value2");
    _VerifyPathValue(files_[1]->path, "n1_value2");
    _VerifyPropertyValue(prop_, "n2_value2");
    HintStats interaction_stats(hm->GetHintStats("INTERACTION"));
    _VerifyStats(interaction_stats, 1, 800, 900);
    std::this_thread::sleep_for(kSLEEP_TOLERANCE_MS);
    // Second LAUNCH hint sent to get the first duration recorded.
    EXPECT_TRUE(hm->DoHint("LAUNCH"));
    launch_stats = hm->GetHintStats("LAUNCH");
    _VerifyStats(launch_stats, 2, 500, 600);
}

// Test parsing nodes
TEST_F(HintManagerTest, ParseNodesTest) {
    std::vector<std::unique_ptr<Node>> nodes =
        HintManager::ParseNodes(json_doc_);
    EXPECT_EQ(3u, nodes.size());
    EXPECT_EQ("CPUCluster0MinFreq", nodes[0]->GetName());
    EXPECT_EQ("CPUCluster1MinFreq", nodes[1]->GetName());
    EXPECT_EQ(files_[0 + 2]->path, nodes[0]->GetPath());
    EXPECT_EQ(files_[1 + 2]->path, nodes[1]->GetPath());
    EXPECT_EQ("1512000", nodes[0]->GetValues()[0]);
    EXPECT_EQ("1134000", nodes[0]->GetValues()[1]);
    EXPECT_EQ("384000", nodes[0]->GetValues()[2]);
    EXPECT_EQ("1512000", nodes[1]->GetValues()[0]);
    EXPECT_EQ("1134000", nodes[1]->GetValues()[1]);
    EXPECT_EQ("384000", nodes[1]->GetValues()[2]);
    EXPECT_EQ(2u, nodes[0]->GetDefaultIndex());
    EXPECT_EQ(2u, nodes[1]->GetDefaultIndex());
    EXPECT_TRUE(nodes[0]->GetResetOnInit());
    EXPECT_FALSE(nodes[1]->GetResetOnInit());
    // no dynamic_cast intentionally in Android
    EXPECT_FALSE(reinterpret_cast<FileNode*>(nodes[0].get())->GetHoldFd());
    EXPECT_TRUE(reinterpret_cast<FileNode*>(nodes[1].get())->GetHoldFd());
    EXPECT_EQ("ModeProperty", nodes[2]->GetName());
    EXPECT_EQ(prop_, nodes[2]->GetPath());
    EXPECT_EQ("HIGH", nodes[2]->GetValues()[0]);
    EXPECT_EQ("LOW", nodes[2]->GetValues()[1]);
    EXPECT_EQ("NONE", nodes[2]->GetValues()[2]);
    EXPECT_EQ(2u, nodes[2]->GetDefaultIndex());
    EXPECT_FALSE(nodes[2]->GetResetOnInit());
}

// Test parsing nodes with duplicate name
TEST_F(HintManagerTest, ParseNodesDuplicateNameTest) {
    std::string from = "CPUCluster0MinFreq";
    size_t start_pos = json_doc_.find(from);
    json_doc_.replace(start_pos, from.length(), "CPUCluster1MinFreq");
    std::vector<std::unique_ptr<Node>> nodes =
        HintManager::ParseNodes(json_doc_);
    EXPECT_EQ(0u, nodes.size());
}

TEST_F(HintManagerTest, ParsePropertyNodesDuplicatNameTest) {
    std::string from = "ModeProperty";
    size_t start_pos = json_doc_.find(from);
    json_doc_.replace(start_pos, from.length(), "CPUCluster1MinFreq");
    std::vector<std::unique_ptr<Node>> nodes =
        HintManager::ParseNodes(json_doc_);
    EXPECT_EQ(0u, nodes.size());
}

// Test parsing nodes with duplicate path
TEST_F(HintManagerTest, ParseNodesDuplicatePathTest) {
    std::string from = files_[0 + 2]->path;
    size_t start_pos = json_doc_.find(from);
    json_doc_.replace(start_pos, from.length(), files_[1 + 2]->path);
    std::vector<std::unique_ptr<Node>> nodes =
        HintManager::ParseNodes(json_doc_);
    EXPECT_EQ(0u, nodes.size());
}

// Test parsing file node with duplicate value
TEST_F(HintManagerTest, ParseFileNodesDuplicateValueTest) {
    std::string from = "1512000";
    size_t start_pos = json_doc_.find(from);
    json_doc_.replace(start_pos, from.length(), "1134000");
    std::vector<std::unique_ptr<Node>> nodes =
        HintManager::ParseNodes(json_doc_);
    EXPECT_EQ(0u, nodes.size());
}

// Test parsing property node with duplicate value
TEST_F(HintManagerTest, ParsePropertyNodesDuplicateValueTest) {
    std::string from = "HIGH";
    size_t start_pos = json_doc_.find(from);
    json_doc_.replace(start_pos, from.length(), "LOW");
    std::vector<std::unique_ptr<Node>> nodes =
        HintManager::ParseNodes(json_doc_);
    EXPECT_EQ(0u, nodes.size());
}

// Test parsing file node with empty value
TEST_F(HintManagerTest, ParseFileNodesEmptyValueTest) {
    std::string from = "384000";
    size_t start_pos = json_doc_.find(from);
    json_doc_.replace(start_pos, from.length(), "");
    std::vector<std::unique_ptr<Node>> nodes =
        HintManager::ParseNodes(json_doc_);
    EXPECT_EQ(0u, nodes.size());
}

// Test parsing property node with empty value
TEST_F(HintManagerTest, ParsePropertyNodesEmptyValueTest) {
    std::string from = "LOW";
    size_t start_pos = json_doc_.find(from);
    json_doc_.replace(start_pos, from.length(), "");
    std::vector<std::unique_ptr<Node>> nodes =
        HintManager::ParseNodes(json_doc_);
    EXPECT_EQ(3u, nodes.size());
    EXPECT_EQ("CPUCluster0MinFreq", nodes[0]->GetName());
    EXPECT_EQ("CPUCluster1MinFreq", nodes[1]->GetName());
    EXPECT_EQ(files_[0 + 2]->path, nodes[0]->GetPath());
    EXPECT_EQ(files_[1 + 2]->path, nodes[1]->GetPath());
    EXPECT_EQ("1512000", nodes[0]->GetValues()[0]);
    EXPECT_EQ("1134000", nodes[0]->GetValues()[1]);
    EXPECT_EQ("384000", nodes[0]->GetValues()[2]);
    EXPECT_EQ("1512000", nodes[1]->GetValues()[0]);
    EXPECT_EQ("1134000", nodes[1]->GetValues()[1]);
    EXPECT_EQ("384000", nodes[1]->GetValues()[2]);
    EXPECT_EQ(2u, nodes[0]->GetDefaultIndex());
    EXPECT_EQ(2u, nodes[1]->GetDefaultIndex());
    EXPECT_TRUE(nodes[0]->GetResetOnInit());
    EXPECT_FALSE(nodes[1]->GetResetOnInit());
    // no dynamic_cast intentionally in Android
    EXPECT_FALSE(reinterpret_cast<FileNode*>(nodes[0].get())->GetHoldFd());
    EXPECT_TRUE(reinterpret_cast<FileNode*>(nodes[1].get())->GetHoldFd());
    EXPECT_EQ("ModeProperty", nodes[2]->GetName());
    EXPECT_EQ(prop_, nodes[2]->GetPath());
    EXPECT_EQ("HIGH", nodes[2]->GetValues()[0]);
    EXPECT_EQ("", nodes[2]->GetValues()[1]);
    EXPECT_EQ("NONE", nodes[2]->GetValues()[2]);
    EXPECT_EQ(2u, nodes[2]->GetDefaultIndex());
    EXPECT_FALSE(nodes[2]->GetResetOnInit());
}

// Test parsing invalid json for nodes
TEST_F(HintManagerTest, ParseBadFileNodesTest) {
    std::vector<std::unique_ptr<Node>> nodes =
        HintManager::ParseNodes("invalid json");
    EXPECT_EQ(0u, nodes.size());
    nodes = HintManager::ParseNodes(
        "{\"devices\":{\"15\":[\"armeabi-v7a\"],\"16\":[\"armeabi-v7a\"],"
        "\"26\":[\"armeabi-v7a\",\"arm64-v8a\",\"x86\",\"x86_64\"]}}");
    EXPECT_EQ(0u, nodes.size());
}

// Test parsing actions
TEST_F(HintManagerTest, ParseActionsTest) {
    std::vector<std::unique_ptr<Node>> nodes =
        HintManager::ParseNodes(json_doc_);
    std::unordered_map<std::string, Hint> actions = HintManager::ParseActions(json_doc_, nodes);
    EXPECT_EQ(5u, actions.size());

    EXPECT_EQ(2u, actions["INTERACTION"].node_actions.size());
    EXPECT_EQ(1u, actions["INTERACTION"].node_actions[0].node_index);
    EXPECT_EQ(1u, actions["INTERACTION"].node_actions[0].value_index);
    EXPECT_EQ(std::chrono::milliseconds(800).count(),
              actions["INTERACTION"].node_actions[0].timeout_ms.count());

    EXPECT_EQ(2u, actions["INTERACTION"].node_actions[1].node_index);
    EXPECT_EQ(1u, actions["INTERACTION"].node_actions[1].value_index);
    EXPECT_EQ(std::chrono::milliseconds(800).count(),
              actions["INTERACTION"].node_actions[1].timeout_ms.count());

    EXPECT_EQ(3u, actions["LAUNCH"].node_actions.size());

    EXPECT_EQ(0u, actions["LAUNCH"].node_actions[0].node_index);
    EXPECT_EQ(1u, actions["LAUNCH"].node_actions[0].value_index);
    EXPECT_EQ(std::chrono::milliseconds(500).count(),
              actions["LAUNCH"].node_actions[0].timeout_ms.count());

    EXPECT_EQ(2u, actions["LAUNCH"].node_actions[1].node_index);
    EXPECT_EQ(0u, actions["LAUNCH"].node_actions[1].value_index);
    EXPECT_EQ(std::chrono::milliseconds(500).count(),
              actions["LAUNCH"].node_actions[1].timeout_ms.count());

    EXPECT_EQ(1u, actions["LAUNCH"].node_actions[2].node_index);
    EXPECT_EQ(0u, actions["LAUNCH"].node_actions[2].value_index);
    EXPECT_EQ(std::chrono::milliseconds(2000).count(),
              actions["LAUNCH"].node_actions[2].timeout_ms.count());

    EXPECT_EQ(1u, actions["MASK_LAUNCH_MODE"].hint_actions.size());
    EXPECT_EQ(HintActionType::MaskHint, actions["MASK_LAUNCH_MODE"].hint_actions[0].type);
    EXPECT_TRUE("LAUNCH" == actions["MASK_LAUNCH_MODE"].hint_actions[0].value);

    EXPECT_EQ(1u, actions["DO_LAUNCH_MODE"].hint_actions.size());
    EXPECT_EQ(HintActionType::DoHint, actions["DO_LAUNCH_MODE"].hint_actions[0].type);
    EXPECT_TRUE("LAUNCH" == actions["DO_LAUNCH_MODE"].hint_actions[0].value);

    EXPECT_EQ(1u, actions["END_LAUNCH_MODE"].hint_actions.size());
    EXPECT_EQ(HintActionType::EndHint, actions["END_LAUNCH_MODE"].hint_actions[0].type);
    EXPECT_TRUE("LAUNCH" == actions["END_LAUNCH_MODE"].hint_actions[0].value);
}

// Test parsing actions with duplicate File node
TEST_F(HintManagerTest, ParseActionDuplicateFileNodeTest) {
    std::string from = R"("Node": "CPUCluster0MinFreq")";
    size_t start_pos = json_doc_.find(from);
    json_doc_.replace(start_pos, from.length(), R"("Node": "CPUCluster1MinFreq")");
    std::vector<std::unique_ptr<Node>> nodes =
        HintManager::ParseNodes(json_doc_);
    EXPECT_EQ(3u, nodes.size());
    auto actions = HintManager::ParseActions(json_doc_, nodes);
    EXPECT_EQ(0u, actions.size());
}

// Test parsing actions with duplicate Property node
TEST_F(HintManagerTest, ParseActionDuplicatePropertyNodeTest) {
    std::string from = R"("Node": "CPUCluster0MinFreq")";
    size_t start_pos = json_doc_.find(from);
    json_doc_.replace(start_pos, from.length(), R"("Node": "ModeProperty")");
    auto nodes = HintManager::ParseNodes(json_doc_);
    EXPECT_EQ(3u, nodes.size());
    auto actions = HintManager::ParseActions(json_doc_, nodes);
    EXPECT_EQ(0u, actions.size());
}

// Test parsing invalid json for actions
TEST_F(HintManagerTest, ParseBadActionsTest) {
    std::vector<std::unique_ptr<Node>> nodes =
        HintManager::ParseNodes(json_doc_);
    auto actions = HintManager::ParseActions("invalid json", nodes);
    EXPECT_EQ(0u, actions.size());
    actions = HintManager::ParseActions(
        "{\"devices\":{\"15\":[\"armeabi-v7a\"],\"16\":[\"armeabi-v7a\"],"
        "\"26\":[\"armeabi-v7a\",\"arm64-v8a\",\"x86\",\"x86_64\"]}}",
        nodes);
    EXPECT_EQ(0u, actions.size());
}

// Test hint/cancel/expire with json config
TEST_F(HintManagerTest, GetFromJSONTest) {
    TemporaryFile json_file;
    ASSERT_TRUE(android::base::WriteStringToFile(json_doc_, json_file.path))
        << strerror(errno);
    std::unique_ptr<HintManager> hm =
        HintManager::GetFromJSON(json_file.path, false);
    EXPECT_NE(nullptr, hm.get());
    EXPECT_FALSE(hm->IsRunning());
    EXPECT_TRUE(hm->Start());
    EXPECT_TRUE(hm->IsRunning());
    hm = HintManager::GetFromJSON(json_file.path);
    EXPECT_NE(nullptr, hm.get());
    EXPECT_TRUE(hm->IsRunning());
    std::this_thread::sleep_for(kSLEEP_TOLERANCE_MS);
    EXPECT_TRUE(hm->IsRunning());
    // Initial default value on Node0
    _VerifyPathValue(files_[0 + 2]->path, "384000");
    _VerifyPathValue(files_[1 + 2]->path, "");
    _VerifyPropertyValue(prop_, "");
    // Do INTERACTION
    EXPECT_TRUE(hm->DoHint("INTERACTION"));
    std::this_thread::sleep_for(kSLEEP_TOLERANCE_MS);
    _VerifyPathValue(files_[0 + 2]->path, "384000");
    _VerifyPathValue(files_[1 + 2]->path, "1134000");
    _VerifyPropertyValue(prop_, "LOW");
    // Do LAUNCH
    EXPECT_TRUE(hm->DoHint("LAUNCH"));
    std::this_thread::sleep_for(kSLEEP_TOLERANCE_MS);
    _VerifyPathValue(files_[0 + 2]->path, "1134000");
    _VerifyPathValue(files_[1 + 2]->path, "1512000");
    _VerifyPropertyValue(prop_, "HIGH");
    std::this_thread::sleep_for(500ms);
    // "LAUNCH" node0 expired
    _VerifyPathValue(files_[0 + 2]->path, "384000");
    _VerifyPathValue(files_[1 + 2]->path, "1512000");
    _VerifyPropertyValue(prop_, "LOW");
    EXPECT_TRUE(hm->EndHint("LAUNCH"));
    std::this_thread::sleep_for(kSLEEP_TOLERANCE_MS);
    // "LAUNCH" canceled
    _VerifyPathValue(files_[0 + 2]->path, "384000");
    _VerifyPathValue(files_[1 + 2]->path, "1134000");
    _VerifyPropertyValue(prop_, "LOW");
    std::this_thread::sleep_for(300ms);
    // "INTERACTION" node1 expired
    _VerifyPathValue(files_[0 + 2]->path, "384000");
    _VerifyPathValue(files_[1 + 2]->path, "384000");
    _VerifyPropertyValue(prop_, "NONE");

    // Mask LAUNCH and do LAUNCH
    EXPECT_TRUE(hm->DoHint("MASK_LAUNCH_MODE"));
    EXPECT_FALSE(hm->DoHint("LAUNCH"));  // should fail
    std::this_thread::sleep_for(kSLEEP_TOLERANCE_MS);
    _VerifyPathValue(files_[0 + 2]->path, "384000");
    _VerifyPathValue(files_[1 + 2]->path, "384000");
    _VerifyPropertyValue(prop_, "NONE");

    // UnMask LAUNCH and do LAUNCH
    EXPECT_TRUE(hm->EndHint("MASK_LAUNCH_MODE"));
    EXPECT_TRUE(hm->DoHint("LAUNCH"));
    std::this_thread::sleep_for(kSLEEP_TOLERANCE_MS);
    _VerifyPathValue(files_[0 + 2]->path, "1134000");
    _VerifyPathValue(files_[1 + 2]->path, "1512000");
    _VerifyPropertyValue(prop_, "HIGH");
    // END_LAUNCH_MODE should deactivate LAUNCH
    EXPECT_TRUE(hm->DoHint("END_LAUNCH_MODE"));
    std::this_thread::sleep_for(kSLEEP_TOLERANCE_MS);
    _VerifyPathValue(files_[0 + 2]->path, "384000");
    _VerifyPathValue(files_[1 + 2]->path, "384000");
    _VerifyPropertyValue(prop_, "NONE");
    EXPECT_TRUE(hm->EndHint("END_LAUNCH_MODE"));

    // DO_LAUNCH_MODE should activate LAUNCH
    EXPECT_TRUE(hm->DoHint("DO_LAUNCH_MODE"));
    std::this_thread::sleep_for(kSLEEP_TOLERANCE_MS);
    _VerifyPathValue(files_[0 + 2]->path, "1134000");
    _VerifyPathValue(files_[1 + 2]->path, "1512000");
    _VerifyPropertyValue(prop_, "HIGH");
}

}  // namespace perfmgr
}  // namespace android
