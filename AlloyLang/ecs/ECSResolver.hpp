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
		ECSResolver(const Source& source, const NamedNodes& namedNodes)
			: m_Source(source), m_NamedNodes(namedNodes)
		{}

		SystemSchedulingInfo ResolveScheduling();

	private:
		template<typename... Args>
		constexpr void logErrorAtToken(Token* pToken, const std::string& format, Args&&... args);

		SystemInputNames getSystemInputNames(NAMED_SYSTEM_DEFINITION* pSystem);
		bool resourceInputsAreCompatible(const SystemInputNames& currentGroupInputs, const SystemInputNames& currentSystemInputs);
		bool componentInputsAreCompatible(const SystemInputNames& currentGroupInputs, const SystemInputNames& currentSystemInputs);
		SystemGroup getSystemsInStage(const std::vector<Token*>& groupNames);
		std::vector<SystemGroup> createSystemGroups(const SystemGroup& systems);

	private:
		const Source& m_Source;
		const NamedNodes& m_NamedNodes;
	};

	template<typename ...Args>
	constexpr void ECSResolver::logErrorAtToken(Token* pToken, const std::string& format, Args&& ...args)
	{
		const Location& location = pToken->Location;
		const size_t tokenSize = pToken->Value.size();
		const std::string_view line = m_Source.GetLine(location.LineStart);

		Log::Error("({0}:{1}) ERROR:", location.Line, location.Column);
		Log::Error("\t{0}", line);
		Log::Error("\t{0}{1}", std::string(location.Column - 1, ' '), std::string(tokenSize, '~'));
		Log::Error("\t{0}{1}", std::string(location.Column - 1, ' '), std::vformat(format, std::make_format_args(args...)));
	}
}