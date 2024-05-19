#include "ECSResolver.hpp"

#include <unordered_set>

namespace AlloyCompiler
{

	SystemInputNames ECSResolver::getSystemInputNames(NAMED_SYSTEM_DEFINITION* pSystem)
	{
		SystemInputNames inputNames;

		for (Token* pResourceRead : pSystem->ResourceReads)
		{
			auto resourceIt = m_NamedNodes.ResourceDefinitions.find(pResourceRead->Value);
			if (resourceIt == m_NamedNodes.ResourceDefinitions.end())
			{
				logErrorAtToken(pResourceRead, "Resource '{0}' is not defined!", pResourceRead->Value);
			}

			else
			{
				inputNames.ResourceReads.insert(pResourceRead->Value);
			}
		}

		for (Token* pResourceWrite : pSystem->ResourceWrites)
		{
			auto resourceIt = m_NamedNodes.ResourceDefinitions.find(pResourceWrite->Value);
			if (resourceIt == m_NamedNodes.ResourceDefinitions.end())
			{
				logErrorAtToken(pResourceWrite, "Resource '{0}' is not defined!", pResourceWrite->Value);
			}

			else
			{
				inputNames.ResourceWrites.insert(pResourceWrite->Value);
			}
		}

		for (Token* pQueryName : pSystem->QueryNames)
		{
			auto queryIt = m_NamedNodes.QueryDefinitions.find(pQueryName->Value);
			if (queryIt == m_NamedNodes.QueryDefinitions.end())
			{
				logErrorAtToken(pQueryName, "Query '{0}' is not defined!", pQueryName->Value);
			}

			else
			{

				NAMED_QUERY_DEFINITION* pQuery = queryIt->second;

				for (Token* pComponentReadName : pQuery->ComponentReadNames)
				{
					if (!m_NamedNodes.ComponentDefinitions.contains(pComponentReadName->Value))
					{
						logErrorAtToken(pComponentReadName, "Component '{0}' is not defined!", pComponentReadName->Value);
					}

					else
					{
						inputNames.ComponentReads.emplace(pComponentReadName->Value);
					}
				}

				for (Token* pComponentWriteName : pQuery->ComponentWriteNames)
				{
					if (!m_NamedNodes.ComponentDefinitions.contains(pComponentWriteName->Value))
					{
						logErrorAtToken(pComponentWriteName, "Component '{0}' is not defined!", pComponentWriteName->Value);
					}

					else
					{
						inputNames.ComponentWrites.emplace(pComponentWriteName->Value);
					}
				}
			}
		}

		return inputNames;
	}

	SystemGroup ECSResolver::getSystemsInStage(const std::vector<Token*>& groupNames)
	{
		SystemGroup systems;

		for (Token* pGroupName : groupNames)
		{
			auto groupIt = m_NamedNodes.GroupDefinitions.find(pGroupName->Value);
			if (groupIt == m_NamedNodes.GroupDefinitions.end())
			{
				logErrorAtToken(pGroupName, "Group '{0}' is not defined!", pGroupName->Value);
			}

			else
			{
				NAMED_GROUP_DEFINITION* pGroup = groupIt->second;
				for (Token* pSystemName : pGroup->SystemNames)
				{
					auto systemIt = m_NamedNodes.SystemDefinitions.find(pSystemName->Value);
					if (systemIt == m_NamedNodes.SystemDefinitions.end())
					{
						logErrorAtToken(pSystemName, "System '{0}' is not defined!", pSystemName->Value);
					}

					else
					{
						systems.push_back(systemIt->second);
					}
				}
			}
		}

		return systems;
	}

	bool ECSResolver::resourceInputsAreCompatible(const SystemInputNames& currentGroupInputs, const SystemInputNames& currentSystemInputs)
	{
		for (const std::string_view& resourceWrite : currentSystemInputs.ResourceWrites)
		{
			if (currentGroupInputs.ResourceWrites.contains(resourceWrite) || currentGroupInputs.ResourceReads.contains(resourceWrite))
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
		for (const std::string_view& componentWrite : currentSystemInputs.ComponentWrites)
		{
			if (currentGroupInputs.ComponentWrites.contains(componentWrite) || currentGroupInputs.ComponentReads.contains(componentWrite))
			{
				return false;
			}
		}

		for (const std::string_view& componentRead : currentSystemInputs.ComponentReads)
		{
			if (currentGroupInputs.ComponentWrites.contains(componentRead))
			{
				return false;
			}
		}

		return true;
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