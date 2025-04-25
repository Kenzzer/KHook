# KHook

A spiritual successor to SourceHook for centralized detouring. KHook is primarily 

## Support

The library currently provides support for Windows (x86 & x86_64) and Linux (x86 & x86_64)

## Building

Although we provide CMake as a convenience, the primary supported build tool is [AMBuild](https://github.com/alliedmodders/ambuild).

Requirement :
- C++17 or higher

Once setup :
- `python configure.py --out release`
- `ambuild release`

## How does it work ?

Built on top of another detouring library named [SafetyHook](https://github.com/alliedmodders/safetyhook), KHook offers the possibility for different consumers to detour a same function address. Consumers have the choice to register PRE & POST callbacks, those are functions that will be invoked before and after the original function is executed.

Each PRE & POST callback is invoked with the exact same parameters and calling convention as the detoured function. Each invoked consumer callback is then offered the possibility by KHook to retrieve (through a thread-local global function) a context pointer, the same context pointer the consummer gave when registering the detour. At the end of a callback execution, the consumer must inform KHook which "Action" they wish to take against the detoured function and they must also provide KHook with a pointer to the return value.

"Action" is the choice each consummer is given when their callbacks get invoked for a detoured function. Three actions are possible :
- `KHook::Action::Ignore` | No actions should be taken.
- `KHook::Action::Override` | The original function should be called, but the return value must be overwritten with the provided return value ptr.
- `KHook::Action::Supercede` | Same as `KHook::Action::Override` but the original function won't be called.

> [!NOTE] 
> As its description imply, using `KHook::Action::Supercede` under a POST callback will not prevent the original function from being called because it has been already called.

> [!WARNING] 
> KHook will override the return value with whichever hook took `KHook::Action::Override` or `KHook::Action::Supercede` first. Meaning if two callbacks set `KHook::Action::Supercede` as their action, only the first invoked callback will see its return value pointer used. KHook makes no effort at resolving such conflicts.

In order to simplify the library usage KHook provides a few class templates that automatically handle the registration of callbacks and return value pointers.
- `KHook::Function<return_type, args...> hook(precallback, postcallback)` | Template for generic functions.
- `KHook::Member<DetourClassName, return_type, args...> hook(precallback, postcallback)` | Template for member functions.
- `KHook::Virtual<DetourClassName, return_type, args...> hook(precallback, postcallback)` | Template for virtual member functions.
A callback definition looks like this :
```cpp
// KHook::Function
KHook::Return<return_type> precallback(args...) {
    return { KHook::Action::Value, return_type::Value };
}
KHook::Return<return_type> postcallback(args...) {
    return { KHook::Action::Value, return_type::Value };
}
// KHook::Member & KHook::Virtual
KHook::Return<return_type> precallback(DetourClassName* thisPtr, args...) {
    return { KHook::Action::Value, return_type::Value };
}
KHook::Return<return_type> postcallback(DetourClassName* thisPtr, args...) {
    return { KHook::Action::Value, return_type::Value };
}
```

If context is required a context pointer can be provided to the constructor.
- `KHook::Function<return_type, args...> hook(context, precallback, postcallback)`
- `KHook::Member<DetourClassName, return_type, args...> hook(context, precallback, postcallback)`
- `KHook::Virtual<DetourClassName, return_type, args...> hook(context, precallback, postcallback)`
Providing a context pointer changes the callback definition to :
```cpp
// KHook::Function
KHook::Return<return_type> ContextClassName::precallback(args...) {
    return { KHook::Action::Value, return_type::Value };
}
KHook::Return<return_type> ContextClassName::postcallback(args...) {
    return { KHook::Action::Value, return_type::Value };
}
// KHook::Member & KHook::Virtual
KHook::Return<return_type> ContextClassName::precallback(DetourClassName* thisPtr, args...) {
    return { KHook::Action::Value, return_type::Value };
}
KHook::Return<return_type> ContextClassName::postcallback(DetourClassName* thisPtr, args...) {
    return { KHook::Action::Value, return_type::Value };
}
```

All hooks can be configured through the use of the function `Configure`.
- `KHook::Function::Configure(void* function_ptr)`
- `KHook::Member::Configure(void* function_ptr)` or `KHook::Member::Configure(DetourClassName::Function* ptr)`
- `KHook::Virtual::Configure(std::uint32_t vtable_index)` or `KHook::Virtual::Configure(DetourClassName::Function* ptr)`

> [!NOTE] 
> Because `KHook::Virtual` is configured with a vtable index and not a function address the detour can't be immediately created. `KHook::Virtual::Add(DetourClassName* thisPtr)` and `KHook::Virtual::Remove(DetourClassName* thisPtr)` must be called.

## Testing

There is currently no test suite.