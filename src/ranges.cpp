#include "ranges.hpp"

#include <vector>
#include <algorithm>
#include <shared_mutex>
#include <mutex>
#include <cstring>
#include <iostream>
#include <iomanip>

namespace KHook::Ranges {

std::vector<std::unique_ptr<Range>> g_ranges;
std::shared_mutex g_mutex;

bool Add(std::unique_ptr<Range> range) {
	std::unique_lock lock(g_mutex);

	if (range->begin > range->end) {
		return false;
	}

	auto it = std::lower_bound(
	g_ranges.begin(),
	g_ranges.end(),
	range->begin,
	[](const std::unique_ptr<Range>& r, std::uintptr_t value) {
		return r->begin < value;
	});

    // Check overlap with previous range
    if (it != g_ranges.begin()) {
        auto prev = std::prev(it);

        if (range->begin <= (*prev)->end) {
			return false;
		}
    }

    // Check overlap with next range
    if (it != g_ranges.end()) {
        if (range->end >= (*it)->begin) {
			return false;
		}
    }

    g_ranges.insert(it, std::move(range));
    return true;
}

bool IsIn(std::uintptr_t value) {
	std::shared_lock lock(g_mutex);

	auto it = std::upper_bound(
	g_ranges.begin(),
	g_ranges.end(),
	value,
	[](uint32_t v, const std::unique_ptr<Range>& r) {
		return v < r->begin;
	});

	if (it == g_ranges.begin()) {
		return false;
	}

	--it;
	return value <= (*it)->end;
}

std::uintptr_t Lookup(std::uintptr_t start, std::size_t size, const std::string& bytes) {
	// Parse the bytes sequence
	std::vector<std::uint16_t> sequence;
	auto c_string = bytes.c_str();

	static auto p = [](const char c) {
		return ('0' <= c && c <= '9') ? c - '0'
		: ('a' <= c && c <= 'f') ? 10 + (c - 'a')
		: ('A' <= c && c <= 'F') ? 10 + (c - 'A') : -1;
	};

	for (int i = 0; i <= bytes.size(); i++) {
		if (i == bytes.size() || bytes[i] == ' ') {
			// New bytes/end of string. Process what we parsed
			if (c_string[i - 2] == '?' && c_string[i - 1] == '?') {
				// Wildcard
				sequence.push_back(0xFFFF);
			} else if (p(c_string[i - 1]) != -1 && p(c_string[i - 2]) != -1) {
				std::uint16_t parsed = p(c_string[i - 1]) + (16 * p(c_string[i - 2]));
				sequence.push_back(parsed);
			} else {
				return 0;
			}
		}
	}

	/*std::cout << "Searching: " << std::setfill('0');
	for (int i = 0; i < sequence.size(); i++) {
		if (sequence[i] == 0xFFFF) {
			std::cout << "??";
		} else {
			std::cout << std::hex << std::setw(2) << sequence[i];
		}
		std::cout << " ";
	}
	std::cout << std::endl;*/

	std::shared_lock lock(g_mutex);

	auto it = std::upper_bound(
	g_ranges.begin(),
	g_ranges.end(),
	start + size,
	[start](std::uintptr_t v, const std::unique_ptr<Range>& r) {
		return r->begin <= v && r->begin >= start;
	});

	const auto& it_end = g_ranges.end();
	for (auto lookup = start, lookup_end = lookup + size; lookup < lookup_end; lookup++) {
		// Move the iterator further until we intersect again
		while (it != it_end && (*it)->end < lookup) {
			it++;
		}

		auto read_it = it;
		bool found = true;
		for (std::size_t i = 0; i < sequence.size() && found; i++) {
			if (sequence[i] == 0xFFFF) {
				// Wildcard, skip
				continue;
			}

			auto read = lookup + i;

			// Move the iterator further until we intersect again
			while (read_it != it_end && (*read_it)->end < read) {
				read_it++;
			}

			if (it_end != read_it) {
				if (read >= (*read_it)->begin && read <= (*read_it)->end) {
					auto diff = read - (*read_it)->begin;
					read = reinterpret_cast<std::uintptr_t>(&((*read_it)->og_bytes[diff]));
					std::cout << " og: " << std::hex << static_cast<std::uint16_t>((*read_it)->og_bytes[diff]);
				}
			}

			// Ensure bytes are matching 
			found &= (*reinterpret_cast<std::uint8_t*>(read) == sequence[i]);
		}

		if (found) {
			std::cout << "found" << std::endl;
			return lookup;
		}
	}

	// Failure
	return 0;
}

}