/* ======== KHook ========
* Copyright (C) 2025
* No warranties of any kind
*
* License: zLib License
*
* Author(s): Benoist "Kenzzer" ANDRÉ
* ============================
*/
#pragma once

#include <cstdint>
#include <unordered_set>
#include <unordered_map>
#include <iostream>
#include <stdexcept>
#include <mutex>

#ifdef KHOOK_STANDALONE
#ifdef KHOOK_EXPORT
#ifdef _WIN32
#define KHOOK_API __declspec(dllexport)
#else
#define KHOOK_API __attribute__((visibility("default")))
#endif
#else
#ifdef _WIN32
#define KHOOK_API __declspec(dllimport)
#else
#define KHOOK_API __attribute__((visibility("default")))
#endif
#endif
#else
#define KHOOK_API inline
#endif

namespace KHook {

enum class Action : std::uint8_t {
	// Hook has taken no specific action
	Ignore = 0,
	// Hook has overwritten the return value
	// But call original anyways if in PRE callback
	// Doesn't do anything in a POST callback
	Override,
	// Hook has overwritten thre return value
	// Don't call the original if in PRE callback
	// Doesn't do anything in a POST callback
	Supercede
};

template<typename RETURN>
struct Return {
	Action action;
	RETURN ret;
};

template<>
struct Return<void> {
	Action action;
};

class __Hook {
};

template<typename RETURN>
class Hook : __Hook {
public:
	Hook();
	~Hook() {
		if constexpr(!std::is_same<RETURN, void>::value) {
			if (_ret) {
				//delete _ret;
			}
			if (_original_return) {
				//delete _original_return;
			}
		}
	}
protected:
	Action _action;
	RETURN* _ret = nullptr;
	RETURN* _original_return = nullptr;
};

template<typename RETURN>
inline Hook<RETURN>::Hook() : _ret(new RETURN), _original_return(new RETURN) {}

template<>
inline Hook<void>::Hook() {}

using HookID_t = std::uint32_t;
constexpr HookID_t INVALID_HOOK = -1;
/**
 * Creates a hook around the given function address.
 *
 * @param function Address of the function to hook.
 * @param hookPtr Pointer of the class with which to call the provided MFPs.
 * @param removedFunctionMFP Member function pointer that will be called when the hook is removed. You should do memory clean up there.
 * @param hookAction Pointer to the hook action value.
 * @param preMFP (Member) function to call with the original this ptr (if any), before the hooked function is called.
 * @param postMFP (Member) function to call with the original this ptr (if any), after the hooked function is called.
 * @param returnOverrideMFP (Member) function to call with the original this ptr (if any), to return the overridden return value.
 * @param returnOriginalMFP (Member) function to call with the original this ptr (if any), to return the original return value.
 * @param callOriginalMFP (Member) function to call with the original this ptr (if any), to call the original function and store the return value if needed.
 * @param callOriginalMFP (Member) function to call with the original this ptr (if any), to call the original function and store the return value if needed.
 * @param async By default set to false. If set to true, the hook will be added synchronously. Beware if performed while the hooked function is processing this could deadlock.
 * @return The created hook id on success, INVALID_HOOK otherwise.
 */
KHOOK_API HookID_t SetupHook(void* function, void* hookPtr, void* removedFunctionMFP, Action* hookAction, void* overrideReturnPtr, void* originalReturnPtr, void* preMFP, void* postMFP, void* returnOverrideMFP, void* returnOriginalMFP, void* callOriginalMFP, bool async = false);

/**
 * Removes a given hook. Beware if this is performed synchronously under a hook callback this could deadlock or crash.
 * 
 * @param id The hook id.
 * @param async By default set to false. If set to true the hook will be removed asynchronously, you should make sure the associated functions and pointer are still loaded in memory until the hook is removed.
*/
KHOOK_API void RemoveHook(HookID_t id, bool async = false);

/**
 * Thread local function, only to be called under KHook callbacks. It returns the pointer value hookPtr provided during SetupHook.
 *
 * @return The stored hookPtr. Behaviour is undefined if called outside hook callbacks.
 */
KHOOK_API void* GetCurrent();

/**
 * Thread local function, only to be called under KHook callbacks. It returns the pointer to the original hooked function.
 *
 * @return The original function pointer. Behaviour is undefined if called outside POST callbacks.
 */
KHOOK_API void* GetOriginalFunction();

/**
 * Thread local function, only to be called under KHook callbacks. It returns a pointer containing the original return value (if not superceded).
 *
 * @return The original value pointer. Behaviour is undefined if called outside POST callbacks.
 */
KHOOK_API void* GetOriginalValuePtr(bool pop = false);

/**
 * Thread local function, only to be called under KHook callbacks. It returns a pointer containing the override return value.
 *
 * @return The override value pointer. Behaviour is undefined if called outside POST callbacks.
 */
KHOOK_API void* GetOverrideValuePtr(bool pop = false);

/**
 * Destroys every registered hooks.
 * Will deadlock or crash if used under a hook callback.
 *
 * @return
 */
KHOOK_API void Shutdown();

template<typename C, typename R, typename... A>
inline void* ExtractMFP(R (C::*mfp)(A...)) {
	union {
		R (C::*mfp)(A...);
		struct {
			void* addr;
#ifdef _WIN32
#else
			intptr_t adjustor;
#endif
		} details;
	} open;

	open.mfp = mfp;
	return open.details.addr;
}

template<typename CLASS, typename RETURN, typename... ARGS>
using __callback__ = RETURN (CLASS::*)(ARGS...);

template<typename C, typename R, typename... A>
inline __callback__<C, R, A...> BuildMFP(void* addr) {
	union {
		R (C::*mfp)(A...);
		struct {
			void* addr;
#ifdef _WIN32
#else
			intptr_t adjustor;
#endif
		} details;
	} open;

	open.details.addr = addr;
#ifdef _WIN32
#else
	open.details.adjustor = 0;
#endif
	return open.mfp;
}

template<typename RETURN, typename... ARGS>
class Function : protected Hook<RETURN> {
	class EmptyClass {};
public:
	template<typename CONTEXT>
	using fnContextCallback = ::KHook::Return<RETURN> (CONTEXT::*)(ARGS...);
	using fnCallback = ::KHook::Return<RETURN> (*)(ARGS...);
	using Self = ::KHook::Function<RETURN, ARGS...>;

	Function(fnCallback pre, fnCallback post) : 
		_pre_callback(pre),
		_post_callback(post),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
	}

	Function(RETURN (*function)(ARGS...), fnCallback pre, fnCallback post) : 
		_pre_callback(pre),
		_post_callback(post),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure((void*)function);
	}

	template<typename CONTEXT>
	Function(CONTEXT* context, fnContextCallback<CONTEXT> pre, fnContextCallback<CONTEXT> post) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(ExtractMFP(post)),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
	}

	template<typename CONTEXT>
	Function(CONTEXT* context, fnContextCallback<CONTEXT> pre, std::nullptr_t) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
	}

	template<typename CONTEXT>
	Function(CONTEXT* context, std::nullptr_t, fnContextCallback<CONTEXT> post) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(nullptr),
		_context_post_callback(ExtractMFP(post)),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
	}

	template<typename CONTEXT>
	Function(RETURN (*function)(ARGS...), CONTEXT* context, fnContextCallback<CONTEXT> pre, fnContextCallback<CONTEXT> post) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(ExtractMFP(post)),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure((void*)function);
	}

	template<typename CONTEXT>
	Function(RETURN (*function)(ARGS...), CONTEXT* context, fnContextCallback<CONTEXT> pre, std::nullptr_t) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure((void*)function);
	}

	template<typename CONTEXT>
	Function(RETURN (*function)(ARGS...), CONTEXT* context, std::nullptr_t, fnContextCallback<CONTEXT> post) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(nullptr),
		_context_post_callback(ExtractMFP(post)),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure((void*)function);
	}

	~Function() {
		_in_deletion = true;
		// Deep copy the whole vector, because it can be modifed by removehook
		std::unordered_set<HookID_t> hook_ids;
		{
			std::lock_guard guard(_hooks_stored);
			hook_ids = _hook_ids;
		}
		for (auto it : hook_ids) {
			::KHook::RemoveHook(it, false);
		}
	}

	void Configure(void* address) {
		if (address == nullptr || _in_deletion) {
			return;
		}

		if (_hooked_addr == address && _associated_hook_id != INVALID_HOOK) {
			// We are not setting up a hook on the same address again..
			return;
		}

		if (_associated_hook_id != INVALID_HOOK) {
			// Remove asynchronously, if synchronous is required re-implement this class
			::KHook::RemoveHook(_associated_hook_id, true);
		}

		_associated_hook_id = ::KHook::SetupHook(
			address,
			this,
			ExtractMFP(&Self::_KHook_RemovedHook),
			&this->_action,
			this->_ret,
			this->_original_return,
			(void*)Self::_KHook_Callback_PRE, // preMFP
			(void*)Self::_KHook_Callback_POST, // postMFP
			(void*)Self::_KHook_MakeOverrideReturn, // returnOverrideMFP,
			(void*)Self::_KHook_MakeOriginalReturn, // returnOriginalMFP
			(void*)Self::_KHook_MakeOriginalCall, // callOriginalMFP
			true // For safety reasons we are adding hooks asynchronously. If performance is required, reimplement this class
		);
		if (_associated_hook_id != INVALID_HOOK) {
			_hooked_addr = address;
			std::lock_guard guard(_hooks_stored);
			_hook_ids.insert(_associated_hook_id);
		}
	}
protected:
	// Various filters to make MemberHook class useful
	fnCallback _pre_callback;
	fnCallback _post_callback;
	void* _context;
	void* _context_pre_callback;
	void* _context_post_callback;

	bool _in_deletion;
	std::mutex _hooks_stored;
	std::unordered_set<HookID_t> _hook_ids;
	 
	HookID_t _associated_hook_id;
	void* _hooked_addr;
	// Called by KHook
	void _KHook_RemovedHook(HookID_t id) {
		std::lock_guard guard(_hooks_stored);
		_hook_ids.erase(id);
		if (id == _associated_hook_id) {
			_associated_hook_id = INVALID_HOOK;
		}
	}

	// Fixed KHook callback
	void _KHook_Callback_Fixed(bool post, ARGS... args) { 
		this->_action = Action::Ignore;

		auto context_callback = (post) ? this->_context_post_callback : this->_context_pre_callback;
		auto callback = (post) ? this->_post_callback : this->_pre_callback;

		if (callback == nullptr && context_callback == nullptr) {
			return;
		}

		Return<RETURN> action = (_context) ? (((EmptyClass*)this)->*BuildMFP<EmptyClass, Return<RETURN>, ARGS...>(context_callback))(args...) : (*callback)(args...);
		if (action.action > this->_action) {
			this->_action = action.action;
			if constexpr(!std::is_same<RETURN, void>::value) {
				*(this->_ret) = action.ret;
			}
		}
	}

	// Called by KHook
	static RETURN _KHook_Callback_PRE(ARGS... args) {
		Self* real_this = (Self*)::KHook::GetCurrent();
		real_this->_KHook_Callback_Fixed(false, args...);
		if constexpr(!std::is_same<RETURN, void>::value) {
			return *real_this->_ret;
		}
	}

	// Called by KHook
	static RETURN _KHook_Callback_POST(ARGS... args) {
		Self* real_this = (Self*)::KHook::GetCurrent();
		real_this->_KHook_Callback_Fixed(true, args...);
		if constexpr(!std::is_same<RETURN, void>::value) {
			return *real_this->_ret;
		}
	}

	// Might be used by KHook
	// Called if hook was selected as override hook
	// It returns the final value the hook will use
	static RETURN _KHook_MakeOverrideReturn(ARGS...) {
		if constexpr(std::is_same<RETURN, void>::value) {
			return;
		} else {
			auto ptr = ::KHook::GetOverrideValuePtr(true);
			return *(RETURN*)ptr;
		}
	}

	// Called if the hook has no return override
	static RETURN _KHook_MakeOriginalReturn(ARGS...) {
		if constexpr(std::is_same<RETURN, void>::value) {
			return;
		} else {
			return *(RETURN*)::KHook::GetOriginalValuePtr(true);
		}
	}

	// Called if the hook wasn't superceded
	static RETURN _KHook_MakeOriginalCall(ARGS ...args) {
		RETURN (*originalFunc)(ARGS...) = (decltype(originalFunc))::KHook::GetOriginalFunction();
		if constexpr(std::is_same<RETURN, void>::value) {
			(*originalFunc)(args...);
		} else {
			RETURN* ret = (RETURN*)::KHook::GetOriginalValuePtr();
			*ret = (*originalFunc)(args...);
			return *ret;
		}
	}
};

template<typename CLASS, typename RETURN, typename... ARGS>
class Member : protected Hook<RETURN> {
	class EmptyClass {};
public:
	template<typename CONTEXT>
	using fnContextCallback = ::KHook::Return<RETURN> (CONTEXT::*)(CLASS*, ARGS...);
	using fnCallback = ::KHook::Return<RETURN> (*)(CLASS*, ARGS...);
	using Self = ::KHook::Member<CLASS, RETURN, ARGS...>;

	Member(fnCallback pre, fnCallback post) : 
		_pre_callback(pre),
		_post_callback(post),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
	}

	Member(RETURN (CLASS::*function)(ARGS...), fnCallback pre, fnCallback post) : 
		_pre_callback(pre),
		_post_callback(post),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(ExtractMFP(function));
	}

	template<typename CONTEXT>
	Member(CONTEXT* context, fnContextCallback<CONTEXT> pre, fnContextCallback<CONTEXT> post) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(ExtractMFP(post)),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
	}

	template<typename CONTEXT>
	Member(CONTEXT* context, fnContextCallback<CONTEXT> pre, std::nullptr_t) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
	}

	template<typename CONTEXT>
	Member(CONTEXT* context, std::nullptr_t, fnContextCallback<CONTEXT> post) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(nullptr),
		_context_post_callback(ExtractMFP(post)),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
	}

	template<typename CONTEXT>
	Member(RETURN (CLASS::*function)(ARGS...), CONTEXT* context, fnContextCallback<CONTEXT> pre, fnContextCallback<CONTEXT> post) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(ExtractMFP(post)),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(ExtractMFP(function));
	}

	template<typename CONTEXT>
	Member(RETURN (CLASS::*function)(ARGS...), CONTEXT* context, fnContextCallback<CONTEXT> pre, std::nullptr_t) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(ExtractMFP(function));
	}

	template<typename CONTEXT>
	Member(RETURN (CLASS::*function)(ARGS...), CONTEXT* context, std::nullptr_t, fnContextCallback<CONTEXT> post) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(nullptr),
		_context_post_callback(ExtractMFP(post)),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(ExtractMFP(function));
	}

	~Member() {
		_in_deletion = true;
		// Deep copy the whole vector, because it can be modifed by removehook
		std::unordered_set<HookID_t> hook_ids;
		{
			std::lock_guard guard(_hooks_stored);
			hook_ids = _hook_ids;
		}
		for (auto it : hook_ids) {
			::KHook::RemoveHook(it, false);
		}
	}

	void Configure(void* address) {
		if (address == nullptr || _in_deletion) {
			return;
		}

		if (_hooked_addr == address && _associated_hook_id != INVALID_HOOK) {
			// We are not setting up a hook on the same address again..
			return;
		}

		if (_associated_hook_id != INVALID_HOOK) {
			// Remove asynchronously, if synchronous is required re-implement this class
			::KHook::RemoveHook(_associated_hook_id, true);
		}

		_associated_hook_id = SetupHook(
			address,
			this,
			ExtractMFP(&Self::_KHook_RemovedHook),
			&this->_action,
			this->_ret,
			this->_original_return,
			ExtractMFP(&Self::_KHook_Callback_PRE), // preMFP
			ExtractMFP(&Self::_KHook_Callback_POST), // postMFP
			ExtractMFP(&Self::_KHook_MakeOverrideReturn), // returnOverrideMFP,
			ExtractMFP(&Self::_KHook_MakeOriginalReturn), // returnOriginalMFP
			ExtractMFP(&Self::_KHook_MakeOriginalCall), // callOriginalMFP
			true // For safety reasons we are adding hooks asynchronously. If performance is required, reimplement this class
		);
		if (_associated_hook_id != INVALID_HOOK) {
			_hooked_addr = address;
			std::lock_guard guard(_hooks_stored);
			_hook_ids.insert(_associated_hook_id);
		}
	}
protected:
	// Various filters to make MemberHook class useful
	fnCallback _pre_callback;
	fnCallback _post_callback;
	void* _context;
	void* _context_pre_callback;
	void* _context_post_callback;

	bool _in_deletion;
	std::mutex _hooks_stored;
	std::unordered_set<HookID_t> _hook_ids;
	 
	HookID_t _associated_hook_id;
	void* _hooked_addr;

	// Called by KHook
	void _KHook_RemovedHook(HookID_t id) {
		std::lock_guard guard(_hooks_stored);
		_hook_ids.erase(id);
		if (id == _associated_hook_id) {
			_associated_hook_id = INVALID_HOOK;
		}
	}

	// Fixed KHook callback
	void _KHook_Callback_Fixed(bool post, CLASS* hooked_this, ARGS... args) { 
		this->_action = Action::Ignore;

		fnContextCallback<EmptyClass> context_callback = KHook::BuildMFP<EmptyClass, Return<RETURN>, CLASS*, ARGS...>((post) ? this->_context_post_callback : this->_context_pre_callback);
		auto callback = (post) ? this->_post_callback : this->_pre_callback;

		if (callback == nullptr && context_callback == nullptr) {
			return;
		}

		Return<RETURN> action = (_context) ? (((EmptyClass*)_context)->*context_callback)(hooked_this, args...) : (*callback)(hooked_this, args...);
		if (action.action > this->_action) {
			this->_action = action.action;
			if constexpr(!std::is_same<RETURN, void>::value) {
				*(this->_ret) = action.ret;
			}
		}
	}

	// Called by KHook
	RETURN _KHook_Callback_PRE(ARGS... args) {
		// Retrieve the real VirtualHook
		Self* real_this = (Self*)::KHook::GetCurrent();
		real_this->_KHook_Callback_Fixed(false, (CLASS*)this, args...);
		if constexpr(!std::is_same<RETURN, void>::value) {
			return *real_this->_ret;
		}
	}

	// Called by KHook
	RETURN _KHook_Callback_POST(ARGS... args) {
		// Retrieve the real VirtualHook
		Self* real_this = (Self*)::KHook::GetCurrent();
		real_this->_KHook_Callback_Fixed(true, (CLASS*)this, args...);
		if constexpr(!std::is_same<RETURN, void>::value) {
			return *real_this->_ret;
		}
	}

	// Might be used by KHook
	// Called if hook was selected as override hook
	// It returns the final value the hook will use
	RETURN _KHook_MakeOverrideReturn(ARGS...) {
		if constexpr(std::is_same<RETURN, void>::value) {
			return;
		} else {
			return *(RETURN*)::KHook::GetOverrideValuePtr(true);
		}
	}

	// Called if the hook has no return override
	RETURN _KHook_MakeOriginalReturn(ARGS...) {
		if constexpr(std::is_same<RETURN, void>::value) {
			return;
		} else {
			return *(RETURN*)::KHook::GetOriginalValuePtr(true);
		}
	}

	// Called if the hook wasn't superceded
	RETURN _KHook_MakeOriginalCall(ARGS ...args) {
		RETURN (EmptyClass::*ptr)(ARGS...) = BuildMFP<EmptyClass, RETURN, ARGS...>(::KHook::GetOriginalFunction());
		if constexpr(std::is_same<RETURN, void>::value) {
			(((EmptyClass*)this)->*ptr)(args...);
		} else {
			RETURN* ret = (RETURN*)::KHook::GetOriginalValuePtr();
			*ret = (((EmptyClass*)this)->*ptr)(args...);
			return *ret;
		}
	}
};

template<typename CLASS, typename RETURN, typename... ARGS>
inline std::int32_t __GetMFPVtableIndex__(RETURN (CLASS::*function)(ARGS...));

template<typename CLASS, typename RETURN, typename... ARGS>
inline void* GetVirtualFunction(RETURN (CLASS::*mfp)(ARGS...), CLASS* ptr) {
	void** vtable = *(void***)ptr;
	auto index = ::KHook::__GetMFPVtableIndex__(mfp);
	if (index == -1) {
		return nullptr;
	}
	return vtable[index];
}

template<typename CLASS, typename RETURN, typename... ARGS>
class Virtual : protected Hook<RETURN> {
	static constexpr std::uint32_t INVALID_VTBL_INDEX = -1;
	class EmptyClass {};
public:
	template<typename CONTEXT>
	using fnContextCallback = ::KHook::Return<RETURN> (CONTEXT::*)(CLASS*, ARGS...);
	using fnCallback = ::KHook::Return<RETURN> (*)(CLASS*, ARGS...);
	using Self = ::KHook::Virtual<CLASS, RETURN, ARGS...>;

	Virtual(fnCallback pre, fnCallback post) :
		_pre_callback(pre),
		_post_callback(post),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_vtbl_index(0),
		_in_deletion(false) {
	}

	Virtual(RETURN (CLASS::*function)(ARGS...), fnCallback pre, fnCallback post) : 
		_pre_callback(pre),
		_post_callback(post),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_vtbl_index(__GetMFPVtableIndex__(function)),
		_in_deletion(false) {
	}

	template<typename CONTEXT>
	Virtual(CONTEXT* context, fnContextCallback<CONTEXT> pre, fnContextCallback<CONTEXT> post) :
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(ExtractMFP(post)),
		_vtbl_index(0),
		_in_deletion(false) {
	}

	template<typename CONTEXT>
	Virtual(CONTEXT* context, std::nullptr_t, fnContextCallback<CONTEXT> post) :
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(nullptr),
		_context_post_callback(ExtractMFP(post)),
		_vtbl_index(0),
		_in_deletion(false) {
	}

	template<typename CONTEXT>
	Virtual(CONTEXT* context, fnContextCallback<CONTEXT> pre, std::nullptr_t) :
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(nullptr),
		_vtbl_index(0),
		_in_deletion(false) {
	}

	template<typename CONTEXT>
	Virtual(RETURN (CLASS::*function)(ARGS...), CONTEXT* context, fnContextCallback<CONTEXT> pre, fnContextCallback<CONTEXT> post) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(ExtractMFP(post)),
		_vtbl_index(__GetMFPVtableIndex__(function)),
		_in_deletion(false) {
	}

	template<typename CONTEXT>
	Virtual(RETURN (CLASS::*function)(ARGS...), CONTEXT* context, std::nullptr_t, fnContextCallback<CONTEXT> post) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(nullptr),
		_context_post_callback(ExtractMFP(post)),
		_vtbl_index(__GetMFPVtableIndex__(function)),
		_in_deletion(false) {
	}

	template<typename CONTEXT>
	Virtual(RETURN (CLASS::*function)(ARGS...), CONTEXT* context, fnContextCallback<CONTEXT> pre, std::nullptr_t) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(nullptr),
		_vtbl_index(__GetMFPVtableIndex__(function)),
		_in_deletion(false) {
	}

	~Virtual() {
		_in_deletion = true;
		// Deep copy the whole vector, because it can be modifed by removehook
		std::unordered_map<HookID_t, void*> hook_ids;
		{
			std::lock_guard guard(_hooks_stored);
			hook_ids = _hook_ids_addr;
		}
		for (auto it : hook_ids) {
			::KHook::RemoveHook(it.first, false);
		}
	}

	void Add(CLASS* this_ptr) {
		{
			std::lock_guard guard(_m_hooked_this);
			_hooked_this.insert(this_ptr);
		}
		Configure(*(void***)this_ptr);
	}

	void Remove(CLASS* this_ptr) {
		{
			std::lock_guard guard(_m_hooked_this);
			_hooked_this.erase(this_ptr);
		}
	}

	void SetIndex(std::int32_t index) {
		if (_vtbl_index == index) {
			return;
		}
		// If index changes, empty all our previous hooks
		{
			std::lock_guard guard(_m_hooked_this);
			_hooked_this.clear();
		}

		std::unordered_map<HookID_t, void*> hook_ids;
		{
			std::lock_guard guard(_hooks_stored);
			hook_ids = _hook_ids_addr;
		}
		for (auto it : hook_ids) {
			::KHook::RemoveHook(it.first, true);
		}
		_vtbl_index = index;
	}
protected:
	// Various filters to make MemberHook class useful
	fnCallback _pre_callback;
	fnCallback _post_callback;
	void* _context;
	void* _context_pre_callback;
	void* _context_post_callback;

	std::int32_t _vtbl_index;

	bool _in_deletion;
	std::mutex _hooks_stored;
	std::unordered_map<HookID_t, void*> _hook_ids_addr;
	std::unordered_map<void*, HookID_t> _addr_hook_ids;

	std::mutex _m_hooked_this;
	std::unordered_set<CLASS*> _hooked_this;

	// Called by KHook
	void _KHook_RemovedHook(HookID_t id) {
		std::lock_guard guard(_hooks_stored);
		auto it = _hook_ids_addr.find(id);
		if (it != _hook_ids_addr.end()) {
			_addr_hook_ids.erase(it->second);
		}
	}

	void Configure(void** vtable) {
		if (vtable == nullptr || _in_deletion) {
			return;
		}

		{
			std::lock_guard guard(_hooks_stored);
			// Retrieve the hookID with this vtable if it exists
			if (_addr_hook_ids.find(vtable) != _addr_hook_ids.end()) {
				// Already hooked so ignore
				return;
			}
		}

		auto id = ::KHook::SetupHook(
			vtable[_vtbl_index],
			this,
			ExtractMFP(&Self::_KHook_RemovedHook),
			&this->_action,
			this->_ret,
			this->_original_return,
			ExtractMFP(&Self::_KHook_Callback_PRE), // preMFP
			ExtractMFP(&Self::_KHook_Callback_POST), // postMFP
			ExtractMFP(&Self::_KHook_MakeOverrideReturn), // returnOverrideMFP,
			ExtractMFP(&Self::_KHook_MakeOriginalReturn), // returnOriginalMFP
			ExtractMFP(&Self::_KHook_MakeOriginalCall), // callOriginalMFP
			true // For safety reasons we are adding hooks asynchronously. If performance is required, reimplement this class
		);
		if (id != INVALID_HOOK) {
			std::lock_guard guard(_hooks_stored);
			_hook_ids_addr[id] = vtable[_vtbl_index];
			_addr_hook_ids[vtable[_vtbl_index]] = id;
		}
	}

	// Fixed KHook callback
	void _KHook_Callback_Fixed(bool post, CLASS* hooked_this, ARGS... args) { 
		this->_action = Action::Ignore;

		{
			std::lock_guard guard(this->_m_hooked_this);
			if (_hooked_this.find(hooked_this) == _hooked_this.end()) {
				if constexpr(!std::is_same<RETURN, void>::value) {
					return;
				}
			}
		}

		fnContextCallback<EmptyClass> context_callback = KHook::BuildMFP<EmptyClass, Return<RETURN>, CLASS*, ARGS...>((post) ? this->_context_post_callback : this->_context_pre_callback);
		auto callback = (post) ? this->_post_callback : this->_pre_callback;

		if (callback == nullptr && context_callback == nullptr) {
			return;
		}

		Return<RETURN> action = (_context) ? (((EmptyClass*)_context)->*context_callback)(hooked_this, args...) : (*callback)(hooked_this, args...);
		if (action.action > this->_action) {
			this->_action = action.action;
			if constexpr(!std::is_same<RETURN, void>::value) {
				*(this->_ret) = action.ret;
			}
		}
	}

	// Called by KHook
	RETURN _KHook_Callback_PRE(ARGS... args) {
		// Retrieve the real VirtualHook
		Self* real_this = (Self*)::KHook::GetCurrent();
		real_this->_KHook_Callback_Fixed(false, (CLASS*)this, args...);
		if constexpr(!std::is_same<RETURN, void>::value) {
			return *real_this->_ret;
		}
	}

	// Called by KHook
	RETURN _KHook_Callback_POST(ARGS... args) {
		// Retrieve the real VirtualHook
		Self* real_this = (Self*)::KHook::GetCurrent();
		real_this->_KHook_Callback_Fixed(true, (CLASS*)this, args...);
		if constexpr(!std::is_same<RETURN, void>::value) {
			return *real_this->_ret;
		}
	}

	// Might be used by KHook
	// Called if hook was selected as override hook
	// It returns the final value the hook will use
	RETURN _KHook_MakeOverrideReturn(ARGS...) {
		if constexpr(std::is_same<RETURN, void>::value) {
			return;
		} else {
			return *(RETURN*)::KHook::GetOverrideValuePtr(true);
		}
	}

	// Called if the hook has no return override
	RETURN _KHook_MakeOriginalReturn(ARGS...) {
		if constexpr(std::is_same<RETURN, void>::value) {
			return;
		} else {
			return *(RETURN*)::KHook::GetOriginalValuePtr(true);
		}
	}

	// Called if the hook wasn't superceded
	RETURN _KHook_MakeOriginalCall(ARGS ...args) {
		RETURN (EmptyClass::*ptr)(ARGS...) = BuildMFP<EmptyClass, RETURN, ARGS...>(::KHook::GetOriginalFunction());
		if constexpr(std::is_same<RETURN, void>::value) {
			(((EmptyClass*)this)->*ptr)(args...);
		} else {
			RETURN* ret = (RETURN*)GetOriginalValuePtr();
			*ret = (((EmptyClass*)this)->*ptr)(args...);
			return *ret;
		}
	}
};

#ifdef _WIN32
inline bool GetVtableIndex(std::uint8_t* func_addr, std::int32_t& vtbl_index) {
	// jmp 'near'
	if (func_addr[0] == 0xE9) {
		func_addr = func_addr + *((std::int32_t*)(func_addr + 1)) + 5;
	}
#ifdef _WIN64
	// mov rax, [rcx]
	if (func_addr[0] == 0x48 && func_addr[1] == 0x8B && func_addr[2] == 0x01) {
		func_addr = func_addr + 3;
	}
#else
	// mov eax, [ecx]
	if (func_addr[0] == 0x8B && func_addr[1] == 0x01) {
		func_addr = func_addr + 2;
	}
	// mov eax, [esp + arg0]
	// mov eax, [eax]
	else if (func_addr[0] == 0x8B && func_addr[1] == 0x44 && func_addr[2] == 0x24 && func_addr[3] == 0x04 &&
		func_addr[4] == 0x8B && func_addr[5] == 0x00) {
		func_addr = func_addr + 6;
	} else {
		return false;
	}
#endif
	// jmp [rax] DISP 0
	if (func_addr[0] == 0xFF && func_addr[1] == 0x20) {
		// Instant jump, so no offset
		vtbl_index = 0;
		return true;
	}
	// jmp [rax + 0xHH] DISP 8
	else if (func_addr[0] == 0xFF && func_addr[1] == 0x60) {
		vtbl_index = *((std::int8_t*)(func_addr + 2)) / sizeof(void*);
		return true;
	}
	// jmp [rax + 0xHHHHHHHH] DISP 32
	else if (func_addr[3] == 0xFF && func_addr[4] == 0xA0) {
		vtbl_index = *((std::int32_t*)(func_addr + 2)) / sizeof(void*);
		return true;
	}
	return false;
}
#endif

template<typename CLASS, typename RETURN, typename... ARGS>
#ifdef _WIN32
inline std::int32_t __GetMFPVtableIndex__(RETURN (CLASS::*function)(ARGS...)) {
	std::int32_t vtblindex = 0;
	if (GetVtableIndex((std::uint8_t*)ExtractMFP(function), vtblindex)) {
		return vtblindex;
	}
	return -1;
}
#else
inline std::int32_t __GetMFPVtableIndex__(RETURN (CLASS::*function)(ARGS...)) {
	struct MFPInfo
	{
		union
		{
			void* addr;
			std::intptr_t vtbl_index;
		};
		std::intptr_t delta;
	};
	
	MFPInfo* info = (MFPInfo*)&function;
	if (info->vtbl_index & 1) {
		return (info->vtbl_index - 1) / sizeof(void*);
	}
	return -1;
}
#endif

class IKHook {
public:
	virtual HookID_t SetupHook(void* function, void* hookPtr, void* removedFunctionMFP, Action* hookAction, void* overrideReturnPtr, void* originalReturnPtr, void* preMFP, void* postMFP, void* returnOverrideMFP, void* returnOriginalMFP, void* callOriginalMFP, bool async = false) = 0;
	virtual void RemoveHook(HookID_t id, bool async = false) = 0;
	virtual void* GetCurrent() = 0;
	virtual void* GetOriginalFunction() = 0;
	virtual void* GetOriginalValuePtr(bool pop = false) = 0;
	virtual void* GetOverrideValuePtr(bool pop = false) = 0;
};
#ifndef KHOOK_STANDALONE
// KHOOK is exposed by something
extern IKHook* __exported__khook;

KHOOK_API HookID_t SetupHook(void* function, void* hookPtr, void* removedFunctionMFP, Action* hookAction, void* overrideReturnPtr, void* originalReturnPtr, void* preMFP, void* postMFP, void* returnOverrideMFP, void* returnOriginalMFP, void* callOriginalMFP, bool async) {
	// For some hooks this is too early
	if (__exported__khook == nullptr) {
		return INVALID_HOOK;
	}
	return __exported__khook->SetupHook(function, hookPtr, removedFunctionMFP, hookAction, overrideReturnPtr, originalReturnPtr, preMFP, postMFP, returnOverrideMFP, returnOriginalMFP, callOriginalMFP, async);
}

KHOOK_API void RemoveHook(HookID_t id, bool async) {
	return __exported__khook->RemoveHook(id, async);
}

KHOOK_API void* GetCurrent() {
	return __exported__khook->GetCurrent();
}

KHOOK_API void* GetOriginalFunction() {
	return __exported__khook->GetOriginalFunction();
}

KHOOK_API void* GetOriginalValuePtr(bool pop) {
	return __exported__khook->GetOriginalValuePtr(pop);
}

KHOOK_API void* GetOverrideValuePtr(bool pop) {
	return __exported__khook->GetOverrideValuePtr(pop);
}

#endif

}