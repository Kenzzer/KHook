/* ======== KHook ========
* Copyright (C) 2025
* No warranties of any kind
*
* License: ZLIB
*
* Author(s): Benoist "Kenzzer" ANDRÉ
* ============================
*/
#pragma once
#include <cstdint>
#include <mutex>
#include <shared_mutex>	
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <condition_variable>
#include <thread>

#include "safetyhook.hpp"

#ifdef KHOOK_X64
#undef KHOOK_X64
#endif

#if defined(__x86_64__) || defined(_WIN64)
#define KHOOK_X64
#endif

#ifdef KHOOK_X64
#include "khook/asm/x86_64.hpp"
#else
#include "khook/asm/x86.hpp"
#endif
#include "khook.hpp"

namespace KHook {
	// A general purpose, thread-safe, detour, it functions in a very straight foward manner :
	//
	// [   DETOUR START]
	// [jit]
	//
	// TO-DO describe with a graph
	// 
	// [jit]
	// [   DETOUR END]
	class DetourCapsule {
	public:
#ifdef KHOOK_X64
		using AsmJit = Asm::x86_64_Jit;
#else
		using AsmJit = Asm::x86_Jit;
#endif

		DetourCapsule();
		~DetourCapsule();

		struct InsertHookDetails {
			std::uintptr_t hook_ptr;
			std::uintptr_t hook_fn_remove;

			std::uintptr_t fn_make_pre;
			std::uintptr_t fn_make_post;

			std::uintptr_t fn_make_call_original;
			std::uintptr_t fn_make_return;

			std::uintptr_t original_return_ptr;
			std::uintptr_t override_return_ptr;
		};

		bool InsertHook(HookID_t, const InsertHookDetails&);
		void RemoveHook(HookID_t);

		void* GetOriginal() {
			std::shared_lock lock(_detour_mutex);
			return reinterpret_cast<void*>(_original_function);
		}

		bool SetupAddress(void* detour_address) {
			auto result = safetyhook::InlineHook::create(detour_address, _jit_func_ptr);
			if (result) {
				// Successfully detour'd the function
				_safetyhook = std::move(result.value());
				_original_function = reinterpret_cast<std::uintptr_t>(_safetyhook.original<void*>());
				return true;
			}
			// Safetyhook setup failed
			return false;
		}

		bool SetupVirtual(void** vtable, int index) {
			auto entry = vtable + index;
			KHook::Memory::SetAccess(entry, sizeof(void*), KHook::Memory::Flags::EXECUTE | KHook::Memory::Flags::READ | KHook::Memory::Flags::WRITE);
			_original_function = reinterpret_cast<std::uintptr_t>(*entry);
			*entry = reinterpret_cast<void*>(_jit_func_ptr);
			KHook::Memory::SetAccess(entry, sizeof(void*), KHook::Memory::Flags::EXECUTE | KHook::Memory::Flags::READ);
			// There's no way to predict whether or not the above code will crash, just always return true
			return true;
		}

	public:
		struct LinkedList {
			LinkedList(LinkedList* p, LinkedList* n) : prev(p), next(n) {
				if (p) {
					p->next = this;
				}
				if (n) {
					n->prev = this;
				}
			}
			~LinkedList() {
				if (prev) {
					prev->next = this->next;
				}
				if (next) {
					next->prev = this->prev;
				}
			}

			void CopyDetails(const InsertHookDetails& details) {
				hook_ptr = details.hook_ptr;
				hook_fn_remove = details.hook_fn_remove;

				fn_make_pre = details.fn_make_pre;
				fn_make_post = details.fn_make_post;

				fn_make_call_original = details.fn_make_call_original;
				fn_make_return = details.fn_make_return;
			}

			LinkedList* prev = nullptr;
			LinkedList* next = nullptr;
			std::uintptr_t hook_ptr;
			std::uintptr_t hook_fn_remove;

			std::uintptr_t fn_make_pre;
			std::uintptr_t fn_make_post;

			std::uintptr_t fn_make_call_original;
			std::uintptr_t fn_make_return;
		};
		// Always safe to read
		bool _in_deletion;

		// Detour callbacks
		std::shared_mutex _detour_mutex;

		// Everything below can only be modified if you own the mutex above
		std::unordered_map<HookID_t, std::unique_ptr<LinkedList>> _callbacks;
		LinkedList* _start_callbacks;
		LinkedList* _end_callbacks;

		// Detour business logic
		AsmJit _jit;
		std::uintptr_t _jit_func_ptr;

		// Detour details
		std::uintptr_t _original_function;
		std::uint32_t _stack_size;

		// Detour library details
		safetyhook::InlineHook _safetyhook;
	};
}