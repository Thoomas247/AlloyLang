#pragma once
#include <memory>
#include <vector>

#include "tokenizer/Tokenizer.hpp"

#include "ASTNodeCompact.hpp"

namespace AlloyCompiler::ParserCompact
{
	class NodeDataBuffers
	{
	public:
		NodeDataBuffers(const Tokenizer::TokenDataBuffers& buffers)
		{
			// assume one node per token
			m_Nodes.reserve(buffers.GetTokenCount());

			// assume 1/10th of node types have lists of children
			m_NodeIDLists.reserve(buffers.GetTokenCount() / 10);
		}

		NodeID CreateNode(const Node& node)
		{
			m_Nodes.push_back(node);
			return (NodeID)(m_Nodes.size() - 1);
		}

		std::vector<NodeID>& CreateNodeIDList()
		{
			return *m_NodeIDLists.emplace_back(std::make_unique<std::vector<NodeID>>()).get();
		}

		const Node& GetNode(NodeID id) const { return m_Nodes[id]; }

	private:
		std::vector<Node> m_Nodes;
		std::vector<std::unique_ptr<std::vector<NodeID>>> m_NodeIDLists;
	};

	NodeDataBuffers Parse(const Tokenizer::TokenDataBuffers& tokenBuffers);
}