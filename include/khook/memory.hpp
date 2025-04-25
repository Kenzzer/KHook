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
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace KHook
{
	namespace Memory
	{
		enum Flags : std::uint8_t {
			READ = (1 << 0),
			WRITE = (1 << 1),
			EXECUTE = (1 << 2)
		};

		inline bool SetAccess(void *addr, std::size_t len, std::uint8_t access)
		{
#ifdef _WIN32
			DWORD tmp;
			DWORD prot;
			switch (access) {
			case Flags::READ:
				prot = PAGE_READONLY; break;
			case Flags::READ | Flags::WRITE:
				prot = PAGE_READWRITE; break;
			case Flags::READ | Flags::EXECUTE:
				prot = PAGE_EXECUTE_READ; break;
			default:
			case Flags::READ | Flags::WRITE | Flags::EXECUTE:
				prot = PAGE_EXECUTE_READWRITE; break;
			}
			return VirtualProtect(addr, len, prot, &tmp) ? true : false;
#else
			int prot = 0;
			if ((access & Flags::EXECUTE) == Flags::EXECUTE) {
				prot |= PROT_EXEC;
			}
			if ((access & Flags::WRITE) == Flags::WRITE) {
				prot |= PROT_WRITE;
			}
			if ((access & Flags::READ) == Flags::READ) {
				prot |= PROT_READ;
			}
			long pagesize = sysconf(_SC_PAGESIZE);
			return mprotect((void*)(((uintptr_t)addr) & ~(pagesize-1)), len + (((uintptr_t)addr) % pagesize), prot) == 0 ? true : false;
#endif
		}
	}
}