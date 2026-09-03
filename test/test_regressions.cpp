#include <gtest/gtest.h>
#include <iostream>
#include <khook.hpp>
#include <vector>
#include "helpers.hpp"

namespace {
	// Tests high-level classes Function, Virtual, and Member and verifies
	// that, after hooking, that all arguments and return values are 
	// passed to the detour callback and the original function unchanged,
	// especially when calling conventions dictate that arguments be passed
	// onto the stack.
	// 
	// This verifies that enough of the stack is being copied by the hook 
	// classes, prior to execution being passed onto callbacks and the 
	// originally hooked function.
	namespace stack_copy_size {
		template <std::uint32_t INDEX, typename RETURN, typename... ARGS>
		class TestCase;

		using Case1 = TestCase<1, void, bool, bool, bool, void*>;
		using Case2 = TestCase<2, void,
			void*, int, int, const char*,
			float, float, int, int, int,
			void*, void*, void*, bool, float, int>;
		using Case3 = TestCase<3, void, bool, void*>;
		using Case4 = TestCase<4, void, bool, bool, bool, bool, bool, int>;
		using Case5 = TestCase<5, int, int>;
		using Case6 = TestCase<6, std::tuple<void*, void*, void*>, int, int>;
		using Case7 = TestCase<7, std::tuple<void*, void*, void*>>;
		using Case8 = TestCase<8, void,
			void*, int, int, const char*,
			float, float, int, int, int,
			void*, void*, void*, bool, float, int,
			bool, bool, bool, bool, int>;
		using AllCases = ::testing::Types<
			Case1,
			Case2,
			Case3,
			Case4,
			Case5,
			Case6,
			Case7,
			Case8>;

		template <std::uint32_t INDEX, typename RETURN, typename... ARGS>
		class TestCase {
		public:
			using Self = TestCase<INDEX, RETURN, ARGS...>;
			using ArgsTuple = std::tuple<ARGS...>;
			using Return = RETURN;
			using FunctionHook = ::KHook::Function<RETURN, ARGS...>;
			using MemberHook = ::KHook::Member<Self, RETURN, ARGS...>;
			using VirtualHook = ::KHook::Virtual<Self, RETURN, ARGS...>;

			static constexpr typename Self::ArgsTuple GetExpectedArgs() {
				if constexpr(std::is_same_v<Self, Case1>) {
					return std::make_tuple(false, true, false, nullptr);
				}
				else if constexpr(std::is_same_v<Self, Case2>) {
					return std::make_tuple(
						reinterpret_cast<void*>((std::uintptr_t)0xDEADBEEF),
						17,
						5,
						"42",
						0.25f,
						0.68f,
						19,
						123,
						7,
						nullptr,
						reinterpret_cast<void*>((std::uintptr_t)0x1002),
						reinterpret_cast<void*>((std::uintptr_t)0x80099),
						false,
						3.5f,
						1337);
				}
				else if constexpr(std::is_same_v<Self, Case3>) {
					return std::make_tuple(true, reinterpret_cast<void*>((std::uintptr_t)0xDEADBEEF));
				}
				else if constexpr(std::is_same_v<Self, Case4>) {
					return std::make_tuple(true, false, false, true, true, 1337);
				}
				else if constexpr(std::is_same_v<Self, Case5>) {
					return std::make_tuple(12);
				}
				else if constexpr(std::is_same_v<Self, Case6>) {
					return std::make_tuple(45, 22);
				}
				else if constexpr(std::is_same_v<Self, Case7>) {
					return std::make_tuple();
				}
				else if constexpr(std::is_same_v<Self, Case8>) {
					return std::make_tuple(
						reinterpret_cast<void*>((std::uintptr_t)0xDEADBEEF),
						17,
						5,
						"42",
						0.25f,
						0.68f,
						19,
						123,
						7,
						nullptr,
						reinterpret_cast<void*>((std::uintptr_t)0x1002),
						reinterpret_cast<void*>((std::uintptr_t)0x80099),
						false,
						3.5f,
						1337,
						true,
						false,
						true,
						true,
						70023);
				}
				throw std::runtime_error("expected arguments not set for test case");
			}

			static constexpr RETURN GetExpectedReturn() {
				if constexpr(std::is_void_v<RETURN>) {
					return;
				}
				else if constexpr(std::is_same_v<Self, Case5>) {
					return 25;
				}
				else if constexpr(std::is_same_v<Self, Case6>) {
					return std::make_tuple(
						reinterpret_cast<void*>((std::uintptr_t)0xDEADBEEF),
						reinterpret_cast<void*>((std::uintptr_t)0x1002),
						reinterpret_cast<void*>((std::uintptr_t)0x80099)
					);
				}
				else if constexpr(std::is_same_v<Self, Case7>) {
					return std::make_tuple(
						reinterpret_cast<void*>((std::uintptr_t)0xDEADBEEF),
						reinterpret_cast<void*>((std::uintptr_t)0xBEEFDEAD),
						reinterpret_cast<void*>((std::uintptr_t)0x1200056)
					);
				}
				throw std::runtime_error("expected return not set for test case");
			}

			TestCase() : _orig_calls(0), _hook_calls(0) {
				_expected_args = GetExpectedArgs();
			}

			static NOINLINE RETURN FunctionCall(ARGS... args) {
				SCOPED_TRACE("original method");
				auto actual = std::make_tuple(args...);
				_orig_static_calls++;
				EXPECT_EQ(_expected_args, actual);
				if constexpr(!std::is_void_v<RETURN>) {
					return GetExpectedReturn();
				}
			}

			NOINLINE RETURN MemberCall(ARGS... args) {
				SCOPED_TRACE("original method");
				auto actual = std::make_tuple(args...);
				_orig_calls++;
				EXPECT_EQ(_expected_args, actual);
				if constexpr(!std::is_void_v<RETURN>) {
					return GetExpectedReturn();
				}
			}

			virtual RETURN VirtualCall(ARGS... args) {
				SCOPED_TRACE("original method");
				auto actual = std::make_tuple(args...);
				_orig_calls++;
				EXPECT_EQ(_expected_args, actual);
				if constexpr(!std::is_void_v<RETURN>) {
					return GetExpectedReturn();
				}
			}

			static KHook::Return<RETURN> Callback(Self* hookedThis, ARGS... args) {
				SCOPED_TRACE("hook callback");
				auto actual = std::make_tuple(args...);
				hookedThis->_hook_calls++;
				EXPECT_EQ(hookedThis->_expected_args, actual);
				return { KHook::Action::Ignore };
			}

			KHook::Return<RETURN> Callback(ARGS... args) {
				SCOPED_TRACE("hook callback");
				auto actual = std::make_tuple(args...);
				_hook_calls++;
				EXPECT_EQ(_expected_args, actual);
				return { KHook::Action::Ignore };
			}
		public:
			static std::uint32_t GetNumOrigStaticCalls() { return _orig_static_calls; }
			std::uint32_t GetNumOrigCalls() const { return _orig_calls; }
			std::uint32_t GetNumHookCalls() const { return _hook_calls; }
		private:
			static inline std::uint32_t _orig_static_calls = 0;
			static inline ArgsTuple _expected_args;
			std::uint32_t _orig_calls;
			std::uint32_t _hook_calls;
		};

		template <typename CASE>
		class Regression_StackCopySizeTests : public ::testing::Test {
		protected:
			void SetUp() override {
				_case = new CASE;
			}

			void TearDown() override {
				delete _case;
			}

			CASE* _case;
		};

		TYPED_TEST_SUITE(Regression_StackCopySizeTests, AllCases);

		TYPED_TEST(Regression_StackCopySizeTests, Function) {
			typename TypeParam::FunctionHook hook(&TypeParam::FunctionCall, this->_case, &TypeParam::Callback, nullptr);

			auto expected = TypeParam::GetExpectedArgs();
			std::apply([&](auto&&... args) {
				if constexpr(std::is_void_v<typename TypeParam::Return>) {
					TypeParam::FunctionCall(std::forward<decltype(args)>(args)...);
				}
				else {
					auto expected_ret = TypeParam::GetExpectedReturn();
					auto actual_ret = TypeParam::FunctionCall(std::forward<decltype(args)>(args)...);
					EXPECT_EQ(expected_ret, actual_ret);
				}
			}, expected);

			EXPECT_EQ(this->_case->GetNumHookCalls(), 1) << "Pre-hook should run exactly once";
			EXPECT_EQ(this->_case->GetNumOrigStaticCalls(), 1) << "Original method should still run after Ignore";
		}

		TYPED_TEST(Regression_StackCopySizeTests, Member) {
			typename TypeParam::MemberHook hook(&TypeParam::MemberCall, &TypeParam::Callback, nullptr);

			auto expected = TypeParam::GetExpectedArgs();
			std::apply([&](auto&&... args) {
				if constexpr(std::is_void_v<typename TypeParam::Return>) {
					this->_case->MemberCall(std::forward<decltype(args)>(args)...);
				}
				else {
					auto expected_ret = TypeParam::GetExpectedReturn();
					auto actual_ret = this->_case->MemberCall(std::forward<decltype(args)>(args)...);
					EXPECT_EQ(expected_ret, actual_ret);
				}
			}, expected);

			EXPECT_EQ(this->_case->GetNumHookCalls(), 1) << "Pre-hook should run exactly once";
			EXPECT_EQ(this->_case->GetNumOrigCalls(), 1) << "Original method should still run after Ignore";
		}

		TYPED_TEST(Regression_StackCopySizeTests, Virtual) {
			typename TypeParam::VirtualHook hook(&TypeParam::VirtualCall, &TypeParam::Callback, nullptr);
			hook.Add(this->_case);

			auto expected = TypeParam::GetExpectedArgs();
			std::apply([&](auto&&... args) {
				if constexpr(std::is_void_v<typename TypeParam::Return>) {
					this->_case->VirtualCall(std::forward<decltype(args)>(args)...);
				}
				else {
					auto expected_ret = TypeParam::GetExpectedReturn();
					auto actual_ret = this->_case->VirtualCall(std::forward<decltype(args)>(args)...);
					EXPECT_EQ(expected_ret, actual_ret);
				}
			}, expected);

			EXPECT_EQ(this->_case->GetNumHookCalls(), 1) << "Pre-hook should run exactly once";
			EXPECT_EQ(this->_case->GetNumOrigCalls(), 1) << "Original method should still run after Ignore";
		}
	}
}