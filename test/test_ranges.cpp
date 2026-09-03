#include <gtest/gtest.h>
#include <khook.hpp>
#include "helpers.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct MemoryRegion
{
    uintptr_t start;
    uintptr_t end;
};

#ifndef WIN32

std::vector<MemoryRegion> GetReadableRegions()
{
    std::vector<MemoryRegion> regions;

    std::ifstream maps("/proc/self/maps");
    std::string line;

    while (std::getline(maps, line))
    {
        char perms[5];
        unsigned long long start, end;

        if (sscanf(line.c_str(),
                "%llx-%llx %4s",
                &start,
                &end,
                perms) == 3)
        {
            if (perms[0] == 'r')
            {
                regions.push_back({
                    static_cast<uintptr_t>(start),
                    static_cast<uintptr_t>(end)
                });
            }
        }
    }
    return regions;
}

#else

#include <windows.h>

bool IsReadable(DWORD protect)
{
    protect &= 0xff;

    switch (protect)
    {
    case PAGE_READONLY:
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        return true;

    default:
        return false;
    }
}

std::vector<MemoryRegion> GetReadableRegions()
{
    std::vector<MemoryRegion> regions;

    SYSTEM_INFO si;
    GetSystemInfo(&si);

    uintptr_t addr =
        reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress);

    uintptr_t maxAddr =
        reinterpret_cast<uintptr_t>(si.lpMaximumApplicationAddress);

    MEMORY_BASIC_INFORMATION mbi;

    while (addr < maxAddr)
    {
        SIZE_T result =
            VirtualQuery(
                reinterpret_cast<LPCVOID>(addr),
                &mbi,
                sizeof(mbi));

        if (result == 0)
            break;

        if (IsReadable(mbi.Protect))
        {
            regions.push_back({
                reinterpret_cast<uintptr_t>(mbi.BaseAddress),
                reinterpret_cast<uintptr_t>(mbi.BaseAddress)
                    + mbi.RegionSize
            });
        }

        addr =
            reinterpret_cast<uintptr_t>(mbi.BaseAddress)
            + mbi.RegionSize;
    }

    return regions;
}

#endif


std::uint64_t do_nothing_big(std::uint64_t x)
{
	volatile std::uint64_t sink;

	x = x * 3 + 7;   x ^= x << 13;
	x = x * 5 + 11;  x ^= x >> 17;
	x = x * 7 + 13;  x ^= x << 9;
	x = x * 11 + 17; x ^= x >> 21;
	x = x * 13 + 19; x ^= x << 5;
	x = x * 17 + 23; x ^= x >> 11;
	x = x * 19 + 29; x ^= x << 7;
	x = x * 23 + 31; x ^= x >> 13;
	x = x * 29 + 37; x ^= x << 17;
	x = x * 31 + 41; x ^= x >> 19;

	sink = x;

	return x;
}

class RangesTest : public ::testing::Test {
public:
    static const constexpr std::size_t SIGNATURE_BYTES = 20;
    RangesTest() {
        // Make a signature of 50 bytes
        _function = reinterpret_cast<void*>(&do_nothing_big);

        std::uint8_t* read = reinterpret_cast<std::uint8_t*>(_function);

        for (std::size_t i = 0; i < SIGNATURE_BYTES; i++) {
            auto byte = read[i];
            auto lower = byte % 16;
            auto upper = (byte - lower) / 16;

            _signature[i * 3 + 0] = (upper >= 10) ? 'A' + (upper - 10) : '0' + upper;
            _signature[i * 3 + 1] = (lower >= 10) ? 'A' + (lower - 10) : '0' + lower;
            _signature[i * 3 + 2] = ' ';
        }
        _signature[sizeof(_signature) - 1] = '\0';

        std::cout << std::hex << reinterpret_cast<std::uintptr_t>(_function) << " | Crafted signature: " << _signature << std::endl;
    }

    bool Lookup() {
        auto regions = GetReadableRegions();
        for (const auto& region : regions) {
            auto ptr = KHook::LookupSignature(reinterpret_cast<void*>(region.start), static_cast<std::size_t>(region.end - region.start), _signature);
            if (_function == ptr) {
                std::cout << "Function lookup success!" << std::endl;
                return true;
            }
        }
        return false;
    }

    char _signature[SIGNATURE_BYTES * 3];
    void* _function;
};

TEST_F(RangesTest, Lookup) {
    EXPECT_EQ(Lookup(), true) << "Failed to sig scan our function";
}

TEST_F(RangesTest, LookupWildcard) {
    static const constexpr int byte1 = 5;
    static_assert(byte1 <= RangesTest::SIGNATURE_BYTES);
    static const constexpr int byte2 = 17;
    static_assert(byte2 <= RangesTest::SIGNATURE_BYTES);
    static const constexpr int byte3 = 8;
    static_assert(byte3 <= RangesTest::SIGNATURE_BYTES);
    
    _signature[byte1 * 3 + 0] = '?';
    _signature[byte1 * 3 + 1] = '?';
    _signature[byte2 * 3 + 0] = '?';
    _signature[byte2 * 3 + 1] = '?';
    _signature[byte3 * 3 + 0] = '?';
    _signature[byte3 * 3 + 1] = '?';

    EXPECT_EQ(Lookup(), true) << "Failed to sig scan our function";
}

TEST_F(RangesTest, LookupWithHook) {
    static auto nothing = [](std::uint64_t x){ return x; };

    EXPECT_NE(KHook::SetupHook(
        reinterpret_cast<void*>(&do_nothing_big),
        nullptr,
        nullptr,
        reinterpret_cast<void*>(&nothing),
        reinterpret_cast<void*>(&nothing),
        reinterpret_cast<void*>(&nothing),
        reinterpret_cast<void*>(&nothing),
        100,
        false
    ), KHook::INVALID_HOOK) << "Failed to setup hook";

    EXPECT_EQ(Lookup(), true) << "Failed to sig scan our function";
}