#pragma once
#include <memory>
#include <vector>

#include "tokenizer/Tokenizer.hpp"

#include "ASTNodes.hpp"

namespace AlloyCompiler
{
	class NodeDataBuffers
	{
	public:
		NodeDataBuffers(const Tokenizer::TokenDataBuffers& nodeBuffers)
		{
			// assume one node per token
			m_Nodes.reserve(nodeBuffers.GetTokenCount());

			// assume 1/10th of node types have lists of children
			m_NodeIDLists.reserve(nodeBuffers.GetTokenCount() / 10);
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

		void SetRootNodeID(NodeID id) { m_RootNodeID = id; }
		NodeID GetRootNodeID() const { return m_RootNodeID; }

	private:
		std::vector<Node> m_Nodes;
		std::vector<std::unique_ptr<std::vector<NodeID>>> m_NodeIDLists;

		NodeID m_RootNodeID = ERROR_NODE_ID;
	};

	NodeDataBuffers Parse(const TokenBuffers& tokenBuffers);
}