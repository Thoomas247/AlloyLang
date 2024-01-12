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

		const SourceView& CurrentSourceView() const
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
	inline static Ptr<TypeIdentifier> parseTypeIdentifier(TokenIterator& iter)
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
					return nullptr;
				}
			}

			else if (iter.CurrentToken().Value == TokenValue::Pointer)
			{
				modifier = TypeIdentifier::Modifier::Pointer;

				if (!iter.Next())
				{
					logError(iter, "Unexpected end of file!");
					return nullptr;
				}
			}
		}

		// check for identifier
		if (iter.CurrentToken().Kind != TokenKind::Identifier && iter.CurrentToken().Kind != TokenKind::BuiltInType)
		{
			logError(iter, "Expected a type identifier! Got '{0}' instead.", iter.CurrentSourceView());
			return nullptr;
		}

		const auto& identifier = iter.CurrentSourceView();

		// consume the identifier
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file!");
			return nullptr;
		}

		return std::make_unique<TypeIdentifier>(modifier, identifier);
	}

#pragma region Expressions

	// forward declaration
	inline static Ptr<Expression> parseExpression(TokenIterator& iter);

	/// <summary>
	/// FN_CALL: FN_NAME '(' [ EXPRESSION { ',' EXPRESSION } ] ')' ;
	/// </summary>
	inline static Ptr<FunctionCall> parseFunctionCall(TokenIterator& iter)
	{
		// assert we have an identifier
		ASSERT(iter.CurrentToken().Kind == TokenKind::Identifier, "Expected an identifier!");

		// get the identifier
		const auto& identifier = iter.CurrentSourceView();

		// consume the identifier
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected a '('.");
			return nullptr;
		}

		// check that we have an opening parenthesis
		if (iter.CurrentToken().Value != TokenValue::OpenParen)
		{
			logError(iter, "Expected a '('! Got '{0}' instead.", iter.CurrentSourceView());
			return nullptr;
		}

		// consume the opening parenthesis
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected an expression or ')' after '('.");
			return nullptr;
		}

		// get all the arguments
		Vec<Expression> arguments;

		while (iter.CurrentToken().Value != TokenValue::CloseParen)
		{
			// parse the expression
			Ptr<Expression> expression = parseExpression(iter);

			// check if the expression was valid
			if (!expression)
			{
				return nullptr;
			}

			// add the expression to the list
			arguments.push_back(std::move(expression));

			// check for comma
			if (iter.CurrentToken().Value == TokenValue::Comma)
			{
				// consume the comma
				if (!iter.Next())
				{
					logError(iter, "Unexpected end of file! Expected an expression or ')' instead of ','.");
					return nullptr;
				}
			}

			// othwerwise we must have a closing parenthesis
			else if (iter.CurrentToken().Value != TokenValue::CloseParen)
			{
				logError(iter, "Expected a ')'! Got '{0}' instead.", iter.CurrentSourceView());
				return nullptr;
			}
		}

		// consume the closing parenthesis
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file!");
			return nullptr;
		}

		return std::make_unique<FunctionCall>(identifier, std::move(arguments));
	}

	/// <summary>
	/// ENCLOSED_EXPRESSION: '(' EXPRESSION [',' EXPRESSION] ')' ;
	/// </summary>
	inline static Ptr<EnclosedExpression> parseEnclosedExpression(TokenIterator& iter)
	{
		// assert we have an opening parenthesis
		ASSERT(iter.CurrentToken().Value == TokenValue::OpenParen, "Expected '('!");

		// consume the opening parenthesis
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected an expression or ')' after '('.");
			return nullptr;
		}

		Vec<Expression> expressions;

		while (iter.CurrentToken().Value != TokenValue::CloseParen)
		{
			// parse the expression
			Ptr<Expression> expression = parseExpression(iter);

			// check if the expression was valid
			if (!expression)
			{
				return nullptr;
			}

			// add the expression to the list
			expressions.push_back(std::move(expression));

			// check for comma
			if (iter.CurrentToken().Value == TokenValue::Comma)
			{
				// consume the comma
				if (!iter.Next())
				{
					logError(iter, "Unexpected end of file! Expected an expression or ')' instead of ','.");
					return nullptr;
				}
			}

			// othwerwise we must have a closing parenthesis
			else if (iter.CurrentToken().Value != TokenValue::CloseParen)
			{
				logError(iter, "Expected a ')'! Got '{0}' instead.", iter.CurrentSourceView());
				return nullptr;
			}
		}

		// consume the closing parenthesis
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file!");
			return nullptr;
		}

		return std::make_unique<EnclosedExpression>(std::move(expressions));
	}

#pragma region Literals

	/// <summary>
	/// INT_LITERAL: NUMBER ;
	/// </summary>
	inline static Ptr<IntegerLiteral> parseIntegerLiteral(TokenIterator& iter)
	{
		// assert that we have an integer literal
		ASSERT(iter.CurrentToken().Value == TokenValue::Integer, "Expected an integer literal!");

		int64_t value = std::stoll(iter.CurrentSourceView().data());

		// consume the integer literal
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file!");
			return nullptr;
		}

		return std::make_unique<IntegerLiteral>(value);
	}

	/// <summary>
	/// FLOAT_LITERAL: NUMBER '.' NUMBER ;
	/// </summary>
	inline static Ptr<FloatLiteral> parseFloatLiteral(TokenIterator& iter)
	{
		// assert that we have a float literal
		ASSERT(iter.CurrentToken().Value == TokenValue::Float, "Expected a float literal!");

		double value = std::stod(iter.CurrentSourceView().data());

		// consume the float literal
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file!");
			return nullptr;
		}

		return std::make_unique<FloatLiteral>(value);
	}

	/// <summary>
	/// BOOLEAN_LITERAL: 'true' | 'false' ;
	/// </summary>
	inline static Ptr<BooleanLiteral> parseBooleanLiteral(TokenIterator& iter)
	{
		// assert that we have a boolean literal
		ASSERT(iter.CurrentToken().Value == TokenValue::Bool, "Expected a boolean literal!");

		bool value = iter.CurrentSourceView() == "true";	// if kind is bool, only possible values are true and false

		// consume the boolean literal
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file!");
			return nullptr;
		}

		return std::make_unique<BooleanLiteral>(value);
	}

	/// <summary>
	/// STRING_LITERAL: '\"' { LETTER } '\"' ;
	/// </summary>
	inline static Ptr<StringLiteral> parseStringLiteral(TokenIterator& iter)
	{
		// assert that we have a string literal
		ASSERT(iter.CurrentToken().Value == TokenValue::String, "Expected a string literal!");

		// store the string
		const auto& string = iter.CurrentSourceView();

		// consume the string
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file!");
			return nullptr;
		}

		return std::make_unique<StringLiteral>(string);
	}

	/// <summary>
	/// CHAR_LITERAL: '\'' [ LETTER ] '\'' ;
	/// </summary>
	inline static Ptr<CharacterLiteral> parseCharacterLiteral(TokenIterator& iter)
	{
		// assert that we have a character literal
		ASSERT(iter.CurrentToken().Value == TokenValue::Character, "Expected a character literal!");

		// store the character
		char c = iter.CurrentSourceView()[0];

		// consume the character
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file!");
			return nullptr;
		}

		return std::make_unique<CharacterLiteral>(c);
	}

	/// <summary>
	/// LITERAL: INT_LITERAL | FLOAT_LITERAL | BOOLEAN_LITERAL | STRING_LITERAL | CHAR_LITERAL ;
	/// </summary>
	inline static Ptr<Literal> parseLiteral(TokenIterator& iter)
	{
		// assert that we have a literal
		ASSERT(iter.CurrentToken().Kind == TokenKind::Literal, "Expected a literal!");

		switch (iter.CurrentToken().Value)
		{
		case TokenValue::Integer:
			return parseIntegerLiteral(iter);

		case TokenValue::Float:
			return parseFloatLiteral(iter);

		case TokenValue::Bool:
			return parseBooleanLiteral(iter);

		case TokenValue::String:
			return parseStringLiteral(iter);

		case TokenValue::Character:
			return parseCharacterLiteral(iter);

		default:
			logError(iter, "Expected a literal! Got '{0}' instead.", iter.CurrentSourceView());
			return nullptr;
		}
	}

#pragma endregion

	// forward declaration
	inline static Ptr<Expression> parseExpression(TokenIterator& iter);

	/// <summary>
	/// PRIMARY_EXPRESSION: VAR_NAME | CONST_NAME | FN_CALL | LITERAL | ENCLOSED_EXPRESSION ;
	/// </summary>
	inline static Ptr<Expression> parsePrimaryExpression(TokenIterator& iter)
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
				return nullptr;
			}

			// check for function call
			if (iter.CurrentToken().Value == TokenValue::OpenParen)
			{
				// go back to the identifier
				iter.Previous();

				return parseFunctionCall(iter);
			}

			// otherwise we have a variable or constant
			return std::make_unique<MemoryAccess>(identifier);
		}

		case TokenKind::Literal:
			return parseLiteral(iter);

		case TokenKind::Delimiter:
		{
			if (iter.CurrentToken().Value == TokenValue::OpenParen)
			{
				return parseEnclosedExpression(iter);
			}

			logError(iter, "Expected an expression! Got '{0}' instead.", iter.CurrentSourceView());
			return nullptr;
		}

		default:
			logError(iter, "Expected an expression! Got '{0}' instead.", iter.CurrentSourceView());
			return nullptr;
		}
	}

	/// <summary>
	/// UNARY_EXPRESSION: UNARY_OPERATOR EXPRESSION ;
	/// </summary>
	inline static Ptr<Expression> parseUnaryExpression(TokenIterator& iter)
	{
		// assert that we have an operator
		ASSERT(iter.CurrentToken().Kind == TokenKind::Operator, "Expected an operator!");

		// save the operator
		TokenValue op = iter.CurrentToken().Value;

		// consume the operator
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected an expression.");
			return nullptr;
		}

		// parse the expression
		Ptr<Expression> expression = parsePrimaryExpression(iter);

		// check if the expression was valid
		if (!expression)
		{
			return nullptr;
		}

		return std::make_unique<UnaryExpression>(op, std::move(expression));
	}

	/// <summary>
	/// BINARY_EXPRESSION: EXPRESSION BINARY_OPERATOR EXPRESSION ;
	/// </summary>
	inline static Ptr<Expression> parseBinaryExpresssion(TokenIterator& iter)
	{
		// handles *, / and %
		auto tryParseMultiplicativeExpression = [&](TokenIterator& iter)->Ptr<Expression>
			{
				Ptr<Expression> left;

				if (iter.CurrentToken().Kind == TokenKind::Operator)
				{
					left = parseUnaryExpression(iter);
				}

				else
				{
					left = parsePrimaryExpression(iter);
				}

				if (!left)
					return nullptr;

				while (iter.CurrentToken().Value == TokenValue::Multiply
					|| iter.CurrentToken().Value == TokenValue::Divide
					|| iter.CurrentToken().Value == TokenValue::Modulo)
				{
					TokenValue op = iter.CurrentToken().Value;

					// consume operator
					if (!iter.Next())
					{
						logError(iter, "Unexpected end of file!");
						return nullptr;
					}

					Ptr<Expression> right;

					if (iter.CurrentToken().Kind == TokenKind::Operator)
					{
						right = parseUnaryExpression(iter);
					}

					else
					{
						right = parsePrimaryExpression(iter);
					}

					if (!right)
						return nullptr;

					left = std::make_unique<BinaryExpression>(op, std::move(left), std::move(right));
				}

				return left;
			};

		// handles + and -
		auto tryParseAdditiveExpression = [&](TokenIterator& iter)->Ptr<Expression>
			{
				Ptr<Expression> left = tryParseMultiplicativeExpression(iter);

				if (!left)
					return nullptr;

				while (iter.CurrentToken().Value == TokenValue::Plus || iter.CurrentToken().Value == TokenValue::Minus)
				{
					TokenValue op = iter.CurrentToken().Value;

					// consume operator
					if (!iter.Next())
					{
						logError(iter, "Unexpected end of file!");
						return nullptr;
					}

					Ptr<Expression> right = tryParseMultiplicativeExpression(iter);

					if (!right)
						return nullptr;

					left = std::make_unique<BinaryExpression>(op, std::move(left), std::move(right));
				}

				return left;
			};

		// handles ==, !=, <, <=, >, >=
		auto tryParseRelationalExpression = [&](TokenIterator& iter)->Ptr<Expression>
			{
				Ptr<Expression> left = tryParseAdditiveExpression(iter);

				if (!left)
					return nullptr;

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
						return nullptr;
					}

					Ptr<Expression> right = tryParseAdditiveExpression(iter);

					if (!right)
						return nullptr;

					left = std::make_unique<BinaryExpression>(op, std::move(left), std::move(right));
				}

				return left;
			};

		// handles && and ||
		auto tryParseLogicalExpression = [&](TokenIterator& iter) -> Ptr<Expression>
			{
				Ptr<Expression> left = tryParseRelationalExpression(iter);

				if (!left)
					return nullptr;

				while (iter.CurrentToken().Value == TokenValue::LogicalAnd
					|| iter.CurrentToken().Value == TokenValue::LogicalOr)
				{
					TokenValue op = iter.CurrentToken().Value;

					// consume operator
					if (!iter.Next())
					{
						logError(iter, "Unexpected end of file! Expected an expression.");
						return nullptr;
					}

					Ptr<Expression> right = tryParseRelationalExpression(iter);

					if (!right)
						return nullptr;

					left = std::make_unique<BinaryExpression>(op, std::move(left), std::move(right));
				}

				return left;
			};

		return tryParseLogicalExpression(iter);
	}

	/// <summary>
	/// ASSIGNMENT_EXPRESSION: VAR_NAME ASSIGNMENT_OPERATOR EXPRESSION ;
	/// </summary>
	inline static Ptr<AssignmentExpression> parseAssignmentExpression(TokenIterator& iter, bool failIsError = true)
	{
		// assert that we have an identifier
		if (iter.CurrentToken().Kind != TokenKind::Identifier)
		{
			if (failIsError)
			{
				logError(iter, "Expected an identifier! Got '{0}' instead.", iter.CurrentSourceView());
			}

			return nullptr;
		}

		// parse the identifier
		const auto& identifier = iter.CurrentSourceView();

		// consume the identifier
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected an assignment operator.");
			return nullptr;
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

			return nullptr;
		}

		// from here, we know we have an assignment expression

		// save the operator
		TokenValue op = iter.CurrentToken().Value;

		// consume the assignment operator
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected an expression.");
			return nullptr;
		}

		// parse the expression
		Ptr<Expression> expression = parseExpression(iter);

		// check if the expression was valid
		if (!expression)
		{
			return nullptr;
		}

		return std::make_unique<AssignmentExpression>(op, identifier, std::move(expression));
	}

	/// <summary>
	/// EXPRESSION: PRIMARY_EXPRESSION | UNARY_EXPRESSION | BINARY_EXPRESSION | ASSIGNMENT_EXPRESSION ;
	/// </summary>
	inline static Ptr<Expression> parseExpression(TokenIterator& iter)
	{
		// assignment expressions are a special case
		// depending on the context, the function is allowed to fail
		if (Ptr<Expression> expression = parseAssignmentExpression(iter, false))
			return std::move(expression);

		Ptr<Expression> expression = parseBinaryExpresssion(iter);

		if (!expression)
		{
			return nullptr;
		}

		return std::move(expression);
	}

#pragma endregion

#pragma region Statements

	// forward declaration
	inline static Ptr<Statement> parseStatement(TokenIterator& iter);

	/// <summary>
	/// VARIABLE_ASSIGNMENT: ASSIGNMENT_EXPRESSION ';' ;
	/// </summary>
	inline static Ptr<VariableAssignment> parseVariableAssignment(TokenIterator& iter)
	{
		Ptr<AssignmentExpression> assignmentExpression = parseAssignmentExpression(iter);

		if (!assignmentExpression)
		{
			return nullptr;
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
			return nullptr;
		}

		return std::make_unique<VariableAssignment>(std::move(assignmentExpression));
	}

	/// <summary>
	/// FOR_LOOP: 'for' '(' EXPRESSION ';' EXPRESSION ';' EXPRESSION ';' ')' STATEMENT ;
	/// </summary>
	inline static Ptr<ForLoop> parseForLoop(TokenIterator& iter)
	{
		logError(iter, "Not implemented!");

		return nullptr;
	}

	/// <summary>
	/// WHILE_LOOP: 'while' ENCLOSED_EXPRESSION STATEMENT ;
	/// </summary>
	inline static Ptr<WhileLoop> parseWhileLoop(TokenIterator& iter)
	{
		logError(iter, "Not implemented!");

		return nullptr;
	}

	/// <summary>
	/// IF_STATEMENT: 'if' ENCLOSED_EXPRESSION STATEMENT ['else' STATEMENT] ;
	/// </summary>
	inline static Ptr<IfStatement> parseIfStatement(TokenIterator& iter)
	{
		logError(iter, "Not implemented!");

		return nullptr;
	}

	/// <summary>
	/// MATCH_STATEMENT: 'match' ENCLOSED_EXPRESSION '{' { EXPRESSION '=>' STATEMENT } '}' ;
	/// </summary>
	inline static Ptr<MatchStatement> parseMatchStatement(TokenIterator& iter)
	{
		logError(iter, "Not implemented!");

		return nullptr;
	}

	/// <summary>
	/// STATEMENT_BLOCK: '{' {STATEMENT} '}' ;
	/// </summary>
	inline static Ptr<StatementBlock> parseStatementBlock(TokenIterator& iter)
	{
		// check for opening brace
		if (iter.CurrentToken().Value != TokenValue::OpenBrace)
		{
			logError(iter, "Expected a '{0}'! Got '{1}' instead.", "{", iter.CurrentSourceView());
			return nullptr;
		}

		// consume the opening brace
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected a statement or '}'.");
			return nullptr;
		}

		// parse all statements
		Vec<Statement> statements;

		while (iter.CurrentToken().Value != TokenValue::CloseBrace)
		{
			// parse the statement
			Ptr<Statement> statement = parseStatement(iter);

			// check if the statement was valid
			if (!statement)
			{
				return nullptr;
			}

			// add the statement to the list
			statements.push_back(std::move(statement));
		}

		// consume the closing brace
		(void)iter.Next();

		return std::make_unique<StatementBlock>(std::move(statements));
	}

	/// <summary>
	/// RETURN_STATEMENT: 'return' [ EXPRESSION ] ';' ;
	/// </summary>
	inline static Ptr<ReturnStatement> parseReturnStatement(TokenIterator& iter)
	{
		// assert we have a return keyword
		ASSERT(iter.CurrentToken().Value == TokenValue::Return, "Expected 'return'!");

		// consume the return keyword
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected an expression or ';' after 'return'.");
			return nullptr;
		}

		Ptr<Expression> expression = nullptr;

		// check for expression
		if (iter.CurrentToken().Value != TokenValue::Semicolon)
		{
			// parse the expression
			expression = parseExpression(iter);

			// check if the expression was valid
			if (!expression)
			{
				return nullptr;
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
			return nullptr;
		}

		return std::make_unique<ReturnStatement>(std::move(expression));
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
	inline static Ptr<Statement> parseStatement(TokenIterator& iter)
	{
		switch (iter.CurrentToken().Value)
		{
		case TokenValue::For:
			return parseForLoop(iter);

		case TokenValue::While:
			return parseWhileLoop(iter);

		case TokenValue::If:
			return parseIfStatement(iter);

		case TokenValue::Match:
			return parseMatchStatement(iter);

		case TokenValue::OpenBrace:
			return parseStatementBlock(iter);

		case TokenValue::Return:
			return parseReturnStatement(iter);

		default:
			logError(iter, "Expected a statement! Got '{0}' instead.", iter.CurrentSourceView());
			return nullptr;
		}
	}

#pragma endregion

#pragma region Declarations

	// forward declaration
	inline static Ptr<TypeDeclaration> parseTypeDeclaration(TokenIterator& iter);

	/// <summary>
	/// TUPLE_TYPE_DECLARATION: '(' TYPE_DECLARATION ',' TYPE_DECLARATION { ',' TYPE_DECLARATION } ')' ;
	/// </summary>
	inline static Ptr<TupleTypeDeclaration> parseTupleTypeDeclaration(TokenIterator& iter)
	{
		// assert we have an opening parenthesis
		ASSERT(iter.CurrentToken().Value == TokenValue::OpenParen, "Expected '('!");

		// consume the opening parenthesis
		if (!iter.Next())
		{
			logError(iter, "Expected a type identifier after '('!");
			return nullptr;
		}

		// parse all type identifiers
		Vec<TypeDeclaration> typeDeclarations;

		while (iter.CurrentToken().Value != TokenValue::CloseParen)
		{
			// parse the type declaration
			Ptr<TypeDeclaration> typeDeclaration = parseTypeDeclaration(iter);

			// check if the type declaration was valid
			if (!typeDeclaration)
			{
				return nullptr;
			}

			// add the type declaration to the list
			typeDeclarations.push_back(std::move(typeDeclaration));

			// check for comma
			if (iter.CurrentToken().Value == TokenValue::Comma)
			{
				// consume the comma
				if (!iter.Next())
				{
					logError(iter, "Unexpected end of file! Expected a type declaration or ')' instead of ','.");
					return nullptr;
				}
			}

			// othwerwise we must have a closing parenthesis
			else if (iter.CurrentToken().Value != TokenValue::CloseParen)
			{
				logError(iter, "Expected a ')'! Got '{0}' instead.", iter.CurrentSourceView());
				return nullptr;
			}
		}

		// consume the closing parenthesis
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected a type identifier after ')'!");
			return nullptr;
		}

		// check we have more than one type declaration
		if (typeDeclarations.size() < 2)
		{
			logError(iter, "Single type tuples are not supported! Remove the '(' and ')'.");
			return nullptr;
		}

		return std::make_unique<TupleTypeDeclaration>(std::move(typeDeclarations));
	}

	/// <summary>
	/// CONST_TYPE_DECLARATION: 'const' TYPE_IDENTIFIER ;
	/// </summary>
	inline static Ptr<ConstantTypeDeclaration> parseConstantTypeDeclaration(TokenIterator& iter)
	{
		// assert we have a const keyword
		ASSERT(iter.CurrentToken().Value == TokenValue::Const, "Expected 'const'!");

		// consume the const keyword
		if (!iter.Next())
		{
			logError(iter, "Expected a type identifier after 'const'!");
			return nullptr;
		}

		// parse the type identifier
		Ptr<TypeIdentifier> typeIdentifier = parseTypeIdentifier(iter);

		if (!typeIdentifier)
		{
			return nullptr;
		}

		return std::make_unique<ConstantTypeDeclaration>(std::move(typeIdentifier));
	}

	/// <summary>
	/// VAR_TYPE_DECLARATION: 'var' TYPE_IDENTIFIER ;
	/// </summary>
	inline static Ptr<VariableTypeDeclaration> parseVariableTypeDeclaration(TokenIterator& iter)
	{
		// assert we have a var keyword
		ASSERT(iter.CurrentToken().Value == TokenValue::Var, "Expected 'var'!");

		// consume the var keyword
		if (!iter.Next())
		{
			logError(iter, "Expected a type identifier after 'var'!");
			return nullptr;
		}

		// parse the type identifier
		Ptr<TypeIdentifier> typeIdentifier = parseTypeIdentifier(iter);

		if (!typeIdentifier)
		{
			return nullptr;
		}

		return std::make_unique<VariableTypeDeclaration>(std::move(typeIdentifier));
	}

	/// <summary>
	/// VALUE_TYPE_DECLARATION: TYPE_IDENTIFIER ;
	/// </summary>
	inline static Ptr<ValueTypeDeclaration> parseValueTypeDeclaration(TokenIterator& iter)
	{
		// parse the type identifier
		Ptr<TypeIdentifier> typeIdentifier = parseTypeIdentifier(iter);

		if (!typeIdentifier)
		{
			return nullptr;
		}

		return std::make_unique<ValueTypeDeclaration>(std::move(typeIdentifier));
	}

	/// <summary>
	/// TYPE_DECLARATION: VALUE_TYPE_DECLARATION | VAR_TYPE_DECLARATION | CONST_TYPE_DECLARATION | TUPLE_TYPE_DECLARATION ;
	/// </summary>
	inline static Ptr<TypeDeclaration> parseTypeDeclaration(TokenIterator& iter)
	{
		switch (iter.CurrentToken().Value)
		{
		case TokenValue::OpenParen:
			return parseTupleTypeDeclaration(iter);

		case TokenValue::Const:
			return parseConstantTypeDeclaration(iter);

		case TokenValue::Var:
			return parseVariableTypeDeclaration(iter);

		default:
			return parseValueTypeDeclaration(iter);
		}
	}

	/// <summary>
	/// VAR_DECLARATION: 'var' VAR_NAME ':' TYPE_IDENTIFIER ;
	/// </summary>
	inline static Ptr<VariableDeclaration> parseVariableDeclaration(TokenIterator& iter)
	{
		// assert we have a var keyword
		ASSERT(iter.CurrentToken().Value == TokenValue::Var, "Expected 'var'!");

		// consume the var keyword
		if (!iter.Next())
		{
			logError(iter, "Expected an identifier after 'var'!");
			return nullptr;
		}

		// check for identifier
		if (iter.CurrentToken().Kind != TokenKind::Identifier)
		{
			logError(iter, "Expected an identifier! Got '{0}' instead.", iter.CurrentSourceView());
			return nullptr;
		}

		auto identifier = iter.CurrentSourceView();

		// consume the identifier
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file!");
			return nullptr;
		}

		// check for colon
		if (iter.CurrentToken().Value != TokenValue::Colon)
		{
			logError(iter, "Expected a ':'! Got '{0}' instead.", iter.CurrentSourceView());
			return nullptr;
		}

		// consume the colon
		if (!iter.Next())
		{
			logError(iter, "Expected a type identifier after ':'!");
			return nullptr;
		}

		// parse the type identifier
		Ptr<TypeIdentifier> typeIdentifier = parseTypeIdentifier(iter);

		// check if the type identifier was valid
		if (!typeIdentifier)
		{
			return nullptr;
		}

		return std::make_unique<VariableDeclaration>(std::move(identifier), std::move(typeIdentifier));
	}

	/// <summary>
	/// CONST_DECLARATION: 'const' CONST_NAME ':' TYPE_IDENTIFIER ;
	/// </summary>
	inline static Ptr<ConstantDeclaration> parseConstantDeclaration(TokenIterator& iter)
	{
		// assert we have a const keyword
		ASSERT(iter.CurrentToken().Value == TokenValue::Const, "Expected 'const'!");

		// consume the const keyword
		if (!iter.Next())
		{
			logError(iter, "Expected an identifier after 'const'!");
			return nullptr;
		}

		// check for identifier
		if (iter.CurrentToken().Kind != TokenKind::Identifier)
		{
			logError(iter, "Expected an identifier! Got '{0}' instead.", iter.CurrentSourceView());
			return nullptr;
		}

		const auto& identifier = iter.CurrentSourceView();

		// consume the identifier
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file!");
			return nullptr;
		}

		// check for colon
		if (iter.CurrentToken().Value != TokenValue::Colon)
		{
			logError(iter, "Expected a ':'! Got '{0}' instead.", iter.CurrentSourceView());
			return nullptr;
		}

		// consume the colon
		if (!iter.Next())
		{
			logError(iter, "Expected a type identifier after ':'!");
			return nullptr;
		}

		// parse the type identifier
		Ptr<TypeIdentifier> typeIdentifier = parseTypeIdentifier(iter);

		// check if the type identifier was valid
		if (!typeIdentifier)
		{
			return nullptr;
		}

		return std::make_unique<ConstantDeclaration>(std::move(identifier), std::move(typeIdentifier));
	}

	/// <summary>
	/// DECLARATION: VAR_DECLARATION | CONST_DECLARATION ;
	/// </summary>
	inline static Ptr<Declaration> parseDeclaration(TokenIterator& iter)
	{
		// quick exit if token kind is not valid
		if (iter.CurrentToken().Kind != TokenKind::Declaration)
			return nullptr;

		switch (iter.CurrentToken().Value)
		{
		case TokenValue::Var:
			return parseVariableDeclaration(iter);

		case TokenValue::Const:
			return parseConstantDeclaration(iter);

		default:
			logError(iter, "Expected a declaration! Got '{0}' instead.", iter.CurrentSourceView());
			return nullptr;
		}
	}

#pragma endregion

#pragma region Definitions

	/// <summary>
	/// FN_DEFINITION: 'fn' FN_NAME '(' [ DECLARATION { ',' DECLARATION } ] ')' [':' TYPE_DECLARATION] STATEMENT_BLOCK ;
	/// </summary>
	inline static Ptr<FunctionDefinition> parseFunctionDefinition(TokenIterator& iter)
	{
		// assert we have a fn keyword
		ASSERT(iter.CurrentToken().Value == TokenValue::Fn, "Expected 'fn'!");

		// consume the fn keyword
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected an identifier after 'fn'.");
			return nullptr;
		}

		// get the function name
		const auto& functionName = iter.CurrentSourceView();

		// consume the function name
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected a '('.");
			return nullptr;
		}

		// check for opening parenthesis
		if (iter.CurrentToken().Value != TokenValue::OpenParen)
		{
			logError(iter, "Expected a '('! Got '{0}' instead.", iter.CurrentSourceView());
			return nullptr;
		}

		// consume the opening parenthesis
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected a parameter declaration.");
			return nullptr;
		}

		// parse the parameter declarations
		Vec<Declaration> parameterDeclarations;

		while (iter.CurrentToken().Value != TokenValue::CloseParen)
		{
			// parse the declaration
			Ptr<Declaration> declaration = parseDeclaration(iter);

			// check if the declaration was valid
			if (!declaration)
			{
				return nullptr;
			}

			// add the declaration to the list
			parameterDeclarations.push_back(std::move(declaration));

			// check for comma
			if (iter.CurrentToken().Value == TokenValue::Comma)
			{
				// consume the comma
				if (!iter.Next())
				{
					logError(iter, "Unexpected end of file! Expected a parameter declaration or ')' instead of ','.");
					return nullptr;
				}
			}

			// othwerwise we must have a closing parenthesis
			else if (iter.CurrentToken().Value != TokenValue::CloseParen)
			{
				logError(iter, "Expected a ')'! Got '{0}' instead.", iter.CurrentSourceView());
				return nullptr;
			}
		}

		// consume the closing parenthesis
		if (!iter.Next())
		{
			logError(iter, "Unexpected end of file! Expected a '->' or '{'.");
			return nullptr;
		}

		// check for return type
		Ptr<TypeDeclaration> returnType;

		if (iter.CurrentToken().Value == TokenValue::Arrow)
		{
			// consume the arrow
			if (!iter.Next())
			{
				logError(iter, "Unexpected end of file! Expected a return type.");
				return nullptr;
			}

			// parse the return type
			returnType = parseTypeDeclaration(iter);

			// check if the return type was valid
			if (!returnType)
			{
				return nullptr;
			}
		}

		// parse the statement block
		Ptr<StatementBlock> statementBlock = parseStatementBlock(iter);

		// check if the statement block was valid
		if (!statementBlock)
		{
			return nullptr;
		}

		return std::make_unique<FunctionDefinition>(functionName, std::move(parameterDeclarations), std::move(returnType), std::move(statementBlock));
	}

	/// <summary>
	/// ENUM_DEFINITION: 'enum' ENUM_NAME '{' { IDENTIFIER } '}' ;
	/// </summary>
	inline static Ptr<EnumDefinition> parseEnumDefinition(TokenIterator& iter)
	{
		// assert we have an enum keyword
		ASSERT(iter.CurrentToken().Value == TokenValue::Enum, "Expected 'enum'!");

		logError(iter, "Not implemented!");

		return nullptr;
	}

	/// <summary>
	/// STRUCT_DEFINITION: 'struct' STRUCT_NAME '{' { DECLARATION } '}' ;
	/// </summary>
	inline static Ptr<StructDefinition> parseStructDefinition(TokenIterator& iter)
	{
		// assert we have a struct keyword
		ASSERT(iter.CurrentToken().Value == TokenValue::Struct, "Expected 'struct'!");

		logError(iter, "Not implemented!");

		return nullptr;
	}

	/// <summary>
	/// CONST_DEFINITION: CONST_DECLARATION '=' EXPRESSION ';' ;
	/// </summary>
	inline static Ptr<ConstantDefinition> parseConstantDefinition(TokenIterator& iter)
	{
		// assert we have a const keyword
		ASSERT(iter.CurrentToken().Value == TokenValue::Const, "Expected 'const'!");

		// parse the declaration
		Ptr<ConstantDeclaration> declaration = parseConstantDeclaration(iter);

		// check if the declaration was valid
		if (!declaration)
		{
			return nullptr;
		}

		// check for assignment operator
		if (iter.CurrentToken().Value != TokenValue::Assign)
		{
			logError(iter, "Expected a '='! Got '{0}' instead.", iter.CurrentSourceView());
			return nullptr;
		}

		// consume the assignment operator
		if (!iter.Next())
		{
			logError(iter, "Expected an expression after '{0}'!", iter.CurrentSourceView());
			return nullptr;
		}

		// parse the expression
		Ptr<Expression> expression = parseExpression(iter);

		// check if the expression was valid
		if (!expression)
		{
			return nullptr;
		}

		// check for semicolon
		if (iter.CurrentToken().Value != TokenValue::Semicolon)
		{
			logError(iter, "Expected a ';'! Got '{0}' instead.", iter.CurrentSourceView());
			return nullptr;
		}

		// consume the semicolon
		(void)iter.Next();

		return std::make_unique<ConstantDefinition>(std::move(declaration), std::move(expression));
	}

	/// <summary>
	/// VAR_DEFINITION: VAR_DECLARATION '=' EXPRESSION ';' ;
	/// </summary>
	inline static Ptr<VariableDefinition> parseVariableDefinition(TokenIterator& iter)
	{
		// assert we have a var keyword
		ASSERT(iter.CurrentToken().Value == TokenValue::Var, "Expected 'var'!");

		// parse the declaration
		Ptr<VariableDeclaration> declaration = parseVariableDeclaration(iter);

		// check if the declaration was valid
		if (!declaration)
		{
			return nullptr;
		}

		// check for assignment operator
		if (iter.CurrentToken().Value != TokenValue::Assign)
		{
			logError(iter, "Expected a '='! Got '{0}' instead.", iter.CurrentSourceView());
			return nullptr;
		}

		// consume the assignment operator
		if (!iter.Next())
		{
			logError(iter, "Expected an expression after '{0}'!", iter.CurrentSourceView());
			return nullptr;
		}

		// parse the expression
		Ptr<Expression> expression = parseExpression(iter);

		// check if the expression was valid
		if (!expression)
		{
			return nullptr;
		}

		// check for semicolon
		if (iter.CurrentToken().Value != TokenValue::Semicolon)
		{
			logError(iter, "Expected a ';'! Got '{0}' instead.", iter.CurrentSourceView());
			return nullptr;
		}

		// consume the semicolon
		(void)iter.Next();

		return std::make_unique<VariableDefinition>(std::move(declaration), std::move(expression));
	}

	/// <summary>
	/// DEFINITION: VAR_DEFINITION 
	///		| CONST_DEFINITION 
	///		| STRUCT_DEFINITION 
	///		| ENUM_DEFINITION 
	///		| FN_DEFINITION ;
	/// </summary>
	inline static Ptr<Definition> parseDefinition(TokenIterator& iter)
	{
		// quick exit if token kind is not valid
		if (iter.CurrentToken().Kind != TokenKind::Declaration)
		{
			logError(iter, "Expected a definition! Got '{0}' instead.", iter.CurrentSourceView());
			return nullptr;
		}

		switch (iter.CurrentToken().Value)
		{
		case TokenValue::Var:
			return parseVariableDefinition(iter);

		case TokenValue::Const:
			return parseConstantDefinition(iter);

		case TokenValue::Struct:
			return parseStructDefinition(iter);

		case TokenValue::Enum:
			return parseEnumDefinition(iter);

		case TokenValue::Fn:
			return parseFunctionDefinition(iter);
		}

		logError(iter, "Expected a definition! Got '{0}' instead.", iter.CurrentSourceView());
		return nullptr;
	}

	/// <summary>
	/// QUALIFIED_DEFINITION: VISIBILITY_QUALIFIER DEFINITION ;
	/// </summary>
	inline static Ptr<QualifiedDefinition> parseQualifiedDefinition(TokenIterator& iter)
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
				return nullptr;
			}

			// check for next token
			if (!iter.Next())
			{
				logError(iter, "Unexpected end of file! Expected a definition.");
				return nullptr;
			}
		}

		// parse definition
		Ptr<Definition> definition = parseDefinition(iter);

		if (!definition)
		{
			return nullptr;
		}

		return std::make_unique<QualifiedDefinition>(visibility, std::move(definition));
	}

#pragma endregion

	/// <summary>
	/// MODULE: { QUALIFIED_DEFINITION } ;
	/// </summary>
	inline static Ptr<Module> parseModule(TokenIterator& iter)
	{
		Vec<QualifiedDefinition> qualifiedDefinitions;

		do
		{
			auto definition = parseQualifiedDefinition(iter);

			if (!definition)
			{
				return nullptr;
			}

			qualifiedDefinitions.push_back(std::move(definition));
		} while (iter.HasNext());

		return std::make_unique<Module>(std::move(qualifiedDefinitions));
	}

	/// <summary>
	/// PROGRAM: { MODULE } ;
	/// </summary>
	inline static Ptr<Program> parseProgram(TokenIterator& iter)
	{
		Vec<Module> modules;

		modules.push_back(std::move(parseModule(iter)));

		return std::make_unique<Program>(std::move(modules));
	}

	Ptr<Program> Parse(const Tokenizer::TokenDataBuffers& buffers)
	{
		TokenIterator iter(buffers);

		return parseProgram(iter);
	}

}
