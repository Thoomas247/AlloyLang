#include "ECSResolver.hpp"

#include <unordered_set>

namespace AlloyCompiler
{
	
	SystemInputNames ECSResolver::getSystemInputNames(NAMED_SYSTEM_DEFINITION* pSystem)
	{

	}

	SystemGroup ECSResolver::getSystemsInStage(const std::vector<std::string_view>& groupNames)
	{
		SystemGroup systems;

		for (const std::string_view& groupName : groupNames)
		{
			auto groupIt = m_NamedNodes.GroupDefinitions.find(groupName);
			if (groupIt == m_NamedNodes.GroupDefinitions.end())
			{
				ASSERT(false, "Group is not defined!");	// TODO: proper error handling
			}

			NAMED_GROUP_DEFINITION* pGroup = groupIt->second;
			for (const std::string_view& systemName : pGroup->SystemNames)
			{
				auto systemIt = m_NamedNodes.SystemDefinitions.find(systemName);
				if (systemIt == m_NamedNodes.SystemDefinitions.end())
				{
					ASSERT(false, "System is not defined!"); // TODO: proper error handling
				}

				systems.push_back(systemIt->second);
			}
		}

		return systems;
	}

	std::vector<SystemGroup> ECSResolver::createSystemGroups(const SystemGroup& systems)
	{
		std::vector<SystemGroup> systemGroups;

		SystemGroup* pCurrentGroup = &systemGroups.emplace_back();
		SystemInputNames currentGroupInputs;

		for (size_t systemIndex = 0; systemIndex < systems.size(); systemIndex++)
		{
			NAMED_SYSTEM_DEFINITION* pSystem = systems[systemIndex];

			SystemInputNames currentSystemInputs = getSystemInputNames(pSystem);


		}

		return systemGroups;
	}

	SystemSchedulingInfo ECSResolver::ResolveScheduling()
	{
		APPLICATION_DEFINITION* pApplication = m_NamedNodes.ApplicationDefinitions.begin()->second;

		// collect the systems in each stage, in the order they are grouped in
		std::vector<std::string_view> startStageSystemNames = getSystemNamesInStage(pApplication->StartGroupNames);
		std::vector<std::string_view> updateStageSystemNames = getSystemNamesInStage(pApplication->UpdateGroupNames);
		std::vector<std::string_view> endStageSystemNames = getSystemNamesInStage(pApplication->EndGroupNames);


		return SystemSchedulingInfo();
	}
}