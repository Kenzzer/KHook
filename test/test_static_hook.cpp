#include <gtest/gtest.h>
#include <iostream>
#include <khook.hpp>
#include <thread>
#include "helpers.hpp"

template<typename Ret, typename... Args>
class NoopStaticHookTemplate {
  public:
    static NOINLINE Ret PrePostNoop(Args... args);
    static NOINLINE Ret CallOriginal(Args... args);
    static NOINLINE Ret MakeReturn(Args... args);
    static NOINLINE void OnRemoved(int hookId);
};

template<typename Ret, typename... Args>
NOINLINE Ret NoopStaticHookTemplate<Ret, Args...>::PrePostNoop(Args... args) {
    std::cout << "PrePostNoop()" << std::endl;
    KHook::SaveReturnValue(
        KHook::Action::Ignore,
        nullptr,
        0,
        nullptr,
        nullptr,
        false
    );
    if constexpr (std::is_same<Ret, void>::value) {
        return;
    } else {
        return Ret();
    }
}

template<typename Ret, typename... Args>
NOINLINE Ret NoopStaticHookTemplate<Ret, Args...>::CallOriginal(Args... args) {
    std::cout << "CallOriginal()" << std::endl;
    auto original =
        reinterpret_cast<Ret (*)(Args...)>(KHook::GetOriginalFunction());
    if constexpr (std::is_same<Ret, void>::value) {
        original(args...);
        KHook::SaveReturnValue(
            KHook::Action::Ignore,
            nullptr,
            0,
            nullptr,
            nullptr,
            true
        );
        return;
    } else {
        Ret result = original(args...);
        KHook::SaveReturnValue(
            KHook::Action::Ignore,
            &result,
            sizeof(Ret),
            (void*)KHook::init_operator<Ret>,
            (void*)KHook::deinit_operator<Ret>,
            true
        );
        return result;
    }
}

template<typename Ret, typename... Args>
NOINLINE Ret NoopStaticHookTemplate<Ret, Args...>::MakeReturn(Args... args) {
    std::cout << "MakeReturn()" << std::endl;
    if constexpr (std::is_same<Ret, void>::value) {
        KHook::DestroyReturnValue();
        return;
    } else {
        Ret result = *((Ret*)KHook::GetCurrentValuePtr(true));
        KHook::DestroyReturnValue();
        return result;
    }
}

template<typename Ret, typename... Args>
NOINLINE void NoopStaticHookTemplate<Ret, Args...>::OnRemoved(int hookId) {
    std::cout << "OnRemoved(" << std::dec << hookId << ")" << std::endl;
}

class StaticHookTest: public ::testing::Test {
  protected:
    class TestObject {
      public:
        int m_testValue;
    };

    class HookedClass {
      public:
        NOINLINE static bool IsAllowed(TestObject* obj) {
            std::cout << "HookedClass::IsAllowed()" << std::endl;
            return true;
        }

        NOINLINE static int SetObjectValue(TestObject* obj, int value) {
            std::cout << "HookedClass::SetObjectValue()" << std::endl;
            obj->m_testValue = value;
            return value;
        }

        NOINLINE static void MyVoid(TestObject* obj) {
            std::cout << "HookedClass::MyVoid()" << std::endl;
        }
    };

    using IsAllowedNoopHook = NoopStaticHookTemplate<bool, TestObject*>;
    using SetObjectValueNoopHook = NoopStaticHookTemplate<int, TestObject*, int>;
    using MyVoidNoopHook = NoopStaticHookTemplate<void, TestObject*>;

    class FakeClass {
      public:
        NOINLINE static bool OverrideIsAllowedReturnValue(TestObject* obj) {
            std::cout << "OverrideReturnValue()" << std::endl;
            bool result = false;
            KHook::SaveReturnValue(
                KHook::Action::Override,
                &result,
                sizeof(bool),
                (void*)KHook::init_operator<bool>,
                (void*)KHook::deinit_operator<bool>,
                false
            );
            return false;
        }

        NOINLINE static bool SupersedeIsAllowedReturnValue(TestObject* obj) {
            std::cout << "SupersedeReturnValue()" << std::endl;
            bool result = false;
            KHook::SaveReturnValue(
                KHook::Action::Supersede,
                &result,
                sizeof(bool),
                (void*)KHook::init_operator<bool>,
                (void*)KHook::deinit_operator<bool>,
                false
            );
            return false;
        }

        NOINLINE static int OverrideSetObjectValue(TestObject* obj, int value) {
            auto recall = reinterpret_cast<int(*)(TestObject*, int)>(
                KHook::DoRecall(
                    KHook::Action::Ignore,
                    nullptr,
                    0,
                    nullptr,
                    nullptr
                )
            );
            recall(obj, 1337);
            return 0;
        }

        NOINLINE static int SupersedeSetObjectValue(TestObject* obj, int value) {
            std::cout << "SupersedeSetObjectValue()" << std::endl;
            int newValue = 9001;
            KHook::SaveReturnValue(
                KHook::Action::Supersede,
                &newValue,
                sizeof(int),
                (void*)KHook::init_operator<int>,
                (void*)KHook::deinit_operator<int>,
                false
            );
            return 0;
        }

        NOINLINE static int HookInsideSetObjectValue(TestObject* obj, int value) {
            m_hookId = KHook::SetupHook(
                (void*)&HookedClass::IsAllowed,
                nullptr,
                (void*)&IsAllowedNoopHook::OnRemoved,
                (void*)&IsAllowedNoopHook::PrePostNoop,
                (void*)&IsAllowedNoopHook::PrePostNoop,
                (void*)&IsAllowedNoopHook::MakeReturn,
                (void*)&IsAllowedNoopHook::CallOriginal,
                false
            );
            KHook::SaveReturnValue(
                KHook::Action::Ignore,
                nullptr,
                0,
                nullptr,
                nullptr,
                false
            );
            return 0;
        }

        NOINLINE static void SupersedeMyVoid(TestObject* obj) {
            std::cout << "SupersedeMyVoid()" << std::endl;
            KHook::SaveReturnValue(
                KHook::Action::Supersede,
                nullptr,
                0,
                nullptr,
                nullptr,
                false
            );
        }
    };

  protected:
    void SetUp() override {
        obj = new TestObject();
    }

    void TearDown() override {
        if (obj) {
            delete obj;
            obj = nullptr;
        }
    }

    TestObject* obj = nullptr;
    static int m_hookId;
};

int StaticHookTest::m_hookId = KHook::INVALID_HOOK;

TEST_F(StaticHookTest, Noop) {
    int hookId = KHook::SetupHook(
        (void*)&HookedClass::IsAllowed,
        nullptr,
        (void*)&IsAllowedNoopHook::OnRemoved,
        (void*)&IsAllowedNoopHook::PrePostNoop,
        (void*)&IsAllowedNoopHook::PrePostNoop,
        (void*)&IsAllowedNoopHook::MakeReturn,
        (void*)&IsAllowedNoopHook::CallOriginal,
        false
    );

    ASSERT_NE(hookId, KHook::INVALID_HOOK) << "Hook setup should succeed";

    testing::internal::CaptureStdout();

    bool overriddenResult = HookedClass::IsAllowed(obj);

    KHook::RemoveHook(hookId, false);

    bool originalResult = HookedClass::IsAllowed(obj);

    std::string output = testing::internal::GetCapturedStdout();

    std::ostringstream expected;
    expected << "PrePostNoop()" << std::endl;
    expected << "CallOriginal()" << std::endl;
    expected << "HookedClass::IsAllowed()" << std::endl;
    expected << "PrePostNoop()" << std::endl;
    expected << "MakeReturn()" << std::endl;
    expected << "OnRemoved(" << std::dec << hookId << ")" << std::endl;
    expected << "HookedClass::IsAllowed()" << std::endl;

    EXPECT_EQ(output, expected.str())
        << "Callback functions should be called in the expected order";
    EXPECT_TRUE(overriddenResult)
        << "Method should return original value when hooked";
    EXPECT_TRUE(originalResult)
        << "Method should return original value after hook removal";
}

TEST_F(StaticHookTest, NoopVoid) {
    int hookId = KHook::SetupHook(
        (void*)&HookedClass::MyVoid,
        nullptr,
        (void*)&MyVoidNoopHook::OnRemoved,
        (void*)&MyVoidNoopHook::PrePostNoop,
        (void*)&MyVoidNoopHook::PrePostNoop,
        (void*)&MyVoidNoopHook::MakeReturn,
        (void*)&MyVoidNoopHook::CallOriginal,
        false
    );

    ASSERT_NE(hookId, KHook::INVALID_HOOK) << "Hook setup should succeed";

    testing::internal::CaptureStdout();

    HookedClass::MyVoid(obj);

    KHook::RemoveHook(hookId, false);

    HookedClass::MyVoid(obj);

    std::string output = testing::internal::GetCapturedStdout();

    std::ostringstream expected;
    expected << "PrePostNoop()" << std::endl;
    expected << "CallOriginal()" << std::endl;
    expected << "HookedClass::MyVoid()" << std::endl;
    expected << "PrePostNoop()" << std::endl;
    expected << "MakeReturn()" << std::endl;
    expected << "OnRemoved(" << std::dec << hookId << ")" << std::endl;
    expected << "HookedClass::MyVoid()" << std::endl;

    ASSERT_EQ(output, expected.str())
        << "Callback functions should be called in the expected order";
}

TEST_F(StaticHookTest, OverrideReturnValuePre) {
    int hookId = KHook::SetupHook(
        (void*)&HookedClass::IsAllowed,
        nullptr,
        (void*)&IsAllowedNoopHook::OnRemoved,
        (void*)&FakeClass::OverrideIsAllowedReturnValue,
        (void*)&IsAllowedNoopHook::PrePostNoop,
        (void*)&IsAllowedNoopHook::MakeReturn,
        (void*)&IsAllowedNoopHook::CallOriginal,
        false
    );

    ASSERT_NE(hookId, KHook::INVALID_HOOK) << "Hook setup should succeed";

    bool result = HookedClass::IsAllowed(obj);
    EXPECT_FALSE(result) << "Method should return false when hooked";

    KHook::RemoveHook(hookId, false);

    result = HookedClass::IsAllowed(obj);
    EXPECT_TRUE(result) << "Method should return true after hook removal";
}

TEST_F(StaticHookTest, OverrideReturnValuePost) {
    int hookId = KHook::SetupHook(
        (void*)&HookedClass::IsAllowed,
        nullptr,
        (void*)&IsAllowedNoopHook::OnRemoved,
        (void*)&IsAllowedNoopHook::PrePostNoop,
        (void*)&FakeClass::OverrideIsAllowedReturnValue,
        (void*)&IsAllowedNoopHook::MakeReturn,
        (void*)&IsAllowedNoopHook::CallOriginal,
        false
    );

    ASSERT_NE(hookId, KHook::INVALID_HOOK) << "Hook setup should succeed";

    bool result = HookedClass::IsAllowed(obj);
    EXPECT_FALSE(result) << "Method should return false when hooked";

    KHook::RemoveHook(hookId, false);

    result = HookedClass::IsAllowed(obj);
    EXPECT_TRUE(result) << "Method should return true after hook removal";
}

TEST_F(StaticHookTest, SupersedeReturnValue) {
    int hookId = KHook::SetupHook(
        (void*)&HookedClass::IsAllowed,
        nullptr,
        (void*)&IsAllowedNoopHook::OnRemoved,
        (void*)&FakeClass::SupersedeIsAllowedReturnValue,
        (void*)&IsAllowedNoopHook::PrePostNoop,
        (void*)&IsAllowedNoopHook::MakeReturn,
        (void*)&IsAllowedNoopHook::CallOriginal,
        false
    );

    ASSERT_NE(hookId, KHook::INVALID_HOOK) << "Hook setup should succeed";

    testing::internal::CaptureStdout();

    bool overriddenResult = HookedClass::IsAllowed(obj);

    KHook::RemoveHook(hookId, false);

    bool originalResult = HookedClass::IsAllowed(obj);

    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(output.find("CallOriginal()"), std::string::npos)
        << "Original method should not be called";
    EXPECT_FALSE(overriddenResult) << "Method should return false when hooked";
    EXPECT_TRUE(originalResult)
        << "Method should return true after hook removal";
}

TEST_F(StaticHookTest, SupersedeVoidReturnValue) {
    int hookId = KHook::SetupHook(
        (void*)&HookedClass::MyVoid,
        nullptr,
        (void*)&MyVoidNoopHook::OnRemoved,
        (void*)&FakeClass::SupersedeMyVoid,
        (void*)&MyVoidNoopHook::PrePostNoop,
        (void*)&MyVoidNoopHook::MakeReturn,
        (void*)&MyVoidNoopHook::CallOriginal,
        false
    );

    ASSERT_NE(hookId, KHook::INVALID_HOOK) << "Hook setup should succeed";

    testing::internal::CaptureStdout();

    HookedClass::MyVoid(obj);

    std::string output = testing::internal::GetCapturedStdout();

    {
        std::ostringstream expected;
        expected << "SupersedeMyVoid()" << std::endl;
        expected << "PrePostNoop()" << std::endl;
        expected << "MakeReturn()" << std::endl;
        ASSERT_EQ(expected.str(), output)
            << "Callbacks should be called in the correct order";
    }

    KHook::RemoveHook(hookId, false);

    testing::internal::CaptureStdout();

    HookedClass::MyVoid(obj);

    output = testing::internal::GetCapturedStdout();

    {
        std::ostringstream expected;
        expected << "HookedClass::MyVoid()" << std::endl;
        ASSERT_EQ(expected.str(), output)
            << "No callbacks should be called after hook removal";
    }
}

TEST_F(StaticHookTest, OverrideParameterWithRecall) {
    int hookId = KHook::SetupHook(
        (void*)&HookedClass::SetObjectValue,
        nullptr,
        (void*)&SetObjectValueNoopHook::OnRemoved,
        (void*)&FakeClass::OverrideSetObjectValue,
        (void*)&SetObjectValueNoopHook::PrePostNoop,
        (void*)&SetObjectValueNoopHook::MakeReturn,
        (void*)&SetObjectValueNoopHook::CallOriginal,
        false
    );

    ASSERT_NE(hookId, KHook::INVALID_HOOK) << "Hook setup should succeed";

    int result = HookedClass::SetObjectValue(obj, 0xDEADBEEF);
    EXPECT_EQ(obj->m_testValue, 1337)
        << "Method should set value to hooked value";
    EXPECT_EQ(result, 1337) << "Method should return hooked value";

    KHook::RemoveHook(hookId, false);

    result = HookedClass::SetObjectValue(obj, 0xDEADBEEF);
    EXPECT_EQ(obj->m_testValue, 0xDEADBEEF)
        << "Method should set value to original value after hook removal";
    EXPECT_EQ(result, 0xDEADBEEF)
        << "Method should return original value after hook removal";
}

TEST_F(StaticHookTest, SupersedeThenNoopVoidHooksOnSameFunction) {
    int firstHookId = KHook::SetupHook(
        (void*)&HookedClass::MyVoid,
        nullptr,
        (void*)&MyVoidNoopHook::OnRemoved,
        (void*)&FakeClass::SupersedeMyVoid,
        (void*)&MyVoidNoopHook::PrePostNoop,
        (void*)&MyVoidNoopHook::MakeReturn,
        (void*)&MyVoidNoopHook::CallOriginal,
        false
    );

    ASSERT_NE(firstHookId, KHook::INVALID_HOOK) << "Hook setup should succeed";

    int secondHookId = KHook::SetupHook(
        (void*)&HookedClass::MyVoid,
        nullptr,
        (void*)&MyVoidNoopHook::OnRemoved,
        (void*)&MyVoidNoopHook::PrePostNoop,
        (void*)&MyVoidNoopHook::PrePostNoop,
        (void*)&MyVoidNoopHook::MakeReturn,
        (void*)&MyVoidNoopHook::CallOriginal,
        false
    );

    ASSERT_NE(secondHookId, KHook::INVALID_HOOK) << "Hook setup should succeed";

    testing::internal::CaptureStdout();
    HookedClass::MyVoid(obj);
    std::string output = testing::internal::GetCapturedStdout();

    {
        std::ostringstream expected;
        expected << "PrePostNoop()" << std::endl;
        expected << "SupersedeMyVoid()" << std::endl;
        expected << "PrePostNoop()" << std::endl;
        expected << "PrePostNoop()" << std::endl;
        expected << "MakeReturn()" << std::endl;

        ASSERT_EQ(expected.str(), output)
            << "Callback functions should be called in the correct order";
    }

    KHook::RemoveHook(firstHookId, false);

    testing::internal::CaptureStdout();
    HookedClass::MyVoid(obj);
    output = testing::internal::GetCapturedStdout();

    {
        std::ostringstream expected;
        expected << "PrePostNoop()" << std::endl;
        expected << "CallOriginal()" << std::endl;
        expected << "HookedClass::MyVoid()" << std::endl;
        expected << "PrePostNoop()" << std::endl;
        expected << "MakeReturn()" << std::endl;

        ASSERT_EQ(expected.str(), output)
            << "Callback functions should be called in the correct order";
    }
}

TEST_F(StaticHookTest, MultipleNoopVoidHooksOnSameFunction) {
    int firstHookId = KHook::SetupHook(
        (void*)&HookedClass::MyVoid,
        nullptr,
        (void*)&MyVoidNoopHook::OnRemoved,
        (void*)&MyVoidNoopHook::PrePostNoop,
        (void*)&MyVoidNoopHook::PrePostNoop,
        (void*)&MyVoidNoopHook::MakeReturn,
        (void*)&MyVoidNoopHook::CallOriginal,
        false
    );

    ASSERT_NE(firstHookId, KHook::INVALID_HOOK) << "Hook setup should succeed";

    int secondHookId = KHook::SetupHook(
        (void*)&HookedClass::MyVoid,
        nullptr,
        (void*)&MyVoidNoopHook::OnRemoved,
        (void*)&MyVoidNoopHook::PrePostNoop,
        (void*)&MyVoidNoopHook::PrePostNoop,
        (void*)&MyVoidNoopHook::MakeReturn,
        (void*)&MyVoidNoopHook::CallOriginal,
        false
    );

    ASSERT_NE(secondHookId, KHook::INVALID_HOOK) << "Hook setup should succeed";

    testing::internal::CaptureStdout();
    HookedClass::MyVoid(obj);
    std::string output = testing::internal::GetCapturedStdout();

    {
        std::ostringstream expected;
        expected << "PrePostNoop()" << std::endl;
        expected << "PrePostNoop()" << std::endl;
        expected << "CallOriginal()" << std::endl;
        expected << "HookedClass::MyVoid()" << std::endl;
        expected << "PrePostNoop()" << std::endl;
        expected << "PrePostNoop()" << std::endl;
        expected << "MakeReturn()" << std::endl;

        ASSERT_EQ(expected.str(), output)
            << "Callback functions should be called in the correct order";
    }

    KHook::RemoveHook(firstHookId, false);

    testing::internal::CaptureStdout();
    HookedClass::MyVoid(obj);
    output = testing::internal::GetCapturedStdout();

    {
        std::ostringstream expected;
        expected << "PrePostNoop()" << std::endl;
        expected << "CallOriginal()" << std::endl;
        expected << "HookedClass::MyVoid()" << std::endl;
        expected << "PrePostNoop()" << std::endl;
        expected << "MakeReturn()" << std::endl;

        ASSERT_EQ(expected.str(), output)
            << "Callback functions should be called in the correct order";
    }
}

TEST_F(StaticHookTest, SupersedeThenNoopHooksOnSameFunction) {
    int firstHookId = KHook::SetupHook(
        (void*)&HookedClass::SetObjectValue,
        nullptr,
        (void*)&SetObjectValueNoopHook::OnRemoved,
        (void*)&FakeClass::SupersedeSetObjectValue,
        (void*)&SetObjectValueNoopHook::PrePostNoop,
        (void*)&SetObjectValueNoopHook::MakeReturn,
        (void*)&SetObjectValueNoopHook::CallOriginal,
        false
    );

    ASSERT_NE(firstHookId, KHook::INVALID_HOOK) << "Hook setup should succeed";

    int secondHookId = KHook::SetupHook(
        (void*)&HookedClass::SetObjectValue,
        nullptr,
        (void*)&SetObjectValueNoopHook::OnRemoved,
        (void*)&SetObjectValueNoopHook::PrePostNoop,
        (void*)&SetObjectValueNoopHook::PrePostNoop,
        (void*)&SetObjectValueNoopHook::MakeReturn,
        (void*)&SetObjectValueNoopHook::CallOriginal,
        false
    );

    ASSERT_NE(secondHookId, KHook::INVALID_HOOK) << "Hook setup should succeed";

    obj->m_testValue = 0x9600;

    testing::internal::CaptureStdout();
    int result = HookedClass::SetObjectValue(obj, 0xDEADBEEF);
    std::string output = testing::internal::GetCapturedStdout();

    {
        std::ostringstream expected;
        expected << "PrePostNoop()" << std::endl;
        expected << "SupersedeSetObjectValue()" << std::endl;
        expected << "PrePostNoop()" << std::endl;
        expected << "PrePostNoop()" << std::endl;
        expected << "MakeReturn()" << std::endl;

        ASSERT_EQ(expected.str(), output)
            << "Callback functions should be called in the correct order";
    }

    KHook::RemoveHook(firstHookId, false);

    obj->m_testValue = 0x9600;

    testing::internal::CaptureStdout();
    result = HookedClass::SetObjectValue(obj, 0xDEADBEEF);
    output = testing::internal::GetCapturedStdout();

    {
        std::ostringstream expected;
        expected << "PrePostNoop()" << std::endl;
        expected << "CallOriginal()" << std::endl;
        expected << "HookedClass::SetObjectValue()" << std::endl;
        expected << "PrePostNoop()" << std::endl;
        expected << "MakeReturn()" << std::endl;

        ASSERT_EQ(expected.str(), output)
            << "Callback functions should be called in the correct order";
    }

    KHook::RemoveHook(secondHookId, false);

    obj->m_testValue = 0x9600;

    testing::internal::CaptureStdout();
    result = HookedClass::SetObjectValue(obj, 0xDEADBEEF);
    output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(output.find("CallOriginal()"), std::string::npos)
        << "CallOriginal() should not be called after all hooks removed";
    EXPECT_EQ(obj->m_testValue, 0xDEADBEEF)
        << "Method should set value to original value after recall hook "
           "removal";
    EXPECT_EQ(result, 0xDEADBEEF)
        << "Method should return original value after recall hook removal";
}

TEST_F(StaticHookTest, HookIsAllowedInsideSetObjectValue) {
    m_hookId = KHook::INVALID_HOOK;

    int hookId = KHook::SetupHook(
        (void*)&HookedClass::SetObjectValue,
        nullptr,
        (void*)&SetObjectValueNoopHook::OnRemoved,
        (void*)&FakeClass::HookInsideSetObjectValue,
        (void*)&SetObjectValueNoopHook::PrePostNoop,
        (void*)&SetObjectValueNoopHook::MakeReturn,
        (void*)&SetObjectValueNoopHook::CallOriginal,
        false
    );

    ASSERT_NE(hookId, KHook::INVALID_HOOK) << "Hook setup should succeed";

    int firstResult = HookedClass::SetObjectValue(obj, 42);

    EXPECT_EQ(obj->m_testValue, 42)
        << "SetObjectValue should set value to 42 (original behavior)";
    EXPECT_EQ(firstResult, 42)
        << "SetObjectValue should return 42 (original value)";
    ASSERT_NE(m_hookId, KHook::INVALID_HOOK)
        << "IsAllowed hook should have been set up inside SetObjectValue";

    bool result = HookedClass::IsAllowed(obj);
    EXPECT_TRUE(result) << "IsAllowed should return true (original value)";
}