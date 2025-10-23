#include <iostream>

#include "detour.hpp"
#include "khook.hpp"

int original_function(float p1, int p2, float p3, int p4, double p5) {
    std::cout << "original" << std::endl;
    return 34;
}

KHook::Return<int> original_function_pre(float p1, int p2, float p3, int p4, double p5) {
    std::cout << "pre " << p1 << "|" << p2 << "|" << p3 << std::endl;
    return { KHook::Action::Ignore };
}

KHook::Return<int> original_function_post(float p1, int p2, float p3, int p4, double p5) {
    std::cout << "post" << std::endl;
    return { KHook::Action::Ignore, 52 };
}

KHook::Return<int> original_function_post2(float p1, int p2, float p3, int p4, double p5) {
    std::cout << "post 2" << std::endl;
    return { KHook::Action::Supersede, 49 };
}

KHook::Function testHook(original_function, original_function_pre, original_function_post);
KHook::Function testHook2(original_function, original_function_pre, original_function_post2);

class TestClass {
public:
    virtual void Foo() {}
    virtual void Goo() {}
    virtual void Boo() {}
    virtual void Xoo() {}
    virtual void Too() {}
    virtual float Test(float x, float y, float z) {
        std::cout << "x: " << std::dec << x << std::endl;
        std::cout << "y: " << std::dec << y << std::endl;
        std::cout << "z: " << std::dec << z << std::endl;
        std::cout << "original this: " << std::hex << this << std::endl;
        return 52.0;
    }
};

KHook::Return<float> test_pre(TestClass* ptr, float x, float y, float z) {
    std::cout << "pre " << x << "|" << y << "|" << z << std::endl;
    std::cout << ptr << std::endl;

    KHook::Recall(KHook::Return<float>{ KHook::Action::Supersede, 66.0f }, ptr, x, y, 69.0f);
    std::cout << "recall over" << std::endl;
    return { KHook::Action::Supersede, 43.0 };
}

KHook::Return<float> test_post(TestClass* ptr, float x, float y, float z) {
    std::cout << "post " << x << "|" << y << "|" << z << std::endl;
    //KHook::Recall(KHook::Return<float>{ KHook::Action::Supersede, 69.0f }, ptr, x, y, 69.0f);
    return { KHook::Action::Supersede, 57.0 };
}

KHook::Virtual testHook3(&TestClass::Test, test_pre, test_post);

int main() {
    std::cin.get();
    TestClass cls;
    std::cout << "this: " << std::hex << &cls << std::endl;
    std::cout << "hook: " << std::hex << &testHook3 << std::endl;
    //float ret2 = cls.Test(8, 6, 9);
    //std::cout << "cls return : " << std::dec << ret2 << std::endl;

    testHook3.Add(&cls);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    //auto mfp = &TestClass::Test;
    //ret2 = (&cls->*mfp)(5, 2, 7);
    float ret2 = cls.Test(5, 2, 7);
    std::cout << "cls return : " << std::dec << ret2 << std::endl;

    KHook::Shutdown();
    return 0;
}
/*
#include <iostream>
#include <cassert>
#include <thread>
#include "khook.hpp"

#include "main.hpp"

#if defined(_MSC_VER)
    #define NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
    #define NOINLINE __attribute__((noinline))
    #define __thiscall
#else
    #define NOINLINE
#endif

class TestObject {};

class TestClass {
public:
    NOINLINE bool IsAllowed(TestObject* obj) {
        std::cout << "TestClass::IsAllowed()" << std::endl;
        return true;
    }
};

NOINLINE void bool_copy(bool* dst, bool* src) {
    *dst = *src;
}

NOINLINE void bool_dtor(bool* ptr) {
    *ptr = false;
}

class FakeClass {
public:
NOINLINE bool TestDetour_Pre(TestObject* obj) {
    std::cout << "TestDetour_Pre()" << std::endl;
    std::cout << "ptr: " << std::hex << this << std::endl;
    std::cout << "obj: " << std::hex << obj << std::endl;
    bool result = false;
    KHook::SaveReturnValue(KHook::Action::Override, &result, sizeof(bool), (void*)bool_copy, (void*)bool_dtor, false);
    return false;
}

NOINLINE bool TestDetour_CallOriginal(TestObject* obj) {
    std::cout << "TestDetour_CallOriginal()" << std::endl;
    std::cout << "ptr: " << std::hex << this << std::endl;
    std::cout << "obj: " << std::hex << obj << std::endl;
    auto original = KHook::BuildMFP<FakeClass, bool, TestObject*>(KHook::GetOriginalFunction());
    bool result = (this->*original)(obj);
    KHook::SaveReturnValue(KHook::Action::Override, &result, sizeof(bool), (void*)bool_copy, (void*)bool_dtor, true);
    return false;
}

NOINLINE bool TestDetour_Post(TestObject* obj) {
    std::cout << "TestDetour_Post()" << std::endl;
    std::cout << "ptr: " << std::hex << this << std::endl;
    std::cout << "obj: " << std::hex << obj << std::endl;
    KHook::SaveReturnValue(KHook::Action::Ignore, nullptr, 0, nullptr, nullptr, false);
    return false;
}

NOINLINE bool TestDetour_MakeReturn(TestObject* obj) {
    std::cout << "TestDetour_MakeReturn()" << std::endl;
    std::cout << "ptr: " << std::hex << this << std::endl;
    std::cout << "obj: " << std::hex << obj << std::endl;
    bool result = *((bool*)KHook::GetCurrentValuePtr(true));
    KHook::DestroyReturnValue();
    return result;
}

NOINLINE void TestDetour_OnRemoved(int hookId)
{
}

};

int main() {
    int hookId = KHook::SetupHook(
        KHook::ExtractMFP(&TestClass::IsAllowed),
        nullptr,
        KHook::ExtractMFP(&FakeClass::TestDetour_OnRemoved),
        KHook::ExtractMFP(&FakeClass::TestDetour_Pre),
        KHook::ExtractMFP(&FakeClass::TestDetour_Post),
        KHook::ExtractMFP(&FakeClass::TestDetour_MakeReturn),
        KHook::ExtractMFP(&FakeClass::TestDetour_CallOriginal),
        true);

    assert(hookId != KHook::INVALID_HOOK);

    std::cout << "hook id: " << std::dec << hookId << std::endl;

    TestClass* target = new TestClass();
    TestObject* obj = new TestObject();

    std::cout << "target: " << std::hex << target << std::endl;
    std::cout << "obj: " << std::hex << obj << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(1));

    bool result = target->IsAllowed(obj);

    std::cout << "returned: " << result << std::endl;

    delete obj;
    delete target;

    KHook::Shutdown();

    return 0;
}*/