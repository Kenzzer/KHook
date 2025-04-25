#include "detour.hpp"

#include <stack>
#include <iostream>

namespace KHook {

#define STACK_SAFETY_BUFFER 112

#ifdef KHOOK_X64
#define FUNCTION_ATTRIBUTE_PREFIX(ret) ret
#define FUNCTION_ATTRIBUTE_SUFFIX
#else
#ifdef _WIN32
#define FUNCTION_ATTRIBUTE_PREFIX(ret) ret __cdecl
#define FUNCTION_ATTRIBUTE_SUFFIX
#else
#define FUNCTION_ATTRIBUTE_PREFIX(ret) __attribute__((cdecl)) ret
#define FUNCTION_ATTRIBUTE_SUFFIX
#endif
#endif

#ifdef _WIN32
#define LINUX_ONLY(x)
#define WIN_ONLY(x) x
#else
#define LINUX_ONLY(x) x
#define WIN_ONLY(x)
#endif

template<typename T, typename Ret, typename... Args>
union MFP {
	MFP(Ret (T::*func)(Args...)) : mfp(func) {
#ifdef _WIN32
#else
		this->details.adjustor = 0;
#endif
	}
	Ret (T::*mfp)(Args...);
	struct {
		void *addr;
#ifdef _WIN32
#else
		intptr_t adjustor;
#endif
	} details;

	std::uintptr_t GetAddress() {
		return reinterpret_cast<std::uintptr_t>(this->details.addr);
	}
};

static thread_local std::stack<void*> g_current_hook;
static FUNCTION_ATTRIBUTE_PREFIX(void) PushPopCurrentHook(void* current_hook, bool push) FUNCTION_ATTRIBUTE_SUFFIX {
	if (push) {
		g_current_hook.push(current_hook);
	} else {
		g_current_hook.pop();
	}
}

static thread_local std::stack<std::pair<void*, void*>> g_hook_fn_original_return;
static FUNCTION_ATTRIBUTE_PREFIX(void) PushHookOriginalReturn(void* original_return_ptr, void* fn_original_function_ptr) FUNCTION_ATTRIBUTE_SUFFIX {
	g_hook_fn_original_return.push(std::make_pair(original_return_ptr, fn_original_function_ptr));
}

static thread_local std::stack<void*> g_hook_override_return;
static void PushHookOverrideReturn(void* override_return) FUNCTION_ATTRIBUTE_SUFFIX {
	g_hook_override_return.push(override_return);
}

static thread_local std::stack<std::uintptr_t> rsp_values;
static FUNCTION_ATTRIBUTE_PREFIX(void) PushRsp(std::uintptr_t rsp) FUNCTION_ATTRIBUTE_SUFFIX {
	//std::cout << "Saving RSP: 0x" << std::hex << rsp << std::endl;
	rsp_values.push(rsp);
}

static FUNCTION_ATTRIBUTE_PREFIX(std::uintptr_t) PeekRsp(std::uintptr_t rsp) FUNCTION_ATTRIBUTE_SUFFIX {
	auto internal_rsp = rsp_values.top();
	assert((internal_rsp + STACK_SAFETY_BUFFER) > rsp);
	return internal_rsp;
}

static FUNCTION_ATTRIBUTE_PREFIX(void) PopRsp() FUNCTION_ATTRIBUTE_SUFFIX {
	rsp_values.pop();
}

static FUNCTION_ATTRIBUTE_PREFIX(void) RecursiveLockUnlockShared(std::shared_mutex* mutex, bool lock) FUNCTION_ATTRIBUTE_SUFFIX {
	static thread_local std::unordered_map<std::shared_mutex*, std::uint32_t> lock_counts;

	auto it = lock_counts.find(mutex);
	if (it == lock_counts.end()) {
		it = lock_counts.insert_or_assign(mutex, 0).first;
	}

	if (lock) {
		it->second++;
		// First time lock
		if (it->second == 1) {
			mutex->lock_shared();
		}
	} else {
		if (it->second == 0) {
			assert(false);
		}

		it->second--;
		// No more locks, so unlock
		if (it->second == 0) {
			mutex->unlock_shared();
		}
	}
}

static FUNCTION_ATTRIBUTE_PREFIX(void) PrintRSP(std::uintptr_t rsp) FUNCTION_ATTRIBUTE_SUFFIX {
	std::cout << "RSP : 0x" << std::hex << rsp << std::endl;
	for (int i = 0; i < 10; i++) {
		auto ptr = (((std::uint8_t*)rsp) + i * sizeof(void*));
		std::cout << "[0x" << std::hex << (rsp + (i * sizeof(void*))) << "](RSP + 0x" << i * sizeof(void*) << ") : 0x" << std::hex << *(std::uintptr_t*)ptr
		<< std::dec <<  " float(" << *(float*)ptr << ")"
		<< std::endl;
	}
}

KHOOK_API void* GetCurrent() {
	return g_current_hook.top();
}

KHOOK_API void* GetOriginalFunction() {
	return g_hook_fn_original_return.top().second;
}

KHOOK_API void* GetOriginalValuePtr(bool pop) {
	auto ret = g_hook_fn_original_return.top().first;
	if (pop) {
		g_hook_fn_original_return.pop();
		g_hook_override_return.pop();
	}
	return ret;
}

KHOOK_API void* GetOverrideValuePtr(bool pop) {
	auto ret = g_hook_override_return.top();
	if (pop) {
		g_hook_fn_original_return.pop();
		g_hook_override_return.pop();
	}
	return ret;
}

struct AsmLoopDetails {
	// Current iterated hook
	std::uintptr_t linked_list_it;
	// Highest hook action so far
	std::uintptr_t action;
	// If Action::Override or higher
	// These will be used to perform the return
	std::uintptr_t fn_make_return;
	// The hook that performed the original call
	std::uintptr_t fn_make_call_original;
	// The original return value ptr
	std::uintptr_t original_return_ptr;
	// The current override return ptr
	std::uintptr_t override_return_ptr;
#ifndef KHOOK_X64
#ifndef _WIN64
	std::uint8_t pad[5];
#endif
#endif
	static_assert(sizeof(std::uintptr_t) == sizeof(void*));
	static_assert(sizeof(std::uint32_t) >= sizeof(KHook::Action));
};

/*void memcpy_detour(std::uintptr_t dst, std::uintptr_t src, std::uintptr_t size) {
	std::cout << std::hex << "dst 0x" << dst << " src 0x" << src << " size 0x" << size << std::endl;
}*/

void copy_stack(DetourCapsule::AsmJit& jit, std::int32_t offset, std::int32_t stack_size, std::int32_t stack_start) {
	using namespace Asm;

#ifdef KHOOK_X64
	jit.push(rax);
	LINUX_ONLY(jit.push(rdi)); WIN_ONLY(jit.push(rcx));
	LINUX_ONLY(jit.push(rsi)); WIN_ONLY(jit.push(rdx));
	LINUX_ONLY(jit.push(rdx)); WIN_ONLY(jit.push(r8));

	// 1st param - Dst
	LINUX_ONLY(jit.lea(rdi, rsp(4 * sizeof(void*) + offset)));
	WIN_ONLY(jit.lea(rcx, rsp(4 * sizeof(void*) + offset)));
	// 2nd param - Src
	LINUX_ONLY(jit.lea(rsi, rsp(4 * sizeof(void*) + offset + stack_size + stack_start + sizeof(void*))));
	WIN_ONLY(jit.lea(rdx, rsp(4 * sizeof(void*) + offset + stack_size + stack_start + sizeof(void*))));
	// 3rd param - Size
	LINUX_ONLY(jit.mov(rdx, stack_size));
	WIN_ONLY(jit.mov(r8, stack_size));

	WIN_ONLY(jit.sub(rsp, 32));
	jit.mov(rax, reinterpret_cast<std::uintptr_t>(memcpy));
	jit.call(rax);
	WIN_ONLY(jit.add(rsp, 32));

	LINUX_ONLY(jit.pop(rdx)); WIN_ONLY(jit.pop(r8));
	LINUX_ONLY(jit.pop(rsi)); WIN_ONLY(jit.pop(rdx));
	LINUX_ONLY(jit.pop(rdi)); WIN_ONLY(jit.pop(rcx));
	jit.pop(rax);
#else
	jit.push(eax);

	jit.push(stack_size); // Size
	jit.lea(eax, esp(2 * sizeof(void*) + offset + stack_size + stack_start + sizeof(void*))); // Src
	jit.push(eax);
	jit.lea(eax, esp(3 * sizeof(void*) + offset)); // Dst
	jit.push(eax);
	jit.mov(eax, reinterpret_cast<std::uintptr_t>(memcpy));
	jit.call(eax);
	jit.add(esp, sizeof(void*) * 3);

	jit.pop(eax);
#endif
}

DetourCapsule::DetourCapsule(void* detour_address) :
	_terminate_edit_thread(false),
	_edit_thread([this]() { this->_EditThread(); }),
	_start_callbacks(nullptr),
	_end_callbacks(nullptr),
	_jit_func_ptr(0),
	_original_function(0),
	_stack_size(STACK_SAFETY_BUFFER) {
	static constexpr auto local_params_size = sizeof(AsmLoopDetails);
	static_assert(local_params_size % 16 == 0);
	// Because we want to be call agnostic we must get clever
	// No register can be used to call a function, so here's the plan
	// mov rax, 0xStart Address of JIT function
	// add rax, <offset calculated later>
	// push rax
	// mov rax, 0xAddress Of function to call
	// push rax
	// retn
	// next instructions
	// !! Rewrite <offset calculated later>
#ifdef KHOOK_X64
	using namespace Asm;

	static auto print_rsp = [](DetourCapsule::AsmJit& jit) {
		WIN_ONLY(jit.sub(rsp, 32));

		jit.push(rdi);
		jit.push(rcx);
		jit.push(rax);
		jit.push(rax);
		
		LINUX_ONLY(jit.lea(rdi, rsp(4 * 8)));
		WIN_ONLY(jit.lea(rcx, rsp(32 + 4 * 8)));

		jit.mov(rax, reinterpret_cast<std::uintptr_t>(PrintRSP));
		jit.call(rax);

		jit.pop(rax);
		jit.pop(rax);
		jit.pop(rcx);
		jit.pop(rdi);

		WIN_ONLY(jit.add(rsp, 32));
	};

	static auto push_current_hook = [](DetourCapsule::AsmJit& jit, x86_64_RegRm reg) {
		WIN_ONLY(jit.sub(rsp, 32));
		// 1st param - Original return ptr
		LINUX_ONLY(jit.mov(rdi, reg));
		WIN_ONLY(jit.mov(rcx, reg));
		// 2nd param - Store
		LINUX_ONLY(jit.mov(rsi, true));
		WIN_ONLY(jit.mov(rdx, true));

		jit.mov(rax, reinterpret_cast<std::uintptr_t>(PushPopCurrentHook));
		jit.call(rax);
		WIN_ONLY(jit.add(rsp, 32));
	};

	static auto pop_current_hook = [](DetourCapsule::AsmJit& jit) {
		WIN_ONLY(jit.sub(rsp, 32));
		// 2nd param - Store
		LINUX_ONLY(jit.mov(rsi, false));
		WIN_ONLY(jit.mov(rdx, false));

		jit.mov(rax, reinterpret_cast<std::uintptr_t>(PushPopCurrentHook));
		jit.call(rax);
		WIN_ONLY(jit.add(rsp, 32));
	};

	static auto push_original_return_ptr = [](DetourCapsule::AsmJit& jit, x86_64_RegRm original_return_ptr, x86_64_Reg fn_original_function_ptr) {
		WIN_ONLY(jit.sub(rsp, 32));
		// 1st param - Original return ptr
		LINUX_ONLY(jit.mov(rdi, original_return_ptr));
		WIN_ONLY(jit.mov(rcx, original_return_ptr));
		// 2nd param - Fn original return ptr
		LINUX_ONLY(jit.mov(rsi, fn_original_function_ptr));
		WIN_ONLY(jit.mov(rdx, fn_original_function_ptr));
		jit.mov(rax, reinterpret_cast<std::uintptr_t>(PushHookOriginalReturn));
		jit.call(rax);
		WIN_ONLY(jit.add(rsp, 32));
	};

	static auto push_rsp = [](DetourCapsule::AsmJit& jit) {
		// We should allocate shadow space, but our stack size should be big enough for it already...
		// 1st param - Rsp
		LINUX_ONLY(jit.mov(rdi, rsp));
		WIN_ONLY(jit.mov(rcx, rsp));
		jit.mov(rax, reinterpret_cast<std::uintptr_t>(PushRsp));
		jit.call(rax);
	};

	static auto peek_rsp = [](DetourCapsule::AsmJit& jit) {
		// Force align rsp
		jit.mov(rax, 0xFFFFFFFFFFFFFFF0);
		jit.l_and(rsp, rax);
		
		// just in case of stack corruption
		static constexpr std::uint32_t stackSpace = 96 + WIN_ONLY(32) LINUX_ONLY(0);
		jit.sub(rsp, stackSpace);

		// 1st param - Rsp
		LINUX_ONLY(jit.lea(rdi, rsp(stackSpace)));
		WIN_ONLY(jit.lea(rcx, rsp(stackSpace)));
		jit.mov(rax, reinterpret_cast<std::uintptr_t>(PeekRsp));
		jit.call(rax);

		jit.mov(rsp, rax);
	};

	static auto push_override_return_ptr = [](DetourCapsule::AsmJit& jit, x86_64_RegRm override_return_ptr) {
		// Shadow space is unrequired it was allocated already
		// 1st param - Original return ptr
		LINUX_ONLY(jit.mov(rdi, override_return_ptr));
		WIN_ONLY(jit.mov(rcx, override_return_ptr));

		jit.mov(rax, reinterpret_cast<std::uintptr_t>(PushHookOverrideReturn));
		jit.call(rax);
	};

	static auto pop_rsp = [](DetourCapsule::AsmJit& jit) {
		// Shadow space is unrequired it was allocated already
		jit.mov(rax, reinterpret_cast<std::uintptr_t>(PopRsp));
		jit.call(rax);
	};

	static auto lock_shared_mutex = [](DetourCapsule::AsmJit& jit, std::shared_mutex* mutex) {
		// Shadow space is unrequired it was allocated already
		// 1st param - Mutex
		LINUX_ONLY(jit.mov(rdi, reinterpret_cast<std::uintptr_t>(mutex)));
		WIN_ONLY(jit.mov(rcx, reinterpret_cast<std::uintptr_t>(mutex)));
		// 2nd param - Lock
		LINUX_ONLY(jit.mov(rsi, true));
		WIN_ONLY(jit.mov(rdx, true));
		jit.mov(rax, reinterpret_cast<std::uintptr_t>(RecursiveLockUnlockShared));
		jit.call(rax);
	};

	static auto unlock_shared_mutex = [](DetourCapsule::AsmJit& jit, std::shared_mutex* mutex) {
		// Shadow space is unrequired it was allocated already
		// 1st param - Mutex
		LINUX_ONLY(jit.mov(rdi, reinterpret_cast<std::uintptr_t>(mutex)));
		WIN_ONLY(jit.mov(rcx, reinterpret_cast<std::uintptr_t>(mutex)));
		// 2nd param - Lock
		LINUX_ONLY(jit.mov(rsi, false));
		WIN_ONLY(jit.mov(rdx, false));
		jit.mov(rax, reinterpret_cast<std::uintptr_t>(RecursiveLockUnlockShared));
		jit.call(rax);
	};

#ifdef _WIN32
	// Save everything pertaining to Windows x86_64 callconv
	static const x86_64_Reg reg[] = { rcx, rdx, r8, r9 }; // 32 bytes so 16 bytes aligned
	// Save XMM0-XMM5
	static const x8664FloatReg float_reg[] = { xmm0, xmm1, xmm2, xmm3 }; // Each register is 16 bytes
#else
	// Save everything pertaining to Linux x86_64 callconv
	static const x86_64_Reg reg[] = { rdi, rsi, rdx, rcx, r8, r9 }; // 48 bytes (so 16 bytes aligned)
	// Save XMM0-XMM7
	static const x8664FloatReg float_reg[] = { xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7 }; // Each register is 16 bytes
#endif
	static constexpr auto reg_count = sizeof(reg) / sizeof(decltype(*reg));
	static constexpr auto float_reg_count = sizeof(float_reg) / sizeof(decltype(*float_reg));
	// Push rbp we're going to be using it and align the stack at the same time
	_jit.push(rbp);
	// RSP is aligned now AND MUST STAY ALIGNED

	// Variable to store various data, should be 16 bytes aligned
	_jit.sub(rsp, local_params_size);

	// Save general purpose registers
	_jit.sub(rsp, sizeof(void*) * reg_count);
	for (int i = 0; i < reg_count; i++) {
		_jit.mov(rsp(sizeof(void*) * i), reg[i]);
	}
	// Save floating point registers
	_jit.sub(rsp, 16 * float_reg_count);
	for (int i = 0; i < float_reg_count; i++) {
		_jit.movsd(rsp(16 * i), float_reg[i]);
	}

	//print_rsp(_jit);

	// Introduce shadow space
	WIN_ONLY(_jit.sub(rsp, 32));
	
	// Bytes offset to get back at where we saved our data
	static constexpr auto reg_start = WIN_ONLY(32) LINUX_ONLY(0);

	// Restore floating point registers
	static auto restore_float_regs = [](DetourCapsule::AsmJit& jit, std::uint32_t func_param_stack_size) {
		for (int i = 0; i < float_reg_count; i++) {
			jit.movsd(float_reg[i], rsp(reg_start + func_param_stack_size + 16 * i));
		}
	};

	// Restore regular registers
	static auto restore_reg = [](DetourCapsule::AsmJit& jit, std::uint32_t func_param_stack_size) {
		for (int i = 0; i < reg_count; i++) {
			jit.mov(reg[i], rsp(reg_start + func_param_stack_size + (16 * float_reg_count) + 8 * i));
		}
	};

	static constexpr auto stack_local_data_start = 16 * float_reg_count + 8 * reg_count + reg_start;
	static constexpr auto func_param_stack_start = stack_local_data_start + local_params_size + 8 /* push rbp */;

	static auto perform_loop = [](DetourCapsule::AsmJit& jit, std::uintptr_t jit_func_ptr, std::int32_t func_param_stack_size, std::int32_t offset_fn_callback, std::int32_t offset_next_it) {
		auto entry_loop = (std::int32_t)jit.get_outputpos();
		jit.mov(r8, rax(offset_fn_callback)); // offsetof(LinkedList, fn_callback)
		jit.test(r8, r8);
		jit.jz(INT32_MAX); auto exit_loop = jit.get_outputpos(); {
			// MAKE PRE/POST CALL
			jit.push(r8);
			jit.push(r8);
			push_current_hook(jit, rax(offsetof(LinkedList, hook_ptr)));
			jit.pop(r8);
			jit.pop(r8);
			jit.mov(rax, jit_func_ptr);
			jit.mov(rax, rax());
			jit.add(rax, INT32_MAX);
			auto make_pre_call_return = jit.get_outputpos();
			jit.push(rax); // Setup return address, basically later in this function
			jit.push(r8); // PRE/POST Callback address
			copy_stack(jit, sizeof(void*) * 2, func_param_stack_size, func_param_stack_start);
			restore_float_regs(jit, func_param_stack_size + sizeof(void*) * 2);
			restore_reg(jit, func_param_stack_size + sizeof(void*) * 2);
			jit.retn();
			jit.rewrite(make_pre_call_return - sizeof(std::uint32_t), jit.get_outputpos());
			peek_rsp(jit);
			pop_current_hook(jit);
			jit.mov(rax, rbp(offsetof(AsmLoopDetails, linked_list_it)));
			jit.mov(r8, rax(offsetof(LinkedList, hook_action)));
			jit.mov(r8, r8());
			jit.l_and(r8, 0xF);
			// If (current_hook->action > highestaction)
			jit.cmp(r8, rbp(offsetof(AsmLoopDetails, action)));
			jit.jle(INT32_MAX);
			auto if_pre_action = jit.get_outputpos(); {
				jit.mov(rbp(offsetof(AsmLoopDetails, action)), r8);
				jit.mov(r8, rax(offsetof(LinkedList, fn_make_override_return)));
				jit.mov(rbp(offsetof(AsmLoopDetails, fn_make_return)), r8);
				jit.mov(r8, rax(offsetof(LinkedList, override_return_ptr)));
				jit.mov(rbp(offsetof(AsmLoopDetails, override_return_ptr)), r8);
			}
			jit.rewrite<std::int32_t>(if_pre_action - sizeof(std::int32_t), jit.get_outputpos() - if_pre_action);
			// Next item in the list
			jit.mov(rax, rax(offset_next_it)); //  offsetof(LinkedList, next)
			jit.mov(rbp(offsetof(AsmLoopDetails, linked_list_it)), rax);
			jit.test(rax, rax);
	
			// Loop
			jit.jnz(INT32_MAX);
			jit.rewrite<std::int32_t>(jit.get_outputpos() - sizeof(std::int32_t), entry_loop - (std::int32_t)jit.get_outputpos());
		}
		jit.rewrite<std::int32_t>(exit_loop - sizeof(std::int32_t), jit.get_outputpos() - exit_loop);
	};

	// Begin thread safety
	lock_shared_mutex(_jit, &_detour_mutex);

	// Early retrieve callbacks
	_jit.mov(rax, reinterpret_cast<std::uintptr_t>(&_start_callbacks));
	_jit.mov(rax, rax());
	
	// If no callbacks, early return
	_jit.test(rax, rax);
	_jit.jnz(INT32_MAX); auto jnz_pos = _jit.get_outputpos(); {
		// Unlock mutex
		unlock_shared_mutex(_jit, &_detour_mutex);

		// Retrieve the call address
		_jit.mov(rax, reinterpret_cast<std::uintptr_t>(&_original_function));
		_jit.mov(rax, rax());

		// Restore rbp now, and setup call address
		_jit.mov(rbp, rsp(stack_local_data_start + local_params_size));
		_jit.mov(rsp(stack_local_data_start + local_params_size), rax);

		// Restore every other registers
		restore_float_regs(_jit, 0);
		restore_reg(_jit, 0);
		_jit.add(rsp, stack_local_data_start + local_params_size);
		_jit.retn();
	}
	// Write our jump offset
	_jit.rewrite<std::int32_t>(jnz_pos - sizeof(std::int32_t), _jit.get_outputpos() - jnz_pos);

	// Copy the stack over
	std::int32_t func_param_stack_size = (_stack_size != 0) ? _stack_size : STACK_SAFETY_BUFFER;
	_jit.sub(rsp, func_param_stack_size);

	// rax still contains the value of this->_start_callbacks
	// rbp is used because it's preserved between calls on x86_64
	_jit.lea(rbp, rsp(stack_local_data_start + func_param_stack_size));
	// Default hook action is to ignore
	_jit.mov(rbp(offsetof(AsmLoopDetails, action)), (std::uint32_t)KHook::Action::Ignore);
	// We can set the first hook as overriding, its only gonna be used by actually overriding
	_jit.mov(r8, rax(offsetof(LinkedList, hook_ptr)));
	// We can also set the override function as the original return because again it will be replaced if actually overriden
	_jit.mov(r8, rax(offsetof(LinkedList, fn_make_original_return)));
	_jit.mov(rbp(offsetof(AsmLoopDetails, fn_make_return)), r8);
	// First hook will be made to call original
	_jit.mov(r8, rax(offsetof(LinkedList, fn_make_call_original)));
	_jit.mov(rbp(offsetof(AsmLoopDetails, fn_make_call_original)), r8);
	_jit.mov(r8, rax(offsetof(LinkedList, original_return_ptr)));
	_jit.mov(rbp(offsetof(AsmLoopDetails, original_return_ptr)), r8);
	// Default init override ptr but it won't be used
	_jit.mov(r8, rax(offsetof(LinkedList, override_return_ptr)));
	_jit.mov(rbp(offsetof(AsmLoopDetails, override_return_ptr)), r8);

	// Remember our whole stack
	// We will restore it after each function call
	push_rsp(_jit);

	// Prelude to PRE LOOP
	// Hooks with a pre callback are enqueued at the start of linked list
	_jit.mov(rax, reinterpret_cast<std::uintptr_t>(&_start_callbacks));
	_jit.mov(rax, rax());
	_jit.mov(rbp(offsetof(AsmLoopDetails, linked_list_it)), rax);

	// PRE LOOP
	perform_loop(_jit, reinterpret_cast<std::uintptr_t>(&_jit_func_ptr), func_param_stack_size, offsetof(LinkedList, fn_make_pre), offsetof(LinkedList, next));

	// Call original (maybe)
	// RBP which we have set much earlier still contains our local variables
	// it should have been saved across all calls as per linux & win callconvs

	_jit.mov(rax, reinterpret_cast<std::uintptr_t>(&_original_function));
	_jit.mov(rax, rax());

	push_original_return_ptr(_jit, rbp(offsetof(AsmLoopDetails, original_return_ptr)), rax);

	_jit.mov(rax, rbp(offsetof(AsmLoopDetails, action)));
	_jit.cmp(rax, (std::int32_t)Action::Supercede);
	_jit.je(INT32_MAX);
	auto if_not_supercede = _jit.get_outputpos(); {
		// MAKE ORIGINAL CALL
		_jit.mov(rax, reinterpret_cast<std::uintptr_t>(&_jit_func_ptr));
		_jit.mov(rax, rax());
		_jit.add(rax, INT32_MAX);
		auto make_pre_call_return = _jit.get_outputpos();
		_jit.push(rax); // Setup return address, basically later in this function
		_jit.mov(rax, rbp(offsetof(AsmLoopDetails, fn_make_call_original)));
		_jit.push(rax); // Call original
		copy_stack(_jit, sizeof(void*) * 2, func_param_stack_size, func_param_stack_start);
		restore_float_regs(_jit, func_param_stack_size + sizeof(void*) * 2);
		restore_reg(_jit, func_param_stack_size + sizeof(void*) * 2);
		_jit.retn();
		_jit.rewrite(make_pre_call_return - sizeof(std::uint32_t), _jit.get_outputpos());
		peek_rsp(_jit);
	}
	_jit.rewrite<std::int32_t>(if_not_supercede - sizeof(std::int32_t), _jit.get_outputpos() - if_not_supercede);

	// Prelude to POST LOOP
	// Hooks with a pre callback are enqueued at the start of linked list
	_jit.mov(rax, reinterpret_cast<std::uintptr_t>(&_end_callbacks));
	_jit.mov(rax, rax());
	_jit.mov(rbp(offsetof(AsmLoopDetails, linked_list_it)), rax);

	// POST LOOP
	perform_loop(_jit, reinterpret_cast<std::uintptr_t>(&_jit_func_ptr), func_param_stack_size, offsetof(LinkedList, fn_make_post), offsetof(LinkedList, prev));

	// EXIT HOOK
	// Free our fake param stack
	_jit.add(rsp, func_param_stack_size);

	push_override_return_ptr(_jit, rbp(offsetof(AsmLoopDetails, override_return_ptr)));
	pop_rsp(_jit);

	// Unlock mutex
	unlock_shared_mutex(_jit, &_detour_mutex);

	// Retrieve the call address
	_jit.mov(rax, rbp(offsetof(AsmLoopDetails, fn_make_return)));

	// Restore rbp now, setup call address
	_jit.mov(rbp, rsp(stack_local_data_start + local_params_size));
	_jit.mov(rsp(stack_local_data_start + local_params_size), rax);

	// Restore every other registers
	restore_float_regs(_jit, 0);
	restore_reg(_jit, 0);
	_jit.add(rsp, stack_local_data_start + local_params_size);

	// fn_make_return will pop our override & original ptr
	_jit.retn();
#else
using namespace Asm;

	static auto print_rsp = [](DetourCapsule::AsmJit& jit) {
		jit.push(eax);

		jit.lea(eax, esp(4));
		jit.push(eax);
		jit.mov(eax, reinterpret_cast<std::uintptr_t>(PrintRSP));
		jit.call(eax);
		jit.add(esp, sizeof(void*) * 1);

		jit.pop(eax);
	};

	static auto push_current_hook = [](DetourCapsule::AsmJit& jit, x86_RegRm reg) {
		jit.push(eax);
		jit.push(eax);

		jit.push(true);
		jit.push(reg);
		jit.mov(eax, reinterpret_cast<std::uintptr_t>(PushPopCurrentHook));
		jit.call(eax);
		jit.add(esp, sizeof(void*) * 2);

		jit.pop(eax);
		jit.pop(eax);
	};

	static auto pop_current_hook = [](DetourCapsule::AsmJit& jit) {
		jit.push(eax);
		jit.push(eax);

		jit.push(false);
		jit.push(0x0);
		jit.mov(eax, reinterpret_cast<std::uintptr_t>(PushPopCurrentHook));
		jit.call(eax);
		jit.add(esp, sizeof(void*) * 2);

		jit.pop(eax);
		jit.pop(eax);
	};

	static auto push_original_return_ptr = [](DetourCapsule::AsmJit& jit, x86_RegRm original_return_ptr, x86_Reg fn_original_function_ptr) {
		jit.push(eax);
		jit.push(eax);

		jit.push(fn_original_function_ptr);
		jit.push(original_return_ptr);
		jit.mov(eax, reinterpret_cast<std::uintptr_t>(PushHookOriginalReturn));
		jit.call(eax);
		jit.add(esp, sizeof(void*) * 2);

		jit.pop(eax);
		jit.pop(eax);
	};

	static auto push_rsp = [](DetourCapsule::AsmJit& jit) {
		jit.push(eax);

		jit.lea(eax, esp(4));
		jit.push(eax);
		jit.mov(eax, reinterpret_cast<std::uintptr_t>(PushRsp));
		jit.call(eax);
		jit.add(esp, sizeof(void*) * 1);

		jit.pop(eax);
	};

	static auto peek_rsp = [](DetourCapsule::AsmJit& jit) {
		// Force align rsp
		jit.mov(eax, 0xFFFFFFF0);
		jit.l_and(esp, eax);
		
		// just in case of stack corruption
		static constexpr std::uint32_t stackSpace = 96;
		jit.sub(esp, stackSpace);

		// 1st param - Rsp
		jit.lea(eax, esp(stackSpace));
		jit.push(eax);
		jit.push(eax); // Keep stack aligned
		jit.mov(eax, reinterpret_cast<std::uintptr_t>(PeekRsp));
		jit.call(eax);

		jit.mov(esp, eax);
	};

	static auto push_override_return_ptr = [](DetourCapsule::AsmJit& jit, x86_RegRm override_return_ptr) {
		jit.push(eax);

		// 1st param - Original return ptr
		jit.push(override_return_ptr);
		jit.mov(eax, reinterpret_cast<std::uintptr_t>(PushHookOverrideReturn));
		jit.call(eax);
		jit.add(esp, sizeof(void*) * 1);

		jit.pop(eax);
	};

	static auto pop_rsp = [](DetourCapsule::AsmJit& jit) {
		// Shadow space is unrequired it was allocated already
		jit.mov(eax, reinterpret_cast<std::uintptr_t>(PopRsp));
		jit.call(eax);
	};

	static auto lock_shared_mutex = [](DetourCapsule::AsmJit& jit, std::shared_mutex* mutex) {
		// 2nd param - Lock
		jit.push(true);
		// 1st param - Mutex
		jit.push(reinterpret_cast<std::uintptr_t>(mutex));
		jit.mov(eax, reinterpret_cast<std::uintptr_t>(RecursiveLockUnlockShared));
		jit.call(eax);
		jit.add(esp, sizeof(void*) * 2);
	};

	static auto unlock_shared_mutex = [](DetourCapsule::AsmJit& jit, std::shared_mutex* mutex) {
		// 2nd param - Lock
		jit.push(false);
		// 1st param - Mutex
		jit.push(reinterpret_cast<std::uintptr_t>(mutex));
		jit.mov(eax, reinterpret_cast<std::uintptr_t>(RecursiveLockUnlockShared));
		jit.call(eax);
		jit.add(esp, sizeof(void*) * 2);
	};

	static const x86_Reg reg[] = { eax, ecx, edx, ebx, ebp, esi, edi, edi };
	static constexpr auto reg_count = sizeof(reg) / sizeof(decltype(*reg));
	static_assert((reg_count * 4) % 16 == 0);
	// Push rbp we're going to be using it and align the stack at the same time
	//_jit.breakpoint();
	//_jit.lea(eax, esp(4));
	//print_rsp(_jit);
	_jit.push(ebp);
	// ESP is aligned now AND MUST STAY ALIGNED

	// Variable to store various data, should be 16 bytes aligned
	_jit.sub(esp, local_params_size);

	// Save general purpose registers
	for (int i = 0; i < reg_count; i++) {
		_jit.push(reg[i]);
	}

	// Bytes offset to get back at where we saved our data
	static constexpr auto reg_start = 0;

	// Restore regular registers
	static auto restore_reg = [](DetourCapsule::AsmJit& jit, std::uint32_t func_param_stack_size) {
		for (int i = 0; i < reg_count; i++) {
			jit.mov(reg[i], esp(reg_start + func_param_stack_size + (4 * (reg_count - 1)) - (4 * i)));
		}
	};

	static constexpr auto stack_local_data_start = 4 * reg_count + reg_start;
	static constexpr auto func_param_stack_start = stack_local_data_start + local_params_size + 4 /* push ebp */;

	static auto perform_loop = [](DetourCapsule::AsmJit& jit, std::uintptr_t jit_func_ptr, std::int32_t func_param_stack_size, std::int32_t offset_fn_callback, std::int32_t offset_next_it) {
		auto entry_loop = (std::int32_t)jit.get_outputpos();
		jit.mov(ecx, eax(offset_fn_callback));
		jit.test(ecx, ecx);
		jit.jz(INT32_MAX); auto exit_loop = jit.get_outputpos(); {
			// MAKE PRE/POST CALL
			jit.push(ecx);
			jit.push(ecx);
			push_current_hook(jit, eax(offsetof(LinkedList, hook_ptr)));
			jit.pop(ecx);
			jit.pop(ecx);
			jit.mov(eax, jit_func_ptr);
			jit.mov(eax, eax());
			jit.add(eax, INT32_MAX);
			auto make_post_call_return = jit.get_outputpos();
			jit.push(eax); // Setup return address, basically later in this function
			jit.push(ecx); // PRE/POST Callback address
			copy_stack(jit, sizeof(void*) * 2, func_param_stack_size, func_param_stack_start);
			restore_reg(jit, func_param_stack_size + sizeof(void*) * 2);
			jit.retn();
			jit.rewrite(make_post_call_return - sizeof(std::uint32_t), jit.get_outputpos());
			peek_rsp(jit);
			pop_current_hook(jit);
			jit.lea(ebp, esp(stack_local_data_start + func_param_stack_size));
			jit.mov(eax, ebp(offsetof(AsmLoopDetails, linked_list_it)));
			jit.mov(ecx, eax(offsetof(LinkedList, hook_action)));
			jit.mov(ecx, ecx());
			jit.l_and(ecx, 0xF);
			// If current_hook-> action > highestaction
			jit.cmp(ecx, ebp(offsetof(AsmLoopDetails, action)));
			jit.jle(INT32_MAX);
			auto if_post_action = jit.get_outputpos(); {
				jit.mov(ebp(offsetof(AsmLoopDetails, action)), ecx);
				jit.mov(ecx, eax(offsetof(LinkedList, fn_make_override_return)));
				jit.mov(ebp(offsetof(AsmLoopDetails, fn_make_return)), ecx);
				jit.mov(ecx, eax(offsetof(LinkedList, override_return_ptr)));
				jit.mov(ebp(offsetof(AsmLoopDetails, override_return_ptr)), ecx);
			}
			jit.rewrite<std::int32_t>(if_post_action - sizeof(std::int32_t), jit.get_outputpos() - if_post_action);
			// Move forward towards the end of linked list
			jit.mov(eax, eax(offset_next_it));
			jit.mov(ebp(offsetof(AsmLoopDetails, linked_list_it)), eax);
			jit.test(eax, eax);
			// Loop
			jit.jnz(INT32_MAX);
			jit.rewrite<std::int32_t>(jit.get_outputpos() - sizeof(std::int32_t), entry_loop - (std::int32_t)jit.get_outputpos());
		}
		jit.rewrite<std::int32_t>(exit_loop - sizeof(std::int32_t), jit.get_outputpos() - exit_loop);
	};

	// Begin thread safety
	lock_shared_mutex(_jit, &_detour_mutex);

	// Early retrieve callbacks
	_jit.mov(eax, reinterpret_cast<std::uintptr_t>(&_start_callbacks));
	_jit.mov(eax, eax());
	
	// If no callbacks, early return
	_jit.test(eax, eax);
	_jit.jnz(INT32_MAX); auto jnz_pos = _jit.get_outputpos(); {
		// Unlock mutex
		unlock_shared_mutex(_jit, &_detour_mutex);

		// Retrieve the call address
		_jit.mov(eax, reinterpret_cast<std::uintptr_t>(&_original_function));
		_jit.mov(eax, eax());

		// Restore rbp now, and setup call address
		_jit.mov(ebp, esp(stack_local_data_start + local_params_size));
		_jit.mov(esp(stack_local_data_start + local_params_size), eax);

		// Restore every other registers
		restore_reg(_jit, 0);
		_jit.add(esp, stack_local_data_start + local_params_size);
		_jit.retn();
	}
	// Write our jump offset
	_jit.rewrite<std::int32_t>(jnz_pos - sizeof(std::int32_t), _jit.get_outputpos() - jnz_pos);

	// Copy the stack over
	std::int32_t func_param_stack_size = (_stack_size > 0) ? _stack_size : STACK_SAFETY_BUFFER;
	_jit.sub(esp, func_param_stack_size);

	// rax still contains the value of this->_start_callbacks
	// rbp is used because it's preserved between calls on x86_64
	_jit.lea(ebp, esp(stack_local_data_start + func_param_stack_size));
	// Default hook action is to ignore
	_jit.mov(ebp(offsetof(AsmLoopDetails, action)), (std::uint32_t)KHook::Action::Ignore);
	// We can set the first hook as overriding, its only gonna be used by actually overriding
	_jit.mov(ecx, eax(offsetof(LinkedList, hook_ptr)));
	// We can also set the override function as the original return because again it will be replaced if actually overriden
	_jit.mov(ecx, eax(offsetof(LinkedList, fn_make_original_return)));
	_jit.mov(ebp(offsetof(AsmLoopDetails, fn_make_return)), ecx);
	// First hook will be made to call original
	_jit.mov(ecx, eax(offsetof(LinkedList, fn_make_call_original)));
	_jit.mov(ebp(offsetof(AsmLoopDetails, fn_make_call_original)), ecx);
	_jit.mov(ecx, eax(offsetof(LinkedList, original_return_ptr)));
	_jit.mov(ebp(offsetof(AsmLoopDetails, original_return_ptr)), ecx);
	// Default init override ptr but it won't be used
	_jit.mov(ecx, eax(offsetof(LinkedList, override_return_ptr)));
	_jit.mov(ebp(offsetof(AsmLoopDetails, override_return_ptr)), ecx);

	// Remember our whole stack
	// We will restore it after each function call
	push_rsp(_jit);

	// Prelude to PRE LOOP
	// Hooks with a pre callback are enqueued at the start of linked list
	_jit.mov(eax, reinterpret_cast<std::uintptr_t>(&_start_callbacks));
	_jit.mov(eax, eax());
	_jit.mov(ebp(offsetof(AsmLoopDetails, linked_list_it)), eax);

	// PRE LOOP
	perform_loop(_jit, reinterpret_cast<std::uintptr_t>(&_jit_func_ptr), func_param_stack_size, offsetof(LinkedList, fn_make_pre), offsetof(LinkedList, next));

	// Call original (maybe)
	// RBP which we have set much earlier still contains our local variables
	// it should have been saved across all calls as per linux & win callconvs

	_jit.mov(eax, reinterpret_cast<std::uintptr_t>(&_original_function));
	_jit.mov(eax, eax());

	push_original_return_ptr(_jit, ebp(offsetof(AsmLoopDetails, original_return_ptr)), eax);

	_jit.mov(eax, ebp(offsetof(AsmLoopDetails, action)));
	_jit.cmp(eax, (std::int32_t)Action::Supercede);
	_jit.je(INT32_MAX);
	auto if_not_supercede = _jit.get_outputpos(); {
		// MAKE ORIGINAL CALL
		_jit.mov(eax, reinterpret_cast<std::uintptr_t>(&_jit_func_ptr));
		_jit.mov(eax, eax());
		_jit.add(eax, INT32_MAX);
		auto make_pre_call_return = _jit.get_outputpos();
		_jit.push(eax); // Setup return address, basically later in this function
		_jit.mov(eax, ebp(offsetof(AsmLoopDetails, fn_make_call_original)));
		_jit.push(eax); // Call original
		copy_stack(_jit, sizeof(void*) * 2, func_param_stack_size, func_param_stack_start);
		restore_reg(_jit, func_param_stack_size + sizeof(void*) * 2);
		//print_rsp(_jit);
		//_jit.breakpoint();
		_jit.retn();
		_jit.rewrite(make_pre_call_return - sizeof(std::uint32_t), _jit.get_outputpos());
		peek_rsp(_jit);
		_jit.lea(ebp, esp(stack_local_data_start + func_param_stack_size));
	}
	_jit.rewrite<std::int32_t>(if_not_supercede - sizeof(std::int32_t), _jit.get_outputpos() - if_not_supercede);

	// Prelude to POST LOOP
	// Hooks with a pre callback are enqueued at the start of linked list
	_jit.mov(eax, reinterpret_cast<std::uintptr_t>(&_end_callbacks));
	_jit.mov(eax, eax());
	_jit.mov(ebp(offsetof(AsmLoopDetails, linked_list_it)), eax);

	// POST LOOP
	perform_loop(_jit, reinterpret_cast<std::uintptr_t>(&_jit_func_ptr), func_param_stack_size, offsetof(LinkedList, fn_make_post), offsetof(LinkedList, prev));

	// EXIT HOOK
	// Free our fake param stack
	_jit.add(esp, func_param_stack_size);

	push_override_return_ptr(_jit, ebp(offsetof(AsmLoopDetails, override_return_ptr)));
	pop_rsp(_jit);

	// Unlock mutex
	unlock_shared_mutex(_jit, &_detour_mutex);

	// Retrieve the call address
	_jit.mov(eax, ebp(offsetof(AsmLoopDetails, fn_make_return)));

	// Restore rbp now, setup call address
	_jit.mov(ebp, esp(stack_local_data_start + local_params_size));
	_jit.mov(esp(stack_local_data_start + local_params_size), eax);

	// Restore every other registers
	restore_reg(_jit, 0);
	_jit.add(esp, stack_local_data_start + local_params_size);

	// fn_make_return will pop our override & original ptr
	_jit.retn();
#endif
	_jit.SetRE();
	void* bridge = _jit;
	_jit_func_ptr = reinterpret_cast<std::uintptr_t>(bridge);

	auto result = safetyhook::InlineHook::create(detour_address, bridge);
	if (result) {
		_safetyhook = std::move(result.value());
		_original_function = reinterpret_cast<std::uintptr_t>(_safetyhook.original<void*>());
	}
}

DetourCapsule::~DetourCapsule() {
	_terminate_edit_thread = true;
	_cv_edit.notify_one();
	_edit_thread.join();
}

std::shared_mutex g_hooks_detour_mutex;
std::unordered_map<void*, std::unique_ptr<DetourCapsule>> g_hooks_detour;
std::shared_mutex g_associated_hooks_mutex;
std::unordered_map<HookID_t, DetourCapsule*> g_associated_hooks;

void DetourCapsule::InsertHook(HookID_t id, DetourCapsule::InsertHookDetails details, bool async) {
	{
		g_associated_hooks_mutex.lock();
		g_associated_hooks[id] = this;
		g_associated_hooks_mutex.unlock();
	}

	if (!async) {
		_detour_mutex.lock();

		if (details.fn_make_post == 0) {
			// Insert at start, it doesn't matter
			_callbacks[id] = std::make_unique<LinkedList>(nullptr, _start_callbacks);
			if (_start_callbacks == nullptr) {
				_end_callbacks = _start_callbacks = _callbacks[id].get();
			} else {
				auto inserted = _callbacks[id].get();
				_start_callbacks = inserted;
			}
		} else if (details.fn_make_pre == 0) {
			// Insert at the end, it doesn't matter
			_callbacks[id] = std::make_unique<LinkedList>(_end_callbacks, nullptr);
			if (_start_callbacks == nullptr) {
				_end_callbacks = _start_callbacks = _callbacks[id].get();
			} else {
				auto inserted = _callbacks[id].get();
				_end_callbacks = inserted;
			}
		} else {
			// Okay iterate through the list and add it in the middle
			LinkedList* prev = nullptr;
			LinkedList* next = nullptr;
	
			LinkedList* curr = _start_callbacks;
			while (curr && curr->fn_make_post == 0 && curr->next) {
				curr = curr->next;
			}
			_callbacks[id] = std::make_unique<LinkedList>((curr) ? curr->prev : nullptr, curr);
			auto inserted = _callbacks[id].get();
			if (curr == _start_callbacks) {
				if (_end_callbacks == nullptr) {
					_end_callbacks = inserted;
				}
				_start_callbacks = inserted;
			}
		}
		auto inserted = _callbacks[id].get();
		inserted->CopyDetails(details);
	
		_detour_mutex.unlock();
	} else {
		_async_mutex.lock();
		_insert_hooks.insert_or_assign(id, details);
		_async_mutex.unlock();
		_cv_edit.notify_one();
	}
}

class EmptyClass {};
void DetourCapsule::RemoveHook(HookID_t id, bool async) {
	union {
		void (EmptyClass::*mfp)(HookID_t id);
		struct {
			void* addr;
#ifdef _WIN32
#else
			intptr_t adjustor;
#endif
		} details;
	} u;
#ifdef _WIN32
#else
	u.details.adjustor = 0;
#endif

	if (!async) {
		_detour_mutex.lock();

		g_associated_hooks_mutex.lock();
		g_associated_hooks.erase(id);
		g_associated_hooks_mutex.unlock();

		bool called = false;
		auto insert_hook = _insert_hooks.find(id);
		if (insert_hook != _insert_hooks.end()) {
			auto hook = reinterpret_cast<void*>(insert_hook->second.hook_ptr);
			u.details.addr = reinterpret_cast<void*>(insert_hook->second.hook_fn_remove);
			_insert_hooks.erase(id);
			(((EmptyClass*)hook)->*u.mfp)(id);
			called = true;
		}

		auto it = _callbacks.find(id);
		if (it != _callbacks.end()) {
			auto hook = it->second.get();
			if (hook == _start_callbacks) {
				_start_callbacks = _start_callbacks->next;
			}
			if (hook == _end_callbacks) {
				_end_callbacks = _end_callbacks->prev;
			}
			u.details.addr = reinterpret_cast<void*>(hook->hook_fn_remove);
			auto hook_ptr = hook->hook_ptr;
			_callbacks.erase(it);
			if (!called) {
				(((EmptyClass*)hook_ptr)->*u.mfp)(id);
			}
		}

		_delete_hooks.erase(id);
	
		_detour_mutex.unlock();
	} else {
		_async_mutex.lock();
		// Are we still currently inserting that hook ?
		// If so early release and call it a day
		auto insert_hook = _insert_hooks.find(id);
		if (insert_hook != _insert_hooks.end()) {

			g_associated_hooks_mutex.lock();
			g_associated_hooks.erase(id);
			g_associated_hooks_mutex.unlock();

			auto hook = reinterpret_cast<void*>(insert_hook->second.hook_ptr);
			u.details.addr = reinterpret_cast<void*>(insert_hook->second.hook_fn_remove);
			_insert_hooks.erase(id);
			(((EmptyClass*)hook)->*u.mfp)(id);
		} else {
			_delete_hooks.insert(id);
		}
		_async_mutex.unlock();
		_cv_edit.notify_one();
	}
}

void DetourCapsule::_EditThread() {
	while (!_terminate_edit_thread) {
		std::unique_lock lock(_async_mutex);
		_cv_edit.wait(lock);

		// Now for each hook add or remove them synchronously
		for (auto insert_hook : _insert_hooks) {
			this->InsertHook(insert_hook.first, insert_hook.second, false);
		}
		for (auto delete_hook : _delete_hooks) {
			this->RemoveHook(delete_hook, false);
		}

		_insert_hooks.clear();
		_delete_hooks.clear();
	}

	// Free every hook associated with this detour
	{
		std::unordered_set<HookID_t> deep_copy;
		{
			_detour_mutex.lock_shared();
			for (auto& cb : _callbacks) {
				deep_copy.insert(cb.first);
			}
			_detour_mutex.unlock_shared();
		}
		for (auto id : deep_copy) {
			this->RemoveHook(id, false);
		}
	}
}

HookID_t g_lastest_hook_id = 0;

KHOOK_API HookID_t SetupHook(void* function,
	void* hookPtr,
	void* removedFunctionMFP,
	Action* hookAction,
	void* overrideReturnPtr,
	void* originalReturnPtr,
	void* preMFP,
	void* postMFP,
	void* returnOverrideMFP,
	void* returnOriginalMFP,
	void* callOriginalMFP,
	bool async) {
	DetourCapsule::InsertHookDetails details;
	details.hook_ptr = reinterpret_cast<std::uintptr_t>(hookPtr);
	details.hook_action = hookAction;
	details.hook_fn_remove = reinterpret_cast<std::uintptr_t>(removedFunctionMFP);

	details.fn_make_pre = reinterpret_cast<std::uintptr_t>(preMFP);
	details.fn_make_post = reinterpret_cast<std::uintptr_t>(postMFP);

	details.fn_make_override_return = reinterpret_cast<std::uintptr_t>(returnOverrideMFP);
	details.fn_make_original_return = reinterpret_cast<std::uintptr_t>(returnOriginalMFP);
	details.fn_make_call_original = reinterpret_cast<std::uintptr_t>(callOriginalMFP);

	details.override_return_ptr = reinterpret_cast<std::uintptr_t>(overrideReturnPtr);
	details.original_return_ptr = reinterpret_cast<std::uintptr_t>(originalReturnPtr);

	auto id = g_lastest_hook_id++;
	g_hooks_detour_mutex.lock_shared();
	auto it = g_hooks_detour.find(function);
	if (it == g_hooks_detour.end()) {
		g_hooks_detour_mutex.unlock_shared();

		g_hooks_detour_mutex.lock();
		auto insert = g_hooks_detour.insert_or_assign(function, std::make_unique<DetourCapsule>(function));
		g_hooks_detour_mutex.unlock();

		if (insert.second) {
			// Detour is just created so insert the new hook immediately
			insert.first->second->InsertHook(id, details, false);
			return id;
		}
		return INVALID_HOOK;
	}
	g_hooks_detour_mutex.unlock_shared();

	it->second->InsertHook(id, details, async);
	return id;
}

KHOOK_API void RemoveHook(HookID_t id, bool async) {
	g_associated_hooks_mutex.lock_shared();
	auto it = g_associated_hooks.find(id);
	if (it != g_associated_hooks.end()) {
		g_associated_hooks_mutex.unlock_shared();
		it->second->RemoveHook(id, async);
		return;
	}
	g_associated_hooks_mutex.unlock_shared();
}

KHOOK_API void Shutdown() {
	g_hooks_detour_mutex.lock();
	g_hooks_detour.clear();
	g_hooks_detour_mutex.unlock();
}

}