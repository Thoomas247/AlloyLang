#pragma once
#include "../log/Log.hpp"
#include "Node.hpp"


namespace AlloyCompiler
{
	template <typename T>
	concept NodeType = std::is_base_of_v<Node, T>;

	class NodeAllocator
	{
	public:
		NodeAllocator(size_t size)
			: m_pMemBlock(new char[size]), m_pCurrentIndex(m_pMemBlock), m_pEnd(m_pMemBlock + size)
		{

		}

		~NodeAllocator()
		{
			if (m_pMemBlock)
			{
				delete[] m_pMemBlock;
			}

			m_pMemBlock = nullptr;
		}

		template<NodeType N, typename... Args>
		N* Allocate(Args&&... args)
		{
			ASSERT((m_pCurrentIndex + sizeof(N)) < m_pEnd, "Allocator is full!");

			// this is the location we want the node to go into
			N* pNode = static_cast<N*>(m_pCurrentIndex);
			
			// set the location for the next node
			m_pCurrentIndex += sizeof(N);

			// create the node in-place
			*pNode = N(args);

			return pNode;
		}

	private:
		char* m_pMemBlock;
		char* m_pCurrentIndex;
		char* m_pEnd;
	};
}