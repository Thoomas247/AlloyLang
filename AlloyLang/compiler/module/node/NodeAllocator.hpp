#pragma once
#include "../../log/Log.hpp"

namespace AlloyCompiler
{
	class NodeAllocator
	{
	public:
		NodeAllocator()
			:m_pMemBlock(nullptr), m_pCurrent(nullptr), m_pEnd(nullptr)
		{}

		NodeAllocator(size_t size)
			: m_pMemBlock(new char[size]), m_pCurrent(m_pMemBlock), m_pEnd(m_pMemBlock + size)
		{}

		NodeAllocator(const NodeAllocator&) = delete;

		NodeAllocator(NodeAllocator&& other) noexcept
			: m_pMemBlock(other.m_pMemBlock), m_pCurrent(other.m_pCurrent), m_pEnd(other.m_pEnd)
		{
			other.m_pMemBlock = nullptr;
		}

		~NodeAllocator()
		{
			Delete();
		}

		void Reset(size_t newSize)
		{
			Delete();

			m_pMemBlock = new char[newSize];
			m_pCurrent = m_pMemBlock;
			m_pEnd = m_pMemBlock + newSize;
		}

		void Delete()
		{
			if (m_pMemBlock)
			{
				delete[] m_pMemBlock;
			}

			m_pMemBlock = nullptr;
		}

		template<typename N>
		N* Create(const N& value)
		{
			ASSERT(m_pMemBlock != nullptr, "Allocator has not been initialized!");

			// allign the current pointer as needed by N
			const uintptr_t offset = (uintptr_t)(m_pCurrent) & (alignof(N) - 1);
			if (offset != 0)
			{
				m_pCurrent += alignof(N) - offset;
			}

			ASSERT((m_pCurrent + sizeof(N)) < m_pEnd, "Allocator is full!");

			// this is the location we want the node to go into
			N* pNode = (N*)m_pCurrent;

			// set the location for the next node
			m_pCurrent += sizeof(N);

			// move the value into the allocated memory
			// the class constructor is called on the allocated object using the placement constructor syntax
			new (pNode) N(value);

			// cannot simply use operator = (the constructor has to be caled on the object)
			// *pNode = value;
			// std::memcpy(pNode, &value, sizeof(N));
			// std::memset((void*)&value, 0, sizeof(N));	// we're modifying an object that is passed in as const, this can't be good

			return pNode;
		}

	private:
		char* m_pMemBlock;
		char* m_pCurrent;
		char* m_pEnd;
	};
}