#pragma once
#include "../parser/Parser.hpp"

namespace AlloyCompiler
{
	using SystemGroup = std::vector<NAMED_SYSTEM_DEFINITION*>;

	struct SystemSchedulingInfo
	{
		std::vector<SystemGroup> StartSystemGroups;
		std::vector<SystemGroup> UpdateSystemGroups;
		std::vector<SystemGroup> EndSystemGroups;

		void Print()
		{
			Log::Print("\n-- Start Groups --");

			for (int i = 0; i < StartSystemGroups.size(); i++)
			{
				Log::Print("\tGroup {0}:", i);

				for (NAMED_SYSTEM_DEFINITION* pSystem : StartSystemGroups[i])
				{
					Log::Print("\t\t{0}", pSystem->pNameToken->Value);
				}
			}

			Log::Print("\n-- Update Groups --");

			for (int i = 0; i < UpdateSystemGroups.size(); i++)
			{
				Log::Print("\tGroup {0}:", i);

				for (NAMED_SYSTEM_DEFINITION* pSystem : UpdateSystemGroups[i])
				{
					Log::Print("\t\t{0}", pSystem->pNameToken->Value);
				}
			}

			Log::Print("\n-- End Groups --");

			for (int i = 0; i < EndSystemGroups.size(); i++)
			{
				Log::Print("\tGroup {0}:", i);

				for (NAMED_SYSTEM_DEFINITION* pSystem : EndSystemGroups[i])
				{
					Log::Print("\t\t{0}", pSystem->pNameToken->Value);
				}
			}

			Log::Print("");
		}
	};

	struct SystemInputNames
	{
		std::unordered_set<std::string_view> ResourceWrites;
		std::unordered_set<std::string_view> ResourceReads;

		std::unordered_set<std::string_view> ComponentWrites;
		std::unordered_set<std::string_view> ComponentReads;

		void Clear()
		{
			ResourceWrites.clear();
			ResourceReads.clear();

			ComponentWrites.clear();
			ComponentReads.clear();
		}

		void AddAll(const SystemInputNames& other)
		{
			ResourceWrites.insert_range(other.ResourceWrites);
			ResourceReads.insert_range(other.ResourceReads);

			ComponentWrites.insert_range(other.ComponentWrites);
			ComponentReads.insert_range(other.ComponentReads);
		}
	};

	class ECSResolver
	{
	public:
		ECSResolver(const NamedNodes& namedNodes)
			: m_NamedNodes(namedNodes)
		{}

		SystemSchedulingInfo ResolveScheduling();

	private:
		SystemInputNames getSystemInputNames(NAMED_SYSTEM_DEFINITION* pSystem);
		bool resourceInputsAreCompatible(const SystemInputNames& currentGroupInputs, const SystemInputNames& currentSystemInputs);
		bool componentInputsAreCompatible(const SystemInputNames& currentGroupInputs, const SystemInputNames& currentSystemInputs);
		SystemGroup getSystemsInStage(const std::vector<Token*>& groupNames);
		std::vector<SystemGroup> createSystemGroups(const SystemGroup& systems);

	private:
		const NamedNodes& m_NamedNodes;
	};
}