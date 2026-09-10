#include <gtest/gtest.h>
#include <khook.hpp>

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

	bool hook_removed = false;
	bool hook_remove_second = false;
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