#include <gtest/gtest.h>

#include "mini_ipc/param_manager.hpp"

#include <fstream>
#include <cstdio>

class ParamManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::ofstream ofs{"/tmp/mini_ipc_test.yaml"};
        ofs << "discovery_daemon:\n"
            << "  ip: 127.0.0.1\n"
            << "  port: 9999\n"
            << "  backlog: 64\n"
            << "publisher:\n"
            << "  backlog: 32\n"
            << "runtime:\n"
            << "  epoll_max_events: 50\n";
        ofs.close();

        auto& param_manager = mini_ipc::ParamManager::instance();
        ASSERT_TRUE(param_manager.load("/tmp/mini_ipc_test.yaml"));
    }

    void TearDown() override {
        std::remove("/tmp/mini_ipc_test.yaml");
    }
};

TEST_F(ParamManagerTest, LoadAndGetValues) {
    auto& pm = mini_ipc::ParamManager::instance();
    EXPECT_EQ(pm.get<std::string>("discovery_daemon.ip", "0.0.0.0"), "127.0.0.1");
    EXPECT_EQ(pm.get<int>("discovery_daemon.port", 0), 9999);
    EXPECT_EQ(pm.get<int>("discovery_daemon.backlog", 0), 64);
    EXPECT_EQ(pm.get<int>("publisher.backlog", 0), 32);
    EXPECT_EQ(pm.get<int>("runtime.epoll_max_events", 0), 50);
}

TEST_F(ParamManagerTest, DefaultWhenKeyMissing) {
    auto& pm = mini_ipc::ParamManager::instance();
    EXPECT_EQ(pm.get<std::string>("nonexistent.key", "fallback"), "fallback");
    EXPECT_EQ(pm.get<int>("nonexistent.key", 42), 42);
}

TEST_F(ParamManagerTest, DefaultWhenFileNotLoaded) {
    auto& pm = mini_ipc::ParamManager::instance();
    EXPECT_TRUE(pm.has("discovery_daemon"));
    EXPECT_TRUE(pm.has("discovery_daemon.ip"));
    EXPECT_FALSE(pm.has("nonexistent.key"));
}