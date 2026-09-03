#include <khook.hpp>

#if defined(_MSC_VER)
    #define NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
    #define NOINLINE __attribute__((noinline))
    #define __thiscall
#else
    #define NOINLINE
#endif

template <typename RETURN, typename... ARGS>
class FunctionContext {
public:
	using fnContextCallback = ::KHook::Return<RETURN> (FunctionContext::*)(ARGS...);
    using fnCallback = ::KHook::Return<RETURN> (*)(ARGS...);
    using fnHooked = RETURN (*)(ARGS...);

    FunctionContext() : m_pre(nullptr), m_post(nullptr) {}
    FunctionContext(fnCallback callback) : m_pre(callback), m_post(callback) {}
    FunctionContext(fnCallback pre, fnCallback post) : m_pre(pre), m_post(post) {}

    void Configure(fnCallback pre, fnCallback post) {
        m_pre = pre;
        m_post = post;
    }

    ::KHook::Return<RETURN> OnPre(ARGS... args) {
        if (m_pre) {
            return m_pre(args...);
        }
        return {KHook::Action::Ignore};
    }

    ::KHook::Return<RETURN> OnPost(ARGS... args) {
        if (m_post) {
            return m_post(args...);
        }
        return {KHook::Action::Ignore};
    }

private:
    fnCallback m_pre;
    fnCallback m_post;
};

template <typename TargetClass, typename RETURN, typename... ARGS>
class MemberContext {
public:
    using fnContextCallback = ::KHook::Return<RETURN> (MemberContext::*)(TargetClass*, ARGS...);
    using fnCallback = ::KHook::Return<RETURN> (*)(TargetClass*, ARGS...);
    using fnHooked = RETURN (*)(TargetClass*, ARGS...);

    MemberContext() : m_pre(nullptr), m_post(nullptr) {}
    MemberContext(fnCallback callback) : m_pre(callback), m_post(callback) {}
    MemberContext(fnCallback pre, fnCallback post) : m_pre(pre), m_post(post) {}

    void Configure(fnCallback pre, fnCallback post) {
        m_pre = pre;
        m_post = post;
    }

    ::KHook::Return<RETURN> OnPre(TargetClass* _this, ARGS... args) {
        if (m_pre) {
            return m_pre(_this, args...);
        }
        return {KHook::Action::Ignore};
    }

    ::KHook::Return<RETURN> OnPost(TargetClass* _this, ARGS... args) {
        if (m_post) {
            return m_post(_this, args...);
        }
        return {KHook::Action::Ignore};
    }

private:
    fnCallback m_pre;
    fnCallback m_post;
};

template <typename TargetClass, typename RETURN, typename... ARGS>
class VirtualContext {
public:
    using fnContextCallback = ::KHook::Return<RETURN> (VirtualContext::*)(TargetClass*, ARGS...);
    using fnCallback = ::KHook::Return<RETURN> (*)(TargetClass*, ARGS...);
    using fnHooked = RETURN (*)(TargetClass*, ARGS...);

    VirtualContext() : m_pre(nullptr), m_post(nullptr) {}
    VirtualContext(fnCallback callback) : m_pre(callback), m_post(callback) {}
    VirtualContext(fnCallback pre, fnCallback post) : m_pre(pre), m_post(post) {}

    void Configure(fnCallback pre, fnCallback post) {
        m_pre = pre;
        m_post = post;
    }

    ::KHook::Return<RETURN> OnPre(TargetClass* _this, ARGS... args) {
        if (m_pre) {
            return m_pre(_this, args...);
        }
        return {KHook::Action::Ignore};
    }

    ::KHook::Return<RETURN> OnPost(TargetClass* _this, ARGS... args) {
        if (m_post) {
            return m_post(_this, args...);
        }
        return {KHook::Action::Ignore};
    }

private:
    fnCallback m_pre;
    fnCallback m_post;
};