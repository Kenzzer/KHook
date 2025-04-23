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

		DetourCapsule(void* detour_address);
		~DetourCapsule();

		struct InsertHookDetails {
			std::uintptr_t hook_ptr;
			KHook::Action* hook_action;
			std::uintptr_t hook_fn_remove;

			std::uintptr_t fn_make_pre;
			std::uintptr_t fn_make_post;

			std::uintptr_t fn_make_call_original;
			std::uintptr_t fn_make_original_return;
			std::uintptr_t fn_make_override_return;

			std::uintptr_t original_return_ptr;
			std::uintptr_t override_return_ptr;
		};

		void InsertHook(HookID_t, InsertHookDetails, bool);
		void RemoveHook(HookID_t, bool);

	private:
		enum class CBAction : std::uint8_t {
			ADD = 1,
			REMOVE = 2
		};
		std::vector<CBAction>& _GetWriteCallback(void* func);
		
		// Detour pending modifications
		bool _terminate_edit_thread;
		std::mutex _async_mutex;
		std::thread _edit_thread;
		std::condition_variable _cv_edit;
		std::unordered_map<HookID_t, InsertHookDetails> _insert_hooks;
		std::unordered_set<HookID_t> _delete_hooks;
		void _EditThread();

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

			void CopyDetails(InsertHookDetails details) {
				hook_ptr = details.hook_ptr;
				hook_action = details.hook_action;
				hook_fn_remove = details.hook_fn_remove;

				fn_make_pre = details.fn_make_pre;
				fn_make_post = details.fn_make_post;

				fn_make_call_original = details.fn_make_call_original;
				fn_make_original_return = details.fn_make_original_return;
				fn_make_override_return = details.fn_make_override_return;

				original_return_ptr = details.original_return_ptr;
				override_return_ptr = details.override_return_ptr;
			}

			LinkedList* prev = nullptr;
			LinkedList* next = nullptr;
			std::uintptr_t hook_ptr;
			KHook::Action* hook_action;
			std::uintptr_t hook_fn_remove;

			std::uintptr_t fn_make_pre;
			std::uintptr_t fn_make_post;

			std::uintptr_t fn_make_call_original;
			std::uintptr_t fn_make_original_return;
			std::uintptr_t fn_make_override_return;

			std::uintptr_t original_return_ptr;
			std::uintptr_t override_return_ptr;
		};
		// Detour callbacks
		std::shared_mutex _detour_mutex;
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