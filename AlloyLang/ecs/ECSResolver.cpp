#include "ECSResolver.hpp"

#include <unordered_set>

namespace AlloyCompiler
{
	
	SystemInputNames ECSResolver::getSystemInputNames(NAMED_SYSTEM_DEFINITION* pSystem)
	{
		SystemInputNames inputNames;

		for (const std::string_view& resourceRead : pSystem->ResourceReads)
		{
			auto resourceIt = m_NamedNodes.ResourceDefinitions.find(resourceRead);
			if (resourceIt != m_NamedNodes.ResourceDefinitions.end())
			{
				ASSERT(false, "Resource is not defined!");
			}

			inputNames.ResourceReads.insert(resourceRead);
		}

		for (const std::string_view& resourceWrite : pSystem->ResourceWrites)
		{
			auto resourceIt = m_NamedNodes.ResourceDefinitions.find(resourceWrite);
			if (resourceIt != m_NamedNodes.ResourceDefinitions.end())
			{
				ASSERT(false, "Resource is not defined!");
			}

			inputNames.ResourceWrites.insert(resourceWrite);
		}

		for (const std::string_view& queryName : pSystem->QueryNames)
		{
			auto queryIt = m_NamedNodes.QueryDefinitions.find(queryName);
			if (queryIt != m_NamedNodes.QueryDefinitions.end())
			{
				ASSERT(false, "Query is not defined!");
			}

			NAMED_QUERY_DEFINITION* pQuery = queryIt->second;

			for (const std::string_view& componentReadName : pQuery->ComponentReadNames)
			{
				if (!m_NamedNodes.ComponentDefinitions.contains(componentReadName))
				{
					ASSERT(false, "Component is not defined!");
				}

				inputNames.ComponentReads.emplace(componentReadName);
			}

			for (const std::string_view& componentWriteName : pQuery->ComponentWriteNames)
			{
				if (!m_NamedNodes.ComponentDefinitions.contains(componentWriteName))
				{
					ASSERT(false, "Component is not defined!");
				}

				inputNames.ComponentWrites.emplace(componentWriteName);
			}
		}

		return inputNames;
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

	bool ECSResolver::resourceInputsAreCompatible(const SystemInputNames& currentGroupInputs, const SystemInputNames& currentSystemInputs)
	{
		for (const std::string_view& resourceWrite : currentSystemInputs.ResourceWrites)
		{
			if (currentGroupInputs.ResourceReads.contains(resourceWrite) || currentGroupInputs.ResourceWrites.contains(resourceWrite))
			{
				return false;
			}
		}

		for (const std::string_view& resourceRead : currentSystemInputs.ResourceReads)
		{
			if (currentGroupInputs.ResourceWrites.contains(resourceRead))
			{
				return false;
			}
		}

		return true;
	}

	bool ECSResolver::componentInputsAreCompatible(const SystemInputNames& currentGroupInputs, const SystemInputNames& currentSystemInputs)
	{

	}

	std::vector<SystemGroup> ECSResolver::createSystemGroups(const SystemGroup& systems)
	{
		std::vector<SystemGroup> systemGroups;

		SystemGroup* pCurrentGroup = &systemGroups.emplace_back();
		SystemInputNames currentGroupInputs;

		for (size_t systemIndex = 0; systemIndex < systems.size();)
		{
			NAMED_SYSTEM_DEFINITION* pSystem = systems[systemIndex];
			SystemInputNames currentSystemInputs = getSystemInputNames(pSystem);

			bool fitsInGroup = true;

			if (!resourceInputsAreCompatible(currentGroupInputs, currentSystemInputs))
			{
				fitsInGroup = false;
			}

			else if (!componentInputsAreCompatible(currentGroupInputs, currentSystemInputs))
			{
				fitsInGroup = false;
			}

			if (fitsInGroup)
			{
				// go to next system and stay in same group
				pCurrentGroup->push_back(pSystem);
				currentGroupInputs.AddAll(currentSystemInputs);
				systemIndex++;	
			}
			else
			{
				// go to next group and stay on same system
				pCurrentGroup = &systemGroups.emplace_back();
				currentGroupInputs.Clear();
			}
		}

		return systemGroups;
	}

	SystemSchedulingInfo ECSResolver::ResolveScheduling()
	{
		APPLICATION_DEFINITION* pApplication = m_NamedNodes.ApplicationDefinitions.begin()->second;

		// collect the systems in each stage, in the order they are grouped in
		SystemGroup startStageSystems = getSystemsInStage(pApplication->StartGroupNames);
		SystemGroup updateStageSystems = getSystemsInStage(pApplication->UpdateGroupNames);
		SystemGroup endStageSystems = getSystemsInStage(pApplication->EndGroupNames);

		SystemSchedulingInfo shedulingInfo;

		shedulingInfo.StartSystemGroups = createSystemGroups(startStageSystems);
		shedulingInfo.UpdateSystemGroups = createSystemGroups(updateStageSystems);
		shedulingInfo.EndSystemGroups = createSystemGroups(endStageSystems);

		return shedulingInfo;
	}
}