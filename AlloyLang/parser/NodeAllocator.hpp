#pragma once
#include "../log/Log.hpp"
#include "Node.hpp"

namespace AlloyCompiler
{
	class NodeAllocator
	{
	public:
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
			if (m_pMemBlock)
			{
				delete[] m_pMemBlock;
			}

			m_pMemBlock = nullptr;
		}

		template<typename N>
		N* Create(const N& value)
		{
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
			// cannot simply use operator= for some reason
			std::memcpy(pNode, &value, sizeof(N));
			std::memset((void*)&value, 0, sizeof(N));

			//*pNode = value;

			return pNode;
		}

	private:
		char* m_pMemBlock;
		char* m_pCurrent;
		char* m_pEnd;
	};
}