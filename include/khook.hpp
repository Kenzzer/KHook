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
	Supersede
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
class Hook : public __Hook {
public:
	Hook();
	virtual ~Hook() {
		if constexpr(!std::is_same<RETURN, void>::value) {
			if (_fake_return) {
				delete _fake_return;
			}
		}
	}
protected:
	RETURN* _fake_return = nullptr;
};

template<typename RETURN>
inline Hook<RETURN>::Hook() : _fake_return(new RETURN) {}

template<>
inline Hook<void>::Hook() {}

using HookID_t = std::uint32_t;
constexpr HookID_t INVALID_HOOK = -1;

template<typename CLASS, typename RETURN, typename... ARGS>
using __mfp_const__ = RETURN (CLASS::*)(ARGS...) const;

template<typename CLASS, typename RETURN, typename... ARGS>
using __mfp__ = RETURN (CLASS::*)(ARGS...);

template<typename C, typename R, typename... A>
inline __mfp__<C, R, A...> BuildMFP(void* addr) {
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

template<typename C, typename R, typename... A>
inline __mfp_const__<C, R, A...> BuildMFP(const void* addr) {
	union {
		R (C::*mfp)(A...) const;
		struct {
			const void* addr;
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

/**
 * Creates a hook around the given function address.
 *
 * @param function Address of the function to hook.
 * @param context Context pointer that will be provided under the hook callbacks.
 * @param removed_function Member function pointer that will be called when the hook is removed. You should do memory clean up there.
 * @param pre Function to call with the original this ptr (if any), before the hooked function is called.
 * @param post Function to call with the original this ptr (if any), after the hooked function is called.
 * @param make_return Function to call with the original this ptr (if any), to make the final return value.
 * @param make_call_original Function to call with the original this ptr (if any), to call the original function and store the return value if needed.
 * @param async By default set to false. If set to true, the hook will be added synchronously. Beware if performed while the hooked function is processing this could deadlock.
 * @return The created hook id on success, INVALID_HOOK otherwise.
 */
KHOOK_API HookID_t SetupHook(void* function, void* context, void* removed_function, void* pre, void* post, void* make_return, void* make_call_original, bool async = false);

/**
 * Creates a hook around the given function retrieved from a vtable.
 *
 * @param vtable Vtable pointer to retrieve the function from.
 * @param index Index into the vtable to retrieve the function from.
 * @param context Context pointer that will be provided under the hook callbacks.
 * @param removed_function Member function pointer that will be called when the hook is removed. You should do memory clean up there.
 * @param pre Function to call with the original this ptr (if any), before the hooked function is called.
 * @param post Function to call with the original this ptr (if any), after the hooked function is called.
 * @param make_return Function to call with the original this ptr (if any), to make the final return value.
 * @param make_call_original Function to call with the original this ptr (if any), to call the original function and store the return value if needed.
 * @param async By default set to false. If set to true, the hook will be added synchronously. Beware if performed while the hooked function is processing this could deadlock.
 * @return The created hook id on success, INVALID_HOOK otherwise.
 */
KHOOK_API HookID_t SetupVirtualHook(void** vtable, int index, void* context, void* removed_function, void* pre, void* post, void* make_return, void* make_call_original, bool async = false);

/**
 * Removes a given hook. Beware if this is performed synchronously under a hook callback this could deadlock or crash.
 * 
 * @param id The hook id.
 * @param async By default set to false. If set to true the hook will be removed asynchronously, you should make sure the associated functions and pointer are still loaded in memory until the hook is removed.
*/
KHOOK_API void RemoveHook(HookID_t id, bool async = false);

/**
 * Thread local function, only to be called under KHook callbacks. It returns the context pointer provided during SetupHook.
 *
 * @return The stored context pointer. Behaviour is undefined if called outside hook callbacks.
 */
KHOOK_API void* GetContext();

/**
 * Thread local function, only to be called under KHook callbacks. If called it allow for a recall of hooked function with new params.
 *
 * @return The hooked function ptr. Behaviour is undefined if called outside hook callbacks.
 */
KHOOK_API void* DoRecall(KHook::Action action, void* ptr_to_return, std::size_t return_size, void* init_op, void* deinit_op);

/**
 * Thread local function, only to be called under KHook callbacks. Saves the return value for the current hook.
 *
 * @return
 */
KHOOK_API void SaveReturnValue(KHook::Action action, void* ptr_to_return, std::size_t return_size, void* init_op, void* deinit_op, bool original);

template<typename TYPE>
void init_operator(TYPE* assignee, TYPE* value) {
	new (assignee) TYPE(*value);
}

template<typename TYPE>
void deinit_operator(TYPE* assignee) {
	assignee->~TYPE();
}

template<typename RETURN>
inline void* __internal__dorecall(const ::KHook::Return<RETURN> &ret) {
	RETURN* return_ptr = nullptr;
	void* init_op = nullptr;
	void* deinit_op = nullptr;
	std::size_t size = 0;
	if constexpr(!std::is_same<RETURN, void>::value) {	
		return_ptr = const_cast<RETURN*>(&ret.ret);
		init_op = reinterpret_cast<void*>(::KHook::init_operator<RETURN>);
		deinit_op = reinterpret_cast<void*>(::KHook::deinit_operator<RETURN>);
		size = sizeof(RETURN);
	}

	return ::KHook::DoRecall(ret.action, return_ptr, size, init_op, deinit_op);
}

template<typename RETURN>
inline void __internal__savereturnvalue(const ::KHook::Return<RETURN> &ret, bool original) {
	RETURN* return_ptr = nullptr;
	void* init_op = nullptr;
	void* deinit_op = nullptr;
	std::size_t size = 0;
	if constexpr(!std::is_same<RETURN, void>::value) {	
		return_ptr = const_cast<RETURN*>(&ret.ret);
		init_op = reinterpret_cast<void*>(::KHook::init_operator<RETURN>);
		deinit_op = reinterpret_cast<void*>(::KHook::deinit_operator<RETURN>);
		size = sizeof(RETURN);
	}

	::KHook::SaveReturnValue(ret.action, return_ptr, size, init_op, deinit_op, original);
}

template<typename RETURN, typename ...ARGS>
inline ::KHook::Return<RETURN> Recall(RETURN (*)(ARGS...), const ::KHook::Return<RETURN> &ret, ARGS... args) {
	RETURN (*function)(ARGS...) = (decltype(function))::KHook::__internal__dorecall(ret);
	(*function)(args...);
	return ret;
}

template<typename CLASS, typename RETURN, typename ...ARGS>
inline ::KHook::Return<RETURN> Recall(RETURN (CLASS::*)(ARGS...), const ::KHook::Return<RETURN> &ret, CLASS* ptr, ARGS... args) {
	auto mfp = ::KHook::BuildMFP<CLASS, RETURN, ARGS...>(::KHook::__internal__dorecall(ret));
	(ptr->*mfp)(args...);
	return ret;
}

template<typename CLASS, typename RETURN, typename ...ARGS>
inline ::KHook::Return<RETURN> Recall(RETURN (CLASS::*)(ARGS...), const ::KHook::Return<RETURN> &ret, const CLASS* ptr, ARGS... args) {
	auto mfp = ::KHook::BuildMFP<CLASS, RETURN, ARGS...>((const void*)::KHook::__internal__dorecall(ret));
	(ptr->*mfp)(args...);
	return ret;
}

template<typename RETURN, typename ...ARGS>
inline ::KHook::Return<RETURN> Recall(const ::KHook::Return<RETURN> &ret, ARGS... args) {
	RETURN (*function)(ARGS...) = (decltype(function))::KHook::__internal__dorecall(ret);
	(*function)(args...);
	return ret;
}

template<typename CLASS, typename RETURN, typename ...ARGS>
inline ::KHook::Return<RETURN> Recall(const ::KHook::Return<RETURN> &ret, CLASS* ptr, ARGS... args) {
	auto mfp = ::KHook::BuildMFP<CLASS, RETURN, ARGS...>(::KHook::__internal__dorecall(ret));
	(ptr->*mfp)(args...);
	return ret;
}

template<typename CLASS, typename RETURN, typename ...ARGS>
inline ::KHook::Return<RETURN> Recall(const ::KHook::Return<RETURN> &ret, const CLASS* ptr, ARGS... args) {
	auto mfp = ::KHook::BuildMFP<CLASS, RETURN, ARGS...>((const void*)::KHook::__internal__dorecall(ret));
	(ptr->*mfp)(args...);
	return ret;
}

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
KHOOK_API void* GetOriginalValuePtr();

/**
 * Thread local function, only to be called under KHook callbacks. It returns a pointer containing the override return value.
 *
 * @return The override value pointer. Behaviour is undefined if called outside POST callbacks.
 */
KHOOK_API void* GetOverrideValuePtr();

/**
 * Thread local function, only to be called under KHook callbacks. It returns the current pointer that KHook plans on using as return value.
 *
 * @return The override or original value pointer. Behaviour is undefined if called outside POST callbacks.
 */
KHOOK_API void* GetCurrentValuePtr(bool pop = false);

/**
 * Thread local function, only to be called when the hook callbacks loop is over, any earlier will cause undefined behaviour.
 *
 * @return
 */
KHOOK_API void DestroyReturnValue();

/**
 * Returns the original function address, if the provided function address is detoured.
 * Useful to bypass hooks and infinite loops.
 *
 * @return Returns a different pointer than the original if there's an associated detour.
 */
KHOOK_API void* FindOriginal(void* function);

/**
 * Returns the original virtual function address, if the provided vtable entry is detoured.
 * Useful to bypass hooks and infinite loops.
 *
 * @param vtable VTable ptr to parse.
 * @param index Entry index in the vtable.
 * @return Returns a different pointer than the one currently held by the vtable if there's an associated detour.
 */
KHOOK_API void* FindOriginalVirtual(void** vtable, int index);

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

template<typename C, typename R, typename... A>
inline const void* ExtractMFP(R (C::*mfp)(A...) const) {
	union {
		R (C::*mfp)(A...) const;
		struct {
			const void* addr;
#ifdef _WIN32
#else
			intptr_t adjustor;
#endif
		} details;
	} open;

	open.mfp = mfp;
	return open.details.addr;
}

template<typename RETURN, typename... ARGS>
class Function : public Hook<RETURN> {
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
		Configure(function);
	}

	Function(RETURN (*function)(ARGS...), fnCallback pre, std::nullptr_t) : 
		_pre_callback(pre),
		_post_callback(nullptr),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(function);
	}

	Function(RETURN (*function)(ARGS...), std::nullptr_t, fnCallback post) : 
		_pre_callback(nullptr),
		_post_callback(post),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(function);
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
		Configure(function);
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
		Configure(function);
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
		Configure(function);
	}

	virtual ~Function() {
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

	inline void Configure(RETURN (*function)(ARGS...)) {
		return _Configure(reinterpret_cast<const void*>(function));
	}

	RETURN CallOriginal(ARGS... args) {
		RETURN (*function)(ARGS...) = (decltype(function))::KHook::FindOriginal((void*)_hooked_addr);
		return (*function)(args...);
	}
protected:
	inline void _Configure(void* address) {
		return _Configure(reinterpret_cast<const void*>(address));
	}

	void _Configure(const void* address) {
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
			(void*)address,
			this,
			ExtractMFP(&Self::_KHook_RemovedHook),
			(void*)Self::_KHook_Callback_PRE, // preMFP
			(void*)Self::_KHook_Callback_POST, // postMFP
			(void*)Self::_KHook_MakeReturn, // returnMFP,
			(void*)Self::_KHook_MakeOriginalCall, // callOriginalMFP
			true // For safety reasons we are adding hooks asynchronously. If performance is required, reimplement this class
		);
		if (_associated_hook_id != INVALID_HOOK) {
			_hooked_addr = address;
			std::lock_guard guard(_hooks_stored);
			_hook_ids.insert(_associated_hook_id);
		}
	}

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
	const void* _hooked_addr;
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
		auto context_callback = (post) ? this->_context_post_callback : this->_context_pre_callback;
		auto callback = (post) ? this->_post_callback : this->_pre_callback;

		if (callback == nullptr && context_callback == nullptr) {
			return;
		}

		Return<RETURN> action = (_context) ? (((EmptyClass*)_context)->*BuildMFP<EmptyClass, Return<RETURN>, ARGS...>(context_callback))(args...) : (*callback)(args...);
		::KHook::__internal__savereturnvalue(action, false);
	}

	// Called by KHook
	static RETURN _KHook_Callback_PRE(ARGS... args) {
		Self* real_this = (Self*)::KHook::GetContext();
		real_this->_KHook_Callback_Fixed(false, args...);
		if constexpr(!std::is_same<RETURN, void>::value) {
			return *real_this->_fake_return;
		}
	}

	// Called by KHook
	static RETURN _KHook_Callback_POST(ARGS... args) {
		Self* real_this = (Self*)::KHook::GetContext();
		real_this->_KHook_Callback_Fixed(true, args...);
		if constexpr(!std::is_same<RETURN, void>::value) {
			return *real_this->_fake_return;
		}
	}

	// Might be used by KHook
	// Called if hook was selected as override hook
	// It returns the final value the hook will use
	static RETURN _KHook_MakeReturn(ARGS...) {
		if constexpr(std::is_same<RETURN, void>::value) {
			::KHook::DestroyReturnValue();
			return;
		} else {
			RETURN ret = *(RETURN*)::KHook::GetCurrentValuePtr(true);
			::KHook::DestroyReturnValue();
			return ret;
		}
	}

	// Called if the hook wasn't superceded
	static RETURN _KHook_MakeOriginalCall(ARGS ...args) {
		RETURN (*originalFunc)(ARGS...) = (decltype(originalFunc))::KHook::GetOriginalFunction();
		if constexpr(std::is_same<RETURN, void>::value) {
			(*originalFunc)(args...);
			::KHook::__internal__savereturnvalue(KHook::Return<void>{ KHook::Action::Ignore }, true);
		} else {
			RETURN ret = (*originalFunc)(args...);
			::KHook::__internal__savereturnvalue(KHook::Return<RETURN>{ KHook::Action::Ignore, ret }, true);
			return ret;
		}
	}
};

template<typename CLASS, typename RETURN, typename... ARGS>
class Member : public Hook<RETURN> {
	class EmptyClass {};
public:
	template<typename CONTEXT>
	using fnContextCallback = ::KHook::Return<RETURN> (CONTEXT::*)(CLASS*, ARGS...);
	template<typename CONTEXT>
	using fnContextCallbackConst = ::KHook::Return<RETURN> (CONTEXT::*)(const CLASS*, ARGS...);
	using fnCallback = ::KHook::Return<RETURN> (*)(CLASS*, ARGS...);
	using fnCallbackConst = ::KHook::Return<RETURN> (*)(const CLASS*, ARGS...);
	using Self = ::KHook::Member<CLASS, RETURN, ARGS...>;

	// CTOR - No function
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
	
	// CTOR - CONST - No function
	Member(fnCallbackConst pre, fnCallbackConst post) : 
		_pre_callback(reinterpret_cast<fnCallback>(pre)),
		_post_callback(reinterpret_cast<fnCallback>(post)),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
	}

	// CTOR - Function
	Member(RETURN (CLASS::*function)(ARGS...), fnCallback pre, fnCallback post) : 
		_pre_callback(pre),
		_post_callback(post),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(function);
	}
	Member(void* function, fnCallback pre, fnCallback post) : 
		_pre_callback(pre),
		_post_callback(post),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(function);
	}

	// CTOR - Function - NULL PRE
	Member(RETURN (CLASS::*function)(ARGS...), std::nullptr_t, fnCallback post) : 
		_pre_callback(nullptr),
		_post_callback(post),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(function);
	}
	Member(void* function, std::nullptr_t, fnCallback post) : 
		_pre_callback(nullptr),
		_post_callback(post),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(function);
	}

	// CTOR - Function - NULL POST
	Member(RETURN (CLASS::*function)(ARGS...), fnCallback pre, std::nullptr_t) : 
		_pre_callback(pre),
		_post_callback(nullptr),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(function);
	}
	Member(void* function, fnCallback pre, std::nullptr_t) : 
		_pre_callback(pre),
		_post_callback(nullptr),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(function);
	}
	
	// CTOR - CONST - Function
	Member(RETURN (CLASS::*function)(ARGS...) const, fnCallbackConst pre, fnCallbackConst post) : 
		_pre_callback(pre),
		_post_callback(post),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(function);
	}
	Member(const void* function, fnCallbackConst pre, fnCallbackConst post) : 
		_pre_callback(pre),
		_post_callback(post),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(function);
	}

	// CTOR - CONST - Function - NULL PRE
	Member(RETURN (CLASS::*function)(ARGS...) const, std::nullptr_t, fnCallbackConst post) : 
		_pre_callback(nullptr),
		_post_callback(post),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(function);
	}
	Member(const void* function, std::nullptr_t, fnCallbackConst post) : 
		_pre_callback(nullptr),
		_post_callback(post),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(function);
	}

	// CTOR - CONST - Function - NULL POST
	Member(RETURN (CLASS::*function)(ARGS...) const, fnCallbackConst pre, std::nullptr_t) : 
		_pre_callback(pre),
		_post_callback(nullptr),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(function);
	}
	Member(const void* function, fnCallbackConst pre, std::nullptr_t) : 
		_pre_callback(pre),
		_post_callback(nullptr),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(function);
	}

	// CTOR - No function - Context
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
	
	// CTOR - CONST - No function - Context
	template<typename CONTEXT>
	Member(CONTEXT* context, fnContextCallbackConst<CONTEXT> pre, fnContextCallbackConst<CONTEXT> post) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(ExtractMFP(post)),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
	}

	// CTOR - No function - Context - NULL POST
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
	
	// CTOR - CONST - No function - Context - NULL POST
	template<typename CONTEXT>
	Member(CONTEXT* context, fnContextCallbackConst<CONTEXT> pre, std::nullptr_t) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
	}

	// CTOR - No function - Context - NULL PRE
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
	
	// CTOR - CONST - No function - Context - NULL PRE
	template<typename CONTEXT>
	Member(CONTEXT* context, std::nullptr_t, fnContextCallbackConst<CONTEXT> post) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(nullptr),
		_context_post_callback(ExtractMFP(post)),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
	}

	// CTOR - Function - Context
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
		Configure(function);
	}
	template<typename CONTEXT>
	Member(void* function, CONTEXT* context, fnContextCallback<CONTEXT> pre, fnContextCallback<CONTEXT> post) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(ExtractMFP(post)),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(function);
	}
	
	// CTOR - CONST - Function - Context
	template<typename CONTEXT>
	Member(RETURN (CLASS::*function)(ARGS...) const, CONTEXT* context, fnContextCallbackConst<CONTEXT> pre, fnContextCallbackConst<CONTEXT> post) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(ExtractMFP(post)),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(function);
	}
	template<typename CONTEXT>
	Member(const void* function, CONTEXT* context, fnContextCallbackConst<CONTEXT> pre, fnContextCallbackConst<CONTEXT> post) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(ExtractMFP(post)),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(function);
	}

	// CTOR - Function - Context - NULL POST
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
		Configure(function);
	}
	template<typename CONTEXT>
	Member(void* function, CONTEXT* context, fnContextCallback<CONTEXT> pre, std::nullptr_t) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(function);
	}
	
	// CTOR - CONST - Function - Context - NULL POST
	template<typename CONTEXT>
	Member(RETURN (CLASS::*function)(ARGS...) const, CONTEXT* context, fnContextCallbackConst<CONTEXT> pre, std::nullptr_t) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(function);
	}
	template<typename CONTEXT>
	Member(const void* function, CONTEXT* context, fnContextCallbackConst<CONTEXT> pre, std::nullptr_t) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(nullptr),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(function);
	}

	// CTOR - Function - Context - NULL PRE
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
		Configure(function);
	}
	template<typename CONTEXT>
	Member(void* function, CONTEXT* context, std::nullptr_t, fnContextCallback<CONTEXT> post) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(nullptr),
		_context_post_callback(ExtractMFP(post)),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(function);
	}
	
	// CTOR - CONST - Function - Context - NULL PRE
	template<typename CONTEXT>
	Member(RETURN (CLASS::*function)(ARGS...) const, CONTEXT* context, std::nullptr_t, fnContextCallbackConst<CONTEXT> post) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(nullptr),
		_context_post_callback(ExtractMFP(post)),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(function);
	}
	template<typename CONTEXT>
	Member(const void* function, CONTEXT* context, std::nullptr_t, fnContextCallbackConst<CONTEXT> post) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(nullptr),
		_context_post_callback(ExtractMFP(post)),
		_in_deletion(false),
		_associated_hook_id(INVALID_HOOK),
		_hooked_addr(nullptr) {
		Configure(function);
	}

	virtual ~Member() {
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

	inline void Configure(RETURN (CLASS::*function)(ARGS...)) {
		return _Configure(::KHook::ExtractMFP(function));
	}

	inline void Configure(RETURN (CLASS::*function)(ARGS...) const) {
		return _Configure(::KHook::ExtractMFP(function));
	}

	RETURN CallOriginal(CLASS* this_ptr, ARGS... args) {
		auto original_func = KHook::FindOriginal((void*)_hooked_addr);
		auto mfp = KHook::BuildMFP<CLASS, RETURN, ARGS...>(original_func);
		return (this_ptr->*mfp)(args...);
	}
protected:
	inline void _Configure(void* address) {
		return _Configure(reinterpret_cast<const void*>(address));
	}
	void _Configure(const void* address) {
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
			(void*)address,
			this,
			ExtractMFP(&Self::_KHook_RemovedHook),
			ExtractMFP(&Self::_KHook_Callback_PRE), // preMFP
			ExtractMFP(&Self::_KHook_Callback_POST), // postMFP
			ExtractMFP(&Self::_KHook_MakeReturn), // returnMFP,
			ExtractMFP(&Self::_KHook_MakeOriginalCall), // callOriginalMFP
			true // For safety reasons we are adding hooks asynchronously. If performance is required, reimplement this class
		);
		if (_associated_hook_id != INVALID_HOOK) {
			_hooked_addr = address;
			std::lock_guard guard(_hooks_stored);
			_hook_ids.insert(_associated_hook_id);
		}
	}

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
	const void* _hooked_addr;

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
		fnContextCallback<EmptyClass> context_callback = KHook::BuildMFP<EmptyClass, Return<RETURN>, CLASS*, ARGS...>((post) ? this->_context_post_callback : this->_context_pre_callback);
		auto callback = (post) ? this->_post_callback : this->_pre_callback;

		if (callback == nullptr && context_callback == nullptr) {
			return;
		}

		Return<RETURN> action = (_context) ? (((EmptyClass*)_context)->*context_callback)(hooked_this, args...) : (*callback)(hooked_this, args...);
		::KHook::__internal__savereturnvalue(action, false);
	}

	// Called by KHook
	RETURN _KHook_Callback_PRE(ARGS... args) {
		// Retrieve the real VirtualHook
		Self* real_this = (Self*)::KHook::GetContext();
		real_this->_KHook_Callback_Fixed(false, (CLASS*)this, args...);
		if constexpr(!std::is_same<RETURN, void>::value) {
			return *real_this->_fake_return;
		}
	}

	// Called by KHook
	RETURN _KHook_Callback_POST(ARGS... args) {
		// Retrieve the real VirtualHook
		Self* real_this = (Self*)::KHook::GetContext();
		real_this->_KHook_Callback_Fixed(true, (CLASS*)this, args...);
		if constexpr(!std::is_same<RETURN, void>::value) {
			return *real_this->_fake_return;
		}
	}

	// Might be used by KHook
	// Called if hook was selected as override hook
	// It returns the final value the hook will use
	RETURN _KHook_MakeReturn(ARGS...) {
		if constexpr(std::is_same<RETURN, void>::value) {
			::KHook::DestroyReturnValue();
			return;
		} else {
			RETURN ret = *(RETURN*)::KHook::GetCurrentValuePtr(true);
			::KHook::DestroyReturnValue();
			return ret;
		}
	}

	// Called if the hook wasn't superceded
	RETURN _KHook_MakeOriginalCall(ARGS ...args) {
		RETURN (EmptyClass::*ptr)(ARGS...) = BuildMFP<EmptyClass, RETURN, ARGS...>(::KHook::GetOriginalFunction());
		if constexpr(std::is_same<RETURN, void>::value) {
			(((EmptyClass*)this)->*ptr)(args...);
			::KHook::__internal__savereturnvalue(KHook::Return<void>{ KHook::Action::Ignore }, true);
		} else {
			RETURN ret = (((EmptyClass*)this)->*ptr)(args...);
			::KHook::__internal__savereturnvalue(KHook::Return<RETURN>{ KHook::Action::Ignore, ret }, true);
			return ret;
		}
	}
};

template<typename CLASS, typename RETURN, typename... ARGS>
inline std::int32_t GetVtableIndex(RETURN (CLASS::*function)(ARGS...));

template<typename CLASS, typename RETURN, typename... ARGS>
inline std::int32_t GetVtableIndex(RETURN (CLASS::*function)(ARGS...) const);

template<typename CLASS, typename RETURN, typename... ARGS>
inline __mfp__<CLASS, RETURN, ARGS...> GetVtableFunction(CLASS* ptr, RETURN (CLASS::*mfp)(ARGS...)) {
	void** vtable = *(void***)ptr;
	auto index = ::KHook::GetVtableIndex(mfp);
	if (index == -1) {
		return nullptr;
	}
	return BuildMFP<CLASS, RETURN, ARGS...>(vtable[index]);
}

template<typename CLASS, typename RETURN, typename... ARGS>
inline __mfp_const__<CLASS, RETURN, ARGS...> GetVtableFunction(const CLASS* ptr, RETURN (CLASS::*mfp)(ARGS...) const) {
	const void** vtable = *(const void***)ptr;
	auto index = ::KHook::GetVtableIndex(mfp);
	if (index == -1) {
		return nullptr;
	}
	return BuildMFP<CLASS, RETURN, ARGS...>(vtable[index]);
}

template<typename CLASS, typename RETURN, typename... ARGS>
inline __mfp__<CLASS, RETURN, ARGS...> GetVtableFunction(CLASS* ptr, std::uint32_t index) {
	void** vtable = *(void***)ptr;
	return BuildMFP<CLASS, RETURN, ARGS...>(vtable[index]);
}

template<typename CLASS, typename RETURN, typename... ARGS>
class Virtual : public Hook<RETURN> {
	static constexpr std::uint32_t INVALID_VTBL_INDEX = -1;
	class EmptyClass {};
public:
	template<typename CONTEXT>
	using fnContextCallback = ::KHook::Return<RETURN> (CONTEXT::*)(CLASS*, ARGS...);
	template<typename CONTEXT>
	using fnContextCallbackConst = ::KHook::Return<RETURN> (CONTEXT::*)(const CLASS*, ARGS...);
	using fnCallback = ::KHook::Return<RETURN> (*)(CLASS*, ARGS...);
	using fnCallbackConst = ::KHook::Return<RETURN> (*)(const CLASS*, ARGS...);
	using Self = ::KHook::Virtual<CLASS, RETURN, ARGS...>;

	// CTOR - No function
	Virtual(fnCallback pre, fnCallback post) :
		_pre_callback(pre),
		_post_callback(post),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_vtbl_index(INVALID_VTBL_INDEX),
		_in_deletion(false) {
	}
	
	// CTOR - CONST - No function
	Virtual(fnCallbackConst pre, fnCallbackConst post) :
		_pre_callback(reinterpret_cast<fnCallback>(pre)),
		_post_callback(reinterpret_cast<fnCallback>(post)),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_vtbl_index(INVALID_VTBL_INDEX),
		_in_deletion(false) {
	}

	// CTOR - Function
	Virtual(RETURN (CLASS::*function)(ARGS...), fnCallback pre, fnCallback post) : 
		_pre_callback(pre),
		_post_callback(post),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_vtbl_index(GetVtableIndex(function)),
		_in_deletion(false) {
	}

	// CTOR - Function - NULL PRE
	Virtual(RETURN (CLASS::*function)(ARGS...), std::nullptr_t, fnCallback post) : 
		_pre_callback(nullptr),
		_post_callback(post),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_vtbl_index(GetVtableIndex(function)),
		_in_deletion(false) {
	}

	// CTOR - Function - NULL POST
	Virtual(RETURN (CLASS::*function)(ARGS...), fnCallback pre, std::nullptr_t) : 
		_pre_callback(pre),
		_post_callback(nullptr),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_vtbl_index(GetVtableIndex(function)),
		_in_deletion(false) {
	}
	
	// CTOR - CONST - Function
	Virtual(RETURN (CLASS::*function)(ARGS...) const, fnCallbackConst pre, fnCallbackConst post) : 
		_pre_callback(reinterpret_cast<fnCallback>(pre)),
		_post_callback(reinterpret_cast<fnCallback>(post)),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_vtbl_index(GetVtableIndex(function)),
		_in_deletion(false) {
	}

	// CTOR - CONST - Function - NULL PRE
	Virtual(RETURN (CLASS::*function)(ARGS...) const, std::nullptr_t, fnCallbackConst post) : 
		_pre_callback(nullptr),
		_post_callback(reinterpret_cast<fnCallback>(post)),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_vtbl_index(GetVtableIndex(function)),
		_in_deletion(false) {
	}

	// CTOR - CONST - Function - NULL POST
	Virtual(RETURN (CLASS::*function)(ARGS...) const, fnCallbackConst pre, std::nullptr_t) : 
		_pre_callback(reinterpret_cast<fnCallback>(pre)),
		_post_callback(nullptr),
		_context(nullptr),
		_context_pre_callback(nullptr),
		_context_post_callback(nullptr),
		_vtbl_index(GetVtableIndex(function)),
		_in_deletion(false) {
	}

	// CTOR - No Function - Context
	template<typename CONTEXT>
	Virtual(CONTEXT* context, fnContextCallback<CONTEXT> pre, fnContextCallback<CONTEXT> post) :
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(ExtractMFP(post)),
		_vtbl_index(INVALID_VTBL_INDEX),
		_in_deletion(false) {
	}

	// CTOR - CONST - No Function - Context
	template<typename CONTEXT>
	Virtual(CONTEXT* context, fnContextCallbackConst<CONTEXT> pre, fnContextCallbackConst<CONTEXT> post) :
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(ExtractMFP(post)),
		_vtbl_index(INVALID_VTBL_INDEX),
		_in_deletion(false) {
	}
	
	// CTOR - No function - Context - NULL PRE
	template<typename CONTEXT>
	Virtual(CONTEXT* context, std::nullptr_t, fnContextCallback<CONTEXT> post) :
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(nullptr),
		_context_post_callback(ExtractMFP(post)),
		_vtbl_index(INVALID_VTBL_INDEX),
		_in_deletion(false) {
	}
	
	// CTOR - CONST - No function - Context - NULL PRE
	template<typename CONTEXT>
	Virtual(CONTEXT* context, std::nullptr_t, fnContextCallbackConst<CONTEXT> post) :
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(nullptr),
		_context_post_callback(ExtractMFP(post)),
		_vtbl_index(INVALID_VTBL_INDEX),
		_in_deletion(false) {
	}

	// CTOR - No function - Context - NULL POST
	template<typename CONTEXT>
	Virtual(CONTEXT* context, fnContextCallback<CONTEXT> pre, std::nullptr_t) :
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(nullptr),
		_vtbl_index(INVALID_VTBL_INDEX),
		_in_deletion(false) {
	}

	// CTOR - CONST - No function - Context - NULL POST
	template<typename CONTEXT>
	Virtual(CONTEXT* context, fnContextCallbackConst<CONTEXT> pre, std::nullptr_t) :
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(nullptr),
		_vtbl_index(INVALID_VTBL_INDEX),
		_in_deletion(false) {
	}
	
	// CTOR - Function - Context
	template<typename CONTEXT>
	Virtual(RETURN (CLASS::*function)(ARGS...), CONTEXT* context, fnContextCallback<CONTEXT> pre, fnContextCallback<CONTEXT> post) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(ExtractMFP(post)),
		_vtbl_index(GetVtableIndex(function)),
		_in_deletion(false) {
	}
	
	// CTOR - CONST - Function - Context
	template<typename CONTEXT>
	Virtual(RETURN (CLASS::*function)(ARGS...) const, CONTEXT* context, fnContextCallbackConst<CONTEXT> pre, fnContextCallbackConst<CONTEXT> post) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(ExtractMFP(post)),
		_vtbl_index(GetVtableIndex(function)),
		_in_deletion(false) {
	}

	// CTOR - Function - Context - NULL PRE
	template<typename CONTEXT>
	Virtual(RETURN (CLASS::*function)(ARGS...), CONTEXT* context, std::nullptr_t, fnContextCallback<CONTEXT> post) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(nullptr),
		_context_post_callback(ExtractMFP(post)),
		_vtbl_index(GetVtableIndex(function)),
		_in_deletion(false) {
	}
	
	// CTOR - CONST - Function - Context - NULL PRE
	template<typename CONTEXT>
	Virtual(RETURN (CLASS::*function)(ARGS...) const, CONTEXT* context, std::nullptr_t, fnContextCallbackConst<CONTEXT> post) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(nullptr),
		_context_post_callback(ExtractMFP(post)),
		_vtbl_index(GetVtableIndex(function)),
		_in_deletion(false) {
	}

	// CTOR - Function - Context - NULL POST
	template<typename CONTEXT>
	Virtual(RETURN (CLASS::*function)(ARGS...), CONTEXT* context, fnContextCallback<CONTEXT> pre, std::nullptr_t) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(nullptr),
		_vtbl_index(GetVtableIndex(function)),
		_in_deletion(false) {
	}
	
	// CTOR - CONST - Function - Context - NULL POST
	template<typename CONTEXT>
	Virtual(RETURN (CLASS::*function)(ARGS...) const, CONTEXT* context, fnContextCallbackConst<CONTEXT> pre, std::nullptr_t) : 
		_pre_callback(nullptr),
		_post_callback(nullptr),
		_context(context),
		_context_pre_callback(ExtractMFP(pre)),
		_context_post_callback(nullptr),
		_vtbl_index(GetVtableIndex(function)),
		_in_deletion(false) {
	}

	virtual ~Virtual() {
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
		_Setup(*(void***)this_ptr);
	}

	void Remove(CLASS* this_ptr) {
		{
			std::lock_guard guard(_m_hooked_this);
			_hooked_this.erase(this_ptr);
		}
	}

	void AddGlobal(CLASS* this_ptr) {
		{
			std::lock_guard guard(_m_hooked_this);
			_hooked_global.insert(*(void***)this_ptr);
		}
		_Setup(*(void***)this_ptr);
	}

	void RemoveGlobal(CLASS* this_ptr) {
		{
			std::lock_guard guard(_m_hooked_this);
			_hooked_global.erase(*(void***)this_ptr);
		}
	}

	RETURN CallOriginal(CLASS* this_ptr, ARGS... args) {
		auto original_func = KHook::FindOriginalVirtual(*(void***)this_ptr, _vtbl_index);
		auto mfp = KHook::BuildMFP<CLASS, RETURN, ARGS...>(original_func);
		return (this_ptr->*mfp)(args...);
	}

	void Configure(std::int32_t index) {
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

	void Configure(RETURN (CLASS::*function)(ARGS...)) {
		std::int32_t index = KHook::GetVtableIndex(function);
		if (index == -1) {
			return;
		}
		Configure(index);
	}

	void Configure(RETURN (CLASS::*function)(ARGS...) const) {
		std::int32_t index = KHook::GetVtableIndex(function);
		if (index == -1) {
			return;
		}
		Configure(index);
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
	std::unordered_set<void**> _hooked_global;

	// Called by KHook
	void _KHook_RemovedHook(HookID_t id) {
		std::lock_guard guard(_hooks_stored);
		auto it = _hook_ids_addr.find(id);
		if (it != _hook_ids_addr.end()) {
			_addr_hook_ids.erase(it->second);
		}
	}

	void _Setup(void** vtable) {
		if (vtable == nullptr || _in_deletion || _vtbl_index == INVALID_VTBL_INDEX) {
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

		auto id = ::KHook::SetupVirtualHook(
			vtable,
			_vtbl_index,
			this,
			ExtractMFP(&Self::_KHook_RemovedHook),
			ExtractMFP(&Self::_KHook_Callback_PRE), // preMFP
			ExtractMFP(&Self::_KHook_Callback_POST), // postMFP
			ExtractMFP(&Self::_KHook_MakeReturn), // returnMFP,
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
		{
			std::lock_guard guard(this->_m_hooked_this);
			// Did we hook this ptr
			if (_hooked_this.find(hooked_this) == _hooked_this.end()) {
				// This is perhaps a global hook instead
				if (_hooked_global.find(*(void***)hooked_this) == _hooked_global.end()) {
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
		::KHook::__internal__savereturnvalue(action, false);
	}

	// Called by KHook
	RETURN _KHook_Callback_PRE(ARGS... args) {
		// Retrieve the real VirtualHook
		Self* real_this = (Self*)::KHook::GetContext();
		real_this->_KHook_Callback_Fixed(false, (CLASS*)this, args...);
		if constexpr(!std::is_same<RETURN, void>::value) {
			return *real_this->_fake_return;
		}
	}

	// Called by KHook
	RETURN _KHook_Callback_POST(ARGS... args) {
		// Retrieve the real VirtualHook
		Self* real_this = (Self*)::KHook::GetContext();
		real_this->_KHook_Callback_Fixed(true, (CLASS*)this, args...);
		if constexpr(!std::is_same<RETURN, void>::value) {
			return *real_this->_fake_return;
		}
	}

	// Might be used by KHook
	// Called if hook was selected as override hook
	// It returns the final value the hook will use
	RETURN _KHook_MakeReturn(ARGS...) {
		if constexpr(std::is_same<RETURN, void>::value) {
			::KHook::DestroyReturnValue();
			return;
		} else {
			RETURN ret = *(RETURN*)::KHook::GetCurrentValuePtr(true);
			::KHook::DestroyReturnValue();
			return ret;
		}
	}

	// Called if the hook wasn't superceded
	RETURN _KHook_MakeOriginalCall(ARGS ...args) {
		RETURN (EmptyClass::*ptr)(ARGS...) = BuildMFP<EmptyClass, RETURN, ARGS...>(::KHook::GetOriginalFunction());
		if constexpr(std::is_same<RETURN, void>::value) {
			(((EmptyClass*)this)->*ptr)(args...);
			::KHook::__internal__savereturnvalue(KHook::Return<void>{ KHook::Action::Ignore }, true);
		} else {
			RETURN ret = (((EmptyClass*)this)->*ptr)(args...);
			::KHook::__internal__savereturnvalue(KHook::Return<RETURN>{ KHook::Action::Ignore, ret }, true);
			return ret;
		}
	}
};

#ifdef _WIN32
inline std::int32_t __GetVtableIndex__(const std::uint8_t* func_addr) {
	std::int32_t vtbl_index = 0;
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
		return -1;
	}
#endif
	// jmp [rax] DISP 0
	if (func_addr[0] == 0xFF && func_addr[1] == 0x20) {
		// Instant jump, so no offset
		vtbl_index = 0;
		return vtbl_index;
	}
	// jmp [rax + 0xHH] DISP 8
	else if (func_addr[0] == 0xFF && func_addr[1] == 0x60) {
		vtbl_index = *((std::int8_t*)(func_addr + 2)) / sizeof(void*);
		return vtbl_index;
	}
	// jmp [rax + 0xHHHHHHHH] DISP 32
	else if (func_addr[0] == 0xFF && func_addr[1] == 0xA0) {
		vtbl_index = *((std::int32_t*)(func_addr + 2)) / sizeof(void*);
		return vtbl_index;
	}
	return -1;
}
#endif

struct __MFPInfo__
{
	union
	{
		void* addr;
		std::intptr_t vtbl_index;
	};
	std::intptr_t delta;
};

template<typename CLASS, typename RETURN, typename... ARGS>
inline std::int32_t GetVtableIndex(RETURN (CLASS::*function)(ARGS...)) {
#ifdef _WIN32
	return __GetVtableIndex__(reinterpret_cast<const std::uint8_t*>(ExtractMFP(function)));
#else
	__MFPInfo__* info = (__MFPInfo__*)&function;
	if (info->vtbl_index & 1) {
		return (info->vtbl_index - 1) / sizeof(void*);
	}
	return -1;
#endif
}

template<typename CLASS, typename RETURN, typename... ARGS>
inline std::int32_t GetVtableIndex(RETURN (CLASS::*function)(ARGS...) const) {
#ifdef _WIN32
	return __GetVtableIndex__(reinterpret_cast<const std::uint8_t*>(ExtractMFP(function)));
#else
	__MFPInfo__* info = (__MFPInfo__*)&function;
	if (info->vtbl_index & 1) {
		return (info->vtbl_index - 1) / sizeof(void*);
	}
	return -1;
#endif
}

template<typename CLASS, typename RETURN, typename... ARGS>
inline RETURN CallOriginal(RETURN (CLASS::*function)(ARGS...), CLASS* this_ptr, ARGS... args) {
	auto vtbl_index = ::KHook::GetVtableIndex(function);
	void* func = nullptr;
	if (vtbl_index != -1) {
		func = ::KHook::FindOriginalVirtual(*(void***)this_ptr, vtbl_index);
	}
	else {
		func = ::KHook::FindOriginal(::KHook::ExtractMFP(function));
	}
	auto mfp = ::KHook::BuildMFP<CLASS, RETURN, ARGS...>(func);
	return (this_ptr->*mfp)(args...);
}

template<typename CLASS, typename RETURN, typename... ARGS>
inline RETURN CallOriginal(RETURN (CLASS::*function)(ARGS...) const, const CLASS* this_ptr, ARGS... args) {
	auto vtbl_index = ::KHook::GetVtableIndex(function);
	const void* func = nullptr;
	if (vtbl_index != -1) {
		func = ::KHook::FindOriginalVirtual(*(void***)this_ptr, vtbl_index);
	}
	else {
		func = ::KHook::FindOriginal(::KHook::ExtractMFP(function));
	}
	auto mfp = ::KHook::BuildMFP<CLASS, RETURN, ARGS...>(func);
	return (this_ptr->*mfp)(args...);
}

template<typename CLASS, typename RETURN, typename... ARGS>
inline RETURN CallOriginal(void* func, CLASS* this_ptr, ARGS... args) {
	func = ::KHook::FindOriginal(func);
	auto mfp = ::KHook::BuildMFP<CLASS, RETURN, ARGS...>(func);
	return (this_ptr->*mfp)(args...);
}

template<typename CLASS, typename RETURN, typename... ARGS>
inline RETURN CallOriginal(const void* func, const CLASS* this_ptr, ARGS... args) {
	func = (const void*)::KHook::FindOriginal((void*)func);
	auto mfp = ::KHook::BuildMFP<CLASS, RETURN, ARGS...>(func);
	return (this_ptr->*mfp)(args...);
}

class IKHook {
public:
	virtual HookID_t SetupHook(void* function, void* context, void* removed_function, void* pre, void* post, void* make_return, void* make_call_original, bool async = false) = 0;
	virtual HookID_t SetupVirtualHook(void** vtable, int index, void* context, void* removed_function, void* pre, void* post, void* make_return, void* make_call_original, bool async = false) = 0;
	virtual void RemoveHook(HookID_t id, bool async = false) = 0;
	virtual void* GetContext() = 0;
	virtual void* GetOriginalFunction() = 0;
	virtual void* GetOriginalValuePtr() = 0;
	virtual void* GetOverrideValuePtr() = 0;
	virtual void* GetCurrentValuePtr(bool pop = false) = 0;
	virtual void DestroyReturnValue() = 0;
	virtual void* FindOriginal(void* function) = 0;
	virtual void* FindOriginalVirtual(void** vtable, int index) = 0;
	virtual void* DoRecall(KHook::Action action, void* ptr_to_return, std::size_t return_size, void* init_op, void* deinit_op) = 0;
	virtual void SaveReturnValue(KHook::Action action, void* ptr_to_return, std::size_t return_size, void* init_op, void* deinit_op, bool original) = 0;
};
#ifndef KHOOK_STANDALONE
// KHOOK is exposed by something
extern IKHook* __exported__khook;

KHOOK_API HookID_t SetupHook(void* function, void* context, void* removed_function, void* pre, void* post, void* make_return, void* make_call_original, bool async) {
	// For some hooks this is too early
	if (__exported__khook == nullptr) {
		std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
		std::cout << "!!!!!!!!!!!!!!! WARNING YOU HAVE SETUP YOUR HOOK TOO EARLY, IT WONT WORK !!!!!!!!!!!!!!!\n";
		std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
		std::cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
		std::cerr << "!!!!!!!!!!!!!!! WARNING YOU HAVE SETUP YOUR HOOK TOO EARLY, IT WONT WORK !!!!!!!!!!!!!!!\n";
		std::cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
		return INVALID_HOOK;
	}
	return __exported__khook->SetupHook(function, context, removed_function, pre, post, make_return, make_call_original, async);
}

KHOOK_API HookID_t SetupVirtualHook(void** vtable, int index, void* context, void* removed_function, void* pre, void* post, void* make_return, void* make_call_original, bool async) {
	// For some hooks this is too early
	if (__exported__khook == nullptr) {
		std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
		std::cout << "!!!!!!!!!!!!!!! WARNING YOU HAVE SETUP YOUR HOOK TOO EARLY, IT WONT WORK !!!!!!!!!!!!!!!\n";
		std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
		std::cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
		std::cerr << "!!!!!!!!!!!!!!! WARNING YOU HAVE SETUP YOUR HOOK TOO EARLY, IT WONT WORK !!!!!!!!!!!!!!!\n";
		std::cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
		return INVALID_HOOK;
	}
	return __exported__khook->SetupVirtualHook(vtable, index, context, removed_function, pre, post, make_return, make_call_original, async);
}

KHOOK_API void RemoveHook(HookID_t id, bool async) {
	return __exported__khook->RemoveHook(id, async);
}

KHOOK_API void* GetContext() {
	return __exported__khook->GetContext();
}

KHOOK_API void* GetOriginalFunction() {
	return __exported__khook->GetOriginalFunction();
}

KHOOK_API void* GetOriginalValuePtr() {
	return __exported__khook->GetOriginalValuePtr();
}

KHOOK_API void* GetOverrideValuePtr() {
	return __exported__khook->GetOverrideValuePtr();
}

KHOOK_API void* GetCurrentValuePtr(bool pop) {
	return __exported__khook->GetCurrentValuePtr(pop);
}

KHOOK_API void DestroyReturnValue() {
	return __exported__khook->DestroyReturnValue();
}

KHOOK_API void* FindOriginal(void* function) {
	return __exported__khook->FindOriginal(function);
}

KHOOK_API void* FindOriginalVirtual(void** vtable, int index) {
	return __exported__khook->FindOriginalVirtual(vtable, index);
}

KHOOK_API void* DoRecall(KHook::Action action, void* ptr_to_return, std::size_t return_size, void* init_op, void* deinit_op) {
	return __exported__khook->DoRecall(action, ptr_to_return, return_size, init_op, deinit_op);
}

KHOOK_API void SaveReturnValue(KHook::Action action, void* ptr_to_return, std::size_t return_size, void* init_op, void* deinit_op, bool original) {
	return __exported__khook->SaveReturnValue(action, ptr_to_return, return_size, init_op, deinit_op, original);
}

#endif

}
