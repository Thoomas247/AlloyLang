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

		void Print()
		{
			Log::Print("\n-- Start Groups --");

			for (int i = 0; i < StartSystemGroups.size(); i++)
			{
				Log::Print("Group {0}:", i);

				for (const std::string_view& systemName : StartSystemGroups[i])
				{
					Log::Print("{0}", systemName);
				}
			}

			Log::Print("\n-- Update Groups --");

			for (int i = 0; i < UpdateSystemGroups.size(); i++)
			{
				Log::Print("Group {0}:", i);

				for (const std::string_view& systemName : UpdateSystemGroups[i])
				{
					Log::Print("{0}", systemName);
				}
			}

			Log::Print("\n-- End Groups --");

			for (int i = 0; i < EndSystemGroups.size(); i++)
			{
				Log::Print("Group {0}:", i);

				for (const std::string_view& systemName : EndSystemGroups[i])
				{
					Log::Print("{0}", systemName);
				}
			}

			Log::Print("");
		}
	};

	SystemSchedulingInfo ResolveApplication(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, NodeID applicationNodeID);
}