#include "Parser.hpp"

namespace AlloyCompiler
{

#pragma region Util

	std::string Parser::tokenKindVectorToString(const std::vector<TokenKind>& tokens) const
	{
		// handle 0 tokens
		if (tokens.empty())
		{
			return "";
		}

		// handle 1 token
		if (tokens.size() == 1)
		{
			return "'" + TOKEN_KIND_NAMES.at(tokens[0]) + "'";
		}

		// handle 2 or more tokens
		std::string result;
		for (size_t i = 0; i < tokens.size() - 1; ++i)
		{
			result += "'" + TOKEN_KIND_NAMES.at(tokens[i]) + "', ";
		}

		result += "or '" + TOKEN_KIND_NAMES.at(tokens[tokens.size() - 1]) + "'";

		return result;
	}

	bool Parser::isEOF() const
	{
		return m_CurrentTokenID > m_TokenBuffers.LastTokenID();
	}

	bool Parser::hasNext() const
	{
		return !isEOF();
	}

	TokenKind Parser::kind() const
	{
		return m_TokenBuffers.GetKind(m_CurrentTokenID);
	}

	std::string_view Parser::value() const
	{
		return m_TokenBuffers.GetValue(m_CurrentTokenID);
	}

	Parser::Result Parser::eat()
	{
		if (isEOF())
		{
			logErrorAtPosition("Unexpected end of file.");
			return EOF_REACHED;
		}

		++m_CurrentTokenID;
		return SUCCESS;
	}

	Parser::Result Parser::expect(TokenKind tokenKind, std::string_view* pValueStringView)
	{
		if (m_TokenBuffers.GetKind(m_CurrentTokenID) != tokenKind)
		{
			logErrorAtPosition("Expected token of kind {0}.", TOKEN_KIND_NAMES.at(tokenKind));
			return UNEXPECTED_TOKEN;
		}

		if (pValueStringView != nullptr)
		{
			*pValueStringView = m_TokenBuffers.GetValue(m_CurrentTokenID);
		}

		++m_CurrentTokenID;
		return SUCCESS;
	}

#pragma endregion

	template<>
	IF_STATEMENT* Parser::parse()
	{
		if (expect<TokenKind::if_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		ENCLOSED_EXPRESSION* pCondition = parse<ENCLOSED_EXPRESSION>();

		if (pCondition == nullptr)
		{
			return nullptr;
		}

		STATEMENT* pStatement = parse<STATEMENT>();

		if (pStatement == nullptr)
		{
			return nullptr;
		}

		STATEMENT* pElseStatement = nullptr;
		if (kind() == TokenKind::else_keyword)
		{
			if (eat() != SUCCESS)
			{
				return nullptr;
			}

			pElseStatement = parse<STATEMENT>();

			if (pElseStatement == nullptr)
			{
				return nullptr;
			}
		}

		return createNode(
			IF_STATEMENT
			{
				.pCondition = pCondition->pExpression,
				.pStatement = pStatement,
				.pElseStatement = pElseStatement
			}
		);
	}

	template<>
	STATEMENT_BLOCK* Parser::parse()
	{
		if (expect<TokenKind::open_brace>() != SUCCESS)
		{
			return nullptr;
		}

		std::vector<STATEMENT*> statements;
		while (kind() != TokenKind::close_brace)
		{
			STATEMENT* pStatement = parse<STATEMENT>();

			if (pStatement == nullptr)
			{
				return nullptr;
			}

			statements.push_back(pStatement);
		}

		if (eat() != SUCCESS)
		{
			return nullptr;
		}

		return createNode(
			STATEMENT_BLOCK
			{
				.Statements = std::move(statements)
			}
		);
	}

	template<>
	RETURN* Parser::parse()
	{
		if (expect<TokenKind::return_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		EXPRESSION* pExpression = nullptr;
		if (kind() != TokenKind::semicolon)
		{
			pExpression = parse<EXPRESSION>();

			if (pExpression == nullptr)
			{
				return nullptr;
			}
		}

		if (expect<TokenKind::semicolon>() != SUCCESS)
		{
			return nullptr;
		}

		return createNode(
			RETURN
			{
				.pValue = pExpression
			}
		);
	}

#pragma region ECS Constructs

	template<>
	NAMED_COMPONENT_DEFINITION* Parser::parse()
	{
		if (expect<TokenKind::component_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		std::string_view name;
		if (expect<TokenKind::identifier>(&name) != SUCCESS)
		{
			return nullptr;
		}

		if (namedNodeExists<NAMED_COMPONENT_DEFINITION>(name))
		{
			logErrorAtPosition("Component '{0}' is already defined.", name);
			return nullptr;
		}

		if (expect<TokenKind::assignment_operator>() != SUCCESS)
		{
			return nullptr;
		}

		TYPE* pType = parse<TYPE>();

		if (pType == nullptr)
		{
			return nullptr;
		}

		if (expect<TokenKind::semicolon>() != SUCCESS)
		{
			return nullptr;
		}

		NAMED_COMPONENT_DEFINITION* pComponentDefinition = createNode(
			NAMED_COMPONENT_DEFINITION
			{
				.Name = name,
				.pType = pType
			}
		);

		addNamedNode(name, pComponentDefinition);

		return pComponentDefinition;
	}

	template<>
	NAMED_RESOURCE_DEFINITION* Parser::parse()
	{
		if (expect<TokenKind::resource_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		std::string_view name;
		if (expect<TokenKind::identifier>(&name) != SUCCESS)
		{
			return nullptr;
		}

		if (namedNodeExists<NAMED_RESOURCE_DEFINITION>(name))
		{
			logErrorAtPosition("Resource '{0}' is already defined.", name);
			return nullptr;
		}

		if (expect<TokenKind::assignment_operator>() != SUCCESS)
		{
			return nullptr;
		}

		TYPE* pType = parse<TYPE>();

		if (pType == nullptr)
		{
			return nullptr;
		}

		if (expect<TokenKind::semicolon>() != SUCCESS)
		{
			return nullptr;
		}

		NAMED_RESOURCE_DEFINITION* pResourceDefinition = createNode(
			NAMED_RESOURCE_DEFINITION
			{
				.Name = name,
				.pType = pType
			}
		);

		addNamedNode(name, pResourceDefinition);

		return pResourceDefinition;
	}

	template<>
	NAMED_QUERY_DEFINITION* Parser::parse()
	{
		if (expect<TokenKind::query_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		std::string_view name;
		if (expect<TokenKind::identifier>(&name) != SUCCESS)
		{
			return nullptr;
		}

		if (namedNodeExists<NAMED_QUERY_DEFINITION>(name))
		{
			logErrorAtPosition("Query '{0}' is already defined.", name);
			return nullptr;
		}

		if (expect<TokenKind::open_brace>() != SUCCESS)
		{
			return nullptr;
		}

		std::vector<std::string_view> reads, writes;
		while (kind() != TokenKind::close_brace)
		{
			TokenKind readOrWrite;

			if (expect<TokenKind::variable_keyword, TokenKind::constant_keyword>(&readOrWrite) != SUCCESS)
			{
				return nullptr;
			}

			std::string_view componentName;
			if (expect<TokenKind::identifier>(&componentName) != SUCCESS)
			{
				return nullptr;
			}

			if (readOrWrite == TokenKind::variable_keyword)
			{
				writes.push_back(componentName);
			}
			else
			{
				reads.push_back(componentName);
			}

			if (expect<TokenKind::semicolon>() != SUCCESS)
			{
				return nullptr;
			}
		}

		if (eat() != SUCCESS)
		{
			return nullptr;
		}

		NAMED_QUERY_DEFINITION* pQueryDefinition = createNode(
			NAMED_QUERY_DEFINITION
			{
				.Name = name,
				.ComponentReadNames = reads,
				.ComponentWriteNames = writes
			}
		);

		addNamedNode(name, pQueryDefinition);

		return pQueryDefinition;
	}

	template<>
	NAMED_SYSTEM_DEFINITION* Parser::parse()
	{
		if (expect<TokenKind::system_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		std::string_view name;
		if (expect<TokenKind::identifier>(&name) != SUCCESS)
		{
			return nullptr;
		}

		if (namedNodeExists<NAMED_SYSTEM_DEFINITION>(name))
		{
			logErrorAtPosition("System '{0}' is already defined.", name);
			return nullptr;
		}

		if (expect<TokenKind::open_paren>() != SUCCESS)
		{
			return nullptr;
		}

		std::vector<std::string_view> queryNames;
		while (kind() != TokenKind::close_paren)
		{
			std::string_view queryName;
			if (expect<TokenKind::identifier>(&queryName) != SUCCESS)
			{
				return nullptr;
			}

			queryNames.push_back(queryName);

			if (kind() == TokenKind::comma)
			{
				if (eat() != SUCCESS)
				{
					return nullptr;
				}
			}
		}

		// consume closing paren
		if (eat() != SUCCESS)
		{
			return nullptr;
		}

		STATEMENT_BLOCK* pBody = parse<STATEMENT_BLOCK>();

		if (pBody == nullptr)
		{
			return nullptr;
		}

		NAMED_SYSTEM_DEFINITION* pSystemDefinition = createNode(
			NAMED_SYSTEM_DEFINITION
			{
				.Name = name,
				.QueryNames = queryNames,
				.pBody = pBody
			}
		);

		addNamedNode(name, pSystemDefinition);

		return pSystemDefinition;
	}

	template<>
	NAMED_GROUP_DEFINITION* Parser::parse()
	{
		if (expect<TokenKind::group_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		std::string_view name;
		if (expect<TokenKind::identifier>(&name) != SUCCESS)
		{
			return nullptr;
		}

		if (namedNodeExists<NAMED_GROUP_DEFINITION>(name))
		{
			logErrorAtPosition("Group '{0}' is already defined.", name);
			return nullptr;
		}

		if (expect<TokenKind::open_brace>() != SUCCESS)
		{
			return nullptr;
		}

		std::vector<std::string_view> systemNames;
		while (kind() != TokenKind::close_brace)
		{
			std::string_view systemName;
			if (expect<TokenKind::identifier>(&systemName) != SUCCESS)
			{
				return nullptr;
			}

			systemNames.push_back(systemName);

			if (kind() == TokenKind::comma)
			{
				if (eat() != SUCCESS)
				{
					return nullptr;
				}
			}
		}

		// eat closing brace
		if (eat() != SUCCESS)
		{
			return nullptr;
		}

		NAMED_GROUP_DEFINITION* pGroupDefinition = createNode(
			NAMED_GROUP_DEFINITION
			{
				.Name = name,
				.SystemNames = systemNames
			}
		);

		addNamedNode(name, pGroupDefinition);

		return pGroupDefinition;
	}

	template<>
	APPLICATION_DEFINITION* Parser::parse()
	{
		if (expect<TokenKind::application_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		std::string_view name;
		if (expect<TokenKind::identifier>(&name) != SUCCESS)
		{
			return nullptr;
		}

		if (namedNodeExists<APPLICATION_DEFINITION>(name))
		{
			logErrorAtPosition("Application '{0}' is already defined.", name);
			return nullptr;
		}

		if (expect<TokenKind::open_brace>() != SUCCESS)
		{
			return nullptr;
		}

		std::vector<std::string_view> startGroups, updateGroups, endGroups;
		bool startFound = false, updateFound = false, endFound = false;

		while (kind() != TokenKind::close_brace)
		{
			std::string_view stageName;
			if (expect<TokenKind::identifier>(&stageName) != SUCCESS)
			{
				return nullptr;
			}

			if (expect<TokenKind::open_brace>() != SUCCESS)
			{
				return nullptr;
			}

			// based on the name of the stage, determine which vector we are inserting into
			std::vector<std::string_view>* pDstGroups = nullptr;

			bool stageRedefinition = false;

			if (stageName == "start")
			{
				pDstGroups = &startGroups;

				if (startFound)
				{
					stageRedefinition = true;
				}

				startFound = true;
			}
			else if (stageName == "update")
			{
				pDstGroups = &updateGroups;

				if (updateFound)
				{
					stageRedefinition = true;
				}

				updateFound = true;
			}
			else if (stageName == "end")
			{
				pDstGroups = &endGroups;

				if (endFound)
				{
					stageRedefinition = true;
				}

				endFound = true;
			}
			else
			{
				logErrorAtPosition("Invalid application stage name '{0}'. Valid stages are 'start', 'update' and 'end'.", stageName);
				return nullptr;
			}

			if (stageRedefinition)
			{
				logErrorAtPosition("Application stage '{0}' is already defined.", stageName);
				return nullptr;
			}

			while (kind() != TokenKind::close_brace)
			{
				std::string_view groupName;
				if (expect<TokenKind::identifier>(&groupName) != SUCCESS)
				{
					return nullptr;
				}

				pDstGroups->push_back(groupName);

				if (kind() == TokenKind::comma)
				{
					if (eat() != SUCCESS)
					{
						return nullptr;
					}
				}
			}

			// eat closing brace
			if (eat() != SUCCESS)
			{
				return nullptr;
			}
		}

		// eat closing brace
		if (eat() != SUCCESS)
		{
			return nullptr;
		}

		APPLICATION_DEFINITION* pApplicationDefinition = createNode(
			APPLICATION_DEFINITION
			{
				.Name = name,
				.StartGroupNames = std::move(startGroups),
				.UpdateGroupNames = std::move(updateGroups),
				.EndGroupNames = std::move(endGroups)
			}
		);

		addNamedNode(name, pApplicationDefinition);

		return pApplicationDefinition;
	}

#pragma endregion

	NamedNodes Parser::Parse()
	{
		while (!isEOF())
		{
			switch (kind())
			{
			case TokenKind::group_keyword:
				parse<NAMED_GROUP_DEFINITION>();

			case TokenKind::system_keyword:
				parse<NAMED_SYSTEM_DEFINITION>();

			case TokenKind::query_keyword:
				parse<NAMED_QUERY_DEFINITION>();

			case TokenKind::type_keyword:
				parse<NAMED_TYPE_DEFINITION>();

			case TokenKind::function_keyword:
				parse<NAMED_FUNCTION_DEFINITION>();

			case TokenKind::extern_keyword:
				parse<EXTERN_FUNCTION_DEFINITION>();

			case TokenKind::application_keyword:
				parse<APPLICATION_DEFINITION>();

			default:
				break;
			}
		}

		return std::move(m_NamedNodes);
	}

}