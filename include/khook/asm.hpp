/* ======== KHook ========
* Copyright (C) 2025
* No warranties of any kind
*
* License: zLib License
*
* Author(s): Benoist "Kenzzer" ANDRÉ
* ============================
*/
/* ======== SourceHook ========
* Copyright (C) 2004-2010 Metamod:Source Development Team
* No warranties of any kind
*
* License: zlib/libpng
*
* Author(s): Pavol "PM OnoTo" Marko, David "BAILOPAN" Anderson
* ============================
*/
#pragma once

#include "memory.hpp"

#include <cstdint>
#include <memory>
#include <cstring>
#include <cassert>

#define assertm(exp, msg) assert((void(msg), exp))

namespace KHook
{
	namespace Asm
	{
		template <class T>
		class List
		{
		public:
			class iterator;
			friend class iterator;
			class ListNode
			{
			public:
				ListNode(const T & o) : obj(o) { };
				ListNode() { };
				T obj;
				ListNode *next;
				ListNode *prev;
			};
		private:
			// Initializes the sentinel node.
			// BAIL used malloc instead of new in order to bypass the need for a constructor.
			ListNode *_Initialize()
			{
				ListNode *n = (ListNode *)malloc(sizeof(ListNode));
				n->next = n;
				n->prev = n;
				return n;
			}
		public:
			List() : m_Head(_Initialize()), m_Size(0)
			{
			}
			List(const List &src) : m_Head(_Initialize()), m_Size(0)
			{
				iterator iter;
				for (iter=src.begin(); iter!=src.end(); iter++)
					push_back( (*iter) );
			}
			~List()
			{
				clear();

				// Don't forget to free the sentinel
				if (m_Head)
				{
					free(m_Head);
					m_Head = NULL;
				}
			}
			void push_back(const T &obj)
			{
				ListNode *node = new ListNode(obj);

				node->prev = m_Head->prev;
				node->next = m_Head;
				m_Head->prev->next = node;
				m_Head->prev = node;

				m_Size++;
			}

			void push_front(const T &obj)
			{
				insert(begin(), obj);
			}

			void push_sorted(const T &obj)
			{
				iterator iter;
				for (iter = begin(); iter != end(); ++iter)
				{
					if (obj < *iter)
					{
						insert(iter, obj);
						return;
					}
				}
				push_back(obj);
			}

			size_t size() const
			{
				return m_Size;
			}

			void clear()
			{
				ListNode *node = m_Head->next;
				ListNode *temp;
				m_Head->next = m_Head;
				m_Head->prev = m_Head;

				// Iterate through the nodes until we find g_Head (the sentinel) again
				while (node != m_Head)
				{
					temp = node->next;
					delete node;
					node = temp;
				}
				m_Size = 0;
			}
			bool empty() const
			{
				return (m_Size == 0);
			}
			T & front()
			{
				return m_Head->next->obj;
			}
			T & back()
			{
				return m_Head->prev->obj;
			}
		private:
			ListNode *m_Head;
			size_t m_Size;
		public:
			class iterator
			{
			friend class List;
			public:
				iterator()
				{
					m_This = NULL;
				}
				iterator(const List &src)
				{
					m_This = src.m_Head;
				}
				iterator(ListNode *n) : m_This(n)
				{
				}
				iterator(const iterator &where)
				{
					m_This = where.m_This;
				}
				//pre decrement
				iterator & operator--()
				{
					if (m_This)
						m_This = m_This->prev;
					return *this;
				}
				//post decrement
				iterator operator--(int)
				{
					iterator old(*this);
					if (m_This)
						m_This = m_This->prev;
					return old;
				}	
				
				//pre increment
				iterator & operator++()
				{
					if (m_This)
						m_This = m_This->next;
					return *this;
				}
				//post increment
				iterator operator++(int)
				{
					iterator old(*this);
					if (m_This)
						m_This = m_This->next;
					return old;
				}
				
				const T & operator * () const
				{
					return m_This->obj;
				}
				T & operator * ()
				{
					return m_This->obj;
				}
				
				T * operator -> ()
				{
					return &(m_This->obj);
				}
				const T * operator -> () const
				{
					return &(m_This->obj);
				}
				
				bool operator != (const iterator &where) const
				{
					return (m_This != where.m_This);
				}
				bool operator ==(const iterator &where) const
				{
					return (m_This == where.m_This);
				}

				operator bool()
				{
					return m_This != NULL;
				}
			private:
				ListNode *m_This;
			};
		public:
			iterator begin() const
			{
				return iterator(m_Head->next);
			}
			iterator end() const
			{
				return iterator(m_Head);
			}
			iterator erase(iterator &where)
			{
				ListNode *pNode = where.m_This;
				iterator iter(where);
				iter++;


				// Works for all cases: empty list, erasing first element, erasing tail, erasing in the middle...
				pNode->prev->next = pNode->next;
				pNode->next->prev = pNode->prev;

				delete pNode;
				m_Size--;

				return iter;
			}

			iterator insert(iterator where, const T &obj)
			{
				// Insert obj right before where

				ListNode *node = new ListNode(obj);
				ListNode *pWhereNode = where.m_This;
				
				pWhereNode->prev->next = node;
				node->prev = pWhereNode->prev;
				pWhereNode->prev = node;
				node->next = pWhereNode;

				m_Size++;

				return iterator(node);
			}

		public:
			void remove(const T & obj)
			{
				iterator b;
				for (b=begin(); b!=end(); b++)
				{
					if ( (*b) == obj )
					{
						erase( b );
						break;
					}
				}
			}
			template <typename U>
			iterator find(const U & equ) const
			{
				iterator iter;
				for (iter=begin(); iter!=end(); iter++)
				{
					if ( (*iter) == equ )
						return iter;
				}
				return end();
			}
			List & operator =(const List &src)
			{
				clear();
				iterator iter;
				for (iter=src.begin(); iter!=src.end(); iter++)
					push_back( (*iter) );
				return *this;
			}
		};
		/*
		Class which lets us allocate memory regions in special pages only meant for on the fly code generation.

		If we alloc with malloc and then set the page access type to read/exec only, other regions returned by
		malloc that are in the same page would lose their write access as well and the process could crash.

		Allocating one page per code generation session is usually a waste of memory and on some platforms also
		a waste of virtual address space (Windows’ VirtualAlloc has a granularity of 64K).


		IMPORTANT: the memory that Alloc() returns is not a in a defined state!
		It could be in read+exec OR read+write mode.
		-> call SetRE() or SetRW() before using allocated memory!
		*/
		class CPageAlloc
		{
			struct AllocationUnit
			{
				std::size_t begin_offset;
				std::size_t size;

				AllocationUnit(std::size_t p_offs, std::size_t p_size) : begin_offset(p_offs), size(p_size)
				{
				}

				bool operator < (const AllocationUnit &other) const
				{
					return begin_offset < other.begin_offset;
				}
			};

			typedef List<AllocationUnit> AUList;
			struct AllocatedRegion
			{
				void *startPtr;
				std::size_t size;
				bool isolated;
				std::size_t minAlignment;
				AUList allocUnits;
				bool isRE;

				void CheckGap(std::size_t gap_begin, std::size_t gap_end, std::size_t reqsize,
					std::size_t &smallestgap_pos, std::size_t &smallestgap_size, std::size_t &outAlignBytes)
				{
					std::size_t gapsize = gap_end - gap_begin;
					// How many bytes do we actually need here?
					//   = requested size + alignment bytes
					std::size_t neededSize = reqsize;
					std::size_t alignBytes = minAlignment - ((reinterpret_cast<intptr_t>(startPtr) + gap_begin) % minAlignment);

					alignBytes %= minAlignment;
					neededSize += alignBytes;

					if (gapsize >= neededSize)
					{
						if (gapsize < smallestgap_size)
						{
							smallestgap_size = gapsize;
							smallestgap_pos = gap_begin;
							outAlignBytes = alignBytes;
						}
					}
				}

				bool TryAlloc(std::size_t reqsize, void * &outAddr)
				{
					// Check for isolated
					if (isolated && !allocUnits.empty())
						return false;

					// Find the smallest gap where req fits
					std::size_t lastend = 0;
					std::size_t smallestgap_pos = size + 1;
					std::size_t smallestgap_size = size + 1;
					std::size_t alignmentbytes = 0;

					for (AUList::iterator iter = allocUnits.begin(); iter != allocUnits.end(); ++iter)
					{
						CheckGap(lastend, iter->begin_offset, reqsize, smallestgap_pos, smallestgap_size, alignmentbytes);
						lastend = iter->begin_offset + iter->size;
					}

					CheckGap(lastend, size, reqsize, smallestgap_pos, smallestgap_size, alignmentbytes);

					if (smallestgap_pos < size)
					{
						outAddr = reinterpret_cast<void*>(reinterpret_cast<char*>(startPtr) + smallestgap_pos + alignmentbytes);
						allocUnits.push_sorted( AllocationUnit(smallestgap_pos, reqsize + alignmentbytes) );
						return true;
					}
					else
					{
						return false;
					}
				}

				bool TryFree(void *addr)
				{
					if (addr < startPtr || addr >= reinterpret_cast<void*>(reinterpret_cast<char*>(startPtr) + size))
						return false;

					intptr_t start = reinterpret_cast<intptr_t>(startPtr);

					for (AUList::iterator iter = allocUnits.begin(); iter != allocUnits.end(); ++iter)
					{
						size_t AUBegin = start + iter->begin_offset;
						void *alignedAUBegin = reinterpret_cast<void*>(
							AUBegin + ((minAlignment - AUBegin % minAlignment) % minAlignment)
							);

						if (addr == alignedAUBegin)
						{
							DebugCleanMemory(reinterpret_cast<unsigned char*>(startPtr) + iter->begin_offset,
								iter->size);
							allocUnits.erase(iter);
							return true;
						}
					}

					return false;
				}

				void DebugCleanMemory(unsigned char* start, size_t size)
				{
					bool wasRE = isRE;
					if (isRE)
					{
						SetRW();
					}

					unsigned char* end = start + size;
					for (unsigned char* p = start; p != end; ++p)
					{
						*p = 0xCC;
					}

					if (wasRE)
					{
						SetRE();
					}
				}

				bool Contains(void *addr)
				{
					return addr >= startPtr && addr < reinterpret_cast<void*>(reinterpret_cast<char*>(startPtr) + size);
				}

				void FreeRegion()
				{
#ifdef _WIN32
					VirtualFree(startPtr, 0, MEM_RELEASE);
#else
					munmap(startPtr, size);
#endif
				}

				void SetRE()
				{
					Memory::SetAccess(startPtr, size, Memory::Flags::READ | Memory::Flags::EXECUTE);
					isRE = true;
				}

				void SetRW()
				{
					Memory::SetAccess(startPtr, size, Memory::Flags::READ | Memory::Flags::WRITE);
					isRE = false;
				}
			};

			typedef List<AllocatedRegion> ARList;

			std::size_t m_MinAlignment;
			std::size_t m_PageSize;
			ARList m_Regions;

			bool AddRegion(std::size_t minSize, bool isolated)
			{
				AllocatedRegion newRegion;
				newRegion.startPtr = 0;
				newRegion.isolated = isolated;
				newRegion.minAlignment = m_MinAlignment;

				// Compute real size -> align up to m_PageSize boundary

				newRegion.size = minSize - (minSize % m_PageSize);
				if (newRegion.size < minSize)
					newRegion.size += m_PageSize;

#ifdef _WIN32
				newRegion.startPtr = VirtualAlloc(nullptr, newRegion.size, MEM_COMMIT, PAGE_READWRITE);
#else
				newRegion.startPtr = mmap(0, newRegion.size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
#endif

				if (newRegion.startPtr)
				{
					newRegion.SetRW();
					m_Regions.push_back(newRegion);
					return true;
				}
				else
				{
					return false;
				}

			}

			void *AllocPriv(std::size_t size, bool isolated)
			{
				void *addr;

				if (!isolated)
				{
					for (ARList::iterator iter = m_Regions.begin(); iter != m_Regions.end(); ++iter)
					{
						if (iter->TryAlloc(size, addr))
							return addr;
					}
				}

				if (!AddRegion(size, isolated))
					return NULL;

				bool tmp = m_Regions.back().TryAlloc(size, addr);
				assertm(tmp, "TryAlloc fails after AddRegion");
				return tmp ? addr : NULL;
			}

		public:
			CPageAlloc(size_t minAlignment = 4 /* power of 2 */ ) : m_MinAlignment(minAlignment)
			{
#ifdef _WIN32
				SYSTEM_INFO sysInfo;
				GetSystemInfo(&sysInfo);
				m_PageSize = sysInfo.dwPageSize;
#else
				m_PageSize = sysconf(_SC_PAGESIZE);
#endif
			}

			~CPageAlloc()
			{
				// Free all regions
				for (ARList::iterator iter = m_Regions.begin(); iter != m_Regions.end(); ++iter)
				{
					iter->FreeRegion();
				}
			}

			void *Alloc(size_t size)
			{
				return AllocPriv(size, false);
			}

			void *AllocIsolated(size_t size)
			{
				return AllocPriv(size, true);
			}

			void Free(void *ptr)
			{
				for (ARList::iterator iter = m_Regions.begin(); iter != m_Regions.end(); ++iter)
				{
					if (iter->TryFree(ptr))
					{
						if (iter->allocUnits.empty())
						{
							iter->FreeRegion();
							m_Regions.erase(iter);
						}
						break;
					}
				}
			}

			void SetRE(void *ptr)
			{
				for (ARList::iterator iter = m_Regions.begin(); iter != m_Regions.end(); ++iter)
				{
					if (iter->Contains(ptr))
					{
						iter->SetRE();
						break;
					}
				}
			}

			void SetRW(void *ptr)
			{
				for (ARList::iterator iter = m_Regions.begin(); iter != m_Regions.end(); ++iter)
				{
					if (iter->Contains(ptr))
					{
						iter->SetRW();
						break;
					}
				}
			}

			std::size_t GetPageSize()
			{
				return m_PageSize;
			}
		};
		static CPageAlloc Allocator;

		class GenBuffer
		{
			unsigned char* m_pData;
			std::uint32_t m_Size;
			std::uint32_t m_AllocatedSize;

		public:
			GenBuffer() : m_pData(nullptr), m_Size(0), m_AllocatedSize(0) {}
			~GenBuffer() { clear(); }
			std::uint32_t GetSize() { return m_Size; }
			unsigned char *GetData() { return m_pData; }

			template <class PT> void push(PT what) {
				push((const unsigned char *)&what, sizeof(PT));
			}

			template <class PT> void rewrite(std::uint32_t offset, PT what) {
				rewrite(offset, (const unsigned char *)&what, sizeof(PT));
			}

			void rewrite(std::uint32_t offset, const unsigned char *data, std::uint32_t size) {
				assertm(offset + size <= m_AllocatedSize, "rewrite too far");
				std::memcpy((void*)(m_pData + offset), (const void*)data, size);
			}

			void clear() {
				if (m_pData) {
					Allocator.Free(reinterpret_cast<void*>(m_pData));
				}
				m_pData = nullptr;
				m_Size = 0;
				m_AllocatedSize = 0;
			}

			void SetRE() {
				Allocator.SetRE(reinterpret_cast<void*>(m_pData));
			}

			operator void *() {
				return reinterpret_cast<void*>(GetData());
			}

			void write_ubyte(std::uint8_t x)		{ push(x); }
			void write_byte(std::int8_t x)			{ push(x); }
			
			void write_ushort(unsigned short x)		{ push(x); }
			void write_short(signed short x)		{ push(x); }

			void write_uint32(std::uint32_t x)		{ push(x); }
			void write_int32(std::int32_t x)		{ push(x); }

			void write_uint64(std::uint64_t x)		{ push(x); }
			void write_int64(std::int64_t x)		{ push(x); }

			std::uint32_t get_outputpos() {
				return m_Size;
			}

			void start_count(std::uint32_t &offs) {
				offs = get_outputpos();
			}
			void end_count(std::uint32_t &offs) {
				offs = get_outputpos() - offs;
			}
private:
			void push(const unsigned char *data, std::uint32_t size) {
				std::uint32_t newSize = m_Size + size;
				if (newSize > m_AllocatedSize) {
					m_AllocatedSize = newSize > m_AllocatedSize*2 ? newSize : m_AllocatedSize*2;
					if (m_AllocatedSize < 64)
						m_AllocatedSize = 64;

					unsigned char *newBuf;
					newBuf = reinterpret_cast<unsigned char*>(Allocator.Alloc(m_AllocatedSize));
					Allocator.SetRW(newBuf);
					if (!newBuf) {
						assertm(false, "bad_alloc: couldn't allocate new bytes of memory\n");
						return;
					}
					std::memset((void*)newBuf, 0xCC, m_AllocatedSize);			// :TODO: remove this !
					std::memcpy((void*)newBuf, (const void*)m_pData, m_Size);
					if (m_pData) {
						Allocator.SetRE(reinterpret_cast<void*>(m_pData));
						Allocator.SetRW(newBuf);
						Allocator.Free(reinterpret_cast<void*>(m_pData));
					}
					m_pData = newBuf;
				}
				std::memcpy((void*)(m_pData + m_Size), (const void*)data, size);
				m_Size = newSize;
			}
		};
	}
}