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

	void Module::Generate(const std::string& source)
	{
		m_Source.SetSourceString(source);
		m_TokenBuffer.Tokenize();
		m_NodeBuffer.Parse();
	}


	const NodeBuffer& Module::GetNodeBuffer() const
	{
		return m_NodeBuffer;
	}

	Definition<MACRO_DEFINITION> Module::GetMacroDefinition(const std::string_view& name)
	{
		return getDefinition(m_NodeBuffer.GetMacroDefinitions(), name);
	}

	Definition<TYPE_DEFINITION> Module::GetTypeDefinition(const std::string_view& name)
	{
		return getDefinition(m_NodeBuffer.GetTypeDefinitions(), name);
	}

	Definition<FUNCTION_DEFINITION> Module::GetFunctionDefinition(const std::string_view& name)
	{
		return getDefinition(m_NodeBuffer.GetFunctionDefinitions(), name);
	}

	Definition<EXTERN_DEFINITION> Module::GetExternDefinition(const std::string_view& name)
	{
		return getDefinition(m_NodeBuffer.GetExternDefinitions(), name);
	}
}