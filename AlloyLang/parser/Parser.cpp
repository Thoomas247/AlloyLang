#include "Parser.hpp"

namespace AlloyCompiler
{

#pragma region Util

	static std::string tokenKindVectorToString(const std::vector<TokenKind>& tokens)
	{
		if (tokens.empty())
		{
			return "";
		}

		if (tokens.size() == 1)
		{
			return TOKEN_KIND_NAMES.at(tokens[0]);
		}

		std::string result;

		for (size_t i = 0; i < tokens.size() - 1; ++i)
		{
			result += TOKEN_KIND_NAMES.at(tokens[i]) + ", ";
		}

		result += "or " + TOKEN_KIND_NAMES.at(tokens[tokens.size() - 1]);

		return result;
	}

	static std::string stringVectorToString(const std::vector<std::string>& strings)
	{
		if (strings.empty())
		{
			return "";
		}

		if (strings.size() == 1)
		{
			return "'" + strings[0] + "'";
		}

		std::string result;

		for (size_t i = 0; i < strings.size() - 1; ++i)
		{
			result += "'" + strings[i] + "'" + ", ";
		}

		result += "or '" + strings[strings.size() - 1] + "'";

		return result;
	}

	template<typename ...Args>
	constexpr void logErrorAtPosition(TokenBuffers::Iterator& iter, const std::string& format, Args && ...args)
	{
		Log::Error("Error at location ({0} : {1}):", iter.GetLocation().Line, iter.GetLocation().Column);
		Log::Error("\t{0}", iter.GetLine());
		Log::Error("\t{0}^", std::string(iter.GetLocation().Column - 1, ' '));
		Log::Error("\t{0}{1}", std::string(iter.GetLocation().Column - 1, ' '), std::vformat(format, std::make_format_args(args...)));
	}

	static bool checkTokenKind(TokenBuffers::Iterator& iter, const std::vector<TokenKind>& options)
	{
		for (const TokenKind option : options)
		{
			if (iter.GetKind() == option)
			{
				return true;
			}
		}

		logErrorAtPosition(iter, "Unexpected token '{0}'! Expected {1}.", iter.GetValue().ToStringView(), tokenKindVectorToString(options));
		return false;
	}

	static bool checkTokenValue(TokenBuffers::Iterator& iter, const std::vector<std::string>& options)
	{
		for (const std::string& option : options)
		{
			if (iter.GetValue().ToStringView() == option)
			{
				return true;
			}
		}

		logErrorAtPosition(iter, "Unexpected token '{0}'! Expected {1}.", iter.GetValue().ToStringView(), stringVectorToString(options));
		return false;
	}

	static void unexpectedEndOfFile(TokenBuffers::Iterator& iter, const std::vector<std::string>& expected)
	{
		logErrorAtPosition(iter, "Unexpected end of file! Expected: {0}.", stringVectorToString(expected));
	}

	static void unexpectedToken(TokenBuffers::Iterator& iter, const std::vector<std::string>& expected)
	{
		logErrorAtPosition(iter, "Unexpected token '{0}'! Expected: {1}.", iter.GetValue().ToStringView(), stringVectorToString(expected));
	}

#pragma endregion

	template <typename T>
	NodeID parse(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter) = delete;

	template <>
	NodeID parse<ASSIGNMENT_STATEMENT>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		// parse assignment expression
		NodeID assignmentExpressionID = parse<ASSIGNMENT_EXPRESSION>(nodeBuffers, iter);

		if (assignmentExpressionID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}
	}

	template <>
	NodeID parse<FOR_LOOP_STATEMENT>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		// check for for keyword
		if (iter.GetKind() != TokenKind::for_keyword)
		{
			unexpectedToken(iter, { "for" });
			return ERROR_NODE_ID;
		}

		// consume for keyword
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "(" });
			return ERROR_NODE_ID;
		}

		// check for opening parenthesis
		if (iter.GetKind() != TokenKind::open_paren)
		{
			unexpectedToken(iter, { "(" });
			return ERROR_NODE_ID;
		}

		// consume opening parenthesis
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "expression" });
			return ERROR_NODE_ID;
		}

		// parse initializer
		NodeID initializerID = parse<EXPRESSION>(nodeBuffers, iter);

		if (initializerID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// check for semicolon
		if (iter.GetKind() != TokenKind::semicolon)
		{
			unexpectedToken(iter, { ";" });
			return ERROR_NODE_ID;
		}

		// consume semicolon
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "expression" });
			return ERROR_NODE_ID;
		}

		// parse condition
		NodeID conditionID = parse<EXPRESSION>(nodeBuffers, iter);

		if (conditionID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// check for semicolon
		if (iter.GetKind() != TokenKind::semicolon)
		{
			unexpectedToken(iter, { ";" });
			return ERROR_NODE_ID;
		}

		// consume semicolon
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "expression" });
			return ERROR_NODE_ID;
		}

		// parse increment
		NodeID incrementID = parse<EXPRESSION>(nodeBuffers, iter);

		if (incrementID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// check for closing parenthesis
		if (iter.GetKind() != TokenKind::close_paren)
		{
			unexpectedToken(iter, { ")" });
			return ERROR_NODE_ID;
		}

		// consume closing parenthesis
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "statement" });
			return ERROR_NODE_ID;
		}

		// parse statement
		NodeID statementID = parse<STATEMENT>(nodeBuffers, iter);

		if (statementID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::FOR_LOOP_STATEMENT,
				.ForLoopStatement = FOR_LOOP_STATEMENT
				{
					.InitExpressionID = initializerID,
					.ConditionExpressionID = conditionID,
					.IncrementExpressionID = incrementID,
					.BodyID = statementID
				}
			}
		);
	}

	template <>
	NodeID parse<WHILE_LOOP_STATEMENT>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		// check for while keyword
		if (iter.GetKind() != TokenKind::while_keyword)
		{
			unexpectedToken(iter, { "while" });
			return ERROR_NODE_ID;
		}

		// consume while keyword
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "(" });
			return ERROR_NODE_ID;
		}

		// check for opening parenthesis
		if (iter.GetKind() != TokenKind::open_paren)
		{
			unexpectedToken(iter, { "(" });
			return ERROR_NODE_ID;
		}

		// consume opening parenthesis
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "expression" });
			return ERROR_NODE_ID;
		}

		// parse condition
		NodeID conditionID = parse<EXPRESSION>(nodeBuffers, iter);

		if (conditionID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// check for closing parenthesis
		if (iter.GetKind() != TokenKind::close_paren)
		{
			unexpectedToken(iter, { ")" });
			return ERROR_NODE_ID;
		}

		// consume closing parenthesis
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "statement" });
			return ERROR_NODE_ID;
		}

		// parse statement
		NodeID statementID = parse<STATEMENT>(nodeBuffers, iter);

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::WHILE_LOOP_STATEMENT,
				.WhileLoopStatement = WHILE_LOOP_STATEMENT
				{
					.ConditionExpressionID = conditionID,
					.BodyID = statementID
				}
			}
		);
	}

	template <>
	NodeID parse<IF_STATEMENT>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		// check for if keyword
		if (iter.GetKind() != TokenKind::if_keyword)
		{
			unexpectedToken(iter, { "if" });
			return ERROR_NODE_ID;
		}

		// consume if keyword
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "(" });
			return ERROR_NODE_ID;
		}

		// check for opening parenthesis
		if (iter.GetKind() != TokenKind::open_paren)
		{
			unexpectedToken(iter, { "(" });
			return ERROR_NODE_ID;
		}

		// consume opening parenthesis
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "expression" });
			return ERROR_NODE_ID;
		}

		// parse condition
		NodeID conditionID = parse<EXPRESSION>(nodeBuffers, iter);

		if (conditionID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// check for closing parenthesis
		if (iter.GetKind() != TokenKind::close_paren)
		{
			unexpectedToken(iter, { ")" });
			return ERROR_NODE_ID;
		}

		// consume closing parenthesis
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "statement" });
			return ERROR_NODE_ID;
		}

		// parse statement
		NodeID statementID = parse<STATEMENT>(nodeBuffers, iter);

		if (statementID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// check for else keyword
		NodeID elseStatementID = ERROR_NODE_ID;

		if (iter.GetKind() == TokenKind::else_keyword)
		{
			// consume else keyword
			if (!iter.Next())
			{
				unexpectedEndOfFile(iter, { "statement" });
				return ERROR_NODE_ID;
			}

			// parse else statement
			elseStatementID = parse<STATEMENT>(nodeBuffers, iter);

			if (elseStatementID == ERROR_NODE_ID)
			{
				return ERROR_NODE_ID;
			}
		}

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::IF_STATEMENT,
				.IfStatement = IF_STATEMENT
				{
					.ConditionExpressionID = conditionID,
					.BodyID = statementID,
					.ElseID = elseStatementID
				}
			}
		);

	}

	template <>
	NodeID parse<BLOCK_STATEMENT>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		// check for opening brace
		if (iter.GetKind() != TokenKind::open_brace)
		{
			unexpectedToken(iter, { "{" });
			return ERROR_NODE_ID;
		}

		// consume opening brace
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "statement" });
			return ERROR_NODE_ID;
		}

		// parse statements
		VectorRef<NodeID> statementIDs = nodeBuffers.CreateNodeIDVector();

		while (iter.GetKind() != TokenKind::close_brace)
		{
			NodeID statementID = parse<STATEMENT>(nodeBuffers, iter);

			if (statementID == ERROR_NODE_ID)
			{
				return ERROR_NODE_ID;
			}

			statementIDs.push_back(statementID);
		}

		// consume closing brace
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "}" });
			return ERROR_NODE_ID;
		}

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::BLOCK_STATEMENT,
				.BlockStatement = BLOCK_STATEMENT
				{
					.StatementIDs = statementIDs
				}
			}
		);
	}

	template <>
	NodeID parse<RETURN_STATEMENT>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		if (iter.GetKind() != TokenKind::return_keyword)
		{
			unexpectedToken(iter, { "return" });
			return ERROR_NODE_ID;
		}

		// consume return keyword
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "expression" });
			return ERROR_NODE_ID;
		}

		NodeID expressionID = ERROR_NODE_ID;

		if (iter.GetKind() != TokenKind::semicolon)
		{
			// parse expression
			expressionID = parse<EXPRESSION>(nodeBuffers, iter);

			if (expressionID == ERROR_NODE_ID)
			{
				return ERROR_NODE_ID;
			}
		}

		// check for semicolon
		if (iter.GetKind() != TokenKind::semicolon)
		{
			unexpectedToken(iter, { ";" });
			return ERROR_NODE_ID;
		}

		// consume semicolon
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "}" });
			return ERROR_NODE_ID;
		}

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::RETURN_STATEMENT,
				.ReturnStatement = RETURN_STATEMENT
				{
					.ExpressionID = expressionID
				}
			}
		);
	}

	template <>
	NodeID parse<STATEMENT>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		switch (iter.GetKind())
		{
		case TokenKind::identifier:
			return parse<ASSIGNMENT_STATEMENT>(nodeBuffers, iter);

		case TokenKind::for_keyword:
			return parse<FOR_LOOP_STATEMENT>(nodeBuffers, iter);

		case TokenKind::while_keyword:
			return parse<WHILE_LOOP_STATEMENT>(nodeBuffers, iter);

		case TokenKind::if_keyword:
			return parse<IF_STATEMENT>(nodeBuffers, iter);

		case TokenKind::open_brace:
			return parse<BLOCK_STATEMENT>(nodeBuffers, iter);

		case TokenKind::return_keyword:
			return parse<RETURN_STATEMENT>(nodeBuffers, iter);

		default:
			unexpectedToken(iter, { "assignment", "for", "while", "if", "{", "return" });
		}
	}

	template <>
	NodeID parse<VALUE_DEFINITION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		// parse value declaration
		NodeID valueDeclarationID = parse<VALUE_DECLARATION>(nodeBuffers, iter);

		if (valueDeclarationID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// check for assignment operator
		if (iter.GetKind() != TokenKind::assignment_operator)
		{
			unexpectedToken(iter, { "=" });
			return ERROR_NODE_ID;
		}

		// consume assignment operator
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "expression" });
			return ERROR_NODE_ID;
		}

		// parse expression
		NodeID expressionID = parse<EXPRESSION>(nodeBuffers, iter);

		if (expressionID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// check for semicolon
		if (iter.GetKind() != TokenKind::semicolon)
		{
			unexpectedToken(iter, { ";" });
			return ERROR_NODE_ID;
		}

		// consume semicolon
		(void)iter.Next();

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::VALUE_DEFINITION,
				.ValueDefinition = VALUE_DEFINITION
				{
					.ValueDeclarationID = valueDeclarationID,
					.ValueID = expressionID
				}
			}
		);
	}

	template <>
	NodeID parse<STRUCT_DEFINITION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		if (iter.GetKind() != TokenKind::struct_keyword)
		{
			unexpectedToken(iter, { "struct" });
			return ERROR_NODE_ID;
		}

		// consume struct keyword
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "struct name" });
			return ERROR_NODE_ID;
		}

		// parse struct name
		NodeID identifierID = parse<IDENTIFIER>(nodeBuffers, iter);

		if (identifierID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// check for opening brace
		if (iter.GetKind() != TokenKind::open_brace)
		{
			unexpectedToken(iter, { "{" });
			return ERROR_NODE_ID;
		}

		// consume opening brace
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "struct member" });
			return ERROR_NODE_ID;
		}

		// parse members
		VectorRef<NodeID> memberIDs = nodeBuffers.CreateNodeIDVector();

		while (iter.GetKind() != TokenKind::close_brace)
		{
			NodeID memberID = parse<VALUE_DECLARATION>(nodeBuffers, iter);

			if (memberID == ERROR_NODE_ID)
			{
				return ERROR_NODE_ID;
			}

			// check for semicolon
			if (iter.GetKind() != TokenKind::semicolon)
			{
				unexpectedToken(iter, { ";" });
				return ERROR_NODE_ID;
			}

			memberIDs.push_back(memberID);
		}

		// consume closing brace
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "}" });
			return ERROR_NODE_ID;
		}

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::STRUCT_DEFINITION,
				.StructDefinition = STRUCT_DEFINITION
				{
					.IdentifierID = identifierID,
					.MemberIDs = memberIDs
				}
			}
		);
	}

	template <>
	NodeID parse<ENUM_DEFINITION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		if (iter.GetKind() != TokenKind::enum_keyword)
		{
			unexpectedToken(iter, { "enum" });
			return ERROR_NODE_ID;
		}

		// consume enum keyword
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "enum name" });
			return ERROR_NODE_ID;
		}

		// parse enum name
		NodeID identifierID = parse<IDENTIFIER>(nodeBuffers, iter);

		if (identifierID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// check for opening brace
		if (iter.GetKind() != TokenKind::open_brace)
		{
			unexpectedToken(iter, { "{" });
			return ERROR_NODE_ID;
		}

		// consume opening brace
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "enum member" });
			return ERROR_NODE_ID;
		}

		// parse members
		VectorRef<NodeID> memberIDs = nodeBuffers.CreateNodeIDVector();

		while (iter.GetKind() != TokenKind::close_brace)
		{
			NodeID memberID = parse<IDENTIFIER>(nodeBuffers, iter);

			if (memberID == ERROR_NODE_ID)
			{
				return ERROR_NODE_ID;
			}

			memberIDs.push_back(memberID);

			if (iter.GetKind() == TokenKind::comma)
			{
				if (!iter.Next())
				{
					unexpectedEndOfFile(iter, { "identifier" });
					return ERROR_NODE_ID;
				}
			}
			else if (iter.GetKind() != TokenKind::close_brace)
			{
				unexpectedToken(iter, { ",", "}" });
				return ERROR_NODE_ID;
			}
		}

		// consume closing brace
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { ";" });
			return ERROR_NODE_ID;
		}

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::ENUM_DEFINITION,
				.EnumDefinition = ENUM_DEFINITION
				{
					.IdentifierID = identifierID,
					.MemberIDs = memberIDs
				}
			}
		);

	}

	template <>
	NodeID parse<FUNCTION_DEFINITION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		if (iter.GetKind() != TokenKind::function_keyword)
		{
			unexpectedToken(iter, { "fn" });
			return ERROR_NODE_ID;
		}

		// consume fn keyword
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "function name" });
			return ERROR_NODE_ID;
		}

		// parse function name
		NodeID identifierID = parse<IDENTIFIER>(nodeBuffers, iter);

		if (identifierID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// check for opening parenthesis
		if (iter.GetKind() != TokenKind::open_paren)
		{
			unexpectedToken(iter, { "(" });
			return ERROR_NODE_ID;
		}

		// consume opening parenthesis
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "function parameter" });
			return ERROR_NODE_ID;
		}

		// parse parameters
		VectorRef<NodeID> parameterIDs = nodeBuffers.CreateNodeIDVector();

		while (iter.GetKind() != TokenKind::close_paren)
		{
			NodeID parameterID = parse<VALUE_DECLARATION>(nodeBuffers, iter);

			if (parameterID == ERROR_NODE_ID)
			{
				return ERROR_NODE_ID;
			}

			parameterIDs.push_back(parameterID);

			if (iter.GetKind() == TokenKind::comma)
			{
				if (!iter.Next())
				{
					unexpectedEndOfFile(iter, { "function parameter" });
					return ERROR_NODE_ID;
				}
			}
			else if (iter.GetKind() != TokenKind::close_paren)
			{
				unexpectedToken(iter, { ",", ")" });
				return ERROR_NODE_ID;
			}
		}

		// consume closing parenthesis
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "->", "{" });
			return ERROR_NODE_ID;
		}

		// check for return type
		NodeID returnTypeID = ERROR_NODE_ID;
		if (iter.GetKind() == TokenKind::arrow)
		{
			// consume arrow
			if (!iter.Next())
			{
				unexpectedEndOfFile(iter, { "function body" });
				return ERROR_NODE_ID;
			}

			// parse return type
			returnTypeID = parse<TYPE_DECLARATION>(nodeBuffers, iter);

			if (returnTypeID == ERROR_NODE_ID)
			{
				return ERROR_NODE_ID;
			}
		}

		// parse body
		NodeID bodyID = parse<BLOCK_STATEMENT>(nodeBuffers, iter);

		if (bodyID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::FUNCTION_DEFINITION,
				.FunctionDefinition = FUNCTION_DEFINITION
				{
					.IdentifierID = identifierID,
					.ParameterIDs = parameterIDs,
					.ReturnTypeID = returnTypeID,
					.BodyID = bodyID
				}
			}
		);
	}

	template <>
	NodeID parse<DEFINITION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		switch (iter.GetKind())
		{
		case TokenKind::constant_keyword:
		case TokenKind::variable_keyword:
			return parse<VALUE_DEFINITION>(nodeBuffers, iter);

		case TokenKind::struct_keyword:
			return parse<STRUCT_DEFINITION>(nodeBuffers, iter);

		case TokenKind::enum_keyword:
			return parse<ENUM_DEFINITION>(nodeBuffers, iter);

		case TokenKind::function_keyword:
			return parse<FUNCTION_DEFINITION>(nodeBuffers, iter);

		default:
			unexpectedToken(iter, { "const", "var", "struct", "enum", "fn" });
			return ERROR_NODE_ID;
		}
	}

	template <>
	NodeID parse<QUALIFIED_DEFINITION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		// check for qualifier
		QUALIFIED_DEFINITION::Qualifier visibility = QUALIFIED_DEFINITION::Qualifier::Default;

		if (iter.GetKind() == TokenKind::export_label || iter.GetKind() == TokenKind::public_label)
		{
			if (iter.GetKind() == TokenKind::export_label)
			{
				visibility = QUALIFIED_DEFINITION::Qualifier::Export;
			}
			else if (iter.GetKind() == TokenKind::public_label)
			{
				visibility = QUALIFIED_DEFINITION::Qualifier::Public;
			}

			if (!iter.Next())
			{
				logErrorAtPosition(iter, "Expected a definition after {0}!", iter.GetValue().ToStringView());
				return ERROR_NODE_ID;
			}
		}

		// parse definition
		NodeID definitionID = parse<DEFINITION>(nodeBuffers, iter);

		if (definitionID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::QUALIFIED_DEFINITION,
				.QualifiedDefinition = QUALIFIED_DEFINITION
				{
					.Visibility = visibility,
					.DefinitionID = definitionID
				}
			}
		);
	}

	template <>
	NodeID parse<MODULE>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		VectorRef<NodeID> qualifiedDefinitionIDs = nodeBuffers.CreateNodeIDVector();

		do
		{
			NodeID qualifiedDefinitionID = parse<QUALIFIED_DEFINITION>(nodeBuffers, iter);

			if (qualifiedDefinitionID == ERROR_NODE_ID)
			{
				return ERROR_NODE_ID;
			}

			qualifiedDefinitionIDs.push_back(qualifiedDefinitionID);
		} while (iter.HasNext());

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::MODULE,
				.Module = MODULE
				{
					.QualifiedDefinitionIDs = qualifiedDefinitionIDs
				}
			}
		);
	}

	template <>
	NodeID parse<PROGRAM>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		VectorRef<NodeID> moduleIDs = nodeBuffers.CreateNodeIDVector();

		// TODO: add support for mutliple modules

		moduleIDs.push_back(parse<MODULE>(nodeBuffers, iter));

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::PROGRAM,
				.Program = PROGRAM
				{
					.ModuleIDs = moduleIDs
				}
			}
		);
	}

	NodeBuffers Parse(const TokenBuffers& tokenBuffers)
	{
		NodeBuffers nodeBuffers;
		TokenBuffers::Iterator tokenIter = tokenBuffers.GetIterator();

		nodeBuffers.SetRootNodeID(parse<PROGRAM>(nodeBuffers, tokenIter));

		return nodeBuffers;
	}
}
