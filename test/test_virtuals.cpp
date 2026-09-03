#include <gtest/gtest.h>
#include <khook.hpp>
#include <thread>
#include "helpers.hpp"

class VirtualTests : public ::testing::Test {
protected:
    class BaseClass {
    public:
        virtual void TargetMethod(int& value) {
            value = 0xBEEFDEAD;
        }
    };

    class TargetClass : public BaseClass {
    public:
        virtual void TargetMethod(int& value) override {
            value = 0xDEADBEEF;
        }
    };

    typedef VirtualContext<BaseClass, void, int&> ContextType;

    static KHook::Return<void> HookMethod(BaseClass* _this, int& value) {
        std::cout << "Here";
        return {KHook::Action::Ignore};
    }

    static KHook::Return<void> HookMethodSupersede(BaseClass* _this, int& value) {
        return {KHook::Action::Supersede};
    }
};

TEST_F(VirtualTests, PreCallback) {
    std::unique_ptr<BaseClass> instance = std::make_unique<TargetClass>();

    KHook::Virtual<BaseClass, void, int&> hook(&BaseClass::TargetMethod, &HookMethod, nullptr);
    hook.Add(instance.get());

    testing::internal::CaptureStdout();

    int value = 0x1337;
    instance->TargetMethod(value);

    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked method was modified";
    EXPECT_EQ(output, "Here") << "No response from callback";
}

TEST_F(VirtualTests, PostCallback) {
    std::unique_ptr<BaseClass> instance = std::make_unique<TargetClass>();

    KHook::Virtual<BaseClass, void, int&> hook(&BaseClass::TargetMethod, nullptr, &HookMethod);
    hook.Add(instance.get());

    testing::internal::CaptureStdout();

    int value = 0x1337;
    instance->TargetMethod(value);

    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked method was modified";
    EXPECT_EQ(output, "Here") << "No response from callback";
}

TEST_F(VirtualTests, PreAndPostCallbacks) {
    std::unique_ptr<BaseClass> instance = std::make_unique<TargetClass>();

    KHook::Virtual<BaseClass, void, int&> hook(&BaseClass::TargetMethod, &HookMethod, &HookMethod);
    hook.Add(instance.get());

    testing::internal::CaptureStdout();

    int value = 0x1337;
    instance->TargetMethod(value);

    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked method was modified";
    EXPECT_EQ(output, "HereHere") << "No response from callbacks";
}

TEST_F(VirtualTests, SupersedePreCallback) {
    std::unique_ptr<BaseClass> instance = std::make_unique<TargetClass>();

    KHook::Virtual<BaseClass, void, int&> hook(&BaseClass::TargetMethod, &HookMethodSupersede, nullptr);
    hook.Add(instance.get());

    int value = 0x1337;
    instance->TargetMethod(value);

    EXPECT_EQ(value, 0x1337) << "Callback did not supersede original method";
}

TEST_F(VirtualTests, InstanceOnly) {
    std::unique_ptr<BaseClass> instance1 = std::make_unique<TargetClass>();
    std::unique_ptr<BaseClass> instance2 = std::make_unique<TargetClass>();

    KHook::Virtual<BaseClass, void, int&> hook(&BaseClass::TargetMethod, &HookMethodSupersede, nullptr);
    hook.Add(instance1.get());

    testing::internal::CaptureStdout();

    int value1 = 0x1337;
    int value2 = 0x1337;
    instance1->TargetMethod(value1);
    instance2->TargetMethod(value2);

    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value1, 0x1337) << "Behaviour of hooked instance was modified";
    EXPECT_EQ(value2, 0xDEADBEEF) << "Behaviour of unhooked instance was modified";
}

TEST_F(VirtualTests, ContextPreCallback) {
    std::unique_ptr<BaseClass> instance = std::make_unique<TargetClass>();

    ContextType context(&HookMethod);
    KHook::Virtual<BaseClass, void, int&> hook(&BaseClass::TargetMethod, &context, &ContextType::OnPre, nullptr);
    hook.Add(instance.get());

    testing::internal::CaptureStdout();

    int value = 0x1337;
    instance->TargetMethod(value);

    std::string output = testing::internal::GetCapturedStdout();

    ASSERT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked method was modified";
    ASSERT_EQ(output, "Here") << "No response from callback";

    hook.RemoveContext(&context);

    testing::internal::CaptureStdout();

    value = 0x1337;
    instance->TargetMethod(value);

    output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked method was modified";
    EXPECT_EQ(output, "") << "Unexpected response from callback after context removal";
}

TEST_F(VirtualTests, ContextPostCallback) {
    std::unique_ptr<BaseClass> instance = std::make_unique<TargetClass>();

    ContextType context(&HookMethod);
    KHook::Virtual<BaseClass, void, int&> hook(&BaseClass::TargetMethod, &context, nullptr, &ContextType::OnPost);
    hook.Add(instance.get());

    testing::internal::CaptureStdout();

    int value = 0x1337;
    instance->TargetMethod(value);

    std::string output = testing::internal::GetCapturedStdout();

    ASSERT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked method was modified";
    ASSERT_EQ(output, "Here") << "No response from callback";

    hook.RemoveContext(&context);

    testing::internal::CaptureStdout();

    value = 0x1337;
    instance->TargetMethod(value);

    output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked method was modified";
    EXPECT_EQ(output, "") << "Unexpected response from callback after context removal";
}

TEST_F(VirtualTests, ContextPreAndPostCallbacks) {
    std::unique_ptr<BaseClass> instance = std::make_unique<TargetClass>();

    ContextType context(&HookMethod);
    KHook::Virtual<BaseClass, void, int&> hook(&BaseClass::TargetMethod, &context, &ContextType::OnPre, &ContextType::OnPost);
    hook.Add(instance.get());

    testing::internal::CaptureStdout();

    int value = 0x1337;
    instance->TargetMethod(value);

    std::string output = testing::internal::GetCapturedStdout();

    ASSERT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked method was modified";
    ASSERT_EQ(output, "HereHere") << "No response from callbacks";

    hook.RemoveContext(&context);

    testing::internal::CaptureStdout();

    value = 0x1337;
    instance->TargetMethod(value);

    output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked method was modified";
    EXPECT_EQ(output, "") << "Unexpected response from callbacks after context removal";
}

TEST_F(VirtualTests, ContextSupersedePreCallback) {
    std::unique_ptr<BaseClass> instance = std::make_unique<TargetClass>();

    ContextType context(&HookMethodSupersede);
    KHook::Virtual<BaseClass, void, int&> hook(&BaseClass::TargetMethod, &context, &ContextType::OnPre, nullptr);
    hook.Add(instance.get());

    int value = 0x1337;
    instance->TargetMethod(value);

    ASSERT_EQ(value, 0x1337) << "Callback did not supersede original method";

    hook.RemoveContext(&context);

    value = 0x1337;
    instance->TargetMethod(value);

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of unhooked method was modified";
}

TEST_F(VirtualTests, ContextInstanceOnly) {
    std::unique_ptr<BaseClass> instance1 = std::make_unique<TargetClass>();
    std::unique_ptr<BaseClass> instance2 = std::make_unique<TargetClass>();

    ContextType context(&HookMethodSupersede);
    KHook::Virtual<BaseClass, void, int&> hook(&BaseClass::TargetMethod, &context, &ContextType::OnPre, nullptr);
    hook.Add(instance1.get());

    int value1 = 0x1337;
    int value2 = 0x1337;
    instance1->TargetMethod(value1);
    instance2->TargetMethod(value2);

    ASSERT_EQ(value1, 0x1337) << "Callback did not supersede original method";
    ASSERT_EQ(value2, 0xDEADBEEF) << "Behaviour of unhooked instance was modified";

    hook.RemoveContext(&context);

    value1 = 0x1337;
    value2 = 0x1337;
    instance1->TargetMethod(value1);
    instance2->TargetMethod(value2);

    EXPECT_EQ(value1, 0xDEADBEEF) << "Behaviour of unhooked instance was modified after context removal";
    EXPECT_EQ(value2, 0xDEADBEEF) << "Behaviour of unhooked instance was modified after context removal";
}

TEST_F(VirtualTests, MultipleContextsPreCallback) {
    std::unique_ptr<BaseClass> instance = std::make_unique<TargetClass>();

    ContextType context1(&HookMethodSupersede);
    ContextType context2(&HookMethod);
    KHook::Virtual<BaseClass, void, int&> hook(&BaseClass::TargetMethod, &context1, &ContextType::OnPre, nullptr);
    hook.Add(instance.get());
    hook.AddContext(&context2, &ContextType::OnPre, nullptr);

    testing::internal::CaptureStdout();

    int value = 0x1337;
    instance->TargetMethod(value);
    std::string output = testing::internal::GetCapturedStdout();

    ASSERT_EQ(value, 0x1337) << "Behaviour of hooked instance was modified";
    ASSERT_EQ(output, "Here") << "Incorrect response from multiple contexts";

    hook.RemoveContext(&context1);

    testing::internal::CaptureStdout();

    value = 0x1337;
    instance->TargetMethod(value);
    output = testing::internal::GetCapturedStdout();

    ASSERT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked instance was modified after context removal";
    ASSERT_EQ(output, "Here") << "Incorrect response from remaining context after context removal";

    hook.RemoveContext(&context2);

    testing::internal::CaptureStdout();

    value = 0x1337;
    instance->TargetMethod(value);
    output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of hooked instance was modified after context removal";
    EXPECT_EQ(output, "") << "Unexpected response from callbacks after context removal";
}