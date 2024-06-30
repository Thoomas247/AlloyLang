#pragma once
#include "source/Source.hpp"
#include "token/TokenBuffer.hpp"
#include "node/NodeBuffer.hpp"

namespace AlloyCompiler
{
	class Module
	{
	public:
		Module(const fs::path& filePath);

		void Generate();
		void Generate(const std::string& source);
		const NodeBuffer& GetNodeBuffer() const;

		Definition<MACRO_DEFINITION> GetMacroDefinition(const std::string_view& name);
		Definition<TYPE_DEFINITION> GetTypeDefinition(const std::string_view& name);
		Definition<FUNCTION_DEFINITION> GetFunctionDefinition(const std::string_view& name);
		Definition<EXTERN_DEFINITION> GetExternDefinition(const std::string_view& name);

	private:
		template <typename T>
		Definition<T> getDefinition(const NodeBuffer::NodeMap<T>& nodeMap, const std::string_view& name);

	private:
		Source m_Source;
		TokenBuffer m_TokenBuffer;
		NodeBuffer m_NodeBuffer;
	};

	template<typename T>
	inline Definition<T> Module::getDefinition(const NodeBuffer::NodeMap<T>& nodeMap, const std::string_view& name)
	{
		auto it = nodeMap.find(name);
		if (it == nodeMap.end())
		{
			return Definition<T>();
		}

		return it->second;
	}
}