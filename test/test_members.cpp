#include <gtest/gtest.h>
#include <khook.hpp>
#include "helpers.hpp"

class MemberTests : public ::testing::Test {
protected:
    class TargetClass {
    public:
        void TargetMethod(int& value) {
            value = 0xDEADBEEF;
        }
    };

    typedef MemberContext<TargetClass, void, int&> ContextType;

    static KHook::Return<void> HookMethod(TargetClass* _this, int& value) {
        std::cout << "Here";
        return {KHook::Action::Ignore};
    }

    static KHook::Return<void> HookMethodSupersede(TargetClass* _this, int& value) {
        return {KHook::Action::Supersede};
    }
};

TEST_F(MemberTests, PreCallback) {
    TargetClass instance;

    KHook::Member<TargetClass, void, int&> hook(&TargetClass::TargetMethod, &HookMethod, nullptr);
    
    testing::internal::CaptureStdout();

    int value = 0x1337;
    instance.TargetMethod(value);

    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked method was modified";
    EXPECT_EQ(output, "Here") << "No response from callback";
}

TEST_F(MemberTests, PostCallback) {
    TargetClass instance;

    KHook::Member<TargetClass, void, int&> hook(&TargetClass::TargetMethod, nullptr, &HookMethod);

    testing::internal::CaptureStdout();

    int value = 0x1337;
    instance.TargetMethod(value);

    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked method was modified";
    EXPECT_EQ(output, "Here") << "No response from callback";
}

TEST_F(MemberTests, PreAndPostCallbacks) {
    TargetClass instance;

    KHook::Member<TargetClass, void, int&> hook(&TargetClass::TargetMethod, &HookMethod, &HookMethod);

    testing::internal::CaptureStdout();

    int value = 0x1337;
    instance.TargetMethod(value);

    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked method was modified";
    EXPECT_EQ(output, "HereHere") << "No response from callbacks";
}

TEST_F(MemberTests, SupersedePreCallback) {
    TargetClass instance;

    KHook::Member<TargetClass, void, int&> hook(&TargetClass::TargetMethod, &HookMethodSupersede, nullptr);
    
    int value = 0x1337;
    instance.TargetMethod(value);

    EXPECT_EQ(value, 0x1337) << "Behaviour of hooked method was modified";
}

TEST_F(MemberTests, ContextPreCallback) {
    TargetClass instance;

    ContextType context(&HookMethod);
    KHook::Member<TargetClass, void, int&> hook(&TargetClass::TargetMethod, &context, &ContextType::OnPre, nullptr);

    testing::internal::CaptureStdout();

    int value = 0x1337;
    instance.TargetMethod(value);

    std::string output = testing::internal::GetCapturedStdout();

    ASSERT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked method was modified";
    ASSERT_EQ(output, "Here") << "No response from callback";

    hook.RemoveContext(&context);

    testing::internal::CaptureStdout();

    value = 0x1337;
    instance.TargetMethod(value);

    output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked method was modified";
    EXPECT_EQ(output, "") << "Unexpected response from callback after context removal";
}

TEST_F(MemberTests, ContextPostCallback) {
    TargetClass instance;

    ContextType context(&HookMethod);
    KHook::Member<TargetClass, void, int&> hook(&TargetClass::TargetMethod, &context, nullptr, &ContextType::OnPost);

    testing::internal::CaptureStdout();

    int value = 0x1337;
    instance.TargetMethod(value);

    std::string output = testing::internal::GetCapturedStdout();

    ASSERT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked method was modified";
    ASSERT_EQ(output, "Here") << "No response from callback";

    hook.RemoveContext(&context);

    testing::internal::CaptureStdout();

    value = 0x1337;
    instance.TargetMethod(value);

    output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked method was modified";
    EXPECT_EQ(output, "") << "Unexpected response from callback after context removal";
}

TEST_F(MemberTests, ContextPreAndPostCallbacks) {
    TargetClass instance;

    ContextType context(&HookMethod);
    KHook::Member<TargetClass, void, int&> hook(&TargetClass::TargetMethod, &context, &ContextType::OnPre, &ContextType::OnPost);

    testing::internal::CaptureStdout();

    int value = 0x1337;
    instance.TargetMethod(value);

    std::string output = testing::internal::GetCapturedStdout();

    ASSERT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked method was modified";
    ASSERT_EQ(output, "HereHere") << "No response from callbacks";

    hook.RemoveContext(&context);

    testing::internal::CaptureStdout();

    value = 0x1337;
    instance.TargetMethod(value);

    output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked method was modified";
    EXPECT_EQ(output, "") << "Unexpected response from callbacks after context removal";
}

TEST_F(MemberTests, ContextSupersedePreCallback) {
    TargetClass instance;

    ContextType context(&HookMethodSupersede);
    KHook::Member<TargetClass, void, int&> hook(&TargetClass::TargetMethod, &context, &ContextType::OnPre, nullptr);

    int value = 0x1337;
    instance.TargetMethod(value);

    ASSERT_EQ(value, 0x1337) << "Behaviour of hooked method was modified";

    hook.RemoveContext(&context);

    value = 0x1337;
    instance.TargetMethod(value);

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked method was modified";
}

TEST_F(MemberTests, MultipleContextsPreCallback) {
    TargetClass instance;

    ContextType context1(&HookMethodSupersede);
    ContextType context2(&HookMethod);
    KHook::Member<TargetClass, void, int&> hook(&TargetClass::TargetMethod, &context1, &ContextType::OnPre, nullptr);
    hook.AddContext(&context2, &ContextType::OnPre, nullptr);

    testing::internal::CaptureStdout();

    int value = 0x1337;
    instance.TargetMethod(value);
    std::string output = testing::internal::GetCapturedStdout();

    ASSERT_EQ(value, 0x1337) << "Behaviour of hooked method was modified";
    ASSERT_EQ(output, "Here") << "Incorrect response from multiple contexts";

    hook.RemoveContext(&context1);

    testing::internal::CaptureStdout();

    value = 0x1337;
    instance.TargetMethod(value);
    output = testing::internal::GetCapturedStdout();

    ASSERT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked method was modified after context removal";
    ASSERT_EQ(output, "Here") << "Incorrect response from remaining context after context removal";

    hook.RemoveContext(&context2);

    testing::internal::CaptureStdout();

    value = 0x1337;
    instance.TargetMethod(value);
    output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked method was modified after context removal";
    EXPECT_EQ(output, "") << "Unexpected response from callbacks after context removal";
}