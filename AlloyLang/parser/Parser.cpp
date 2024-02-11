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

#pragma region Literals

	template <>
	NodeID parse<LITERAL>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		TokenID literalTokenID = iter.GetCurrentID();

		LITERAL::Type kind;

		switch (iter.GetKind())
		{
		case TokenKind::boolean_literal:
			kind = LITERAL::Type::Boolean;
			break;

		case TokenKind::character_literal:
			kind = LITERAL::Type::Character;
			break;

		case TokenKind::integer_literal:
			kind = LITERAL::Type::Integer;
			break;

		case TokenKind::float_literal:
			kind = LITERAL::Type::Float;
			break;

		case TokenKind::string_literal:
			kind = LITERAL::Type::String;
			break;

		default:
			unexpectedToken(iter, { "literal" });
			return ERROR_NODE_ID;
		}

		// consume literal
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "statement" });
			return ERROR_NODE_ID;
		}

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::LITERAL,
				.Literal = LITERAL
				{
					.Kind = kind,
					.InfoTokenID = literalTokenID
				}
			},
			literalTokenID
		);
	}

#pragma endregion

#pragma region Identifiers

	template <>
	NodeID parse<IDENTIFIER>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		// check that we have an identifier
		if (iter.GetKind() != TokenKind::identifier)
		{
			unexpectedToken(iter, { "identifier" });
			return ERROR_NODE_ID;
		}

		TokenID identifierTokenID = iter.GetCurrentID();

		// consume identifier
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "statement" });
			return ERROR_NODE_ID;
		}

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::IDENTIFIER,
				.Identifier = IDENTIFIER
				{
					.IdentifierTokenID = identifierTokenID
				}
			},
			identifierTokenID
		);
	}

	template <>
	NodeID parse<TYPE_IDENTIFIER>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		// check for reference or pointer modifier
		TYPE_IDENTIFIER::Modifier modifier = TYPE_IDENTIFIER::Modifier::None;

		if (iter.GetValue() == "&")
		{
			modifier = TYPE_IDENTIFIER::Modifier::Reference;

			// consume & token
			if (!iter.Next())
			{
				unexpectedEndOfFile(iter, { "type identifier" });
				return ERROR_NODE_ID;
			}
		}

		else if (iter.GetValue() == "*")
		{
			modifier = TYPE_IDENTIFIER::Modifier::Pointer;

			// consume * token
			if (!iter.Next())
			{
				unexpectedEndOfFile(iter, { "type identifier" });
				return ERROR_NODE_ID;
			}
		}

		// we should now have an identifier or an open square bracket
		if (iter.GetKind() != TokenKind::identifier && iter.GetKind() != TokenKind::open_bracket)
		{
			unexpectedToken(iter, { "identifier", "[" });
			return ERROR_NODE_ID;
		}

		NodeID typeIdentifierID = ERROR_NODE_ID;
		NodeID arraySizeID = ERROR_NODE_ID;

		if (iter.GetKind() == TokenKind::identifier)
		{
			typeIdentifierID = parse<IDENTIFIER>(nodeBuffers, iter);

			if (typeIdentifierID == ERROR_NODE_ID)
			{
				return ERROR_NODE_ID;
			}
		}

		else if (iter.GetKind() == TokenKind::open_bracket)
		{
			// consume opening square bracket
			if (!iter.Next())
			{
				unexpectedEndOfFile(iter, { "type identifier" });
				return ERROR_NODE_ID;
			}

			// parse array element type
			typeIdentifierID = parse<TYPE_IDENTIFIER>(nodeBuffers, iter);

			if (typeIdentifierID == ERROR_NODE_ID)
			{
				return ERROR_NODE_ID;
			}

			// check if the array has a known size
			if (iter.GetKind() == TokenKind::semicolon)
			{
				// consume semicolon
				if (!iter.Next())
				{
					unexpectedEndOfFile(iter, { "type identifier" });
					return ERROR_NODE_ID;
				}

				// check that we have an integer literal
				if (iter.GetKind() != TokenKind::integer_literal)
				{
					unexpectedToken(iter, { "integer literal" });
					return ERROR_NODE_ID;
				}

				// parse array size
				arraySizeID = parse<LITERAL>(nodeBuffers, iter);

				if (arraySizeID == ERROR_NODE_ID)
				{
					return ERROR_NODE_ID;
				}
			}

			// check for closing square bracket
			if (iter.GetKind() != TokenKind::close_bracket)
			{
				unexpectedToken(iter, { "]" });
				return ERROR_NODE_ID;
			}

			// consume closing square bracket
			if (!iter.Next())
			{
				unexpectedEndOfFile(iter, { "type identifier" });
				return ERROR_NODE_ID;
			}
		}

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::TYPE_IDENTIFIER,
				.TypeIdentifier = TYPE_IDENTIFIER
				{
					.Mod = modifier,
					.ArraySizeID = arraySizeID,
					.TypeIdentifierID = typeIdentifierID
				}
			},
			iter.GetCurrentID()
		);
	}

#pragma endregion

#pragma region Declarations

	template <>
	NodeID parse<TYPE_DECLARATION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		TYPE_DECLARATION::Type type = TYPE_DECLARATION::Type::Copy;

		if (iter.GetKind() == TokenKind::constant_keyword)
		{
			type = TYPE_DECLARATION::Type::Constant;

			// consume const keyword
			if (!iter.Next())
			{
				unexpectedEndOfFile(iter, { "identifier" });
				return ERROR_NODE_ID;
			}
		}

		else if (iter.GetKind() == TokenKind::variable_keyword)
		{
			type = TYPE_DECLARATION::Type::Variable;

			// consume var keyword
			if (!iter.Next())
			{
				unexpectedEndOfFile(iter, { "type identifier" });
				return ERROR_NODE_ID;
			}
		}

		// otherwise we must have a type identifier
		else if (iter.GetKind() != TokenKind::identifier)
		{
			unexpectedToken(iter, { "const", "var", "type identifier" });
			return ERROR_NODE_ID;
		}

		// parse identifier
		TokenID errorInfo = iter.GetCurrentID();
		NodeID identifierID = parse<TYPE_IDENTIFIER>(nodeBuffers, iter);

		if (identifierID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::TYPE_DECLARATION,
				.TypeDeclaration = TYPE_DECLARATION
				{
					.Kind = type,
					.TypeIdentifierID = identifierID
				}
			},
			errorInfo
		);
	}

	template <>
	NodeID parse<VALUE_DECLARATION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		VALUE_DECLARATION::Type type;

		// check for const or var keyword
		if (iter.GetKind() == TokenKind::constant_keyword)
		{
			type = VALUE_DECLARATION::Type::Constant;
		}

		else if (iter.GetKind() == TokenKind::variable_keyword)
		{
			type = VALUE_DECLARATION::Type::Variable;
		}

		else
		{
			unexpectedToken(iter, { "const", "var" });
			return ERROR_NODE_ID;
		}


		// consume const or var keyword
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "identifier" });
			return ERROR_NODE_ID;
		}

		// parse identifier
		TokenID errorInfo = iter.GetCurrentID();
		NodeID identifierID = parse<IDENTIFIER>(nodeBuffers, iter);

		if (identifierID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// check for colon
		if (iter.GetKind() != TokenKind::colon)
		{
			unexpectedToken(iter, { ":" });
			return ERROR_NODE_ID;
		}

		// consume colon
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "type" });
			return ERROR_NODE_ID;
		}

		// parse type
		NodeID typeID = parse<TYPE_IDENTIFIER>(nodeBuffers, iter);

		if (typeID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::VALUE_DECLARATION,
				.ValueDeclaration = VALUE_DECLARATION
				{
					.Kind = type,
					.IdentifierID = identifierID,
					.TypeIdentifierID = typeID
				}
			},
			errorInfo
		);
	}

	template <>
	NodeID parse<FUNCTION_DECLARATION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		if (iter.GetKind() != TokenKind::function_keyword)
		{
			unexpectedToken(iter, { "fn" });
			return ERROR_NODE_ID;
		}

		TokenID errorInfo = iter.GetCurrentID();

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

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::FUNCTION_DECLARATION,
				.FunctionDeclaration = FUNCTION_DECLARATION
				{
					.IdentifierID = identifierID,
					.ParameterIDs = parameterIDs,
					.ReturnTypeID = returnTypeID
				}
			},
			errorInfo
		);
	}

#pragma endregion

#pragma region Expressions

	template <>
	NodeID parse<EXPRESSION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter);

	template <>
	NodeID parse<UNARY_EXPRESSION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter);

	template <>
	NodeID parse<PRIMARY_EXPRESSION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter);

	template <>
	NodeID parse<POINTER_INITIALIZER_EXPRESSION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		// check for new keyword
		if (iter.GetKind() != TokenKind::new_keyword)
		{
			unexpectedToken(iter, { "new" });
			return ERROR_NODE_ID;
		}

		TokenID errorInfo = iter.GetCurrentID();

		// consume new keyword
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "expression" });
			return ERROR_NODE_ID;
		}

		NodeID valueID = ERROR_NODE_ID;
		NodeID sizeID = ERROR_NODE_ID;

		if (iter.GetKind() == TokenKind::open_bracket)
		{
			// consume opening square bracket
			if (!iter.Next())
			{
				unexpectedEndOfFile(iter, { "expression" });
				return ERROR_NODE_ID;
			}

			// parse value expression
			valueID = parse<EXPRESSION>(nodeBuffers, iter);

			if (valueID == ERROR_NODE_ID)
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

			// parse size expression
			sizeID = parse<EXPRESSION>(nodeBuffers, iter);

			if (sizeID == ERROR_NODE_ID)
			{
				return ERROR_NODE_ID;
			}

			// check for closing square bracket
			if (iter.GetKind() != TokenKind::close_bracket)
			{
				unexpectedToken(iter, { "]" });
				return ERROR_NODE_ID;
			}

			// consume closing square bracket
			if (!iter.Next())
			{
				unexpectedEndOfFile(iter, { "expression" });
				return ERROR_NODE_ID;
			}
		}

		else
		{
			// parse value expression
			valueID = parse<EXPRESSION>(nodeBuffers, iter);

			if (valueID == ERROR_NODE_ID)
			{
				return ERROR_NODE_ID;
			}
		}

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::POINTER_INITIALIZER_EXPRESSION,
				.PointerInitializerExpression = POINTER_INITIALIZER_EXPRESSION
				{
					.ValueID = valueID,
					.CountID = sizeID
				}
			},
			errorInfo
		);
	}

	template <>
	NodeID parse<INITIALIZER_LIST_EXPRESSION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		// check for opening brace
		if (iter.GetKind() != TokenKind::open_brace)
		{
			unexpectedToken(iter, { "{" });
			return ERROR_NODE_ID;
		}

		TokenID errorInfo = iter.GetCurrentID();

		// consume opening brace
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "expression" });
			return ERROR_NODE_ID;
		}

		// parse expressions
		VectorRef<NodeID> expressionIDs = nodeBuffers.CreateNodeIDVector();

		while (iter.GetKind() != TokenKind::close_brace)
		{
			NodeID expressionID = parse<EXPRESSION>(nodeBuffers, iter);

			if (expressionID == ERROR_NODE_ID)
			{
				return ERROR_NODE_ID;
			}

			expressionIDs.push_back(expressionID);

			if (iter.GetKind() == TokenKind::comma)
			{
				if (!iter.Next())
				{
					unexpectedEndOfFile(iter, { "expression" });
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
			unexpectedEndOfFile(iter, { "expression" });
			return ERROR_NODE_ID;
		}

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::INITIALIZER_LIST_EXPRESSION,
				.InitializerListExpression = INITIALIZER_LIST_EXPRESSION
				{
					.ValueIDs = expressionIDs
				}
			},
			errorInfo
		);
	}

	template <>
	NodeID parse<VALUE_DEFINITION_EXPRESSION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
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

		TokenID operatorTokenID = iter.GetCurrentID();

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

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::VALUE_DEFINITION_EXPRESSION,
				.ValueDefinitionExpression = VALUE_DEFINITION_EXPRESSION
				{
					.ValueDeclarationID = valueDeclarationID,
					.ValueID = expressionID
				}
			},
			operatorTokenID
		);
	}

	template <>
	NodeID parse<ARRAY_ACCESS_EXPRESSION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		TokenID errorInfo = iter.GetCurrentID();

		// parse array identifier
		NodeID arrayIdentifierID = parse<IDENTIFIER>(nodeBuffers, iter);

		if (arrayIdentifierID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// check for opening square bracket
		if (iter.GetKind() != TokenKind::open_bracket)
		{
			unexpectedToken(iter, { "[" });
			return ERROR_NODE_ID;
		}

		// consume opening square bracket
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "expression" });
			return ERROR_NODE_ID;
		}

		// parse index expression
		NodeID indexExpressionID = parse<EXPRESSION>(nodeBuffers, iter);

		if (indexExpressionID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// check for closing square bracket
		if (iter.GetKind() != TokenKind::close_bracket)
		{
			unexpectedToken(iter, { "]" });
			return ERROR_NODE_ID;
		}

		// consume closing square bracket
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "expression" });
			return ERROR_NODE_ID;
		}

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::ARRAY_ACCESS_EXPRESSION,
				.ArrayAccessExpression = ARRAY_ACCESS_EXPRESSION
				{
					.ArrayIdentifierID = arrayIdentifierID,
					.IndexExpressionID = indexExpressionID
				}
			},
			errorInfo
		);
	}

	template <>
	NodeID parse<FUNCTION_CALL_EXPRESSION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		TokenID errorInfo = iter.GetCurrentID();
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
			unexpectedEndOfFile(iter, { "expression", ")" });
			return ERROR_NODE_ID;
		}

		// parse arguments
		VectorRef<NodeID> argumentIDs = nodeBuffers.CreateNodeIDVector();

		while (iter.GetKind() != TokenKind::close_paren)
		{
			NodeID argumentID = parse<EXPRESSION>(nodeBuffers, iter);

			if (argumentID == ERROR_NODE_ID)
			{
				return ERROR_NODE_ID;
			}

			argumentIDs.push_back(argumentID);

			if (iter.GetKind() == TokenKind::comma)
			{
				if (!iter.Next())
				{
					unexpectedEndOfFile(iter, { "expression" });
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
			unexpectedEndOfFile(iter, { ";" });
			return ERROR_NODE_ID;
		}

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::FUNCTION_CALL_EXPRESSION,
				.FunctionCallExpression = FUNCTION_CALL_EXPRESSION
				{
					.IdentifierID = identifierID,
					.ArgumentIDs = argumentIDs
				}
			},
			errorInfo
		);
	}

	template <>
	NodeID parse<ENCLOSED_EXPRESSION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		if (iter.GetKind() != TokenKind::open_paren)
		{
			unexpectedToken(iter, { "(" });
			return ERROR_NODE_ID;
		}

		TokenID errorInfo = iter.GetCurrentID();

		// consume opening parenthesis
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

		// check for closing parenthesis
		if (iter.GetKind() != TokenKind::close_paren)
		{
			unexpectedToken(iter, { ")" });
			return ERROR_NODE_ID;
		}

		// consume closing parenthesis
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "expression" });
			return ERROR_NODE_ID;
		}

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::ENCLOSED_EXPRESSION,
				.EnclosedExpression = ENCLOSED_EXPRESSION
				{
					.ExpressionID = expressionID
				}
			},
			errorInfo
		);
	}

	template <>
	NodeID parse<BINARY_EXPRESSION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		// handles *, / and %
		auto tryParseMultiplicativeExpression = [&](TokenBuffers::Iterator& iter) -> NodeID
			{
				NodeID left = parse<UNARY_EXPRESSION>(nodeBuffers, iter);

				if (left == ERROR_NODE_ID)
					return ERROR_NODE_ID;

				while (iter.GetKind() == TokenKind::multiplicative_operator)
				{
					TokenID opTokenID = iter.GetCurrentID();

					// consume operator
					if (!iter.Next())
					{
						unexpectedEndOfFile(iter, { "expression" });
						return ERROR_NODE_ID;
					}

					NodeID right = parse<UNARY_EXPRESSION>(nodeBuffers, iter);

					if (right == ERROR_NODE_ID)
						return ERROR_NODE_ID;

					left = nodeBuffers.CreateNode(
						Node{
							.Kind = NodeKind::BINARY_EXPRESSION,
							.BinaryExpression = BINARY_EXPRESSION
							{
								.OperatorTokenID = opTokenID,
								.LeftID = left,
								.RightID = right
							}
						},
						opTokenID
					);
				}

				return left;
			};

		// handles + and -
		auto tryParseAdditiveExpression = [&](TokenBuffers::Iterator& iter) -> NodeID
			{
				NodeID left = tryParseMultiplicativeExpression(iter);

				if (left == ERROR_NODE_ID)
					return ERROR_NODE_ID;

				while (iter.GetKind() == TokenKind::additive_operator)
				{
					TokenID opTokenID = iter.GetCurrentID();

					// consume operator
					if (!iter.Next())
					{
						unexpectedEndOfFile(iter, { "expression" });
						return ERROR_NODE_ID;
					}

					NodeID right = tryParseMultiplicativeExpression(iter);

					if (right == ERROR_NODE_ID)
						return ERROR_NODE_ID;

					left = nodeBuffers.CreateNode(
						Node{
							.Kind = NodeKind::BINARY_EXPRESSION,
							.BinaryExpression = BINARY_EXPRESSION
							{
								.OperatorTokenID = opTokenID,
								.LeftID = left,
								.RightID = right
							}
						},
						opTokenID
					);
				}

				return left;
			};

		// handles ==, !=, <, <=, >, >=
		auto tryParseRelationalExpression = [&](TokenBuffers::Iterator& iter) -> NodeID
			{
				NodeID left = tryParseAdditiveExpression(iter);

				if (left == ERROR_NODE_ID)
					return ERROR_NODE_ID;

				while (iter.GetKind() == TokenKind::relational_operator)
				{
					TokenID opTokenID = iter.GetCurrentID();

					// consume operator
					if (!iter.Next())
					{
						unexpectedEndOfFile(iter, { "expression" });
						return ERROR_NODE_ID;
					}

					NodeID right = tryParseAdditiveExpression(iter);

					if (right == ERROR_NODE_ID)
						return ERROR_NODE_ID;

					left = nodeBuffers.CreateNode(
						Node{
							.Kind = NodeKind::BINARY_EXPRESSION,
							.BinaryExpression = BINARY_EXPRESSION
							{
								.OperatorTokenID = opTokenID,
								.LeftID = left,
								.RightID = right
							}
						},
						opTokenID
					);
				}

				return left;
			};

		// handles && and ||
		auto tryParseLogicalExpression = [&](TokenBuffers::Iterator& iter) -> NodeID
			{
				NodeID left = tryParseRelationalExpression(iter);

				if (left == ERROR_NODE_ID)
					return ERROR_NODE_ID;

				while (iter.GetKind() == TokenKind::logical_operator)
				{
					TokenID opTokenID = iter.GetCurrentID();

					// consume operator
					if (!iter.Next())
					{
						unexpectedEndOfFile(iter, { "expression" });
						return ERROR_NODE_ID;
					}

					NodeID right = tryParseRelationalExpression(iter);

					if (right == ERROR_NODE_ID)
						return ERROR_NODE_ID;

					left = nodeBuffers.CreateNode(
						Node{
							.Kind = NodeKind::BINARY_EXPRESSION,
							.BinaryExpression = BINARY_EXPRESSION
							{
								.OperatorTokenID = opTokenID,
								.LeftID = left,
								.RightID = right
							}
						},
						opTokenID
					);
				}

				return left;
			};

		// handles =
		auto tryParseAssignmentExpression = [&](TokenBuffers::Iterator& iter) -> NodeID
			{
				NodeID left = tryParseLogicalExpression(iter);

				if (left == ERROR_NODE_ID)
					return ERROR_NODE_ID;

				if (iter.GetKind() == TokenKind::assignment_operator)
				{
					TokenID opTokenID = iter.GetCurrentID();

					// consume operator
					if (!iter.Next())
					{
						unexpectedEndOfFile(iter, { "expression" });
						return ERROR_NODE_ID;
					}

					NodeID right = tryParseLogicalExpression(iter);

					if (right == ERROR_NODE_ID)
						return ERROR_NODE_ID;

					left = nodeBuffers.CreateNode(
						Node{
							.Kind = NodeKind::BINARY_EXPRESSION,
							.BinaryExpression = BINARY_EXPRESSION
							{
								.OperatorTokenID = opTokenID,
								.LeftID = left,
								.RightID = right
							}
						},
						opTokenID
					);
				}

				return left;
			};

		return tryParseAssignmentExpression(iter);
	}

	template <>
	NodeID parse<UNARY_EXPRESSION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		TokenID errorInfo = iter.GetCurrentID();

		// check for unary operator
		if (iter.GetKind() != TokenKind::unary_operator && iter.GetKind() != TokenKind::reference && iter.GetKind() != TokenKind::additive_operator)
		{
			// try to parse primary expression
			return parse<PRIMARY_EXPRESSION>(nodeBuffers, iter);
		}

		TokenID operatorTokenID = iter.GetCurrentID();

		// consume unary operator
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "expression" });
			return ERROR_NODE_ID;
		}

		// parse primary expression
		NodeID primaryExpressionID = parse<PRIMARY_EXPRESSION>(nodeBuffers, iter);

		if (primaryExpressionID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::UNARY_EXPRESSION,
				.UnaryExpression = UNARY_EXPRESSION
				{
					.OperatorTokenID = operatorTokenID,
					.OperandID = primaryExpressionID
				}
			},
			errorInfo
		);
	}

	template <>
	NodeID parse<PRIMARY_EXPRESSION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		switch (iter.GetKind())
		{
		case TokenKind::new_keyword:
			return parse<POINTER_INITIALIZER_EXPRESSION>(nodeBuffers, iter);

		case TokenKind::open_brace:
			return parse<INITIALIZER_LIST_EXPRESSION>(nodeBuffers, iter);

		case TokenKind::constant_keyword:
		case TokenKind::variable_keyword:
			return parse<VALUE_DEFINITION_EXPRESSION>(nodeBuffers, iter);

		case TokenKind::identifier:
		{
			// check if next token is opening parenthesis
			if (iter.Next() && iter.GetKind() == TokenKind::open_paren)
			{
				iter.Previous();

				return parse<FUNCTION_CALL_EXPRESSION>(nodeBuffers, iter);
			}

			// check if next token is opening square bracket
			if (iter.GetKind() == TokenKind::open_bracket)
			{
				iter.Previous();

				return parse<ARRAY_ACCESS_EXPRESSION>(nodeBuffers, iter);
			}

			iter.Previous();
			return parse<IDENTIFIER>(nodeBuffers, iter);
		}

		case TokenKind::boolean_literal:
		case TokenKind::character_literal:
		case TokenKind::integer_literal:
		case TokenKind::float_literal:
		case TokenKind::string_literal:
			return parse<LITERAL>(nodeBuffers, iter);

		case TokenKind::open_paren:
			return parse<ENCLOSED_EXPRESSION>(nodeBuffers, iter);

		default:
			unexpectedToken(iter, { "identifier", "literal", "new", "(" });
			return ERROR_NODE_ID;
		}
	}

	template <>
	NodeID parse<ASSIGNMENT_EXPRESSION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		NodeID identifierID = parse<IDENTIFIER>(nodeBuffers, iter);

		if (identifierID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// check for assignment operator
		if (iter.GetKind() != TokenKind::assignment_operator)
		{
			unexpectedToken(iter, { "=" });
			return ERROR_NODE_ID;
		}

		TokenID operatorTokenID = iter.GetCurrentID();

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

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::ASSIGNMENT_EXPRESSION,
				.AssignmentExpression = ASSIGNMENT_EXPRESSION
				{
					.IdentifierID = identifierID,
					.OperatorTokenID = operatorTokenID,
					.ValueID = expressionID
				}
			},
			operatorTokenID
		);
	}

	template <>
	NodeID parse<EXPRESSION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		return parse<BINARY_EXPRESSION>(nodeBuffers, iter);
	}

#pragma endregion

#pragma region Statements

	template <>
	NodeID parse<STATEMENT>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter);

	template <>
	NodeID parse<VALUE_DEFINITION_STATEMENT>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		TokenID errorInfo = iter.GetCurrentID();
		NodeID valueDefinitionExpressionID = parse<VALUE_DEFINITION_EXPRESSION>(nodeBuffers, iter);

		if (valueDefinitionExpressionID == ERROR_NODE_ID)
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
			unexpectedEndOfFile(iter, { "}" });
			return ERROR_NODE_ID;
		}

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::VALUE_DEFINITION_STATEMENT,
				.ValueDefinitionStatement = VALUE_DEFINITION_STATEMENT
				{
					.ValueDefinitionExpressionID = valueDefinitionExpressionID
				}
			},
			errorInfo
		);
	}

	template <>
	NodeID parse<ASSIGNMENT_STATEMENT>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		TokenID errorInfo = iter.GetCurrentID();

		// parse assignment expression
		NodeID assignmentExpressionID = parse<ASSIGNMENT_EXPRESSION>(nodeBuffers, iter);

		if (assignmentExpressionID == ERROR_NODE_ID)
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
			unexpectedEndOfFile(iter, { "}" });
			return ERROR_NODE_ID;
		}

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::ASSIGNMENT_STATEMENT,
				.AssignmentStatement = ASSIGNMENT_STATEMENT
				{
					.AssignmentExpressionID = assignmentExpressionID
				}
			},
			errorInfo
		);
	}

	template <>
	NodeID parse<FUNCTION_CALL_STATEMENT>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		TokenID errorInfo = iter.GetCurrentID();
		NodeID functionCallExpressionID = parse<FUNCTION_CALL_EXPRESSION>(nodeBuffers, iter);

		if (functionCallExpressionID == ERROR_NODE_ID)
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
			unexpectedEndOfFile(iter, { "}" });
			return ERROR_NODE_ID;
		}

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::FUNCTION_CALL_STATEMENT,
				.FunctionCallStatement = FUNCTION_CALL_STATEMENT
				{
					.FunctionCallExpressionID = functionCallExpressionID
				}
			},
			errorInfo
		);
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

		TokenID errorInfo = iter.GetCurrentID();

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
			},
			errorInfo
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

		TokenID errorInfo = iter.GetCurrentID();

		// consume while keyword
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "(" });
			return ERROR_NODE_ID;
		}

		// parse condition
		NodeID conditionID = parse<ENCLOSED_EXPRESSION>(nodeBuffers, iter);

		if (conditionID == ERROR_NODE_ID)
		{
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
			},
			errorInfo
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

		TokenID errorInfo = iter.GetCurrentID();

		// consume if keyword
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "(" });
			return ERROR_NODE_ID;
		}

		// parse condition
		NodeID conditionID = parse<ENCLOSED_EXPRESSION>(nodeBuffers, iter);

		if (conditionID == ERROR_NODE_ID)
		{
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
			},
			errorInfo
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

		TokenID errorInfo = iter.GetCurrentID();

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
		(void)iter.Next();

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::BLOCK_STATEMENT,
				.BlockStatement = BLOCK_STATEMENT
				{
					.StatementIDs = statementIDs
				}
			},
			errorInfo
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

		TokenID errorInfo = iter.GetCurrentID();

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
			},
			errorInfo
		);
	}

	template <>
	NodeID parse<STATEMENT>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		switch (iter.GetKind())
		{
		case TokenKind::constant_keyword:
		case TokenKind::variable_keyword:
			return parse<VALUE_DEFINITION_STATEMENT>(nodeBuffers, iter);

		case TokenKind::identifier:
		{
			// check if next token is opening parenthesis
			if (iter.Next() && iter.GetKind() == TokenKind::open_paren)
			{
				iter.Previous();

				return parse<FUNCTION_CALL_STATEMENT>(nodeBuffers, iter);
			}

			iter.Previous();

			return parse<ASSIGNMENT_STATEMENT>(nodeBuffers, iter);
		}

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
			return ERROR_NODE_ID;
		}
	}

#pragma endregion

#pragma region Definitions

	template <>
	NodeID parse<EXTERN_DEFINITION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		// check for extern keyword
		if (iter.GetKind() != TokenKind::extern_keyword)
		{
			unexpectedToken(iter, { "extern" });
			return ERROR_NODE_ID;
		}

		TokenID errorInfo = iter.GetCurrentID();

		// consume extern keyword
		if (!iter.Next())
		{
			unexpectedEndOfFile(iter, { "function declaration" });
			return ERROR_NODE_ID;
		}

		// parse function declaration
		NodeID functionDeclarationID = parse<FUNCTION_DECLARATION>(nodeBuffers, iter);

		if (functionDeclarationID == ERROR_NODE_ID)
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
			unexpectedEndOfFile(iter, { ";" });
			return ERROR_NODE_ID;
		}

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::EXTERN_DEFINITION,
				.ExternDefinition = EXTERN_DEFINITION
				{
					.FunctionDeclarationID = functionDeclarationID
				}
			},
			errorInfo
		);
	}

	template <>
	NodeID parse<VALUE_DEFINITION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		TokenID errorInfo = iter.GetCurrentID();
		NodeID valueDefinitionExpressionID = parse<VALUE_DEFINITION_EXPRESSION>(nodeBuffers, iter);

		if (valueDefinitionExpressionID == ERROR_NODE_ID)
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
					.ValueDefinitionExpressionID = valueDefinitionExpressionID
				}
			},
			errorInfo
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

		TokenID errorInfo = iter.GetCurrentID();

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

			// consume semicolon
			if (!iter.Next())
			{
				unexpectedEndOfFile(iter, { "struct member" });
				return ERROR_NODE_ID;
			}

			memberIDs.push_back(memberID);
		}

		// consume closing brace
		(void)iter.Next();

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::STRUCT_DEFINITION,
				.StructDefinition = STRUCT_DEFINITION
				{
					.IdentifierID = identifierID,
					.MemberIDs = memberIDs
				}
			},
			errorInfo
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

		TokenID errorInfo = iter.GetCurrentID();

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
		(void)iter.Next();

		return nodeBuffers.CreateNode(
			Node
			{
				.Kind = NodeKind::ENUM_DEFINITION,
				.EnumDefinition = ENUM_DEFINITION
				{
					.IdentifierID = identifierID,
					.MemberIDs = memberIDs
				}
			},
			errorInfo
		);

	}

	template <>
	NodeID parse<FUNCTION_DEFINITION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		TokenID errorInfo = iter.GetCurrentID();

		NodeID declarationID = parse<FUNCTION_DECLARATION>(nodeBuffers, iter);

		if (declarationID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
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
					.FunctionDeclarationID = declarationID,
					.BodyID = bodyID
				}
			},
			errorInfo
		);
	}

	template <>
	NodeID parse<DEFINITION>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		switch (iter.GetKind())
		{
		case TokenKind::extern_keyword:
			return parse<EXTERN_DEFINITION>(nodeBuffers, iter);

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
		TokenID errorInfo = iter.GetCurrentID();
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
			},
			errorInfo
		);
	}

#pragma endregion

	template <>
	NodeID parse<MODULE>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		TokenID errorInfo = iter.GetCurrentID();

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
			},
			errorInfo
		);
	}

	template <>
	NodeID parse<PROGRAM>(NodeBuffers& nodeBuffers, TokenBuffers::Iterator& iter)
	{
		TokenID errorInfo = iter.GetCurrentID();

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
			},
			errorInfo
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
