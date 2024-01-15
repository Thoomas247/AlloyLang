#include "Parser.hpp"

#include "log/Log.hpp"

namespace AlloyCompiler::Parser
{
	class TokenIterator
	{
	public:
		TokenIterator(const Tokenizer::TokenDataBuffers& buffers)
			: m_Buffers(buffers), m_CurrentTokenID(0)
		{}
		TokenIterator(const TokenIterator&) = delete;

		[[nodiscard]] bool Next()
		{
			++m_CurrentTokenID;

			if (m_CurrentTokenID >= m_Buffers.GetTokenCount())
				return false;

			return true;
		}

		[[nodiscard]] bool HasNext() const { return m_CurrentTokenID + 1 < m_Buffers.GetTokenCount(); }

		void Previous()
		{
			--m_CurrentTokenID;
		}

		const Token& CurrentToken() const
		{
			return m_Buffers.GetToken(m_CurrentTokenID);
		}

		const Location& CurrentLocation() const
		{
			return m_Buffers.GetLocation(m_CurrentTokenID);
		}

		const std::string_view& CurrentSourceView() const
		{
			return m_Buffers.GetSourceView(m_CurrentTokenID);
		}

		std::string_view GetLine() const
		{
			auto source = m_Buffers.GetSourceView();

			size_t startIndex = m_Buffers.GetLocation(m_CurrentTokenID).LineStart;
			size_t endIndex = m_Buffers.GetSourceView().size();

			// find first token on new line
			for (TokenID i = m_CurrentTokenID; i < m_Buffers.GetTokenCount(); ++i)
			{
				if (m_Buffers.GetLocation(i).Line != m_Buffers.GetLocation(m_CurrentTokenID).Line)
				{
					endIndex = m_Buffers.GetLocation(i).LineStart;
					break;
				}
			}

			return source.substr(startIndex, endIndex - startIndex - 1);
		}

	private:
		const Tokenizer::TokenDataBuffers& m_Buffers;
		TokenID m_CurrentTokenID;
	};

	template <typename... Args>
	inline constexpr void logError(TokenIterator& iter, const std::string& format, Args&&... args)
	{
		Log::Error("Error at location ({0} : {1}):", iter.CurrentLocation().Line, iter.CurrentLocation().Column);
		Log::Error("\t{0}", iter.GetLine());
		Log::Error("\t{0}^", std::string(iter.CurrentLocation().Column - 1, ' '));
		Log::Error("\t{0}{1}", std::string(iter.CurrentLocation().Column - 1, ' '), std::vformat(format, std::make_format_args(args...)));
	}

	/// <summary>
	/// TYPE_IDENTIFIER: ['&' | '*'] TYPE_NAME ;
	/// </summary>
	inline static NodeID parseTypeIdentifier(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// check for reference or pointer
		TypeIdentifier::Modifier modifier = TypeIdentifier::Modifier::None;

		if (iter.CurrentToken().Kind == TokenKind::Operator)
		{
			if (iter.CurrentToken().Value == TokenValue::Reference)
			{
				modifier = TypeIdentifier::Modifier::Reference;

				if (!iter.Next())
				{
					logError(iter, "Unexpected end of file!");
					return ERROR_NODE_ID;
				}
			}

			else if (iter.CurrentToken().Value == TokenValue::Pointer)
			{
				modifier = TypeIdentifier::Modifier::Pointer;

				if (!iter.Next())
				{
					logError(iter, "Unexpected end of file!");
					return ERROR_NODE_ID;
				}
			}
		}

		// check for identifier
		if (iter.CurrentToken().Kind != TokenKind::Identifier && iter.CurrentToken().Kind != TokenKind::BuiltInType)
		{
			logError(iter, "Expected a type identifier! Got '{0}' instead.", iter.CurrentSourceView());
			return ERROR_NODE_ID;
		}

		const auto& identifier = iter.CurrentSourceView();

		// consume the identifier
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file!");
			return ERROR_NODE_ID;
		}

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::TypeIdentifier, .TypeIdentifier = TypeIdentifier{ modifier, identifier } });
	}


#pragma region Expressions

	// forward declaration
	inline static NodeID parseExpression(TokenIterator& iter, NodeDataBuffers& buffers);

	/// <summary>
	/// FN_CALL: FN_NAME '(' [ EXPRESSION { ',' EXPRESSION } ] ')' ;
	/// </summary>
	inline static NodeID parseFunctionCall(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// assert we have an identifier
		ASSERT(iter.CurrentToken().Kind == TokenKind::Identifier, "Expected an identifier!");

		// get the identifier
		const auto& identifier = iter.CurrentSourceView();

		// consume the identifier
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected a '('.");
			return ERROR_NODE_ID;
		}

		// check that we have an opening parenthesis
		if (iter.CurrentToken().Value != TokenValue::OpenParen)
		{
			logError(iter, "Expected a '('! Got '{0}' instead.", iter.CurrentSourceView());
			return ERROR_NODE_ID;
		}

		// consume the opening parenthesis
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected an expression or ')' after '('.");
			return ERROR_NODE_ID;
		}

		// get all the arguments
		auto& argNodeIDs = buffers.CreateNodeIDList();

		while (iter.CurrentToken().Value != TokenValue::CloseParen)
		{
			// parse the expression
			NodeID expressionID = parseExpression(iter, buffers);

			// check if the expression was valid
			if (expressionID == ERROR_NODE_ID)
			{
				return ERROR_NODE_ID;
			}

			// add the expression to the list
			argNodeIDs.push_back(expressionID);

			// check for comma
			if (iter.CurrentToken().Value == TokenValue::Comma)
			{
				// consume the comma
				if (!iter.Next())
				{
					logError(iter, "Unexpected end of file! Expected an expression or ')' instead of ','.");
					return ERROR_NODE_ID;
				}
			}

			// othwerwise we must have a closing parenthesis
			else if (iter.CurrentToken().Value != TokenValue::CloseParen)
			{
				logError(iter, "Expected a ')'! Got '{0}' instead.", iter.CurrentSourceView());
				return ERROR_NODE_ID;
			}
		}

		// consume the closing parenthesis
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file!");
			return ERROR_NODE_ID;
		}

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::FunctionCall, .FunctionCall = FunctionCall{ identifier, std::move(argNodeIDs) } });
	}

	/// <summary>
	/// ENCLOSED_EXPRESSION: '(' EXPRESSION [',' EXPRESSION] ')' ;
	/// </summary>
	inline static NodeID parseEnclosedExpression(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// assert we have an opening parenthesis
		ASSERT(iter.CurrentToken().Value == TokenValue::OpenParen, "Expected '('!");

		// consume the opening parenthesis
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected an expression or ')' after '('.");
			return ERROR_NODE_ID;
		}

		auto& expressionNodeIDs = buffers.CreateNodeIDList();

		while (iter.CurrentToken().Value != TokenValue::CloseParen)
		{
			// parse the expression
			NodeID expressionID = parseExpression(iter, buffers);

			// check if the expression was valid
			if (expressionID == ERROR_NODE_ID)
			{
				return ERROR_NODE_ID;
			}

			// add the expression to the list
			expressionNodeIDs.push_back(expressionID);

			// check for comma
			if (iter.CurrentToken().Value == TokenValue::Comma)
			{
				// consume the comma
				if (!iter.Next())
				{
					logError(iter, "Unexpected end of file! Expected an expression or ')' instead of ','.");
					return ERROR_NODE_ID;
				}
			}

			// othwerwise we must have a closing parenthesis
			else if (iter.CurrentToken().Value != TokenValue::CloseParen)
			{
				logError(iter, "Expected a ')'! Got '{0}' instead.", iter.CurrentSourceView());
				return ERROR_NODE_ID;
			}
		}

		// consume the closing parenthesis
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file!");
			return ERROR_NODE_ID;
		}

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::Enclosed, .Enclosed = Enclosed{ std::move(expressionNodeIDs) } });
	}

#pragma region Literals

	/// <summary>
	/// INT_LITERAL: NUMBER ;
	/// </summary>
	inline static NodeID parseIntegerLiteral(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// assert that we have an integer literal
		ASSERT(iter.CurrentToken().Value == TokenValue::Integer, "Expected an integer literal!");

		//uint64_t value = std::stoll(iter.CurrentSourceView().data());

		uint64_t value;
		(void)std::from_chars(iter.CurrentSourceView().data(), iter.CurrentSourceView().data() + iter.CurrentSourceView().size(), value);

		// consume the integer literal
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file!");
			return ERROR_NODE_ID;
		}

		// create the node
		return buffers.CreateNode({ .Kind = NodeKind::IntegerLiteral, .IntegerLiteral = IntegerLiteral{ value } });
	}

	/// <summary>
	/// FLOAT_LITERAL: NUMBER '.' NUMBER ;
	/// </summary>
	inline static NodeID parseFloatLiteral(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// assert that we have a float literal
		ASSERT(iter.CurrentToken().Value == TokenValue::Float, "Expected a float literal!");

		//double value = std::stod(iter.CurrentSourceView().data());

		double value;
		(void)std::from_chars(iter.CurrentSourceView().data(), iter.CurrentSourceView().data() + iter.CurrentSourceView().size(), value);

		// consume the float literal
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file!");
			return ERROR_NODE_ID;
		}

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::FloatLiteral, .FloatLiteral = FloatLiteral{ value } });
	}

	/// <summary>
	/// BOOLEAN_LITERAL: 'true' | 'false' ;
	/// </summary>
	inline static NodeID parseBooleanLiteral(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// assert that we have a boolean literal
		ASSERT(iter.CurrentToken().Value == TokenValue::Bool, "Expected a boolean literal!");

		bool value = iter.CurrentSourceView() == "true";	// if kind is bool, only possible values are true and false

		// consume the boolean literal
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file!");
			return ERROR_NODE_ID;
		}

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::BooleanLiteral, .BooleanLiteral = BooleanLiteral{ value } });
	}

	/// <summary>
	/// STRING_LITERAL: '\"' { LETTER } '\"' ;
	/// </summary>
	inline static NodeID parseStringLiteral(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// assert that we have a string literal
		ASSERT(iter.CurrentToken().Value == TokenValue::String, "Expected a string literal!");

		// store the string
		const auto& string = iter.CurrentSourceView();

		// consume the string
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file!");
			return ERROR_NODE_ID;
		}

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::StringLiteral, .StringLiteral = StringLiteral{ string } });
	}

	/// <summary>
	/// CHAR_LITERAL: '\'' [ LETTER ] '\'' ;
	/// </summary>
	inline static NodeID parseCharacterLiteral(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// assert that we have a character literal
		ASSERT(iter.CurrentToken().Value == TokenValue::Character, "Expected a character literal!");

		// store the character
		char c = iter.CurrentSourceView()[0];

		// consume the character
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file!");
			return ERROR_NODE_ID;
		}

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::CharacterLiteral, .CharacterLiteral = CharacterLiteral{ c } });
	}

	/// <summary>
	/// LITERAL: INT_LITERAL | FLOAT_LITERAL | BOOLEAN_LITERAL | STRING_LITERAL | CHAR_LITERAL ;
	/// </summary>
	inline static NodeID parseLiteral(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// assert that we have a literal
		ASSERT(iter.CurrentToken().Kind == TokenKind::Literal, "Expected a literal!");

		switch (iter.CurrentToken().Value)
		{
		case TokenValue::Integer:
			return parseIntegerLiteral(iter, buffers);

		case TokenValue::Float:
			return parseFloatLiteral(iter, buffers);

		case TokenValue::Bool:
			return parseBooleanLiteral(iter, buffers);

		case TokenValue::String:
			return parseStringLiteral(iter, buffers);

		case TokenValue::Character:
			return parseCharacterLiteral(iter, buffers);

		default:
			logError(iter, "Expected a literal! Got '{0}' instead.", iter.CurrentSourceView());
			return ERROR_NODE_ID;
		}
	}

#pragma endregion

	// forward declaration
	inline static NodeID parseExpression(TokenIterator& iter, NodeDataBuffers& buffers);

	/// <summary>
	/// PRIMARY_EXPRESSION: VAR_NAME | CONST_NAME | FN_CALL | LITERAL | ENCLOSED_EXPRESSION ;
	/// </summary>
	inline static NodeID parsePrimaryExpression(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		switch (iter.CurrentToken().Kind)
		{
		case TokenKind::Identifier:
		{
			// get the identifier
			const auto& identifier = iter.CurrentSourceView();

			// consume the identifier
			if (!iter.Next())
			{
				logError(iter, "Unexpected end of file!");
				return ERROR_NODE_ID;
			}

			// check for function call
			if (iter.CurrentToken().Value == TokenValue::OpenParen)
			{
				// go back to the identifier
				iter.Previous();

				return parseFunctionCall(iter, buffers);
			}

			// otherwise we have a variable or constant
			return buffers.CreateNode(Node{ .Kind = NodeKind::MemoryAccess, .MemoryAccess = MemoryAccess{ identifier } });
		}

		case TokenKind::Literal:
			return parseLiteral(iter, buffers);

		case TokenKind::Delimiter:
		{
			if (iter.CurrentToken().Value == TokenValue::OpenParen)
			{
				return parseEnclosedExpression(iter, buffers);
			}

			logError(iter, "Expected an expression! Got '{0}' instead.", iter.CurrentSourceView());
			return ERROR_NODE_ID;
		}

		default:
			logError(iter, "Expected an expression! Got '{0}' instead.", iter.CurrentSourceView());
			return ERROR_NODE_ID;
		}
	}

	/// <summary>
	/// UNARY_EXPRESSION: UNARY_OPERATOR EXPRESSION ;
	/// </summary>
	inline static NodeID parseUnaryExpression(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// assert that we have an operator
		ASSERT(iter.CurrentToken().Kind == TokenKind::Operator, "Expected an operator!");

		// save the operator
		TokenValue op = iter.CurrentToken().Value;

		// consume the operator
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected an expression.");
			return ERROR_NODE_ID;
		}

		// parse the expression
		NodeID expressionID = parsePrimaryExpression(iter, buffers);

		// check if the expression was valid
		if (expressionID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::Unary, .Unary = Unary{ op, expressionID } });
	}

	/// <summary>
	/// BINARY_EXPRESSION: EXPRESSION BINARY_OPERATOR EXPRESSION ;
	/// </summary>
	inline static NodeID parseBinaryExpresssion(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// handles *, / and %
		auto tryParseMultiplicativeExpression = [&](TokenIterator& iter) -> NodeID
			{
				NodeID left;

				if (iter.CurrentToken().Kind == TokenKind::Operator)
				{
					left = parseUnaryExpression(iter, buffers);
				}

				else
				{
					left = parsePrimaryExpression(iter, buffers);
				}

				if (left == ERROR_NODE_ID)
					return ERROR_NODE_ID;

				while (iter.CurrentToken().Value == TokenValue::Multiply
					|| iter.CurrentToken().Value == TokenValue::Divide
					|| iter.CurrentToken().Value == TokenValue::Modulo)
				{
					TokenValue op = iter.CurrentToken().Value;

					// consume operator
					if (!iter.Next())
					{
						logError(iter, "Unexpected end of file!");
						return ERROR_NODE_ID;
					}

					NodeID right;

					if (iter.CurrentToken().Kind == TokenKind::Operator)
					{
						right = parseUnaryExpression(iter, buffers);
					}

					else
					{
						right = parsePrimaryExpression(iter, buffers);
					}

					if (right == ERROR_NODE_ID)
						return ERROR_NODE_ID;

					left = buffers.CreateNode(Node{ .Kind = NodeKind::Binary, .Binary = Binary{ op, left, right } });
				}

				return left;
			};

		// handles + and -
		auto tryParseAdditiveExpression = [&](TokenIterator& iter) -> NodeID
			{
				NodeID left = tryParseMultiplicativeExpression(iter);

				if (left == ERROR_NODE_ID)
					return ERROR_NODE_ID;

				while (iter.CurrentToken().Value == TokenValue::Plus || iter.CurrentToken().Value == TokenValue::Minus)
				{
					TokenValue op = iter.CurrentToken().Value;

					// consume operator
					if (!iter.Next())
					{
						logError(iter, "Unexpected end of file!");
						return ERROR_NODE_ID;
					}

					NodeID right = tryParseMultiplicativeExpression(iter);

					if (right == ERROR_NODE_ID)
						return ERROR_NODE_ID;

					left = buffers.CreateNode(Node{ .Kind = NodeKind::Binary, .Binary = Binary{ op, left, right } });
				}

				return left;
			};

		// handles ==, !=, <, <=, >, >=
		auto tryParseRelationalExpression = [&](TokenIterator& iter) -> NodeID
			{
				NodeID left = tryParseAdditiveExpression(iter);

				if (left == ERROR_NODE_ID)
					return ERROR_NODE_ID;

				while (iter.CurrentToken().Value == TokenValue::Equal
					|| iter.CurrentToken().Value == TokenValue::NotEqual
					|| iter.CurrentToken().Value == TokenValue::LessThan
					|| iter.CurrentToken().Value == TokenValue::LessThanOrEqual
					|| iter.CurrentToken().Value == TokenValue::GreaterThan
					|| iter.CurrentToken().Value == TokenValue::GreaterThanOrEqual)
				{
					TokenValue op = iter.CurrentToken().Value;

					// consume operator
					if (!iter.Next())
					{
						logError(iter, "Unexpected end of file!");
						return ERROR_NODE_ID;
					}

					NodeID right = tryParseAdditiveExpression(iter);

					if (right == ERROR_NODE_ID)
						return ERROR_NODE_ID;

					left = buffers.CreateNode(Node{ .Kind = NodeKind::Binary, .Binary = Binary{ op, left, right } });
				}

				return left;
			};

		// handles && and ||
		auto tryParseLogicalExpression = [&](TokenIterator& iter) -> NodeID
			{
				NodeID left = tryParseRelationalExpression(iter);

				if (left == ERROR_NODE_ID)
					return ERROR_NODE_ID;

				while (iter.CurrentToken().Value == TokenValue::LogicalAnd
					|| iter.CurrentToken().Value == TokenValue::LogicalOr)
				{
					TokenValue op = iter.CurrentToken().Value;

					// consume operator
					if (!iter.Next())
					{
						logError(iter, "Unexpected end of file! Expected an expression.");
						return ERROR_NODE_ID;
					}

					NodeID right = tryParseRelationalExpression(iter);

					if (right == ERROR_NODE_ID)
						return ERROR_NODE_ID;

					left = buffers.CreateNode(Node{ .Kind = NodeKind::Binary, .Binary = Binary{ op, left, right } });
				}

				return left;
			};

		return tryParseLogicalExpression(iter);
	}

	/// <summary>
	/// ASSIGNMENT_EXPRESSION: VAR_NAME ASSIGNMENT_OPERATOR EXPRESSION ;
	/// </summary>
	inline static NodeID parseAssignmentExpression(TokenIterator& iter, NodeDataBuffers& buffers, bool failIsError = true)
	{
		// assert that we have an identifier
		if (iter.CurrentToken().Kind != TokenKind::Identifier)
		{
			if (failIsError)
			{
				logError(iter, "Expected an identifier! Got '{0}' instead.", iter.CurrentSourceView());
			}

			return ERROR_NODE_ID;
		}

		// parse the identifier
		const auto& identifier = iter.CurrentSourceView();

		// consume the identifier
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected an assignment operator.");
			return ERROR_NODE_ID;
		}

		// check for assignment operator
		if (iter.CurrentToken().Value != TokenValue::Assign)
		{
			if (failIsError)
			{
				logError(iter, "Expected an assignment operator! Got '{0}' instead.", iter.CurrentSourceView());
			}
			else
			{
				// go back to the identifier
				iter.Previous();
			}

			return ERROR_NODE_ID;
		}

		// from here, we know we have an assignment expression

		// save the operator
		TokenValue op = iter.CurrentToken().Value;

		// consume the assignment operator
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected an expression.");
			return ERROR_NODE_ID;
		}

		// parse the expression
		NodeID expressionID = parseExpression(iter, buffers);

		// check if the expression was valid
		if (expressionID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		return buffers.CreateNode(Node{ .Kind = NodeKind::AssignmentExpression, .AssignmentExpression = AssignmentExpression{ op, identifier, expressionID } });
	}

	/// <summary>
	/// EXPRESSION: PRIMARY_EXPRESSION | UNARY_EXPRESSION | BINARY_EXPRESSION | ASSIGNMENT_EXPRESSION ;
	/// </summary>
	inline static NodeID parseExpression(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		NodeID expressionID;

		// assignment expressions are a special case
		// depending on the context, the function is allowed to fail
		if ((expressionID = parseAssignmentExpression(iter, buffers, false)) != ERROR_NODE_ID)
			return expressionID;

		expressionID = parseBinaryExpresssion(iter, buffers);

		if (expressionID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		return expressionID;
	}

#pragma endregion

#pragma region Statements

	// forward declaration
	inline static NodeID parseStatement(TokenIterator& iter, NodeDataBuffers& buffers);

	/// <summary>
	/// VARIABLE_ASSIGNMENT: ASSIGNMENT_EXPRESSION ';' ;
	/// </summary>
	inline static NodeID parseVariableAssignment(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		NodeID assignmentExpressionID = parseAssignmentExpression(iter, buffers);

		if (assignmentExpressionID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// check for semicolon
		if (iter.CurrentToken().Value != TokenValue::Semicolon)
		{
			logError(iter, "Expected a ';'! Got '{0}' instead.", iter.CurrentSourceView());
		}

		// consume the semicolon
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file!");
			return ERROR_NODE_ID;
		}

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::AssignmentStatement, .AssignmentStatement = AssignmentStatement{ assignmentExpressionID } });
	}

	/// <summary>
	/// FOR_LOOP: 'for' '(' EXPRESSION ';' EXPRESSION ';' EXPRESSION ';' ')' STATEMENT ;
	/// </summary>
	inline static NodeID parseForLoop(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		logError(iter, "Not implemented!");

		return ERROR_NODE_ID;
	}

	/// <summary>
	/// WHILE_LOOP: 'while' ENCLOSED_EXPRESSION STATEMENT ;
	/// </summary>
	inline static NodeID parseWhileLoop(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		logError(iter, "Not implemented!");

		return ERROR_NODE_ID;
	}

	/// <summary>
	/// IF_STATEMENT: 'if' ENCLOSED_EXPRESSION STATEMENT ['else' STATEMENT] ;
	/// </summary>
	inline static NodeID parseIfStatement(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		logError(iter, "Not implemented!");

		return ERROR_NODE_ID;
	}

	/// <summary>
	/// MATCH_STATEMENT: 'match' ENCLOSED_EXPRESSION '{' { EXPRESSION '=>' STATEMENT } '}' ;
	/// </summary>
	inline static NodeID parseMatchStatement(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		logError(iter, "Not implemented!");

		return ERROR_NODE_ID;
	}

	/// <summary>
	/// STATEMENT_BLOCK: '{' {STATEMENT} '}' ;
	/// </summary>
	inline static NodeID parseStatementBlock(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// check for opening brace
		if (iter.CurrentToken().Value != TokenValue::OpenBrace)
		{
			logError(iter, "Expected a '{0}'! Got '{1}' instead.", "{", iter.CurrentSourceView());
			return ERROR_NODE_ID;
		}

		// consume the opening brace
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected a statement or '}'.");
			return ERROR_NODE_ID;
		}

		// parse all statements
		auto& statementIDs = buffers.CreateNodeIDList();

		while (iter.CurrentToken().Value != TokenValue::CloseBrace)
		{
			// parse the statement
			NodeID statementID = parseStatement(iter, buffers);

			// check if the statement was valid
			if (statementID == ERROR_NODE_ID)
			{
				return ERROR_NODE_ID;
			}

			// add the statement to the list
			statementIDs.push_back(statementID);
		}

		// consume the closing brace
		(void)iter.Next();

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::StatementBlock, .StatementBlock = StatementBlock{ std::move(statementIDs) } });
	}

	/// <summary>
	/// RETURN_STATEMENT: 'return' [ EXPRESSION ] ';' ;
	/// </summary>
	inline static NodeID parseReturnStatement(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// assert we have a return keyword
		ASSERT(iter.CurrentToken().Value == TokenValue::Return, "Expected 'return'!");

		// consume the return keyword
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected an expression or ';' after 'return'.");
			return ERROR_NODE_ID;
		}

		NodeID expressionID = ERROR_NODE_ID;

		// check for expression
		if (iter.CurrentToken().Value != TokenValue::Semicolon)
		{
			// parse the expression
			expressionID = parseExpression(iter, buffers);

			// check if the expression was valid
			if (expressionID == ERROR_NODE_ID)
			{
				return ERROR_NODE_ID;
			}
		}

		// check for semicolon
		if (iter.CurrentToken().Value != TokenValue::Semicolon)
		{
			logError(iter, "Expected a ';'! Got '{0}' instead.", iter.CurrentSourceView());
		}

		// consume the semicolon
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file!");
			return ERROR_NODE_ID;
		}

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::Return, .Return = Return{ expressionID } });
	}

	/// <summary>
	/// STATEMENT: DEFINITION 
	///		| FN_CALL ';' 
	///		| VARIABLE_ASSIGNMENT 
	///		| FOR_LOOP 
	///		| WHILE_LOOP 
	///		| IF_STATEMENT 
	///		| MATCH_STATEMENT 
	///		| STATEMENT_BLOCK
	///		| RETURN_STATEMENT ;
	/// </summary>
	inline static NodeID parseStatement(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		switch (iter.CurrentToken().Value)
		{
		case TokenValue::For:
			return parseForLoop(iter, buffers);

		case TokenValue::While:
			return parseWhileLoop(iter, buffers);

		case TokenValue::If:
			return parseIfStatement(iter, buffers);

		case TokenValue::Match:
			return parseMatchStatement(iter, buffers);

		case TokenValue::OpenBrace:
			return parseStatementBlock(iter, buffers);

		case TokenValue::Return:
			return parseReturnStatement(iter, buffers);

		default:
			logError(iter, "Expected a statement! Got '{0}' instead.", iter.CurrentSourceView());
			return ERROR_NODE_ID;
		}
	}

#pragma endregion

#pragma region Declarations

	// forward declaration
	inline static NodeID parseTypeDeclaration(TokenIterator& iter, NodeDataBuffers& buffers);

	/// <summary>
	/// TUPLE_TYPE_DECLARATION: '(' TYPE_DECLARATION ',' TYPE_DECLARATION { ',' TYPE_DECLARATION } ')' ;
	/// </summary>
	inline static NodeID parseTupleTypeDeclaration(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// assert we have an opening parenthesis
		ASSERT(iter.CurrentToken().Value == TokenValue::OpenParen, "Expected '('!");

		// consume the opening parenthesis
		if (!iter.Next())
		{
			logError(iter, "Expected a type identifier after '('!");
			return ERROR_NODE_ID;
		}

		// parse all type identifiers
		auto& typeDeclarationIDs = buffers.CreateNodeIDList();

		while (iter.CurrentToken().Value != TokenValue::CloseParen)
		{
			// parse the type declaration
			NodeID typeDeclarationID = parseTypeDeclaration(iter, buffers);

			// check if the type declaration was valid
			if (typeDeclarationID == ERROR_NODE_ID)
			{
				return ERROR_NODE_ID;
			}

			// add the type declaration to the list
			typeDeclarationIDs.push_back(typeDeclarationID);

			// check for comma
			if (iter.CurrentToken().Value == TokenValue::Comma)
			{
				// consume the comma
				if (!iter.Next())
				{
					logError(iter, "Unexpected end of file! Expected a type declaration or ')' instead of ','.");
					return ERROR_NODE_ID;
				}
			}

			// othwerwise we must have a closing parenthesis
			else if (iter.CurrentToken().Value != TokenValue::CloseParen)
			{
				logError(iter, "Expected a ')'! Got '{0}' instead.", iter.CurrentSourceView());
				return ERROR_NODE_ID;
			}
		}

		// consume the closing parenthesis
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected a type identifier after ')'!");
			return ERROR_NODE_ID;
		}

		// check we have more than one type declaration
		if (typeDeclarationIDs.size() < 2)
		{
			logError(iter, "Single type tuples are not supported! Remove the '(' and ')'.");
			return ERROR_NODE_ID;
		}

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::TupleTypeDeclaration, .TupleTypeDeclaration = TupleTypeDeclaration{ std::move(typeDeclarationIDs) } });
	}

	/// <summary>
	/// CONST_TYPE_DECLARATION: 'const' TYPE_IDENTIFIER ;
	/// </summary>
	inline static NodeID parseConstantTypeDeclaration(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// assert we have a const keyword
		ASSERT(iter.CurrentToken().Value == TokenValue::Const, "Expected 'const'!");

		// consume the const keyword
		if (!iter.Next())
		{
			logError(iter, "Expected a type identifier after 'const'!");
			return ERROR_NODE_ID;
		}

		// parse the type identifier
		NodeID typeIdentifierID = parseTypeIdentifier(iter, buffers);

		if (typeIdentifierID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::ConstantTypeDeclaration, .ConstantTypeDeclaration = ConstantTypeDeclaration{ typeIdentifierID } });
	}

	/// <summary>
	/// VAR_TYPE_DECLARATION: 'var' TYPE_IDENTIFIER ;
	/// </summary>
	inline static NodeID parseVariableTypeDeclaration(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// assert we have a var keyword
		ASSERT(iter.CurrentToken().Value == TokenValue::Var, "Expected 'var'!");

		// consume the var keyword
		if (!iter.Next())
		{
			logError(iter, "Expected a type identifier after 'var'!");
			return ERROR_NODE_ID;
		}

		// parse the type identifier
		NodeID typeIdentifierID = parseTypeIdentifier(iter, buffers);

		if (typeIdentifierID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::VariableTypeDeclaration, .VariableTypeDeclaration = VariableTypeDeclaration{ typeIdentifierID } });
	}

	/// <summary>
	/// VALUE_TYPE_DECLARATION: TYPE_IDENTIFIER ;
	/// </summary>
	inline static NodeID parseValueTypeDeclaration(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// parse the type identifiers
		NodeID typeIdentifierID = parseTypeIdentifier(iter, buffers);

		if (typeIdentifierID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::ValueTypeDeclaration, .ValueTypeDeclaration = ValueTypeDeclaration{ typeIdentifierID } });
	}

	/// <summary>
	/// TYPE_DECLARATION: VALUE_TYPE_DECLARATION | VAR_TYPE_DECLARATION | CONST_TYPE_DECLARATION | TUPLE_TYPE_DECLARATION ;
	/// </summary>
	inline static NodeID parseTypeDeclaration(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		switch (iter.CurrentToken().Value)
		{
		case TokenValue::OpenParen:
			return parseTupleTypeDeclaration(iter, buffers);

		case TokenValue::Const:
			return parseConstantTypeDeclaration(iter, buffers);

		case TokenValue::Var:
			return parseVariableTypeDeclaration(iter, buffers);

		default:
			return parseValueTypeDeclaration(iter, buffers);
		}
	}

	/// <summary>
	/// VAR_DECLARATION: 'var' VAR_NAME ':' TYPE_IDENTIFIER ;
	/// </summary>
	inline static NodeID parseVariableDeclaration(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// assert we have a var keyword
		ASSERT(iter.CurrentToken().Value == TokenValue::Var, "Expected 'var'!");

		// consume the var keyword
		if (!iter.Next())
		{
			logError(iter, "Expected an identifier after 'var'!");
			return ERROR_NODE_ID;
		}

		// check for identifier
		if (iter.CurrentToken().Kind != TokenKind::Identifier)
		{
			logError(iter, "Expected an identifier! Got '{0}' instead.", iter.CurrentSourceView());
			return ERROR_NODE_ID;
		}

		const auto& identifier = iter.CurrentSourceView();

		// consume the identifier
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file!");
			return ERROR_NODE_ID;
		}

		// check for colon
		if (iter.CurrentToken().Value != TokenValue::Colon)
		{
			logError(iter, "Expected a ':'! Got '{0}' instead.", iter.CurrentSourceView());
			return ERROR_NODE_ID;
		}

		// consume the colon
		if (!iter.Next())
		{
			logError(iter, "Expected a type identifier after ':'!");
			return ERROR_NODE_ID;
		}

		// parse the type identifier
		NodeID typeIdentifierID = parseTypeIdentifier(iter, buffers);

		// check if the type identifier was valid
		if (typeIdentifierID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::VariableDeclaration, .VariableDeclaration = VariableDeclaration{ identifier, typeIdentifierID } });
	}

	/// <summary>
	/// CONST_DECLARATION: 'const' CONST_NAME ':' TYPE_IDENTIFIER ;
	/// </summary>
	inline static NodeID parseConstantDeclaration(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// assert we have a const keyword
		ASSERT(iter.CurrentToken().Value == TokenValue::Const, "Expected 'const'!");

		// consume the const keyword
		if (!iter.Next())
		{
			logError(iter, "Expected an identifier after 'const'!");
			return ERROR_NODE_ID;
		}

		// check for identifier
		if (iter.CurrentToken().Kind != TokenKind::Identifier)
		{
			logError(iter, "Expected an identifier! Got '{0}' instead.", iter.CurrentSourceView());
			return ERROR_NODE_ID;
		}

		const auto& identifier = iter.CurrentSourceView();

		// consume the identifier
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file!");
			return ERROR_NODE_ID;
		}

		// check for colon
		if (iter.CurrentToken().Value != TokenValue::Colon)
		{
			logError(iter, "Expected a ':'! Got '{0}' instead.", iter.CurrentSourceView());
			return ERROR_NODE_ID;
		}

		// consume the colon
		if (!iter.Next())
		{
			logError(iter, "Expected a type identifier after ':'!");
			return ERROR_NODE_ID;
		}

		// parse the type identifier
		NodeID typeIdentifierID = parseTypeIdentifier(iter, buffers);

		// check if the type identifier was valid
		if (typeIdentifierID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::ConstantDeclaration, .ConstantDeclaration = ConstantDeclaration{ identifier, typeIdentifierID } });
	}

	/// <summary>
	/// DECLARATION: VAR_DECLARATION | CONST_DECLARATION ;
	/// </summary>
	inline static NodeID parseDeclaration(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// quick exit if token kind is not valid
		if (iter.CurrentToken().Kind != TokenKind::Declaration)
		{
			logError(iter, "Expected a declaration! Got '{0}' instead.", iter.CurrentSourceView());
			return ERROR_NODE_ID;
		}

		switch (iter.CurrentToken().Value)
		{
		case TokenValue::Var:
			return parseVariableDeclaration(iter, buffers);

		case TokenValue::Const:
			return parseConstantDeclaration(iter, buffers);

		default:
			logError(iter, "Expected a declaration! Got '{0}' instead.", iter.CurrentSourceView());
			return ERROR_NODE_ID;
		}
	}

#pragma endregion

#pragma region Definitions

	/// <summary>
	/// FN_DEFINITION: 'fn' FN_NAME '(' [ DECLARATION { ',' DECLARATION } ] ')' [':' TYPE_DECLARATION] STATEMENT_BLOCK ;
	/// </summary>
	inline static NodeID parseFunctionDefinition(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// assert we have a fn keyword
		ASSERT(iter.CurrentToken().Value == TokenValue::Fn, "Expected 'fn'!");

		// consume the fn keyword
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected an identifier after 'fn'.");
			return ERROR_NODE_ID;
		}

		// get the function name
		const auto& functionName = iter.CurrentSourceView();

		// consume the function name
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected a '('.");
			return ERROR_NODE_ID;
		}

		// check for opening parenthesis
		if (iter.CurrentToken().Value != TokenValue::OpenParen)
		{
			logError(iter, "Expected a '('! Got '{0}' instead.", iter.CurrentSourceView());
			return ERROR_NODE_ID;
		}

		// consume the opening parenthesis
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected a parameter declaration.");
			return ERROR_NODE_ID;
		}

		// parse the parameter declarations
		auto& parameterDeclarationIDs = buffers.CreateNodeIDList();

		while (iter.CurrentToken().Value != TokenValue::CloseParen)
		{
			// parse the declaration
			NodeID declarationID = parseDeclaration(iter, buffers);

			// check if the declaration was valid
			if (declarationID == ERROR_NODE_ID)
			{
				return ERROR_NODE_ID;
			}

			// add the declaration to the list
			parameterDeclarationIDs.push_back(declarationID);

			// check for comma
			if (iter.CurrentToken().Value == TokenValue::Comma)
			{
				// consume the comma
				if (!iter.Next())
				{
					logError(iter, "Unexpected end of file! Expected a parameter declaration or ')' instead of ','.");
					return ERROR_NODE_ID;
				}
			}

			// othwerwise we must have a closing parenthesis
			else if (iter.CurrentToken().Value != TokenValue::CloseParen)
			{
				logError(iter, "Expected a ')'! Got '{0}' instead.", iter.CurrentSourceView());
				return ERROR_NODE_ID;
			}
		}

		// consume the closing parenthesis
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected a '->' or '{'.");
			return ERROR_NODE_ID;
		}

		// check for return type
		NodeID returnTypeID = ERROR_NODE_ID;

		if (iter.CurrentToken().Value == TokenValue::Arrow)
		{
			// consume the arrow
			if (!iter.Next())
			{
				logError(iter, "Unexpected end of file! Expected a return type.");
				return ERROR_NODE_ID;
			}

			// parse the return type
			returnTypeID = parseTypeDeclaration(iter, buffers);

			// check if the return type was valid
			if (returnTypeID == ERROR_NODE_ID)
			{
				return ERROR_NODE_ID;
			}
		}

		// parse the statement block
		NodeID statementBlockID = parseStatementBlock(iter, buffers);

		// check if the statement block was valid
		if (statementBlockID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		FunctionDefinition functionDefinition{ functionName, std::move(parameterDeclarationIDs), returnTypeID, statementBlockID };

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::FunctionDefinition, .FunctionDefinition = FunctionDefinition { functionName, parameterDeclarationIDs, returnTypeID, statementBlockID } });
	}

	/// <summary>
	/// ENUM_DEFINITION: 'enum' ENUM_NAME '{' { IDENTIFIER } '}' ;
	/// </summary>
	inline static NodeID parseEnumDefinition(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// assert we have an enum keyword
		ASSERT(iter.CurrentToken().Value == TokenValue::Enum, "Expected 'enum'!");

		// consume the enum keyword
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected an identifier after 'enum'.");
			return ERROR_NODE_ID;
		}

		// get the enum name
		const auto& enumName = iter.CurrentSourceView();

		// consume the enum name
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected a '{'.");
			return ERROR_NODE_ID;
		}

		// check for opening brace
		if (iter.CurrentToken().Value != TokenValue::OpenBrace)
		{
			logError(iter, "Expected a '{0}'! Got '{1}' instead.", "{", iter.CurrentSourceView());
			return ERROR_NODE_ID;
		}

		// consume the opening brace
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected an identifier.");
			return ERROR_NODE_ID;
		}

		// parse all identifiers
		auto& identifierIDs = buffers.CreateNodeIDList();

		while (iter.CurrentToken().Value != TokenValue::CloseBrace)
		{
			// check for identifier
			if (iter.CurrentToken().Kind != TokenKind::Identifier)
			{
				logError(iter, "Expected an identifier! Got '{0}' instead.", iter.CurrentSourceView());
				return ERROR_NODE_ID;
			}

			const auto& identifier = iter.CurrentSourceView();

			// consume the identifier
			if (!iter.Next())
			{
				logError(iter, "Unexpected end of file! Expected an identifier or '}'.");
				return ERROR_NODE_ID;
			}

			// add the identifier to the list
			identifierIDs.push_back(buffers.CreateNode(Node{ .Kind = NodeKind::EnumMember, .EnumMember = EnumMember{ identifier } }));

			// check for comma
			if (iter.CurrentToken().Value == TokenValue::Comma)
			{
				// consume the comma
				if (!iter.Next())
				{
					logError(iter, "Unexpected end of file! Expected an identifier or '}'.");
					return ERROR_NODE_ID;
				}
			}

			// othwerwise we must have a closing brace
			else if (iter.CurrentToken().Value != TokenValue::CloseBrace)
			{
				logError(iter, "Expected a '}'! Got '{0}' instead.", iter.CurrentSourceView());
				return ERROR_NODE_ID;
			}
		}

		// consume the closing brace
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected a ';' or '}'.");
			return ERROR_NODE_ID;
		}

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::EnumDefinition, .EnumDefinition = EnumDefinition{ enumName, std::move(identifierIDs) } });
	}

	/// <summary>
	/// STRUCT_DEFINITION: 'struct' STRUCT_NAME '{' { DECLARATION ';' } '}' ;
	/// </summary>
	inline static NodeID parseStructDefinition(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// assert we have a struct keyword
		ASSERT(iter.CurrentToken().Value == TokenValue::Struct, "Expected 'struct'!");

		// consume the struct keyword
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected an identifier after 'struct'.");
			return ERROR_NODE_ID;
		}

		// get the struct name
		const auto& structName = iter.CurrentSourceView();

		// consume the struct name
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected a '{0}'.", "{");
			return ERROR_NODE_ID;
		}

		// check for opening brace
		if (iter.CurrentToken().Value != TokenValue::OpenBrace)
		{
			logError(iter, "Expected a '{0}'! Got '{1}' instead.", "{", iter.CurrentSourceView());
			return ERROR_NODE_ID;
		}

		// consume the opening brace
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected a declaration.");
			return ERROR_NODE_ID;
		}

		// parse the declarations
		auto& declarationIDs = buffers.CreateNodeIDList();

		while (iter.CurrentToken().Value != TokenValue::CloseBrace)
		{
			// parse the declaration
			NodeID declarationID = parseDeclaration(iter, buffers);

			// check if the declaration was valid
			if (declarationID == ERROR_NODE_ID)
			{
				return ERROR_NODE_ID;
			}

			// check for semicolon
			if (iter.CurrentToken().Value != TokenValue::Semicolon)
			{
				logError(iter, "Expected a ';'! Got '{0}' instead.", iter.CurrentSourceView());
				return ERROR_NODE_ID;
			}

			// consume the semicolon
			if (!iter.Next())
			{
				logError(iter, "Unexpected end of file! Expected a declaration or '}'.");
				return ERROR_NODE_ID;
			}

			// add the declaration to the list
			declarationIDs.push_back(declarationID);
		}

		// consume the closing brace
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected a ';' or '}'.");
			return ERROR_NODE_ID;
		}

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::StructDefinition, .StructDefinition = StructDefinition{ structName, std::move(declarationIDs) } });
	}

	/// <summary>
	/// CONST_DEFINITION: CONST_DECLARATION '=' EXPRESSION ';' ;
	/// </summary>
	inline static NodeID parseConstantDefinition(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// assert we have a const keyword
		ASSERT(iter.CurrentToken().Value == TokenValue::Const, "Expected 'const'!");

		// parse the declaration
		NodeID declarationID = parseConstantDeclaration(iter, buffers);

		// check if the declaration was valid
		if (declarationID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// check for assignment operator
		if (iter.CurrentToken().Value != TokenValue::Assign)
		{
			logError(iter, "Expected a '='! Got '{0}' instead.", iter.CurrentSourceView());
			return ERROR_NODE_ID;
		}

		// consume the assignment operator
		if (!iter.Next())
		{
			logError(iter, "Expected an expression after '{0}'!", iter.CurrentSourceView());
			return ERROR_NODE_ID;
		}

		// parse the expression
		NodeID expressionID = parseExpression(iter, buffers);

		// check if the expression was valid
		if (expressionID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// check for semicolon
		if (iter.CurrentToken().Value != TokenValue::Semicolon)
		{
			logError(iter, "Expected a ';'! Got '{0}' instead.", iter.CurrentSourceView());
			return ERROR_NODE_ID;
		}

		// consume the semicolon
		(void)iter.Next();

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::ConstantDefinition, .ConstantDefinition = ConstantDefinition{ declarationID, expressionID } });
	}

	/// <summary>
	/// VAR_DEFINITION: VAR_DECLARATION '=' EXPRESSION ';' ;
	/// </summary>
	inline static NodeID parseVariableDefinition(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// assert we have a var keyword
		ASSERT(iter.CurrentToken().Value == TokenValue::Var, "Expected 'var'!");

		// parse the declaration
		NodeID declarationID = parseVariableDeclaration(iter, buffers);

		// check if the declaration was valid
		if (declarationID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// check for assignment operator
		if (iter.CurrentToken().Value != TokenValue::Assign)
		{
			logError(iter, "Expected a '='! Got '{0}' instead.", iter.CurrentSourceView());
			return ERROR_NODE_ID;
		}

		// consume the assignment operator
		if (!iter.Next())
		{
			logError(iter, "Expected an expression after '{0}'!", iter.CurrentSourceView());
			return ERROR_NODE_ID;
		}

		// parse the expression
		NodeID expressionID = parseExpression(iter, buffers);

		// check if the expression was valid
		if (expressionID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// check for semicolon
		if (iter.CurrentToken().Value != TokenValue::Semicolon)
		{
			logError(iter, "Expected a ';'! Got '{0}' instead.", iter.CurrentSourceView());
			return ERROR_NODE_ID;
		}

		// consume the semicolon
		(void)iter.Next();

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::VariableDefinition, .VariableDefinition = VariableDefinition{ declarationID, expressionID } });
	}

	/// <summary>
	/// DEFINITION: VAR_DEFINITION 
	///		| CONST_DEFINITION 
	///		| STRUCT_DEFINITION 
	///		| ENUM_DEFINITION 
	///		| FN_DEFINITION ;
	/// </summary>
	inline static NodeID parseDefinition(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// quick exit if token kind is not valid
		if (iter.CurrentToken().Kind != TokenKind::Declaration)
		{
			logError(iter, "Expected a definition! Got '{0}' instead.", iter.CurrentSourceView());
			return ERROR_NODE_ID;
		}

		switch (iter.CurrentToken().Value)
		{
		case TokenValue::Var:
			return parseVariableDefinition(iter, buffers);

		case TokenValue::Const:
			return parseConstantDefinition(iter, buffers);

		case TokenValue::Struct:
			return parseStructDefinition(iter, buffers);

		case TokenValue::Enum:
			return parseEnumDefinition(iter, buffers);

		case TokenValue::Fn:
			return parseFunctionDefinition(iter, buffers);
		}

		logError(iter, "Expected a definition! Got '{0}' instead.", iter.CurrentSourceView());
		return ERROR_NODE_ID;
	}

	/// <summary>
	/// QUALIFIED_DEFINITION: VISIBILITY_QUALIFIER DEFINITION ;
	/// </summary>
	inline static NodeID parseQualifiedDefinition(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		// check for qualifier, default is private
		QualifiedDefinition::Qualifier visibility = QualifiedDefinition::Qualifier::Private;

		if (iter.CurrentToken().Kind == TokenKind::Qualifier)
		{
			if (iter.CurrentToken().Value == TokenValue::Pub)
			{
				visibility = QualifiedDefinition::Qualifier::Public;
			}
			else if (iter.CurrentToken().Value == TokenValue::Exp)
			{
				visibility = QualifiedDefinition::Qualifier::Export;
			}
			else
			{
				ASSERT(false, "Unknown qualifier {0}!", iter.CurrentSourceView());
				return ERROR_NODE_ID;
			}

			// check for next token
			if (!iter.Next())
			{
				logError(iter, "Unexpected end of file! Expected a definition.");
				return ERROR_NODE_ID;
			}
		}

		// parse definition
		NodeID definitionID = parseDefinition(iter, buffers);

		if (definitionID == ERROR_NODE_ID)
		{
			return ERROR_NODE_ID;
		}

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::QualifiedDefinition, .QualifiedDefinition = QualifiedDefinition{ visibility, definitionID } });
	}

#pragma endregion

	/// <summary>
	/// MODULE: { QUALIFIED_DEFINITION } ;
	/// </summary>
	inline static NodeID parseModule(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		auto& qualifiedDefinitionIDs = buffers.CreateNodeIDList();

		do
		{
			NodeID definitionID = parseQualifiedDefinition(iter, buffers);

			if (definitionID == ERROR_NODE_ID)
			{
				return ERROR_NODE_ID;
			}

			qualifiedDefinitionIDs.push_back(definitionID);
		} while (iter.HasNext());

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::Module, .Module = Module{ std::move(qualifiedDefinitionIDs) } });
	}

	/// <summary>
	/// PROGRAM: { MODULE } ;
	/// </summary>
	inline static NodeID parseProgram(TokenIterator& iter, NodeDataBuffers& buffers)
	{
		auto& moduleIDs = buffers.CreateNodeIDList();

		moduleIDs.push_back(parseModule(iter, buffers));

		// create the node
		return buffers.CreateNode(Node{ .Kind = NodeKind::Program, .Program = Program{ std::move(moduleIDs) } });
	}


	NodeDataBuffers Parse(const Tokenizer::TokenDataBuffers& tokenBuffers)
	{
		TokenIterator iter(tokenBuffers);
		NodeDataBuffers buffers(tokenBuffers);

		// parse the program
		NodeID programID = parseProgram(iter, buffers);

		return buffers;
	}
}
