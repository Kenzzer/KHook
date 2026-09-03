#pragma once

#include <cstdint>
#include <string>
#include <memory>

#include <khook/memory.hpp>

namespace KHook::Ranges {

struct Range {
	using Self = Range;
	Range(const Self&) = delete;
	Range& operator= (const Self&) = delete;
	Range(uintptr_t begin, uintptr_t end) : begin(begin), end(end) {
		auto len = (end - begin) + 1;
		og_bytes = new std::uint8_t[len];

		auto read = reinterpret_cast<std::uint8_t*>(begin);
		for (decltype(len) i = 0; i < len; i++, read++) {
			og_bytes[i] = *read;
		}
	}
	~Range() {
		delete[] og_bytes;
	}

    uintptr_t begin;
    uintptr_t end;
	std::uint8_t* og_bytes;
};

bool Add(std::unique_ptr<Range> range);

// 0 - If lookup fails
std::uintptr_t Lookup(std::uintptr_t start, std::size_t size, const std::string& bytes);

}