#pragma once
#include "../parser/Node.hpp"

namespace AlloyCompiler
{
	using SystemGroup = std::vector<std::string_view>;

	struct SystemSchedulingInfo
	{
		std::vector<SystemGroup> StartSystemGroups;
		std::vector<SystemGroup> UpdateSystemGroups;
		std::vector<SystemGroup> EndSystemGroups;
	};

	SystemSchedulingInfo ResolveApplication(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID applicationNodeID);
}