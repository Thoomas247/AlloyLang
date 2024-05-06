#include "Parser.hpp"

namespace AlloyCompiler
{
	template<typename ...Args>
	constexpr void logErrorAtPosition(TokenBuffers::Iterator& iter, const std::string& format, Args && ...args)
	{
		Log::Error("Error at location ({0} : {1}):", iter.GetLocation().Line, iter.GetLocation().Column);
		Log::Error("\t{0}", iter.GetLine());
		Log::Error("\t{0}^", std::string(iter.GetLocation().Column - 1, ' '));
		Log::Error("\t{0}{1}", std::string(iter.GetLocation().Column - 1, ' '), std::vformat(format, std::make_format_args(args...)));
	}

	struct ParsingState
	{
		constexpr static auto NUM_NODES_FACTOR = 1.5;

		NamedNodes NamedNodes;
		NodeAllocator Allocator;
		TokenBuffers::Iterator TokenIter;

		ParsingState(const TokenBuffers& tokenBuffers)
			: NamedNodes()
			, Allocator((size_t)((size_t)tokenBuffers.LastTokenID() * NUM_NODES_FACTOR))
			, TokenIter(tokenBuffers.GetIterator())
		{
		}
	};

	template <typename T, typename... Ts>
	T* parse(ParsingState&, Ts...) = delete;



#pragma region ECS Constructs

	template <>
	NAMED_GROUP_DEFINITION* parse(ParsingState& state)
	{
		// check for group keyword
		if (state.TokenIter.GetKind() != TokenKind::group_keyword)
		{
			logErrorAtPosition(state.TokenIter, "Expected to find 'group' keyword!");
			return nullptr;
		}

		// check for identifier
		if (!state.TokenIter.Next() || state.TokenIter.GetKind() != TokenKind::identifier)
		{
			logErrorAtPosition(state.TokenIter, "Expected to find group name!");
			return nullptr;
		}

		// get the group name
		std::string_view groupName = state.TokenIter.GetValue();

		// check if we already have a group with the same name
		if (state.NamedNodes.GroupDefinitions.contains(groupName))
		{
			logErrorAtPosition(state.TokenIter, "Group '{0}' is already defined!", groupName);
			return nullptr;
		}

		// check for open brace
		if (!state.TokenIter.Next() || state.TokenIter.GetKind() != TokenKind::open_brace)
		{
			logErrorAtPosition(state.TokenIter, "Expected to find a '{0}' after the group name!", "{");
			return nullptr;
		}

		// consume open brace
		if (!state.TokenIter.Next())
		{
			logErrorAtPosition(state.TokenIter, "Unexpected end of file! Expected group definition.");
			return nullptr;
		}

		std::vector<std::string_view> systemNames;

		while (state.TokenIter.GetKind() != TokenKind::close_brace)
		{
			// check for identifier
			if (state.TokenIter.GetKind() != TokenKind::identifier)
			{
				logErrorAtPosition(state.TokenIter, "Expected to find system name!");
				return nullptr;
			}

			// get the system name
			std::string_view systemName = state.TokenIter.GetValue();

			systemNames.push_back(systemName);

			if (!state.TokenIter.Next() || (state.TokenIter.GetKind() != TokenKind::comma && state.TokenIter.GetKind() != TokenKind::close_brace))
			{
				logErrorAtPosition(state.TokenIter, "Expected to find a ',' or '}' after system name!");
				return nullptr;
			}

			if (state.TokenIter.GetKind() == TokenKind::comma)
			{
				// consume comma
				if (!state.TokenIter.Next())
				{
					logErrorAtPosition(state.TokenIter, "Unexpected end of file! Expected group definition.");
					return nullptr;
				}
			}
		}

		// consume closing brace
		(void)state.TokenIter.Next();

		NAMED_GROUP_DEFINITION* pGroupDefinition = state.Allocator.Create(
			NAMED_GROUP_DEFINITION
			{
				.Name = groupName,
				.SystemNames = systemNames
			}
		);

		state.NamedNodes.GroupDefinitions[groupName] = pGroupDefinition;

		return pGroupDefinition;
	}

	template <>
	NAMED_SYSTEM_DEFINITION* parse(ParsingState& state)
	{
		// check for system keyword
		if (state.TokenIter.GetKind() != TokenKind::system_keyword)
		{
			logErrorAtPosition(state.TokenIter, "Expected to find 'system' keyword!");
			return nullptr;
		}

		// check for identifier
		if (!state.TokenIter.Next() || state.TokenIter.GetKind() != TokenKind::identifier)
		{
			logErrorAtPosition(state.TokenIter, "Expected to find system name!");
			return nullptr;
		}

		// get the system name
		std::string_view systemName = state.TokenIter.GetValue();

		// check if we already have a system with the same name
		if (state.NamedNodes.SystemDefinitions.contains(systemName))
		{
			logErrorAtPosition(state.TokenIter, "System '{0}' is already defined!", systemName);
			return nullptr;
		}

		// check for open parenthesis
		if (!state.TokenIter.Next() || state.TokenIter.GetKind() != TokenKind::open_paren)
		{
			logErrorAtPosition(state.TokenIter, "Expected to find a '(' after the system name!");
			return nullptr;
		}

		// consume open parenthesis
		if (!state.TokenIter.Next())
		{
			logErrorAtPosition(state.TokenIter, "Unexpected end of file! Expected system definition.");
			return nullptr;
		}

		// collect query names

		std::vector<std::string_view> queryNames;

		while (state.TokenIter.GetKind() != TokenKind::close_paren)
		{
			// check for identifier
			if (state.TokenIter.GetKind() != TokenKind::identifier)
			{
				logErrorAtPosition(state.TokenIter, "Expected to find query name!");
				return nullptr;
			}

			// get the query name
			std::string_view queryName = state.TokenIter.GetValue();

			queryNames.push_back(queryName);

			if (!state.TokenIter.Next() || (state.TokenIter.GetKind() != TokenKind::comma && state.TokenIter.GetKind() != TokenKind::close_paren))
			{
				logErrorAtPosition(state.TokenIter, "Expected to find a ',' or ')' after query name!");
				return nullptr;
			}

			if (state.TokenIter.GetKind() == TokenKind::comma)
			{
				// consume comma
				if (!state.TokenIter.Next())
				{
					logErrorAtPosition(state.TokenIter, "Unexpected end of file! Expected system definition.");
					return nullptr;
				}
			}
		}

		// consume closing parenthesis
		(void)state.TokenIter.Next();

		// parse body
		STATEMENT_BLOCK* pBody = parse<STATEMENT_BLOCK>(state);

		if (pBody == nullptr)
		{
			return nullptr;
		}

		NAMED_SYSTEM_DEFINITION* pSystemDefinition = state.Allocator.Create(
			NAMED_SYSTEM_DEFINITION
			{
				.Name = systemName,
				.QueryNames = queryNames,
				.pBody = pBody
			}
		);

		state.NamedNodes.SystemDefinitions[systemName] = pSystemDefinition;

		return pSystemDefinition;
	}

	template <>
	NAMED_QUERY_DEFINITION* parse(ParsingState& state)
	{
		// check for query keyword
		if (state.TokenIter.GetKind() != TokenKind::query_keyword)
		{
			logErrorAtPosition(state.TokenIter, "Expected to find 'query' keyword!");
			return nullptr;
		}

		// check for identifier
		if (!state.TokenIter.Next() || state.TokenIter.GetKind() != TokenKind::identifier)
		{
			logErrorAtPosition(state.TokenIter, "Expected to find query name!");
			return nullptr;
		}

		// get the query name
		std::string_view queryName = state.TokenIter.GetValue();

		// check if we already have a query with the same name
		if (state.NamedNodes.QueryDefinitions.contains(queryName))
		{
			logErrorAtPosition(state.TokenIter, "Query '{0}' is already defined!", queryName);
			return nullptr;
		}

		// check for open brace
		if (!state.TokenIter.Next() || state.TokenIter.GetKind() != TokenKind::open_brace)
		{
			logErrorAtPosition(state.TokenIter, "Expected to find a '{0}' after the query name!", "{");
			return nullptr;
		}

		// consume open brace
		if (!state.TokenIter.Next())
		{
			logErrorAtPosition(state.TokenIter, "Unexpected end of file! Expected query definition.");
			return nullptr;
		}

		std::vector<std::string_view> reads;
		std::vector<std::string_view> writes;

		while (state.TokenIter.GetKind() != TokenKind::close_brace)
		{
			bool isWrite = false;

			if (state.TokenIter.GetKind() == TokenKind::constant_keyword)
			{
				isWrite = false;
			}
			else if (state.TokenIter.GetKind() == TokenKind::variable_keyword)
			{
				isWrite = true;
			}
			else
			{
				logErrorAtPosition(state.TokenIter, "Expected to find 'var' or 'const' keyword!");
				return nullptr;
			}

			// check for identifier
			if (!state.TokenIter.Next() || state.TokenIter.GetKind() != TokenKind::identifier)
			{
				logErrorAtPosition(state.TokenIter, "Expected to find component name!");
				return nullptr;
			}

			std::string_view componentName = state.TokenIter.GetValue();

			// consume component name
			if (!state.TokenIter.Next())
			{
				logErrorAtPosition(state.TokenIter, "Unexpected end of file! Expected queryable definition.");
				return nullptr;
			}

			if (isWrite)
			{
				writes.push_back(componentName);
			}
			else
			{
				reads.push_back(componentName);
			}

			// check for semicolon
			if (state.TokenIter.GetKind() != TokenKind::semicolon)
			{
				logErrorAtPosition(state.TokenIter, "Expected to find a ';' after component or resource name!");
				return nullptr;
			}

			// consume semicolon
			if (!state.TokenIter.Next())
			{
				logErrorAtPosition(state.TokenIter, "Unexpected end of file! Expected query definition.");
				return nullptr;
			}
		}

		// consume closing brace
		(void)state.TokenIter.Next();

		NAMED_QUERY_DEFINITION* pQueryDefinition = state.Allocator.Create(
			NAMED_QUERY_DEFINITION
			{
				.Name = queryName,
				.ComponentReadNames = reads,
				.ComponentWriteNames = writes,
			}
		);

		state.NamedNodes.QueryDefinitions[queryName] = pQueryDefinition;

		return pQueryDefinition;
	}

	template <>
	NAMED_TYPE_DEFINITION* parse(ParsingState& state)
	{
		// check for type keyword
		if (state.TokenIter.GetKind() != TokenKind::type_keyword)
		{
			logErrorAtPosition(state.TokenIter, "Expected to find 'type' keyword!");
			return nullptr;
		}

		// check for identifier
		if (!state.TokenIter.Next() || state.TokenIter.GetKind() != TokenKind::identifier)
		{
			logErrorAtPosition(state.TokenIter, "Expected to find type name!");
			return nullptr;
		}

		// get the type name
		std::string_view typeName = state.TokenIter.GetValue();

		// check if we already have a type with the same name
		if (state.NamedNodes.TypeDefinitions.contains(typeName))
		{
			logErrorAtPosition(state.TokenIter, "Type '{0}' is already defined!", typeName);
			return nullptr;
		}

		// check for assignment operator
		if (!state.TokenIter.Next() || state.TokenIter.GetKind() != TokenKind::assignment_operator)
		{
			logErrorAtPosition(state.TokenIter, "Expected to find an '=' after the type name!");
			return nullptr;
		}

		// consume assignment operator
		if (!state.TokenIter.Next())
		{
			logErrorAtPosition(state.TokenIter, "Unexpected end of file! Expected type definition.");
			return nullptr;
		}

		// parse type
		TYPE* pType = parse<TYPE>(state);

		if (pType == nullptr)
		{
			return nullptr;
		}

		// check for semicolon
		if (state.TokenIter.GetKind() != TokenKind::semicolon)
		{
			logErrorAtPosition(state.TokenIter, "Expected to find a ';' after type definition!");
			return nullptr;
		}

		// consume semicolon
		(void)state.TokenIter.Next();

		NAMED_TYPE_DEFINITION* pTypeDefinition = state.Allocator.Create(
			NAMED_TYPE_DEFINITION
			{
				.Name = typeName,
				.pType = pType
			}
		);

		state.NamedNodes.TypeDefinitions[typeName] = pTypeDefinition;

		return pTypeDefinition;
	}

	template <>
	NAMED_FUNCTION_DEFINITION* parse(ParsingState& state)
	{
		// parse function signature
		FUNCTION_SIGNATURE* pSignature = parse<FUNCTION_SIGNATURE>(state, /*allowVarArgs*/false);

		if (pSignature == nullptr)
		{
			return nullptr;
		}

		std::string_view functionName = pSignature->Name;

		// check if we already have a function with the same name
		if (state.NamedNodes.FunctionDefinitions.contains(functionName) || state.NamedNodes.ExternDefinitions.contains(functionName))
		{
			logErrorAtPosition(state.TokenIter, "Function with name '{0}' already exists!", functionName);
			return nullptr;
		}

		// parse body
		STATEMENT_BLOCK* pBody = parse<STATEMENT_BLOCK>(state);

		if (pBody == nullptr)
		{
			return nullptr;
		}

		NAMED_FUNCTION_DEFINITION* pFunctionDefinition = state.Allocator.Create(
			NAMED_FUNCTION_DEFINITION
			{
				.pSignature = pSignature,
				.pBody = pBody
			}
		);

		state.NamedNodes.FunctionDefinitions[functionName] = pFunctionDefinition;

		return pFunctionDefinition;
	}

	template <>
	EXTERN_FUNCTION_DEFINITION* parse(ParsingState& state)
	{
		// check for extern keyword
		if (state.TokenIter.GetKind() != TokenKind::extern_keyword)
		{
			logErrorAtPosition(state.TokenIter, "Expected to find 'extern' keyword!");
			return nullptr;
		}

		// consume extern keyword
		if (!state.TokenIter.Next())
		{
			logErrorAtPosition(state.TokenIter, "Unexpected end of file! Expected extern definition.");
			return nullptr;
		}

		// parse function signature
		FUNCTION_SIGNATURE* pSignature = parse<FUNCTION_SIGNATURE>(state, /*allowVarArgs*/true);

		if (pSignature == nullptr)
		{
			return nullptr;
		}

		std::string_view externName = pSignature->Name;

		// check if we already have an extern with the same name
		if (state.NamedNodes.ExternDefinitions.contains(externName) || state.NamedNodes.FunctionDefinitions.contains(externName))
		{
			logErrorAtPosition(state.TokenIter, "Function with name '{0}' already exists!", externName);
			return nullptr;
		}

		// check for semicolon
		if (state.TokenIter.GetKind() != TokenKind::semicolon)
		{
			logErrorAtPosition(state.TokenIter, "Expected to find a ';' after extern definition!");
			return nullptr;
		}

		// consume semicolon
		(void)state.TokenIter.Next();

		EXTERN_FUNCTION_DEFINITION* pExternDefinition = state.Allocator.Create(
			EXTERN_FUNCTION_DEFINITION
			{
				.pSignature = pSignature
			}
		);

		state.NamedNodes.ExternDefinitions[externName] = pExternDefinition;

		return pExternDefinition;
	}

	template <>
	APPLICATION_DEFINITION* parse(ParsingState& state)
	{
		if (state.TokenIter.GetKind() != TokenKind::application_keyword)
		{
			logErrorAtPosition(state.TokenIter, "Expected to find 'application' keyword!");
			return nullptr;
		}

		if (!state.TokenIter.Next() || state.TokenIter.GetKind() != TokenKind::identifier)
		{
			logErrorAtPosition(state.TokenIter, "Expected to find application name!");
			return nullptr;
		}

		// get the application name
		std::string_view applicationName = state.TokenIter.GetValue();

		// check if we already have an application with the same name
		if (state.NamedNodes.ApplicationDefinitions.contains(applicationName))
		{
			logErrorAtPosition(state.TokenIter, "Application '{0}' is already defined!", applicationName);
			return nullptr;
		}

		if (!state.TokenIter.Next() || state.TokenIter.GetKind() != TokenKind::open_brace)
		{
			logErrorAtPosition(state.TokenIter, "Expected to find a {0} after the application name!", "{");
			return nullptr;
		}

		if (!state.TokenIter.Next())
		{
			logErrorAtPosition(state.TokenIter, "Unexpected end of file! Expected application definition.");
			return nullptr;
		}

		std::unordered_map<std::string_view, std::pair<bool, std::vector<std::string_view>>> stageGroupLists = {
			{ "start",	{ false, {} } },
			{ "update", { false, {} } },
			{ "end",	{ false, {} } }
		};

		while (state.TokenIter.GetKind() != TokenKind::close_brace)
		{
			if (state.TokenIter.GetKind() != TokenKind::identifier)
			{
				logErrorAtPosition(state.TokenIter, "Expected to find stage identifier!");
				return nullptr;
			}

			const std::string_view stageName = state.TokenIter.GetValue();

			auto it = stageGroupLists.find(stageName);

			// check that stage name is valid
			if (it == stageGroupLists.end())
			{
				logErrorAtPosition(state.TokenIter, "Invalid application stage '{0}'! Valid stages are 'start', 'update', and 'end'.", stageName);
				return nullptr;
			}

			// check that stage name is not duplicate
			if (it->second.first == true)
			{
				logErrorAtPosition(state.TokenIter, "Application stage '{0}' is already defined!", stageName);
				return nullptr;
			}

			if (!state.TokenIter.Next() || state.TokenIter.GetKind() != TokenKind::open_brace)
			{
				logErrorAtPosition(state.TokenIter, "Expected to find a {0} after the stage name!", "{");
				return nullptr;
			}

			if (!state.TokenIter.Next())
			{
				logErrorAtPosition(state.TokenIter, "Unexpected end of file! Expected to find a list of group names!");
				return nullptr;
			}

			// collect group names

			std::vector<std::string_view>& groupNames = it->second.second;

			while (state.TokenIter.GetKind() != TokenKind::close_brace)
			{
				if (state.TokenIter.GetKind() != TokenKind::identifier)
				{
					logErrorAtPosition(state.TokenIter, "Expected to find group name!");
					return nullptr;
				}

				const std::string_view groupName = state.TokenIter.GetValue();

				groupNames.push_back(groupName);

				if (!state.TokenIter.Next() || (state.TokenIter.GetKind() != TokenKind::comma && state.TokenIter.GetKind() != TokenKind::close_brace))
				{
					logErrorAtPosition(state.TokenIter, "Expected to find a ',' or '}' after group name!");
					return nullptr;
				}

				if (state.TokenIter.GetKind() == TokenKind::comma)
				{
					// consume comma
					if (!state.TokenIter.Next())
					{
						logErrorAtPosition(state.TokenIter, "Unexpected end of file! Expected to find a list of group names!");
						return nullptr;
					}
				}
			}
		}

		// consume closing brace
		(void)state.TokenIter.Next();

		APPLICATION_DEFINITION* pApplicationDefinition = state.Allocator.Create(
			APPLICATION_DEFINITION
			{
				.Name = applicationName,
				.StartGroupNames = stageGroupLists["start"].second,
				.UpdateGroupNames = stageGroupLists["update"].second,
				.EndGroupNames = stageGroupLists["end"].second
			}
		);

		state.NamedNodes.ApplicationDefinitions[applicationName] = pApplicationDefinition;

		return pApplicationDefinition;
	}

#pragma endregion

#pragma region Definitions

	DEFINITION* parse_DEFINITION(ParsingState& state)
	{
		// TODO
	}

#pragma endregion


	NamedNodes Parse(const TokenBuffers& tokenBuffers)
	{
		ParsingState state(tokenBuffers);
		
		while (state.TokenIter.HasNext())
		{
			switch (state.TokenIter.GetKind())
			{
			case TokenKind::group_keyword:
				parse<NAMED_GROUP_DEFINITION>(state);

			case TokenKind::system_keyword:
				parse<NAMED_SYSTEM_DEFINITION>(state);

			case TokenKind::query_keyword:
				parse<NAMED_QUERY_DEFINITION>(state);

			case TokenKind::type_keyword:
				parse<NAMED_TYPE_DEFINITION>(state);

			case TokenKind::function_keyword:
				parse<NAMED_FUNCTION_DEFINITION>(state);

			case TokenKind::extern_keyword:
				parse<EXTERN_FUNCTION_DEFINITION>(state);

			case TokenKind::application_keyword:
				parse<APPLICATION_DEFINITION>(state);

			default:
				break;
			}
		}

		return state.NamedNodes;
	}
}