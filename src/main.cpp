#include <iostream>

#include "detour.hpp"
#include "khook.hpp"

int original_function(float p1, int p2, float p3, int p4, double p5) {
    std::cout << "original" << std::endl;
    return 34;
}

KHook::Return<int> original_function_pre(float p1, int p2, float p3, int p4, double p5) {
    std::cout << "pre" << std::endl;
    return { KHook::Action::Ignore };
}

KHook::Return<int> original_function_post(float p1, int p2, float p3, int p4, double p5) {
    std::cout << "post" << std::endl;
    return { KHook::Action::Ignore, 52 };
}

KHook::Return<int> original_function_post2(float p1, int p2, float p3, int p4, double p5) {
    std::cout << "post 2" << std::endl;
    return { KHook::Action::Supercede, 49 };
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
    std::cout << "pre" << std::endl;
    return { KHook::Action::Ignore, 43.0 };
}

KHook::Return<float> test_post(TestClass* ptr, float x, float y, float z) {
    std::cout << "post" << std::endl;
    return { KHook::Action::Supercede, 57.0 };
}

KHook::Virtual testHook3(&TestClass::Test, test_pre, test_post);

int main() {
    std::cin.get();
    /*std::cout << "Call original_function" << std::endl;
    int ret = original_function(4.0, 5, 6.0, 2, 7.0);
    std::cout << "return : " << std::dec << ret << std::endl;
    */
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