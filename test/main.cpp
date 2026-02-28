#include <gtest/gtest.h>
#include <khook.hpp>

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    int result = RUN_ALL_TESTS();

    KHook::Shutdown();

    return result;
}