#include <gtest/gtest.h>
#include <khook.hpp>
#include "helpers.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <thread>

class HookRemovalTests : public ::testing::Test {
protected:
    class TargetClass {
    public:
        void TargetMethod(int& value) {
            value = 0xDEADBEEF;
        }
    };

	static KHook::Return<void> HookMethod(TargetClass* _this, int& value) {
        std::cout << "Here";
        return {KHook::Action::Ignore};
    }

	static void HookRemove(KHook::HookID_t id) {
		KHook::GetContext<HookRemovalTests>()->hook_removed = true;
	}

	static void HookRemoveCtx(KHook::HookID_t id, HookRemovalTests* ctx) {
		ctx->hook_remove_second = true;
	}

	static NOINLINE void TargetFunction(int& value) {
		value = 0xDEADBEEF;
	}

	static void HookPreRemove(int& value) {
		auto self = KHook::GetContext<HookRemovalTests>();
		if (++self->pre_calls == 1) {
			KHook::RemoveHook(self->first_hook, true);
			// Let the delete worker pick up the first removal while the detour is still running
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			KHook::RemoveHook(self->second_hook, true);
		}
		KHook::SaveReturnValue(KHook::Action::Ignore, nullptr, 0, nullptr, nullptr, false);
	}

	static void HookCallOriginal(int& value) {
		reinterpret_cast<void (*)(int&)>(KHook::GetOriginalFunction())(value);
		KHook::SaveReturnValue(KHook::Action::Ignore, nullptr, 0, nullptr, nullptr, true);
	}

	static void HookMakeReturn(int& value) {
		KHook::DestroyReturnValue();
	}

	bool hook_removed = false;
	bool hook_remove_second = false;
	int pre_calls = 0;
	KHook::HookID_t first_hook = KHook::INVALID_HOOK;
	KHook::HookID_t second_hook = KHook::INVALID_HOOK;
};

TEST_F(HookRemovalTests, RemoveAsync) {
    TargetClass instance;

	auto id = KHook::SetupHook(KHook::ExtractMFP(&TargetClass::TargetMethod), this, (void*)&HookRemove, (void*)&HookMethod, nullptr, nullptr, nullptr, 0x20, false);
	EXPECT_NE(id, KHook::INVALID_HOOK);

	KHook::RemoveHook(id, true, reinterpret_cast<void(*)(KHook::HookID_t, void*)>(&HookRemoveCtx), this);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	EXPECT_EQ(this->hook_removed, true);
	EXPECT_EQ(this->hook_remove_second, true);
}

TEST_F(HookRemovalTests, RemoveAsyncNoContext) {
    TargetClass instance;

	auto id = KHook::SetupHook(KHook::ExtractMFP(&TargetClass::TargetMethod), nullptr, nullptr, (void*)&HookMethod, nullptr, nullptr, nullptr, 0x20, false);
	EXPECT_NE(id, KHook::INVALID_HOOK);

	KHook::RemoveHook(id, true, reinterpret_cast<void(*)(KHook::HookID_t, void*)>(&HookRemoveCtx), this);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	EXPECT_EQ(this->hook_removed, false);
	EXPECT_EQ(this->hook_remove_second, true);
}

TEST_F(HookRemovalTests, RemoveAsyncInsideDetour) {
	first_hook = KHook::SetupHook((void*)&TargetFunction, this, nullptr, (void*)&HookPreRemove, nullptr, (void*)&HookMakeReturn, (void*)&HookCallOriginal, 0x20, false);
	ASSERT_NE(first_hook, KHook::INVALID_HOOK) << "Hook setup should succeed";
	second_hook = KHook::SetupHook((void*)&TargetFunction, this, nullptr, (void*)&HookPreRemove, nullptr, (void*)&HookMakeReturn, (void*)&HookCallOriginal, 0x20, false);
	ASSERT_NE(second_hook, KHook::INVALID_HOOK) << "Hook setup should succeed";

	auto caller = std::async(std::launch::async, [] {
		int value = 0x1337;
		TargetFunction(value);
		return value;
	});
	if (caller.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
		ADD_FAILURE() << "Hook removal deadlocked";
		std::fflush(stdout);
		std::_Exit(EXIT_FAILURE);
	}

	EXPECT_EQ(caller.get(), 0xDEADBEEF) << "Behaviour of hooked function was modified";
	EXPECT_EQ(this->pre_calls, 2) << "Both pre-hooks should run while removal is pending";

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	int value = 0x1337;
	TargetFunction(value);

	EXPECT_EQ(value, 0xDEADBEEF) << "Behaviour of function was modified after hook removal";
	EXPECT_EQ(this->pre_calls, 2) << "Hooks should be removed after the detour returns";
}
