#include "Node.hpp"

namespace AlloyCompiler
{

	NodeID NodeBuffers::CreateNode(const Node& node, TokenID errorInfo)
	{
		m_Nodes.push_back(node);
		m_ErrorInfos.push_back(errorInfo);
		return (NodeID)(m_Nodes.size() - 1);
	}

	VectorRef<NodeID> NodeBuffers::CreateNodeIDVector()
	{
		return *m_NodeIDVectors.emplace_back(std::make_unique<std::vector<NodeID>>());
	}

	VectorRef<TokenID> NodeBuffers::CreateTokenIDVector()
	{
		return *m_TokenIDVectors.emplace_back(std::make_unique<std::vector<TokenID>>());
	}

	const Node& NodeBuffers::GetNode(NodeID id) const { return m_Nodes[(size_t)id]; }

	TokenID NodeBuffers::GetErrorInfo(NodeID id) const
	{
		return m_ErrorInfos[(size_t)id];
	}

	void NodeBuffers::SetRootNodeID(NodeID id) { m_RootNodeID = id; }

	NodeID NodeBuffers::GetRootNodeID() const { return m_RootNodeID; }

}