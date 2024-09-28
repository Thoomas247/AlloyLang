#include "Module.hpp"

namespace AlloyCompiler
{
	Module::Module(const fs::path& filePath)
		: m_Source(filePath)
		, m_TokenBuffer(m_Source)
		, m_NodeBuffer(m_Source, m_TokenBuffer)
	{}

	bool Module::Generate()
	{
		if (!m_Source.ReadFile())
		{
			return false;
		}

		if (!m_TokenBuffer.Tokenize())
		{
			return false;
		}

		if (!m_NodeBuffer.Parse())
		{
			return false;
		}

		return true;
	}

	bool Module::GenerateFromString(const std::string& sourceString)
	{
		m_Source = Source(sourceString);

		if (!m_TokenBuffer.Tokenize())
		{
			return false;
		}

		if (!m_NodeBuffer.Parse())
		{
			return false;
		}

		return true;
	}

	NodeBuffer& Module::GetNodeBuffer()
	{
		return m_NodeBuffer;
	}

	const Source& Module::GetSource() const
	{
		return m_Source;
	}

	Definition<MACRO_DEFINITION> Module::GetMacroDefinition(const std::string_view& name) const
	{
		return getDefinition(m_NodeBuffer.GetMacroDefinitions(), name);
	}

	Definition<TYPE_DEFINITION> Module::GetTypeDefinition(const std::string_view& name) const
	{
		return getDefinition(m_NodeBuffer.GetTypeDefinitions(), name);
	}

	Definition<FUNCTION_DEFINITION> Module::GetFunctionDefinition(const std::string_view& name) const
	{
		return getDefinition(m_NodeBuffer.GetFunctionDefinitions(), name);
	}

	Definition<VARIABLE_DEFINITION> Module::GetGlobalVariableDefinition(const std::string_view& name) const
	{
		return getDefinition(m_NodeBuffer.GetGlobalVariableDefinitions(), name);
	}
}