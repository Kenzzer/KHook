#include <gtest/gtest.h>
#include <khook.hpp>
#include "helpers.hpp"

class FunctionTests : public ::testing::Test {
protected:
    typedef FunctionContext<void, int&> ContextType;

    FunctionTests()
    {
    }

    static void TargetFunction(int& value) {
        value = 0xDEADBEEF;
    }

    static KHook::Return<void> HookFunction(int& value) {
        std::cout << "Here";
        return {KHook::Action::Ignore};
    }

    static KHook::Return<void> HookFunctionSupersede(int& value) {
        return {KHook::Action::Supersede};
    }
};

TEST_F(FunctionTests, NoopPreCallback) {
    KHook::Function<void, int&> hook(&TargetFunction, &HookFunction, nullptr);
    
    testing::internal::CaptureStdout();

    int value = 0x1337;
    TargetFunction(value);

    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked function was modified";
    EXPECT_EQ(output, "Here") << "No response from callback";
}

TEST_F(FunctionTests, NoopPostCallback) {
    KHook::Function<void, int&> hook(&TargetFunction, nullptr, &HookFunction);

    testing::internal::CaptureStdout();

    int value = 0x1337;
    TargetFunction(value);

    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked function was modified";
    EXPECT_EQ(output, "Here") << "No response from callback";
}

TEST_F(FunctionTests, NoopPreAndPostCallbacks) {
    KHook::Function<void, int&> hook(&TargetFunction, &HookFunction, &HookFunction);

    testing::internal::CaptureStdout();

    int value = 0x1337;
    TargetFunction(value);

    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked function was modified";
    EXPECT_EQ(output, "HereHere") << "No response from callbacks";
}

TEST_F(FunctionTests, SupersedePreCallback) {
    KHook::Function<void, int&> hook(&TargetFunction, &HookFunctionSupersede, nullptr);
    
    int value = 0x1337;
    TargetFunction(value);

    EXPECT_EQ(value, 0x1337) << "Behaviour of hooked function was modified";
}

TEST_F(FunctionTests, ContextPreCallback) {
    ContextType context(&HookFunction);
    KHook::Function<void, int&> hook(&TargetFunction, &context, &ContextType::OnPre, nullptr);

    testing::internal::CaptureStdout();

    int value = 0x1337;
    TargetFunction(value);

    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked function was modified";
    EXPECT_EQ(output, "Here") << "No response from callback";

    hook.RemoveContext(&context);

    testing::internal::CaptureStdout();

    value = 0x1337;
    TargetFunction(value);

    output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked function was modified";
    EXPECT_EQ(output, "") << "Unexpected response from callback after context removal";
}

TEST_F(FunctionTests, ContextPostCallback) {
    ContextType context(&HookFunction);
    KHook::Function<void, int&> hook(&TargetFunction, &context, nullptr, &ContextType::OnPost);

    testing::internal::CaptureStdout();

    int value = 0x1337;
    TargetFunction(value);

    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked function was modified";
    EXPECT_EQ(output, "Here") << "No response from callback";

    hook.RemoveContext(&context);

    testing::internal::CaptureStdout();

    value = 0x1337;
    TargetFunction(value);

    output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked function was modified";
    EXPECT_EQ(output, "") << "Unexpected response from callback after context removal";
}

TEST_F(FunctionTests, ContextPreAndPostCallbacks) {
    ContextType context(&HookFunction);
    KHook::Function<void, int&> hook(&TargetFunction, &context, &ContextType::OnPre, &ContextType::OnPost);

    testing::internal::CaptureStdout();

    int value = 0x1337;
    TargetFunction(value);

    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked function was modified";
    EXPECT_EQ(output, "HereHere") << "No response from callbacks";

    hook.RemoveContext(&context);

    testing::internal::CaptureStdout();

    value = 0x1337;
    TargetFunction(value);

    output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked function was modified";
    EXPECT_EQ(output, "") << "Unexpected response from callbacks after context removal";
}

TEST_F(FunctionTests, ContextSupersedePreCallback) {
    ContextType context(&HookFunctionSupersede);
    KHook::Function<void, int&> hook(&TargetFunction, &context, &ContextType::OnPre, nullptr);

    int value = 0x1337;
    TargetFunction(value);

    ASSERT_EQ(value, 0x1337) << "Behaviour of hooked function was modified";

    hook.RemoveContext(&context);

    value = 0x1337;
    TargetFunction(value);

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked function was modified";
}

TEST_F(FunctionTests, MultipleContextsPreCallback) {
    ContextType context1(&HookFunctionSupersede);
    ContextType context2(&HookFunction);
    KHook::Function<void, int&> hook(&TargetFunction, &context1, &ContextType::OnPre, nullptr);
    hook.AddContext(&context2, &ContextType::OnPre, nullptr);

    testing::internal::CaptureStdout();

    int value = 0x1337;
    TargetFunction(value);

    std::string output = testing::internal::GetCapturedStdout();

    ASSERT_EQ(value, 0x1337) << "Behaviour of hooked function was modified";
    ASSERT_EQ(output, "Here") << "Incorrect response from multiple contexts";

    hook.RemoveContext(&context1);

    testing::internal::CaptureStdout();

    value = 0x1337;
    TargetFunction(value);

    output = testing::internal::GetCapturedStdout();

    ASSERT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked function was modified after context removal";
    ASSERT_EQ(output, "Here") << "Incorrect response from remaining context after context removal";

    hook.RemoveContext(&context2);

    testing::internal::CaptureStdout();

    value = 0x1337;
    TargetFunction(value);

    output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked function was modified after context removal";
    EXPECT_EQ(output, "") << "Unexpected response from contexts after context removal";
}