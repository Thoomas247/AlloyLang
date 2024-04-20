#include "ECSResolver.hpp"

#include <unordered_set>

namespace AlloyCompiler
{
	struct SystemReadWrites
	{
		// "struct" refers to resource or component
		std::unordered_set<std::string_view> StructWrites;
		std::unordered_set<std::string_view> StructReads;
	};

	using SystemReadWritesMap = std::unordered_map<std::string_view, SystemReadWrites>;
	using ComponentExclusionMap = std::unordered_map<std::string_view, std::unordered_set<std::string_view>>;

	/// <summary>
	/// Returns the names of all systems in a stage.
	/// If the stage is not used, an empty vector is returned.
	/// </summary>
	std::vector<std::string_view> getSystemNamesInStage(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID groupListID)
	{
		// handle stage not being used
		if (groupListID == ERROR_NODE_ID)
		{
			return {};
		}

		ASSERT(nodeBuffers.GetNode(groupListID).Kind == NodeKind::IDENTIFIER_LIST, "Invalid groupListID!");

		std::vector<std::string_view> systemNames;

		// loop through every group in the group list (a list of groups is a stage in the application)
		for (NodeID groupIdentifierID : nodeBuffers.GetNode(groupListID).IdentifierList.IdentifierIDs)
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

	/// <summary>
	/// Adds a query's reads and writes to the system's reads and writes sets.
	/// </summary>
	void getQueryReadWrites(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID queryID, SystemReadWrites& systemReadWrites)
	{
		const auto getStructs = [](const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, const QUERY_DEFINITION& queryNode,
			const VectorRef<NodeID> identifierIDs, std::unordered_set<std::string_view>& destinationSet)
			{
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

					else if (nodeBuffers.GetNode(structIDIter->second).StructDefinition.Kind == STRUCT_DEFINITION::Type::Struct)
					{
						ASSERT(false, "Query can only contain resources or components!"); // TODO: proper error handling
					}

					else
					{
						destinationSet.insert(structName);
					}

				}
			};

		const QUERY_DEFINITION& queryNode = nodeBuffers.GetNode(queryID).QueryDefinition;

		getStructs(tokenBuffers, nodeBuffers, queryNode, queryNode.WriteIdentifierIDs, systemReadWrites.StructWrites);
		getStructs(tokenBuffers, nodeBuffers, queryNode, queryNode.ReadIdentifierIDs, systemReadWrites.StructReads);
	}

	/// <summary>
	/// Creates a map of system names to the reads and writes of each system.
	/// If a system does not read or write any resources or components, it will still be added to the map with empty sets.
	/// </summary>
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
					getQueryReadWrites(tokenBuffers, nodeBuffers, queryIDIter->second, systemReadWritesMap[systemName]);
				}
			}
		}

		return systemReadWritesMap;
	}

	/// <summary>
	/// Creates a map of component names to the components that are mutually exclusive with the component.
	/// If a component does not exclude any other components, it will not be added to the map.
	/// </summary>
	ComponentExclusionMap getComponentExclusions(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers)
	{
		ComponentExclusionMap componentExclusionMap;

		for (auto& [structName, structID] : nodeBuffers.GetNamedNodes().StructNodeIDs)
		{
			const STRUCT_DEFINITION& structNode = nodeBuffers.GetNode(structID).StructDefinition;

			if (structNode.Kind == STRUCT_DEFINITION::Type::Component)
			{
				const IDENTIFIER_LIST& excludeIdentifierList = nodeBuffers.GetNode(structNode.ExcludeIdentifierListID).IdentifierList;

				// collect excludes
				for (NodeID excludeIdentifierID : excludeIdentifierList.IdentifierIDs)
				{
					const IDENTIFIER& excludeIdentifierNode = nodeBuffers.GetNode(excludeIdentifierID).Identifier;
					std::string_view excludeName = tokenBuffers.GetValue(excludeIdentifierNode.IdentifierTokenID).ToStringView();

					auto excludeIDIter = nodeBuffers.GetNamedNodes().StructNodeIDs.find(excludeName);
					if (excludeIDIter == nodeBuffers.GetNamedNodes().StructNodeIDs.end())
					{
						ASSERT(false, "Component does not exist!"); // TODO: proper error handling
					}

					else
					{
						componentExclusionMap[structName].insert(excludeName);
					}
				}
			}
		}

		return componentExclusionMap;
	}

	/// <summary>
	/// Creates groups of systems that can be executed in parallel.
	/// The relative order of systems that use the same resources or components is preserved.
	/// </summary>
	std::vector<SystemGroup> createSystemGroups(const SystemReadWritesMap& systemReadWritesMap, const ComponentExclusionMap& componentExclusionMap, 
		const std::vector<std::string_view>& systemNames)
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

			// try to add this system's writes to the current group
			for (auto& writeName : systemReadWrites.StructWrites)
			{
				// if this component is already in the list, we might still be able to add it depending on the exclusions
				if (structReadsInGroup.contains(writeName) || structWritesInGroup.contains(writeName))
				{
					// check if this component has any excludes
					auto it = componentExclusionMap.find(writeName);
					if (it != componentExclusionMap.end())
					{
						// if this component has excludes, check how many of them are in the current group
						size_t numExcludesInGroup = 0;

						for (auto& excludeName : it->second)
						{
							if (structReadsInGroup.contains(excludeName) || structWritesInGroup.contains(excludeName))
							{
								numExcludesInGroup++;
							}
						}

						// if there is only one exclude in the group, we can still add this component
						if (numExcludesInGroup == 1)
						{
							structWritesInGroup.insert(writeName);
						}

						// if there are more than one excludes in the group, we need to check that they are mutually exclusive
						else if (numExcludesInGroup > 1)
						{
							
						}

					}

					// if it doesn't have any excludes, the component does not fit in the group since it is a write
					else
					{
						fitsInGroup = false;
						break;
					}
				}


				auto it = componentExclusionMap.find(writeName);

				// if this component is already in the list and has excludes check that this component is mutually exclusive with all the components that are mutually exclusive 
				// to the components that are mutually exclusive to this component
				if ( && it != componentExclusionMap.end())
				{
					for (auto& excludeName : it->second)
					{
						auto excludeIt = componentExclusionMap.find(excludeName);

						if (excludeIt != componentExclusionMap.end())
						{
							for (auto& excludeExcludeName : excludeIt->second)
							{
								if (!componentExclusionMap.at(excludeExcludeName).contains(writeName))
								{
									fitsInGroup = false;
									break;
								}
							}
						}

						if (!fitsInGroup)
						{
							break;
						}
					}
				}

				// if in exclude list, then this system can still fit in the group, since we know the components we already have are mutually exclusive to this component
				if (componentExcludesInGroup.contains(writeName))
				{
					
				}

				// writes collide with either reads or writes
				else if (structReadsInGroup.contains(writeName) || structWritesInGroup.contains(writeName))
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
					if (componentExcludesInGroup.contains(readName))
					{
						fitsInGroup = false;
						break;
					}

					// reads collide only with writes
					else if (structWritesInGroup.contains(readName))
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

		// collect the components that are mutually exclusive
		ComponentExclusionMap componentExclusionMap = getComponentExclusions(tokenBuffers, nodeBuffers);

		// group systems such that they can be executed in parallel
		SystemSchedulingInfo systemSchedulingInfo;
		systemSchedulingInfo.StartSystemGroups = createSystemGroups(systemReadWritesMap, startStageSystemNames);
		systemSchedulingInfo.UpdateSystemGroups = createSystemGroups(systemReadWritesMap, updateStageSystemNames);
		systemSchedulingInfo.EndSystemGroups = createSystemGroups(systemReadWritesMap, endStageSystemNames);

		return systemSchedulingInfo;
	}
}