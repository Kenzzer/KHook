#include "detour.hpp"

#define KHOOK_X64

namespace KHook {

std::mutex gDetoursUpdate;
std::unordered_set<DetourCapsule*>  gDetoursToUpdate;

#define STACK_SAFETY_BUFFER 200

#ifdef WIN32
#define LINUX_ONLY(x)
#define WIN_ONLY(x) x
#else
#define LINUX_ONLY(x) x
#define WIN_ONLY(x)
#endif

template<typename T, typename Ret, typename... Args>
union MFP {
	MFP(Ret (T::*func)(Args...)) : mfp(func) {
#ifdef WIN32
#else
		this->details.adjustor = 0;
#endif
	}
	Ret (T::*mfp)(Args...);
	struct {
		void *addr;
#ifdef WIN32
#else
		intptr_t adjustor;
#endif
	} details;

	std::uintptr_t GetAddress() {
		return reinterpret_cast<std::uintptr_t>(this->details.addr);
	}
};

static std::uintptr_t SaveRestoreRsp(void* jit, std::uintptr_t rsp, bool store) {
	static thread_local std::unordered_map<void*, std::uintptr_t> rsp_values;

	auto it = rsp_values.find(jit);
	if (it == rsp_values.end()) {
		it = rsp_values.insert_or_assign(jit, 0).first;
	}

	if (store) {
		it->second = rsp;
	} else {
		// If stack restoration is bigger than this, something went horribly wrong
		assert((it->second + STACK_SAFETY_BUFFER) <= rsp);
	}

	return it->second;
}

static void RecursiveLockUnlockShared(std::shared_mutex* mutex, bool lock) {
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

DetourCapsule::DetourCapsule() : _start_callbacks(nullptr) {
#ifdef KHOOK_X64
	using namespace Asm;

	static auto save_rsp = [](DetourCapsule::AsmJit& jit) {
		jit.mov(rax, reinterpret_cast<std::uintptr_t>(SaveRestoreRsp));
		// 1st param - Jit
		LINUX_ONLY(jit.mov(rdi, reinterpret_cast<std::uintptr_t>(&jit)));
		WIN_ONLY(jit.mov(rcx, reinterpret_cast<std::uintptr_t>(&jit)));
		// 2nd param - Rsp
		LINUX_ONLY(jit.mov(rsi, rsp));
		WIN_ONLY(jit.mov(rdx, rsp));
		// 3rd param - Store
		LINUX_ONLY(jit.mov(rdx, true));
		WIN_ONLY(jit.mov(r8, true));
		jit.call(rax);
	};

	static auto restore_rsp = [](DetourCapsule::AsmJit& jit) {
		// Force align rsp
		jit.mov(rax, 0xFFFFFFFFFFFFFFF0);
		jit.l_and(rsp, rax);
		
		// just in case of stack corruption
		static constexpr std::uint32_t stackSpace = 96 + WIN_ONLY(32) LINUX_ONLY(0);
		jit.sub(rsp, stackSpace);

		jit.mov(rax, reinterpret_cast<std::uintptr_t>(SaveRestoreRsp));
		// 1st param - Jit
		LINUX_ONLY(jit.mov(rdi, reinterpret_cast<std::uintptr_t>(&jit)));
		WIN_ONLY(jit.mov(rcx, reinterpret_cast<std::uintptr_t>(&jit)));
		// 2nd param - Rsp
		LINUX_ONLY(jit.lea(rsi, rsp(stackSpace)));
		WIN_ONLY(jit.lea(rdx, rsp(stackSpace)));
		// 3rd param - Store
		LINUX_ONLY(jit.mov(rdx, true));
		WIN_ONLY(jit.mov(r8, true));
		jit.call(rax);

		jit.mov(rsp, rax);
	};

	static auto lock_shared_mutex = [](DetourCapsule::AsmJit& jit, std::shared_mutex* mutex) {
		jit.mov(rax, reinterpret_cast<std::uintptr_t>(RecursiveLockUnlockShared));
		// 1st param - Mutex
		LINUX_ONLY(jit.mov(rdi, reinterpret_cast<std::uintptr_t>(mutex)));
		WIN_ONLY(jit.mov(rcx, reinterpret_cast<std::uintptr_t>(mutex)));
		// 2nd param - Lock
		LINUX_ONLY(jit.mov(rsi, true));
		WIN_ONLY(jit.mov(rdx, true));
		jit.call(rax);
	};

	static auto unlock_shared_mutex = [](DetourCapsule::AsmJit& jit, std::shared_mutex* mutex) {
		jit.mov(rax, reinterpret_cast<std::uintptr_t>(RecursiveLockUnlockShared));
		// 1st param - Mutex
		LINUX_ONLY(jit.mov(rdi, reinterpret_cast<std::uintptr_t>(mutex)));
		WIN_ONLY(jit.mov(rcx, reinterpret_cast<std::uintptr_t>(mutex)));
		// 2nd param - Lock
		LINUX_ONLY(jit.mov(rsi, false));
		WIN_ONLY(jit.mov(rdx, false));
		jit.call(rax);
	};

#ifdef WIN32
	// Save everything pertaining to Windows x86_64 callconv
	static const x8664Reg reg[] = { rcx, rdx, r8, r9 }; // 32 bytes so 16 bytes aligned
	// Save XMM0-XMM5
	static const x8664FloatReg float_reg[] = { xmm0, xmm1, xmm2, xmm3 }; // Each register is 16 bytes
#else
	// Save everything pertaining to Linux x86_64 callconv
	static const x8664Reg reg[] = { rdi, rsi, rdx, rcx, r8, r9 }; // 48 bytes (so 16 bytes aligned)
	// Save XMM0-XMM7
	static const x8664FloatReg float_reg[] = { xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7 }; // Each register is 16 bytes
#endif
	static constexpr auto reg_count = sizeof(reg) / sizeof(decltype(*reg));
	static constexpr auto float_reg_count = sizeof(float_reg) / sizeof(decltype(*float_reg));
	// Push rbp we're going to be using it and align the stack at the same time
	_jit.push(rbp);
	// RSP is aligned now AND MUST STAY ALIGNED

	// Variable to store various data, should be 16 bytes aligned
	_jit.sub(rsp, 16);

	// Save general purpose registers
	for (int i = 0; i < reg_count; i++) {
		_jit.push(reg[i]);
	}
	// Save floating point registers
	_jit.sub(rsp, 16 * float_reg_count);
	for (int i = 0; i < float_reg_count; i++) {
		_jit.movsd(rsp(16 * i), float_reg[i]);
	}

	// Restore floating point registers
	static auto restore_float_regs = [](DetourCapsule::AsmJit& jit) {
		for (int i = 0; i < float_reg_count; i++) {
			jit.movsd(float_reg[i], rsp(16 * i));
		}
	};

	// Restore regular registers
	static auto restore_reg = [](DetourCapsule::AsmJit& jit) {
		for (int i = 0; i < reg_count; i++) {
			jit.mov(reg[i], rsp((16 * float_reg_count) + (8 * (reg_count - 1)) - (8 * i)));
		}
	};

	// Introduce shadow space
	WIN_ONLY(_jit.sub(rsp, 32));
	// Bytes offset to get back at where we saved our data
	static constexpr auto reg_start = WIN_ONLY(32) LINUX_ONLY(0);
	static constexpr auto stack_local_data_start = 16 * float_reg_count + 8 * reg_count + reg_start;
	// Frees the entire stack and unaligns it
	static auto free_stack = [](DetourCapsule::AsmJit& jit) {
		jit.add(rsp, stack_local_data_start);
		jit.add(rsp, 16);
		jit.pop(rbp);
	};

	// Begin thread safety
	lock_shared_mutex(_jit, &_detour_mutex);

	// Early retrieve callbacks
	_jit.mov(rax, reinterpret_cast<std::uintptr_t>(&_start_callbacks));
	_jit.mov(rax, rax());
	
	// If no callbacks, early return
	_jit.test(rax, rax);
	_jit.jnz(0x0);
	auto jnz_pos = _jit.get_outputpos();

	// Unlock mutex
	unlock_shared_mutex(_jit, &_detour_mutex);

	restore_float_regs(_jit);
	restore_reg(_jit);
	// This unaligns the stack
	free_stack(_jit);

	_jit.mov(rax, reinterpret_cast<std::uintptr_t>(&_original_function));
	_jit.mov(rax, rax());
	// Go back to original function with original stack
	_jit.jump(rax);

	// Write our jump offset
	_jit.rewrite<std::int32_t>(jnz_pos - sizeof(std::int32_t), _jit.get_outputpos() - jnz_pos);

	// Prelude to loop
	save_rsp(_jit);

	// Begin loop
	//TODO

	// Call func
	// Save return in RBP (it's callee saved)
	_jit.mov(rbp, rax);
	// In case the function freed the stack, reset it to how it was before
	// This should be unnecessary on x86_64, however this is to keep things
	// consistent with x86
	restore_rsp(_jit);
	_jit.mov(rax, rbp);

	// Read pointer for details loop
	//TODO

	restore_float_regs(_jit);
	restore_reg(_jit);
	free_stack(_jit);
	// Jump to the original / Overwritten return value function
	_jit.jump(rax);
#endif
}

DetourCapsule::~DetourCapsule() {
	std::lock_guard guard(gDetoursUpdate);
	gDetoursToUpdate.erase(this);
}

void DetourCapsule::AddCallback(void* func) {
	std::lock_guard guard(gDetoursUpdate);
	_GetWriteCallback(func).push_back(CBAction::ADD);
	gDetoursToUpdate.insert(this);
}

void DetourCapsule::RemoveCallback(void* func) {
	std::lock_guard guard(gDetoursUpdate);
	_GetWriteCallback(func).push_back(CBAction::REMOVE);
	gDetoursToUpdate.insert(this);
}

std::vector<DetourCapsule::CBAction>& DetourCapsule::_GetWriteCallback(void* func) {
	auto it = _write_callbacks.find(func);
	if (it == _write_callbacks.end()) {
		it = _write_callbacks.emplace(func, std::vector<DetourCapsule::CBAction>()).first;
	}
	return it->second;
}


}