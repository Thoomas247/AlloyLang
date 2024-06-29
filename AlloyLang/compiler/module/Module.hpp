#pragma once
#include "source/Source.hpp"
#include "token/TokenBuffer.hpp"
#include "node/NodeBuffer.hpp"

namespace AlloyCompiler
{
	class Module
	{
	public:
		Module(const fs::path& filePath)
			: m_Source(filePath)
			, m_TokenBuffer(m_Source)
			, m_NodeBuffer(m_Source, m_TokenBuffer)
		{}

		void Generate()
		{
			m_Source.ReadFile();
			m_TokenBuffer.Tokenize();
			m_NodeBuffer.Parse();
		}

		Definition<MACRO_DEFINITION> GetMacroDefinition(const std::string_view& name)
		{
			return getDefinition(m_NodeBuffer.GetMacroDefinitions(), name);
		}

		Definition<TYPE_DEFINITION> GetTypeDefinition(const std::string_view& name)
		{
			return getDefinition(m_NodeBuffer.GetTypeDefinitions(), name);
		}

		Definition<FUNCTION_DEFINITION> GetFunctionDefinition(const std::string_view& name)
		{
			return getDefinition(m_NodeBuffer.GetFunctionDefinitions(), name);
		}

		Definition<EXTERN_DEFINITION> GetExternDefinition(const std::string_view& name)
		{
			return getDefinition(m_NodeBuffer.GetExternDefinitions(), name);
		}

	private:
		template <typename T>
		Definition<T> getDefinition(const NodeBuffer::NodeMap<T>& nodeMap, const std::string_view& name)
		{
			auto it = nodeMap.find(name);
			if (it == nodeMap.end())
			{
				return Definition<T>();
			}

			return it->second;
		}

	private:
		Source m_Source;
		TokenBuffer m_TokenBuffer;
		NodeBuffer m_NodeBuffer;
	};
}