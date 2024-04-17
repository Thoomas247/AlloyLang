#include "Node.hpp"

namespace AlloyCompiler
{
	bool ECSElements::AddSystemNodeID(const std::string_view& name, NodeID nodeID)
	{
		if (m_SystemNodeIDs.contains(name))
		{
			return false;
		}

		m_SystemNodeIDs[name] = nodeID;
		return true;
	}

	bool ECSElements::AddGroupNodeID(const std::string_view& name, NodeID nodeID)
	{
		if (m_GroupNodeIDs.contains(name))
		{
			return false;
		}

		m_GroupNodeIDs[name] = nodeID;
		return true;
	}

	bool ECSElements::AddQueryNodeID(const std::string_view& name, NodeID nodeID)
	{
		if (m_QueryNodeIDs.contains(name))
		{
			return false;
		}

		m_QueryNodeIDs[name] = nodeID;
		return true;
	}

	bool ECSElements::AddStructNodeID(const std::string_view& name, NodeID nodeID)
	{
		if (m_StructNodeIDs.contains(name))
		{
			return false;
		}

		m_StructNodeIDs[name] = nodeID;
		return true;
	}

	const std::unordered_map<std::string_view, NodeID>& ECSElements::GetSystemNodeIDs() const
	{
		return m_SystemNodeIDs;
	}

	const std::unordered_map<std::string_view, NodeID>& ECSElements::GetGroupNodeIDs() const
	{
		return m_GroupNodeIDs;
	}

	const std::unordered_map<std::string_view, NodeID>& ECSElements::GetQueryNodeIDs() const
	{
		return m_QueryNodeIDs;
	}

	const std::unordered_map<std::string_view, NodeID>& ECSElements::GetStructNodeIDs() const
	{
		return m_StructNodeIDs;
	}

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

	const Node& NodeBuffers::GetNode(NodeID id) const
	{
		ASSERT((size_t)id < m_Nodes.size(), "Invalid ID!");
		return m_Nodes[(size_t)id];
	}

	TokenID NodeBuffers::GetErrorInfo(NodeID id) const
	{
		return m_ErrorInfos[(size_t)id];
	}

	void NodeBuffers::AddApplicationNodeID(NodeID id)
	{
		m_ApplicationNodeIDs.push_back(id);
	}

	const std::vector<NodeID>& NodeBuffers::GetApplicationNodeIDs() const
	{
		return m_ApplicationNodeIDs;
	}

	ECSElements& NodeBuffers::GetECSElements()
	{
		return m_ECSElements;
	}

	const ECSElements& NodeBuffers::GetECSElements() const
	{
		return m_ECSElements;
	}

}