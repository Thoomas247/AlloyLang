#include "ECSResolver.hpp"

#include <unordered_set>

namespace AlloyCompiler
{
	struct SystemReadWrites
	{
		std::unordered_set<std::string_view> StructWrites;	// "struct" refers to resource or component
		std::unordered_set<std::string_view> StructReads;	// "struct" refers to resource or component
	};

	using SystemReadWritesMap = std::unordered_map<std::string_view, SystemReadWrites>;

	std::vector<std::string_view> getSystemNamesInStage(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID groupListID)
	{
		// handle stage not being used
		if (groupListID == ERROR_NODE_ID)
		{
			return {};
		}

		ASSERT(nodeBuffers.GetNode(groupListID).Kind == NodeKind::GROUP_LIST, "Invalid groupListID!");

		std::vector<std::string_view> systemNames;

		// loop through every group in the group list (a list of groups is a stage in the application)
		for (NodeID groupIdentifierID : nodeBuffers.GetNode(groupListID).GroupList.GroupIdentifierIDs)
		{
			ASSERT(nodeBuffers.GetNode(groupIdentifierID).Kind == NodeKind::IDENTIFIER, "Invalid groupIdentifierID!");

			const IDENTIFIER& groupIdentifierNode = nodeBuffers.GetNode(groupIdentifierID).Identifier;
			std::string_view groupName = tokenBuffers.GetValue(groupIdentifierNode.IdentifierTokenID).ToStringView();

			auto groupIDIter = nodeBuffers.GetNamedNodes().GroupNodeIDs.find(groupName);
			if (groupIDIter == nodeBuffers.GetNamedNodes().GroupNodeIDs.end())
			{
				ASSERT(false, "Group does not exist!"); // TODO: proper error handling
			}

			else
			{
				NodeID groupID = groupIDIter->second;
				const GROUP_DEFINITION& groupNode = nodeBuffers.GetNode(groupID).GroupDefinition;

				// loop through every system in the group
				for (NodeID systemIdentifierID : groupNode.SystemIdentifierIDs)
				{
					const IDENTIFIER& systemIdentifierNode = nodeBuffers.GetNode(systemIdentifierID).Identifier;
					std::string_view systemName = tokenBuffers.GetValue(systemIdentifierNode.IdentifierTokenID).ToStringView();

					// check that the system exists
					if (!nodeBuffers.GetNamedNodes().SystemNodeIDs.contains(systemName))
					{
						ASSERT(false, "System does not exist!"); // TODO: proper error handling
					}

					else
					{
						systemNames.push_back(systemName);
					}
				}
			}
		}

		return systemNames;
	}

	SystemReadWritesMap getSystemReadWrites(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers)
	{
		SystemReadWritesMap systemReadWritesMap;

		for (auto& [systemName, systemID] : nodeBuffers.GetNamedNodes().SystemNodeIDs)
		{
			const SYSTEM_DEFINITION& systemDefinitionNode = nodeBuffers.GetNode(systemID).SystemDefinition;

			// initialize here so that systems with no inputs still get added to the map
			systemReadWritesMap[systemName] = SystemReadWrites();

			for (NodeID queryIdentifierID : systemDefinitionNode.QueryIdentifierIDs)
			{
				const IDENTIFIER& queryIdentifierNode = nodeBuffers.GetNode(queryIdentifierID).Identifier;
				std::string_view queryName = tokenBuffers.GetValue(queryIdentifierNode.IdentifierTokenID).ToStringView();

				auto queryIDIter = nodeBuffers.GetNamedNodes().QueryNodeIDs.find(queryName);
				if (queryIDIter == nodeBuffers.GetNamedNodes().QueryNodeIDs.end())
				{
					ASSERT(false, "Query does not exist!"); // TODO: proper error handling
				}

				else
				{
					NodeID queryID = queryIDIter->second;
					const QUERY_DEFINITION& queryNode = nodeBuffers.GetNode(queryID).QueryDefinition;

					// collect writes
					for (NodeID structIdentifierID : queryNode.WriteIdentifierIDs)
					{
						// "struct" here can be either component or resource
						const IDENTIFIER& structIdentifierNode = nodeBuffers.GetNode(structIdentifierID).Identifier;
						std::string_view structName = tokenBuffers.GetValue(structIdentifierNode.IdentifierTokenID).ToStringView();

						auto structIDIter = nodeBuffers.GetNamedNodes().StructNodeIDs.find(structName);
						if (structIDIter == nodeBuffers.GetNamedNodes().StructNodeIDs.end())
						{
							ASSERT(false, "Resource or component does not exist!"); // TODO: proper error handling
						}

						else
						{
							systemReadWritesMap[systemName].StructWrites.insert(structName);
						}
					}

					// collect reads
					for (NodeID structIdentifierID : queryNode.ReadIdentifierIDs)
					{
						// "struct" here can be either component or resource
						const IDENTIFIER& structIdentifierNode = nodeBuffers.GetNode(structIdentifierID).Identifier;
						std::string_view structName = tokenBuffers.GetValue(structIdentifierNode.IdentifierTokenID).ToStringView();

						auto structIDIter = nodeBuffers.GetNamedNodes().StructNodeIDs.find(structName);
						if (structIDIter == nodeBuffers.GetNamedNodes().StructNodeIDs.end())
						{
							ASSERT(false, "Resource or component does not exist!"); // TODO: proper error handling
						}

						else if (nodeBuffers.GetNode(structIDIter->second).StructDefinition.Kind == STRUCT_DEFINITION::Type::Struct)
						{
							ASSERT(false, "Query can only contain resources or components!"); // TODO: proper error handling
						}

						else
						{
							systemReadWritesMap[systemName].StructReads.insert(structName);
						}
					}
				}
			}
		}

		return systemReadWritesMap;
	}

	std::vector<SystemGroup> createSystemGroups(const SystemReadWritesMap& systemReadWritesMap, const std::vector<std::string_view>& systemNames)
	{
		std::vector<SystemGroup> systemGroups;

		auto systemNameIter = systemNames.begin();
		std::vector<std::string_view>* pCurrentGroup = &systemGroups.emplace_back();

		std::unordered_set<std::string_view> structReadsInGroup;
		std::unordered_set<std::string_view> structWritesInGroup;

		while (systemNameIter != systemNames.end())
		{
			const std::string_view& systemName = *systemNameIter;
			const SystemReadWrites& systemReadWrites = systemReadWritesMap.at(systemName);

			bool fitsInGroup = true;

			// check for collisions with this system's writes
			for (auto& writeName : systemReadWrites.StructWrites)
			{
				// writes collide with either reads or writes
				if (structReadsInGroup.contains(writeName) || structWritesInGroup.contains(writeName))
				{
					fitsInGroup = false;
					break;
				}

				else
				{
					structWritesInGroup.insert(writeName);
				}
			}

			if (fitsInGroup)
			{
				// check for collisions with this system's reads
				for (auto& readName : systemReadWrites.StructReads)
				{
					// reads collide only with writes
					if (structWritesInGroup.contains(readName))
					{
						fitsInGroup = false;
						break;
					}

					else
					{
						structReadsInGroup.insert(readName);
					}
				}
			}

			if (fitsInGroup)
			{
				// advance to the next system but stay in this group
				pCurrentGroup->push_back(systemName);
				systemNameIter++;
			}

			else
			{
				// advance to the next group but stay on this system
				pCurrentGroup = &systemGroups.emplace_back();

				structReadsInGroup.clear();
				structWritesInGroup.clear();
			}
		}

		return systemGroups;
	}

	SystemSchedulingInfo ResolveApplication(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID applicationNodeID)
	{
		const APPLICATION_DEFINITION& applicationNode = nodeBuffers.GetNode(applicationNodeID).ApplicationDefinition;

		// collect the systems in each stage, in the order they are grouped in
		std::vector<std::string_view> startStageSystemNames = getSystemNamesInStage(tokenBuffers, nodeBuffers, applicationNode.StartGroupListID);
		std::vector<std::string_view> updateStageSystemNames = getSystemNamesInStage(tokenBuffers, nodeBuffers, applicationNode.UpdateGroupListID);
		std::vector<std::string_view> endStageSystemNames = getSystemNamesInStage(tokenBuffers, nodeBuffers, applicationNode.EndGroupListID);

		// collect the reads and writes of each system
		SystemReadWritesMap systemReadWritesMap = getSystemReadWrites(tokenBuffers, nodeBuffers);

		// group systems such that they can be executed in parallel
		SystemSchedulingInfo systemSchedulingInfo;
		systemSchedulingInfo.StartSystemGroups = createSystemGroups(systemReadWritesMap, startStageSystemNames);
		systemSchedulingInfo.UpdateSystemGroups = createSystemGroups(systemReadWritesMap, updateStageSystemNames);
		systemSchedulingInfo.EndSystemGroups = createSystemGroups(systemReadWritesMap, endStageSystemNames);

		return systemSchedulingInfo;
	}
}