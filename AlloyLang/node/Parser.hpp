#pragma once

#include "NodeAllocator.hpp"
#include "Nodes.hpp"

namespace AlloyCompiler
{
	struct NamedNodes
	{
		std::unordered_map<std::string_view, APPLICATION_DEFINITION*> ApplicationDefinitions;

		std::unordered_map<std::string_view, NAMED_GROUP_DEFINITION*> GroupDefinitions;
		std::unordered_map<std::string_view, NAMED_SYSTEM_DEFINITION*> SystemDefinitions;
		std::unordered_map<std::string_view, NAMED_QUERY_DEFINITION*> QueryDefinitions;
		std::unordered_map<std::string_view, NAMED_RESOURCE_DEFINITION*> ResourceDefinitions;
		std::unordered_map<std::string_view, NAMED_COMPONENT_DEFINITION*> ComponentDefinitions;
		std::unordered_map<std::string_view, NAMED_TYPE_DEFINITION*> TypeDefinitions;
		std::unordered_map<std::string_view, NAMED_FUNCTION_DEFINITION*> FunctionDefinitions;
		std::unordered_map<std::string_view, EXTERN_FUNCTION_DEFINITION*> ExternDefinitions;
	};

	class Parser
	{
	public:
		Parser(const TokenBuffers& tokenBuffers)
			: m_NamedNodes()
			, m_NodeAllocator(tokenBuffers.LastTokenID() * 120) // TODO: remove magic number
			, m_TokenBuffers(tokenBuffers)
			, m_CurrentTokenID(0)
		{
		}

		NamedNodes Parse();

	private:
		enum Result
		{
			SUCCESS = 0,
			EOF_REACHED,
			UNEXPECTED_TOKEN,
		};

	private:
		std::string tokenKindVectorToString(const std::vector<TokenKind>& tokens) const;

		template<typename... Args>
		constexpr void logErrorAtPosition(const std::string& format, Args&&... args);

		template <typename T, typename... Ts>
		T* parse(Ts...) = delete;

		template <typename T>
		T* createNode(const T& node);
		template <typename T>
		bool namedNodeExists(const std::string_view& name) const = delete;
		template <typename T>
		void addNamedNode(const std::string_view& name, T* pNode) = delete;

		bool isEOF() const;
		bool hasNext() const;
		TokenKind kind() const;
		std::string_view value() const;

		/// <summary>
		/// Moves onto the next token if current token is not EOF.
		/// </summary>
		Result eat();

		/// <summary>
		/// Moves onto the next token if the current token is one of the expected kinds.
		/// If pTokenValue is not nullptr, it will be set to the kind of the current token.
		/// If pValueView is not nullptr, it will be set to the value of the current token.
		/// </summary>
		template<TokenKind... Tokens>
		Result expect(TokenKind* pTokenValue = nullptr, std::string_view* pValueStringView = nullptr);

		template<TokenKind... Tokens>
		Result expect(std::string_view* pValueStringView, TokenKind* pTokenValue = nullptr);

	private:
		NamedNodes m_NamedNodes;
		NodeAllocator m_NodeAllocator;

		const TokenBuffers& m_TokenBuffers;
		TokenID m_CurrentTokenID;
	};

	template<typename... Args>
	constexpr void Parser::logErrorAtPosition(const std::string& format, Args&&... args)
	{
		const Location& location = m_TokenBuffers.GetLocation(m_CurrentTokenID);
		const std::string_view line = m_TokenBuffers.GetLine(location.Line);

		Log::Error("[{0} : {1}] ERROR:", location.Line, location.Column);
		Log::Error("\t{0}", line);
		Log::Error("\t{0}^", std::string(location.Column - 1, ' '));
		Log::Error("\t{0}{1}", std::string(location.Column - 1, ' '), std::vformat(format, std::make_format_args(args...)));
	}

	template<typename T>
	T* Parser::createNode(const T& node)
	{
		return m_NodeAllocator.Create<T>(node);
	}

	template<TokenKind ...Tokens>
	Parser::Result Parser::expect(TokenKind* pTokenValue, std::string_view* pValueStringView)
	{
		if ((m_TokenBuffers.GetKind(m_CurrentTokenID) != Tokens || ...))
		{
			logErrorAtPosition("Expected token of kind {0}.", tokenKindVectorToString({ (Tokens, ...) }));
			return UNEXPECTED_TOKEN;
		}

		if (pTokenValue != nullptr)
		{
			*pTokenValue = m_TokenBuffers.GetKind(m_CurrentTokenID);
		}

		if (pValueStringView != nullptr)
		{
			*pValueStringView = m_TokenBuffers.GetValue(m_CurrentTokenID);
		}

		++m_CurrentTokenID;
		return SUCCESS;
	}

	template<TokenKind ...Tokens>
	Parser::Result Parser::expect(std::string_view* pValueStringView, TokenKind* pTokenValue)
	{
		return expect<Tokens...>(pTokenValue, pValueStringView);
	}

#pragma region Parser::namedNodeExists

	template<>
	bool Parser::namedNodeExists<APPLICATION_DEFINITION>(const std::string_view& name) const
	{
		return m_NamedNodes.ApplicationDefinitions.contains(name);
	}

	template<>
	bool Parser::namedNodeExists<NAMED_GROUP_DEFINITION>(const std::string_view& name) const
	{
		return m_NamedNodes.GroupDefinitions.contains(name);
	}

	template<>
	bool Parser::namedNodeExists<NAMED_SYSTEM_DEFINITION>(const std::string_view& name) const
	{
		return m_NamedNodes.SystemDefinitions.contains(name);
	}

	template<>
	bool Parser::namedNodeExists<NAMED_QUERY_DEFINITION>(const std::string_view& name) const
	{
		return m_NamedNodes.QueryDefinitions.contains(name);
	}

	template<>
	bool Parser::namedNodeExists<NAMED_RESOURCE_DEFINITION>(const std::string_view& name) const
	{
		return m_NamedNodes.ResourceDefinitions.contains(name);
	}

	template<>
	bool Parser::namedNodeExists<NAMED_COMPONENT_DEFINITION>(const std::string_view& name) const
	{
		return m_NamedNodes.ComponentDefinitions.contains(name);
	}

	template<>
	bool Parser::namedNodeExists<NAMED_TYPE_DEFINITION>(const std::string_view& name) const
	{
		return m_NamedNodes.TypeDefinitions.contains(name);
	}

	template<>
	bool Parser::namedNodeExists<NAMED_FUNCTION_DEFINITION>(const std::string_view& name) const
	{
		return m_NamedNodes.FunctionDefinitions.contains(name);
	}

	template<>
	bool Parser::namedNodeExists<EXTERN_FUNCTION_DEFINITION>(const std::string_view& name) const
	{
		return m_NamedNodes.ExternDefinitions.contains(name);
	}

#pragma endregion

#pragma region Parser::addNamedNode

	template<>
	void Parser::addNamedNode(const std::string_view& name, APPLICATION_DEFINITION* pNode)
	{
		m_NamedNodes.ApplicationDefinitions[name] = pNode;
	}

	template<>
	void Parser::addNamedNode(const std::string_view& name, NAMED_GROUP_DEFINITION* pNode)
	{
		m_NamedNodes.GroupDefinitions[name] = pNode;
	}

	template<>
	void Parser::addNamedNode(const std::string_view& name, NAMED_SYSTEM_DEFINITION* pNode)
	{
		m_NamedNodes.SystemDefinitions[name] = pNode;
	}

	template<>
	void Parser::addNamedNode(const std::string_view& name, NAMED_QUERY_DEFINITION* pNode)
	{
		m_NamedNodes.QueryDefinitions[name] = pNode;
	}

	template<>
	void Parser::addNamedNode(const std::string_view& name, NAMED_RESOURCE_DEFINITION* pNode)
	{
		m_NamedNodes.ResourceDefinitions[name] = pNode;
	}

	template<>
	void Parser::addNamedNode(const std::string_view& name, NAMED_COMPONENT_DEFINITION* pNode)
	{
		m_NamedNodes.ComponentDefinitions[name] = pNode;
	}

	template<>
	void Parser::addNamedNode(const std::string_view& name, NAMED_TYPE_DEFINITION* pNode)
	{
		m_NamedNodes.TypeDefinitions[name] = pNode;
	}

	template<>
	void Parser::addNamedNode(const std::string_view& name, NAMED_FUNCTION_DEFINITION* pNode)
	{
		m_NamedNodes.FunctionDefinitions[name] = pNode;
	}

	template<>
	void Parser::addNamedNode(const std::string_view& name, EXTERN_FUNCTION_DEFINITION* pNode)
	{
		m_NamedNodes.ExternDefinitions[name] = pNode;
	}

#pragma endregion

}