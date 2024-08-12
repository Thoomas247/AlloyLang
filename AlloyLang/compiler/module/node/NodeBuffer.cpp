#include "NodeBuffer.hpp"

namespace AlloyCompiler
{

#pragma region Util

	std::string NodeBuffer::tokenKindVectorToString(const std::vector<TokenKind>& tokens) const
	{
		// handle 0 tokens
		if (tokens.empty())
		{
			return "";
		}

		// handle 1 token
		if (tokens.size() == 1)
		{
			return "'" + TOKEN_KIND_VALUES.at(tokens[0]) + "'";
		}

		// handle 2 or more tokens
		std::string result;
		for (size_t i = 0; i < tokens.size() - 2; ++i)	// exclude last 2 because we print them without a comma
		{
			result += "'" + TOKEN_KIND_VALUES.at(tokens[i]) + "', ";
		}

		const std::string& lastTokenName = TOKEN_KIND_VALUES.at(tokens[tokens.size() - 1]);
		const std::string& secondToLastTokenName = TOKEN_KIND_VALUES.at(tokens[tokens.size() - 2]);

		result += "'" + secondToLastTokenName + "' or '" + lastTokenName + "'";

		return result;
	}

	std::string NodeBuffer::stringVectorToString(const std::vector<std::string>& tokens) const
	{
		// handle 0 tokens
		if (tokens.empty())
		{
			return "";
		}

		// handle 1 token
		if (tokens.size() == 1)
		{
			return "'" + tokens[0] + "'";
		}

		// handle 2 or more tokens
		std::string result;
		for (size_t i = 0; i < tokens.size() - 2; ++i)	// exclude last 2 because we print them without a comma
		{
			result += "'" + tokens[i] + "', ";
		}

		result += "'" + tokens[tokens.size() - 2] + "' or '" + tokens[tokens.size() - 1] + "'";

		return result;
	}

	bool NodeBuffer::isEOF() const
	{
		return m_CurrentTokenIndex >= (m_TokenBuffer.NumTokens() - 1);
	}

	bool NodeBuffer::hasNext() const
	{
		return !isEOF();
	}

	Token* NodeBuffer::token()
	{
		return m_TokenBuffer.GetToken(m_CurrentTokenIndex);
	}

	Token* NodeBuffer::peekToken(size_t offset)
	{
		return m_TokenBuffer.GetToken(m_CurrentTokenIndex + offset + 1);
	}

	NodeBuffer::Result NodeBuffer::eat()
	{
		if (isEOF())
		{
			logErrorAtCurrentPosition("Unexpected end of file.");
			return EOF_REACHED;
		}

		++m_CurrentTokenIndex;
		return SUCCESS;
	}

	NodeBuffer::Result NodeBuffer::expectValue(const std::vector<std::string>& values, Token** ppToken)
	{
		bool found = false;
		for (const std::string& value : values)
		{
			if (value == m_TokenBuffer.GetToken(m_CurrentTokenIndex)->Value)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			logErrorAtCurrentPosition("Expected {0}.", stringVectorToString(values));
			return UNEXPECTED_VALUE;
		}

		if (ppToken != nullptr)
		{
			*ppToken = m_TokenBuffer.GetToken(m_CurrentTokenIndex);
		}

		++m_CurrentTokenIndex;
		return SUCCESS;
	}

	NodeBuffer::Result NodeBuffer::checkAnnotations(const std::unordered_set<std::string_view>& validAnnotations)
	{
		Result result = SUCCESS;

		for (auto& [name, args] : m_CurrentAnnotations)
		{
			if (!validAnnotations.contains(name))
			{
				logErrorAtCurrentPosition("Annotation '{0}' is not valid here.", name);
				result = INVALID_ANNOTATION;
			}
		}

		return result;
	}

	bool NodeBuffer::consumeAnnotation(const std::string_view& name, AnnotationArgs* pArgs)
	{
		if (!m_CurrentAnnotations.contains(name))
		{
			return false;
		}

		if (pArgs != nullptr)
		{
			*pArgs = std::move(m_CurrentAnnotations[name]);
		}

		m_CurrentAnnotations.erase(name);

		return true;
	}

	size_t NodeBuffer::getOffsetToClosing(TokenKind closing)
	{
		TokenKind opening = peekToken()->Kind;

		size_t offset = 1;	// start after current token
		size_t depth = 1;

		while (depth > 0)
		{
			Token* pToken = peekToken(offset);

			if (pToken->Kind == TokenKind::end_of_file)
			{
				break;
			}

			if (pToken->Kind == opening)
			{
				depth++;
			}

			else if (pToken->Kind == closing)
			{
				depth--;
			}

			offset++;
		}

		return offset;
	}

#pragma endregion

#pragma region Types

	template<>
	TYPE* NodeBuffer::parse();

	template<>
	TYPE_NAME* NodeBuffer::parse()
	{
		Token* pNameToken = nullptr;
		if (expectKind<TokenKind::identifier, TokenKind::long_identifier>(&pNameToken) != SUCCESS)
		{
			return nullptr;
		}

		// check if we have any arguments to take in
		std::vector<TYPE*> genericArguments;
		if (token()->Kind == TokenKind::open_paren)
		{
			(void)eat();

			while (token()->Kind != TokenKind::close_paren)
			{
				TYPE* pArgument = parse<TYPE>();

				if (pArgument == nullptr)
				{
					return nullptr;
				}

				genericArguments.push_back(pArgument);

				if (token()->Kind == TokenKind::comma)
				{
					(void)eat();
				}
			}

			(void)eat();
		}

		return createNode(
			TYPE_NAME
			{
				.pNameToken = pNameToken,
				.GenericArguments = std::move(genericArguments)
			}
		);

	}

	template<>
	VARIABLE_DECLARATION* NodeBuffer::parse<VARIABLE_DECLARATION>(bool allowTypeInferring);

	template<>
	STRUCT_TYPE* NodeBuffer::parse()
	{
		if (expectKind<TokenKind::struct_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		if (expectKind<TokenKind::open_brace>() != SUCCESS)
		{
			return nullptr;
		}

		std::vector<std::pair<Token*, TYPE*>> members;
		while (token()->Kind != TokenKind::close_brace)
		{
			Token* pMemberNameToken = nullptr;
			if (expectKind<TokenKind::identifier>(&pMemberNameToken) != SUCCESS)
			{
				return nullptr;
			}

			if (expectKind<TokenKind::colon>() != SUCCESS)
			{
				return nullptr;
			}

			TYPE* pType = parse<TYPE>();

			if (pType == nullptr)
			{
				return nullptr;
			}

			members.push_back({ pMemberNameToken, pType });

			if (token()->Kind == TokenKind::comma)
			{
				(void)eat();
			}
			else if (token()->Kind != TokenKind::close_brace)
			{
				(void)expectKind<TokenKind::comma, TokenKind::close_brace>();
				return nullptr;
			}
		}

		(void)eat();

		return createNode(
			STRUCT_TYPE
			{
				.Members = std::move(members)
			}
		);
	}

	template<>
	ENUM_TYPE* NodeBuffer::parse()
	{
		if (expectKind<TokenKind::enum_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		if (expectKind<TokenKind::open_brace>() != SUCCESS)
		{
			return nullptr;
		}

		std::vector<std::pair<Token*, TYPE*>> members;
		while (token()->Kind != TokenKind::close_brace)
		{
			Token* pMemberNameToken = nullptr;
			if (expectKind<TokenKind::identifier>(&pMemberNameToken) != SUCCESS)
			{
				return nullptr;
			}

			// check for a payload
			if (token()->Kind == TokenKind::colon)
			{
				(void)eat();

				TYPE* pType = parse<TYPE>();

				if (pType == nullptr)
				{
					return nullptr;
				}

				members.push_back({ pMemberNameToken, pType });
			}

			if (token()->Kind == TokenKind::comma)
			{
				(void)eat();
			}
			else if (token()->Kind != TokenKind::close_brace)
			{
				(void)expectKind<TokenKind::comma, TokenKind::close_brace>();
				return nullptr;
			}
		}

		(void)eat();

		return createNode(
			ENUM_TYPE
			{
				.Members = std::move(members)
			}
		);
	}

	template<>
	LITERAL* NodeBuffer::parse<LITERAL>();

	template<>
	ARRAY_TYPE* NodeBuffer::parse()
	{
		if (expectKind<TokenKind::open_bracket>() != SUCCESS)
		{
			return nullptr;
		}

		TYPE* pType = parse<TYPE>();

		if (pType == nullptr)
		{
			return nullptr;
		}

		LITERAL* pSizeLiteral = nullptr;
		if (token()->Kind == TokenKind::semicolon)
		{
			(void)eat();

			pSizeLiteral = parse<LITERAL>();

			if (pSizeLiteral == nullptr)
			{
				return nullptr;
			}

			if (pSizeLiteral->Type != LiteralType::Integer)
			{
				logErrorAtCurrentPosition("Array size must be an integer.");
				return nullptr;
			}
		}

		if (expectKind<TokenKind::close_bracket>() != SUCCESS)
		{
			return nullptr;
		}

		return createNode(
			ARRAY_TYPE
			{
				.pElementType = pType,
				.pSizeLiteral = pSizeLiteral
			}
		);
	}

	template<>
	MACRO_CALL* NodeBuffer::parse();

	template<>
	TYPE* NodeBuffer::parse()
	{
		TypeModifier modifier = TypeModifier::None;

		if (token()->Value == "&")
		{
			modifier = TypeModifier::Reference;
		}
		else if (token()->Value == "*")
		{
			modifier = TypeModifier::Pointer;
		}

		if (modifier != TypeModifier::None)
		{
			(void)eat();
		}

		using enum TokenKind;

		VariantNode<TYPE_NAME, STRUCT_TYPE, ENUM_TYPE, ARRAY_TYPE, MACRO_CALL> type;
		switch (token()->Kind)
		{
		case at_symbol:
		{
			MACRO_CALL* pMacroCall = parse<MACRO_CALL>();

			if (pMacroCall == nullptr)
			{
				return nullptr;
			}

			type.Set(pMacroCall);
			break;
		}

		case identifier:
		case long_identifier:
		{
			TYPE_NAME* pNamedType = parse<TYPE_NAME>();

			if (pNamedType == nullptr)
			{
				return nullptr;
			}

			type.Set(pNamedType);
			break;
		}
		case struct_keyword:
		{
			STRUCT_TYPE* pStructType = parse<STRUCT_TYPE>();

			if (pStructType == nullptr)
			{
				return nullptr;
			}

			type.Set(pStructType);
			break;
		}
		case enum_keyword:
		{
			ENUM_TYPE* pEnumType = parse<ENUM_TYPE>();

			if (pEnumType == nullptr)
			{
				return nullptr;
			}

			type.Set(pEnumType);
			break;
		}
		case open_bracket:
		{
			ARRAY_TYPE* pArrayType = parse<ARRAY_TYPE>();

			if (pArrayType == nullptr)
			{
				return nullptr;
			}

			type.Set(pArrayType);
			break;
		}
		default:
		{
			(void)expectKind<identifier, struct_keyword, open_bracket>();
			return nullptr;
		}
		}

		return createNode(
			TYPE
			{
				.Modifier = modifier,
				.Type = type
			}
		);
	}

	template<>
	GENERIC_PARAMETER* NodeBuffer::parse();

	template<>
	TYPE_IDENTIFIER* NodeBuffer::parse()
	{
		Token* pNameToken = nullptr;
		if (expectKind<TokenKind::identifier>(&pNameToken) != SUCCESS)
		{
			return nullptr;
		}

		// check if we have generic parameters
		std::vector<GENERIC_PARAMETER*> genericParameters;
		if (token()->Kind == TokenKind::open_paren)
		{
			(void)eat();

			while (token()->Kind != TokenKind::close_paren)
			{
				GENERIC_PARAMETER* pGenericParam = parse<GENERIC_PARAMETER>();

				if (pGenericParam == nullptr)
				{
					return nullptr;
				}

				genericParameters.push_back(pGenericParam);

				if (token()->Kind == TokenKind::comma)
				{
					(void)eat();
				}
			}

			(void)eat();
		}

		return createNode(
			TYPE_IDENTIFIER
			{
				.pNameToken = pNameToken,
				.GenericParameters = std::move(genericParameters)
			}
		);
	}

	template<>
	TYPE_DEFINITION* NodeBuffer::parse()
	{
		if (checkAnnotations({ }) != SUCCESS)
		{
			return nullptr;
		}

		if (expectKind<TokenKind::type_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		TYPE_IDENTIFIER* pTypeIdentifier = parse<TYPE_IDENTIFIER>();

		if (pTypeIdentifier == nullptr)
		{
			return nullptr;
		}

		if (expectKind<TokenKind::assignment_operator>() != SUCCESS)
		{
			return nullptr;
		}

		TYPE* pType = parse<TYPE>();
		if (pType == nullptr)
		{
			return nullptr;
		}

		if (expectKind<TokenKind::semicolon>() != SUCCESS)
		{
			return nullptr;
		}


		return createNode(
			TYPE_DEFINITION
			{
				.pTypeIdentifier = pTypeIdentifier,
				.pType = pType,
			}
		);
	}

#pragma endregion

#pragma region Variables

	template<>
	VARIABLE* NodeBuffer::parse()
	{
		Token* pNameToken = nullptr;
		if (expectKind<TokenKind::identifier>(&pNameToken) != SUCCESS)
		{
			return nullptr;
		}

		return createNode(
			VARIABLE
			{
				.pNameToken = pNameToken
			}
		);
	}

	template<>
	VARIABLE_DECLARATION* NodeBuffer::parse(bool allowTypeInferring)
	{
		Token* pToken = nullptr;
		if (expectKind<TokenKind::variable_keyword, TokenKind::constant_keyword>(&pToken) != SUCCESS)
		{
			return nullptr;
		}

		VariableType varType;

		if (pToken->Kind == TokenKind::variable_keyword)
		{
			varType = VariableType::Variable;
		}
		else
		{
			varType = VariableType::Constant;
		}

		Token* pNameToken = nullptr;
		if (expectKind<TokenKind::identifier>(&pNameToken) != SUCCESS)
		{
			return nullptr;
		}

		// check if the type is explicitely stated
		TYPE* pType = nullptr;
		if (allowTypeInferring)
		{
			if (token()->Kind == TokenKind::colon)
			{
				(void)eat();

				pType = parse<TYPE>();

				if (pType == nullptr)
				{
					return nullptr;
				}
			}
		}
		else
		{
			if (expectKind<TokenKind::colon>() != SUCCESS)
			{
				return nullptr;
			}

			pType = parse<TYPE>();

			if (pType == nullptr)
			{
				return nullptr;
			}
		}

		return createNode(
			VARIABLE_DECLARATION
			{
				.VarType = varType,
				.pNameToken = pNameToken,
				.pType = pType
			}
		);
	}

	template<>
	EXPRESSION* NodeBuffer::parse<EXPRESSION>();

	template<>
	VARIABLE_DEFINITION* NodeBuffer::parse()
	{
		VARIABLE_DECLARATION* pVariableDeclaration = parse<VARIABLE_DECLARATION>(/*allowTypeInferring*/true);

		if (pVariableDeclaration == nullptr)
		{
			return nullptr;
		}

		if (expectKind<TokenKind::assignment_operator>() != SUCCESS)
		{
			return nullptr;
		}

		EXPRESSION* pValue = parse<EXPRESSION>();

		if (pValue == nullptr)
		{
			return nullptr;
		}

		return createNode(
			VARIABLE_DEFINITION
			{
				.pDeclaration = pVariableDeclaration,
				.pValue = pValue
			}
		);
	}

#pragma endregion

#pragma region Functions

	template<>
	FUNCTION_PARAMETER* NodeBuffer::parse()
	{
		switch (token()->Kind)
		{
		case TokenKind::variable_keyword:
		case TokenKind::constant_keyword:
		{
			VARIABLE_DECLARATION* pVariableDeclaration = parse<VARIABLE_DECLARATION>(/*allowTypeInferring*/false);

			if (pVariableDeclaration == nullptr)
			{
				return nullptr;
			}

			return createNode(FUNCTION_PARAMETER(pVariableDeclaration));
		}
		case TokenKind::type_keyword:
		case TokenKind::function_keyword:
		{
			GENERIC_PARAMETER* pGenericParameter = parse<GENERIC_PARAMETER>();

			if (pGenericParameter == nullptr)
			{
				return nullptr;
			}

			return createNode(FUNCTION_PARAMETER(pGenericParameter));
		}
		default:
		{
			(void)expectKind<TokenKind::variable_keyword, TokenKind::constant_keyword, TokenKind::type_keyword, TokenKind::function_keyword>();
			return nullptr;
		}
		}
	}

	template<>
	RETURN_TYPE* NodeBuffer::parse()
	{
		ReturnType retType = ReturnType::Copy;

		if (token()->Kind == TokenKind::variable_keyword)
		{
			retType = ReturnType::Variable;
		}
		else if (token()->Kind == TokenKind::constant_keyword)
		{
			retType = ReturnType::Constant;
		}

		if (retType != ReturnType::Copy)
		{
			(void)eat();
		}

		TYPE* pType = parse<TYPE>();

		if (pType == nullptr)
		{
			return nullptr;
		}

		return createNode(
			RETURN_TYPE
			{
				.RetType = retType,
				.pType = pType
			}
		);
	}

	template<>
	FUNCTION_TYPE* NodeBuffer::parse(bool allowVarArg, bool allowSelf)
	{
		if (expectKind<TokenKind::open_paren>())
		{
			return nullptr;
		}

		bool isVarArg = false;
		bool isGeneric = false;
		bool hasSelf = false;
		std::vector<FUNCTION_PARAMETER*> parameters;
		while (token()->Kind != TokenKind::close_paren)
		{
			// handle vararg
			if (token()->Kind == TokenKind::ellipsis)
			{
				(void)eat();

				if (!allowVarArg)
				{
					logErrorAtPreviousPosition("Only extern functions can have variable arguments.");
					return nullptr;
				}

				isVarArg = true;

				if (token()->Kind != TokenKind::close_paren)
				{
					logErrorAtPreviousPosition("Variable argument specifier '...' must be the last parameter.");
					return nullptr;
				}

				break;
			}

			FUNCTION_PARAMETER* pFunctionParameter = parse<FUNCTION_PARAMETER>();

			if (pFunctionParameter == nullptr)
			{
				return nullptr;
			}

			// check if the first parameter is of type 'Self'
			// 'Self' type has to be TYPE_NAME
			if (pFunctionParameter->Is<VARIABLE_DECLARATION>())
			{
				if (pFunctionParameter->Get<VARIABLE_DECLARATION>()->pType->Type.Is<TYPE_NAME>()
					&& pFunctionParameter->Get<VARIABLE_DECLARATION>()->pType->Type.Get<TYPE_NAME>()->pNameToken->Value == "Self")
				{
					if (!allowSelf)
					{
						logErrorAtPreviousPosition("Only member functions can have a parameter of type 'Self'.");
						return nullptr;
					}

					if (hasSelf)
					{
						logErrorAtPreviousPosition("Only one parameter of type 'Self' is allowed.");
						return nullptr;
					}

					if (parameters.size() != 0)
					{
						logErrorAtPreviousPosition("Parameter of type 'Self' must be the first parameter.");
						return nullptr;
					}
				}
			}

			if (pFunctionParameter->Is<GENERIC_PARAMETER>())
			{
				isGeneric = true;
			}

			parameters.push_back(pFunctionParameter);

			if (token()->Kind == TokenKind::comma)
			{
				(void)eat();
			}
		}

		(void)eat();

		RETURN_TYPE* pReturnType = nullptr;
		if (token()->Kind == TokenKind::arrow)
		{
			(void)eat();

			pReturnType = parse<RETURN_TYPE>();

			if (pReturnType == nullptr)
			{
				return nullptr;
			}
		}

		return createNode(
			FUNCTION_TYPE
			{
				.IsVarArg = isVarArg,
				.IsGeneric = isGeneric,
				.IsMember = hasSelf,
				.Parameters = std::move(parameters),
				.pReturnType = pReturnType
			}
		);
	}

	template<>
	STATEMENT_BLOCK* NodeBuffer::parse<STATEMENT_BLOCK>();

	template<>
	FUNCTION_DEFINITION* NodeBuffer::parse()
	{
		if (checkAnnotations({ }) != SUCCESS)
		{
			return nullptr;
		}

		Token* pFnKeywordToken = nullptr;
		if (expectKind<TokenKind::function_keyword, TokenKind::extern_keyword>(&pFnKeywordToken) != SUCCESS)
		{
			return nullptr;
		}

		const bool isExtern = pFnKeywordToken->Kind == TokenKind::extern_keyword;

		TYPE_IDENTIFIER* pTypeIdentifier = nullptr;
		if (!isExtern
			&& (peekToken()->Kind == TokenKind::colon || peekToken(getOffsetToClosing(TokenKind::close_paren))->Kind == TokenKind::colon))
		{
			pTypeIdentifier = parse<TYPE_IDENTIFIER>();

			if (pTypeIdentifier == nullptr)
			{
				return nullptr;
			}

			(void)eat();	// eat colon
		}

		Token* pFunctionNameToken = nullptr;
		if (expectKind<TokenKind::identifier>(&pFunctionNameToken))
		{
			return nullptr;
		}

		const bool allowSelf = !isExtern && pTypeIdentifier != nullptr;
		FUNCTION_TYPE* pFunctionType = parse<FUNCTION_TYPE>(/*allowVarArg*/isExtern, /*allowSelf*/allowSelf);

		if (pFunctionType == nullptr)
		{
			return nullptr;
		}

		STATEMENT_BLOCK* pBody = nullptr;
		
		if (!isExtern)
		{
			pBody = parse<STATEMENT_BLOCK>();

			if (pBody == nullptr)
			{
				return nullptr;
			}
		}

		else if (expectKind<TokenKind::semicolon>() != SUCCESS)
		{
			return nullptr;
		}

		return createNode(
			FUNCTION_DEFINITION
			{
				.pTypeIdentifier = pTypeIdentifier,
				.pFunctionNameToken = pFunctionNameToken,
				.pFunctionType = pFunctionType,
				.pBody = pBody
			}
		);
	}

	template<>
	FUNCTION_CALL* NodeBuffer::parse()
	{
		TYPE_NAME* pTypeName = nullptr;
		if (peekToken()->Kind == TokenKind::colon
			|| peekToken(getOffsetToClosing(TokenKind::close_paren))->Kind == TokenKind::colon)
		{
			pTypeName = parse<TYPE_NAME>();

			if (pTypeName == nullptr)
			{
				return nullptr;
			}

			(void)eat();	// eat colon
		}

		Token* pFunctionNameToken = nullptr;
		if (pTypeName == nullptr)
		{
			// long_identifier here is only valid if this is not a member function
			if (expectKind<TokenKind::identifier, TokenKind::long_identifier>(&pFunctionNameToken) != SUCCESS)
			{
				return nullptr;
			}
		}
		else
		{
			if (expectKind<TokenKind::identifier>(&pFunctionNameToken) != SUCCESS)
			{
				return nullptr;
			}
		}

		if (expectKind<TokenKind::open_paren>() != SUCCESS)
		{
			return nullptr;
		}

		std::vector<EXPRESSION*> arguments;
		while (token()->Kind != TokenKind::close_paren)
		{
			EXPRESSION* pArgument = parse<EXPRESSION>();

			if (pArgument == nullptr)
			{
				return nullptr;
			}

			arguments.push_back(pArgument);

			if (token()->Kind == TokenKind::comma)
			{
				(void)eat();
			}
		}

		(void)eat();

		return createNode(
			FUNCTION_CALL
			{
				.pTypeOrVariableName = pTypeName,
				.pFunctionNameToken = pFunctionNameToken,
				.Arguments = arguments
			}
		);
	}

#pragma endregion

#pragma region Generics

	template<>
	GENERIC_PARAMETER* NodeBuffer::parse()
	{
		if (expectKind<TokenKind::type_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		Token* pIdentifierToken = nullptr;
		if (expectKind<TokenKind::identifier>(&pIdentifierToken) != SUCCESS)
		{
			return nullptr;
		}

		return createNode(GENERIC_PARAMETER
			{
				.pIdentifierToken = pIdentifierToken
			});
	}

#pragma endregion

#pragma region Macros

	template<>
	MACRO_STATEMENT* NodeBuffer::parse();

	template<>
	MACRO_VARIABLE_IDENTIFIER* NodeBuffer::parse()
	{
		Token* pIdentifierToken = nullptr;

		if (expectKind<TokenKind::identifier>(&pIdentifierToken) != SUCCESS)
		{
			return nullptr;
		}

		return createNode(
			MACRO_VARIABLE_IDENTIFIER
			{
				.pNameToken = pIdentifierToken
			}
		);
	}

	template<>
	MACRO_CALL* NodeBuffer::parse()
	{
		if (expectKind<TokenKind::at_symbol>() != SUCCESS)
		{
			return nullptr;
		}

		Token* pMacroNameToken = nullptr;
		if (expectKind<TokenKind::identifier, TokenKind::long_identifier>(&pMacroNameToken) != SUCCESS)
		{
			return nullptr;
		}

		if (expectKind<TokenKind::open_paren>() != SUCCESS)
		{
			return nullptr;
		}

		std::vector<VariantNode<MACRO_VARIABLE_IDENTIFIER, MACRO_CALL>> arguments;
		while (token()->Kind != TokenKind::close_paren)
		{
			VariantNode<MACRO_VARIABLE_IDENTIFIER, MACRO_CALL> variantNode;

			if (token()->Kind == TokenKind::identifier)
			{
				MACRO_VARIABLE_IDENTIFIER* pVarIdentifier = parse<MACRO_VARIABLE_IDENTIFIER>();

				if (pVarIdentifier == nullptr)
				{
					return nullptr;
				}

				variantNode.Set(pVarIdentifier);
			}
			else if (token()->Kind == TokenKind::at_symbol)
			{
				MACRO_CALL* pMacroCall = parse<MACRO_CALL>();

				if (pMacroCall == nullptr)
				{
					return nullptr;
				}

				variantNode.Set(pMacroCall);
			}

			else
			{
				(void)expectKind<TokenKind::identifier, TokenKind::at_symbol>();
				return nullptr;
			}

			arguments.push_back(variantNode);

			if (token()->Kind == TokenKind::comma)
			{
				(void)eat();
			}
		}

		(void)eat();

		return createNode(
			MACRO_CALL
			{
				.pMacroNameToken = pMacroNameToken,
				.Arguments = std::move(arguments)
			}
		);
	}

	template<>
	MACRO_DEFINITION* NodeBuffer::parse()
	{
		if (expectKind<TokenKind::macro_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		Token* pNameToken = nullptr;
		if (expectKind<TokenKind::identifier>(&pNameToken) != SUCCESS)
		{
			return nullptr;
		}

		if (expectKind<TokenKind::open_paren>() != SUCCESS)
		{
			return nullptr;
		}

		std::vector<std::pair<MacroVariableType, Token*>> parameters;
		while (token()->Kind != TokenKind::close_paren)
		{
			Token* pTypeToken = nullptr;
			if (expectKind<TokenKind::type_keyword, TokenKind::function_keyword>(&pTypeToken) != SUCCESS)
			{
				return nullptr;
			}

			MacroVariableType type;

			if (pTypeToken->Kind == TokenKind::type_keyword)
			{
				type = MacroVariableType::Type;
			}
			else
			{
				type = MacroVariableType::Fn;
			}

			Token* pParamNameToken = nullptr;
			if (expectKind<TokenKind::identifier>(&pParamNameToken) != SUCCESS)
			{
				return nullptr;
			}

			parameters.push_back({ type, pParamNameToken });

			if (token()->Kind == TokenKind::comma)
			{
				(void)eat();
			}
		}

		(void)eat();

		MacroVariableType returnType = MacroVariableType::None;
		if (token()->Kind == TokenKind::arrow)
		{
			(void)eat();

			Token* pTypeToken = nullptr;
			if (expectKind<TokenKind::type_keyword, TokenKind::function_keyword>(&pTypeToken) != SUCCESS)
			{
				return nullptr;
			}

			if (pTypeToken->Kind == TokenKind::type_keyword)
			{
				returnType = MacroVariableType::Type;
			}
			else
			{
				returnType = MacroVariableType::Fn;
			}
		}

		if (expectKind<TokenKind::open_brace>() != SUCCESS)
		{
			return nullptr;
		}

		std::vector<MACRO_STATEMENT*> macroStatements;
		while (token()->Kind != TokenKind::close_brace)
		{
			MACRO_STATEMENT* pMacroStatement = parse<MACRO_STATEMENT>();

			if (pMacroStatement == nullptr)
			{
				return nullptr;
			}

			macroStatements.push_back(pMacroStatement);

			if (token()->Kind == TokenKind::comma)
			{
				(void)eat();
			}
		}

		(void)eat();

		return createNode(
			MACRO_DEFINITION
			{
				.pNameToken = pNameToken,
				.Parameters = std::move(parameters),
				.ReturnType = returnType,
				.Body = std::move(macroStatements)
			}
		);
	}

	// MACRO_RETURN:	return_keyword ( MACRO_VARIABLE_IDENTIFIER | MACRO_CALL | TYPE ) semicolon ;
	template<>
	MACRO_RETURN* NodeBuffer::parse()
	{
		if (expectKind<TokenKind::return_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		MACRO_RETURN macroReturn;

		switch (token()->Kind)
		{
		case TokenKind::identifier:
		{
			MACRO_VARIABLE_IDENTIFIER* pVarIdentifier = parse<MACRO_VARIABLE_IDENTIFIER>();

			if (pVarIdentifier == nullptr)
			{
				return nullptr;
			}

			macroReturn.Set(pVarIdentifier);
			break;
		}

		case TokenKind::at_symbol:
		{
			MACRO_CALL* pMacroCall = parse<MACRO_CALL>();

			if (pMacroCall == nullptr)
			{
				return nullptr;
			}

			macroReturn.Set(pMacroCall);
			break;
		}

		case TokenKind::struct_keyword:
		case TokenKind::open_bracket:
		{
			TYPE* pType = parse<TYPE>();

			if (pType == nullptr)
			{
				return nullptr;
			}

			macroReturn.Set(pType);
			break;
		}

		default:
		{
			(void)expectKind<TokenKind::identifier, TokenKind::at_symbol, TokenKind::struct_keyword, TokenKind::open_bracket>();
			(void)eat();
			return nullptr;
		}
		}

		if (expectKind<TokenKind::semicolon>() != SUCCESS)
		{
			return nullptr;
		}

		return createNode(macroReturn);
	}

	template<>
	MACRO_STATEMENT* NodeBuffer::parse()
	{
		switch (token()->Kind)
		{
		case TokenKind::macro_keyword:
		{
			MACRO_DEFINITION* pMacroDefinition = parse<MACRO_DEFINITION>();

			if (pMacroDefinition == nullptr)
			{
				return nullptr;
			}

			return createNode(MACRO_STATEMENT(pMacroDefinition));
		}

		case TokenKind::at_symbol:
		{
			MACRO_CALL* pMacroCall = parse<MACRO_CALL>();

			if (pMacroCall == nullptr)
			{
				return nullptr;
			}

			if (expectKind<TokenKind::semicolon>() != SUCCESS)
			{
				return nullptr;
			}

			return createNode(MACRO_STATEMENT(pMacroCall));
		}

		case TokenKind::type_keyword:
		{
			TYPE_DEFINITION* pTypeDefinition = parse<TYPE_DEFINITION>();

			if (pTypeDefinition == nullptr)
			{
				return nullptr;
			}

			return createNode(MACRO_STATEMENT(pTypeDefinition));
		}

		case TokenKind::function_keyword:
		{
			FUNCTION_DEFINITION* pFunctionDefinition = parse<FUNCTION_DEFINITION>();

			if (pFunctionDefinition == nullptr)
			{
				return nullptr;
			}

			return createNode(MACRO_STATEMENT(pFunctionDefinition));
		}

		case TokenKind::return_keyword:
		{
			MACRO_RETURN* pMacroReturn = parse<MACRO_RETURN>();

			if (pMacroReturn == nullptr)
			{
				return nullptr;
			}

			return createNode(MACRO_STATEMENT(pMacroReturn));
		}

		default:
		{
			(void)expectKind<TokenKind::macro_keyword, TokenKind::at_symbol, TokenKind::type_keyword, TokenKind::function_keyword>();
			(void)eat();
			return nullptr;
		}
		}
	}

#pragma endregion

#pragma region Expressions

	template<>
	EXPRESSION* NodeBuffer::parse();

	template<>
	LITERAL* NodeBuffer::parse()
	{
		using enum TokenKind;

		Token* pLiteralToken = nullptr;
		if (expectKind<integer_literal,
			float_literal,
			boolean_literal,
			string_literal,
			character_literal>(&pLiteralToken) != SUCCESS)
		{
			return nullptr;
		}

		LiteralType literalType;
		switch (pLiteralToken->Kind)
		{
		case integer_literal:
			literalType = LiteralType::Integer;
			break;

		case float_literal:
			literalType = LiteralType::Float;
			break;

		case boolean_literal:
			literalType = LiteralType::Boolean;
			break;

		case string_literal:
			literalType = LiteralType::String;
			break;

		case character_literal:
			literalType = LiteralType::Character;
			break;

		default:
			ASSERT(false, "Invalid literal!");
			break;
		}

		return createNode(
			LITERAL
			{
				.Type = literalType,
				.pValueToken = pLiteralToken
			}
		);
	}

	template<>
	CONSTRUCTOR* NodeBuffer::parse()
	{
		TYPE_NAME* pNamedType = parse<TYPE_NAME>();

		if (pNamedType == nullptr)
		{
			return nullptr;
		}

		if (expectKind<TokenKind::open_brace>() != SUCCESS)
		{
			return nullptr;
		}

		std::vector<std::pair<Token*, EXPRESSION*>> arguments;
		while (token()->Kind != TokenKind::close_brace)
		{
			Token* pMemberNameToken = nullptr;
			if (expectKind<TokenKind::identifier>(&pMemberNameToken) != SUCCESS)
			{
				return nullptr;
			}

			if (expectKind<TokenKind::assignment_operator>() != SUCCESS)
			{
				return nullptr;
			}

			EXPRESSION* pValue = parse<EXPRESSION>();

			if (pValue == nullptr)
			{
				return nullptr;
			}

			arguments.push_back({ pMemberNameToken, pValue });

			if (token()->Kind == TokenKind::comma)
			{
				(void)eat();
			}
		}

		// eat closing brace
		(void)eat();

		return createNode(
			CONSTRUCTOR
			{
				.pType = pNamedType,
				.Arguments = std::move(arguments)
			}
		);
	}

	template<>
	POINTER_INIT* NodeBuffer::parse()
	{
		if (expectKind<TokenKind::new_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		EXPRESSION* pValue = nullptr;
		EXPRESSION* pSize = nullptr;

		// handle array allocation
		if (token()->Kind == TokenKind::open_bracket)
		{
			(void)eat();

			pValue = parse<EXPRESSION>();

			if (pValue == nullptr)
			{
				return nullptr;
			}

			if (expectKind<TokenKind::semicolon>() != SUCCESS)
			{
				return nullptr;
			}

			pSize = parse<EXPRESSION>();

			if (pSize == nullptr)
			{
				return nullptr;
			}

			if (expectKind<TokenKind::close_bracket>() != SUCCESS)
			{
				return nullptr;
			}
		}

		else
		{
			pValue = parse<EXPRESSION>();

			if (pValue == nullptr)
			{
				return nullptr;
			}
		}

		return createNode(
			POINTER_INIT
			{
				.pValue = pValue,
				.pSize = pSize
			}
		);
	}

	template<>
	POINTER_MOVE* NodeBuffer::parse()
	{
		if (expectKind<TokenKind::move_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		// TODO: pointer could be struct member, array member, ... need to fix!
		VARIABLE* pNamedVariable = parse<VARIABLE>();

		if (pNamedVariable == nullptr)
		{
			return nullptr;
		}

		return createNode(
			POINTER_MOVE
			{
				.pVariable = pNamedVariable
			}
		);
	}

	template<>
	INITIALIZER_LIST* NodeBuffer::parse()
	{
		if (expectKind<TokenKind::open_brace>() != SUCCESS)
		{
			return nullptr;
		}

		std::vector<EXPRESSION*> values;
		while (token()->Kind != TokenKind::close_brace)
		{
			EXPRESSION* pExpression = parse<EXPRESSION>();

			if (pExpression == nullptr)
			{
				return nullptr;
			}

			values.push_back(pExpression);

			if (token()->Kind == TokenKind::comma)
			{
				if (eat() != SUCCESS)
				{
					return nullptr;
				}
			}
		}

		if (eat() != SUCCESS)
		{
			return nullptr;
		}

		return createNode(
			INITIALIZER_LIST
			{
				.Values = std::move(values)
			}
		);
	}

	template<>
	ENCLOSED_EXPRESSION* NodeBuffer::parse()
	{
		if (expectKind<TokenKind::open_paren>() != SUCCESS)
		{
			return nullptr;
		}

		EXPRESSION* pExpression = parse<EXPRESSION>();

		if (pExpression == nullptr)
		{
			return nullptr;
		}

		if (expectKind<TokenKind::close_paren>() != SUCCESS)
		{
			return nullptr;
		}

		return createNode(
			ENCLOSED_EXPRESSION
			{
				.pExpression = pExpression
			}
		);
	}

	template<>
	ENUM_VALUE* NodeBuffer::parse()
	{
		TYPE_NAME* pEnumName = parse<TYPE_NAME>();

		if (pEnumName == nullptr)
		{
			return nullptr;
		}

		if (expectKind<TokenKind::pipe_operator>() != SUCCESS)
		{
			return nullptr;
		}

		Token* pEnumValueNameToken = nullptr;
		if (expectKind<TokenKind::identifier>(&pEnumValueNameToken) != SUCCESS)
		{
			return nullptr;
		}

		EXPRESSION* pPayloadValue = nullptr;
		if (token()->Kind == TokenKind::open_paren)
		{
			(void)eat();

			pPayloadValue = parse<EXPRESSION>();

			if (pPayloadValue == nullptr)
			{
				return nullptr;
			}

			if (expectKind<TokenKind::close_paren>() != SUCCESS)
			{
				return nullptr;
			}
		}

		return createNode(
			ENUM_VALUE
			{
				.pEnumName = pEnumName,
				.pEnumValueNameToken = pEnumValueNameToken,
				.pPayloadValue = pPayloadValue
			}
		);
	}

	EXPRESSION* NodeBuffer::parse_PRIMARY()
	{
		using enum TokenKind;

		switch (token()->Kind)
		{
		case integer_literal:
		case float_literal:
		case boolean_literal:
		case string_literal:
		case character_literal:
		{
			LITERAL* pLiteral = parse<LITERAL>();

			if (pLiteral == nullptr)
			{
				return nullptr;
			}

			return createNode(EXPRESSION(createNode(PRIMARY(pLiteral))));
		}

		case variable_keyword:
		case constant_keyword:
		{
			VARIABLE_DEFINITION* pVariableDefinition = parse<VARIABLE_DEFINITION>();

			if (pVariableDefinition == nullptr)
			{
				return nullptr;
			}

			return createNode(EXPRESSION(createNode(PRIMARY(pVariableDefinition))));
		}

		case long_identifier:
		case identifier:
		{
			// ambiguous case, look further ahead
			if (peekToken()->Kind == open_paren)
			{
				const size_t offset = getOffsetToClosing(close_paren);

				if (peekToken(offset)->Kind == TokenKind::open_brace)
				{
					goto parse_constructor;
				}

				else if (peekToken(offset)->Kind == TokenKind::pipe_operator)
				{
					goto parse_enum_value;
				}

				else
				{
					goto parse_function_call;
				}
			}

			// FUNCTION_CALL
			if (peekToken()->Kind == colon)
			{
			parse_function_call:

				FUNCTION_CALL* pFunctionCall = parse<FUNCTION_CALL>();

				if (pFunctionCall == nullptr)
				{
					return nullptr;
				}

				return createNode(EXPRESSION(createNode(PRIMARY(pFunctionCall))));
			}

			// CONSTRUCTOR
			if (peekToken()->Kind == open_brace)
			{
			parse_constructor:

				CONSTRUCTOR* pConstructor = parse<CONSTRUCTOR>();

				if (pConstructor == nullptr)
				{
					return nullptr;
				}

				return createNode(EXPRESSION(createNode(PRIMARY(pConstructor))));
			}

			// ENUM_VALUE
			if (peekToken()->Kind == pipe_operator)
			{
			parse_enum_value:

				ENUM_VALUE* pEnumValue = parse<ENUM_VALUE>();

				if (pEnumValue == nullptr)
				{
					return nullptr;
				}

				return createNode(EXPRESSION(createNode(PRIMARY(pEnumValue))));
			}

			// VARIABLE
			VARIABLE* pNamedVariable = parse<VARIABLE>();

			if (pNamedVariable == nullptr)
			{
				return nullptr;
			}

			return createNode(EXPRESSION(createNode(PRIMARY(pNamedVariable))));
		}

		case new_keyword:
		{
			POINTER_INIT* pPointerInit = parse<POINTER_INIT>();

			if (pPointerInit == nullptr)
			{
				return nullptr;
			}

			return createNode(EXPRESSION(createNode(PRIMARY(pPointerInit))));
		}

		case move_keyword:
		{
			POINTER_MOVE* pPointerMove = parse<POINTER_MOVE>();

			if (pPointerMove == nullptr)
			{
				return nullptr;
			}

			return createNode(EXPRESSION(createNode(PRIMARY(pPointerMove))));
		}

		case open_brace:
		{
			INITIALIZER_LIST* pInitializerList = parse<INITIALIZER_LIST>();

			if (pInitializerList == nullptr)
			{
				return nullptr;
			}

			return createNode(EXPRESSION(createNode(PRIMARY(pInitializerList))));
		}

		case open_paren:
		{
			ENCLOSED_EXPRESSION* pEnclosedExpression = parse<ENCLOSED_EXPRESSION>();

			if (pEnclosedExpression == nullptr)
			{
				return nullptr;
			}

			return createNode(EXPRESSION(createNode(PRIMARY(pEnclosedExpression))));
		}

		default:
		{
			(void)expectKind<integer_literal, float_literal, boolean_literal, string_literal, character_literal,
				identifier, new_keyword, move_keyword, open_brace, open_paren>();
			return nullptr;
		}
		}
	}

	EXPRESSION* NodeBuffer::parse_POSTFIX()
	{
		// even though left side can be any expression in the syntax, due to the order we parse expressions in
		// we can only have a primary expression at this point
		EXPRESSION* pLeft = parse_PRIMARY();

		if (pLeft == nullptr)
		{
			return nullptr;
		}

		while (token()->Kind == TokenKind::open_bracket || token()->Kind == TokenKind::dot)
		{
			// POSTFIX node to insert either ARRAY_ACCESS or MEMBER_ACCESS into
			POSTFIX* pPostfix = createNode(POSTFIX());

			if (token()->Kind == TokenKind::open_bracket)
			{
				(void)eat();

				EXPRESSION* pRight = parse<EXPRESSION>();

				if (pRight == nullptr)
				{
					return nullptr;
				}

				if (expectKind<TokenKind::close_bracket>() != SUCCESS)
				{
					return nullptr;
				}

				// set the POSTFIX expression to an ARRAY_ACCESS
				pPostfix->Set(
					createNode(ARRAY_ACCESS
						{
							.pArray = pLeft,
							.pIndex = pRight
						}
					)
				);
			}

			else if (token()->Kind == TokenKind::dot)
			{
				(void)eat();

				Token* pMemberNameToken = nullptr;
				if (expectKind<TokenKind::identifier>(&pMemberNameToken) != SUCCESS)
				{
					return nullptr;
				}

				// set the POSTFIX expression to a MEMBER_ACCESS
				pPostfix->Set(
					createNode(
						MEMBER_ACCESS
						{
							.pObject = pLeft,
							.pMemberNameToken = pMemberNameToken
						}
					)
				);
			}

			else
			{
				(void)expectKind<TokenKind::open_bracket, TokenKind::dot>();
				return nullptr;
			}

			// set the left to our newly-parsed POSTFIX expression
			/* MDFDA: pLeft->Set(pPostfix) is wrong as it modifies the pArray or pObject of the POSTFIX expression,
			* we should instead create a new expression and return it
			* pLeft->Set(pPostfix);
			*/
			pLeft = createNode(EXPRESSION(pPostfix));

		}

		return pLeft;	// if no POSTFIX was found, this returns a PRIMARY wrapped in an EXPRESSION as expected
	}

	EXPRESSION* NodeBuffer::parse_UNARY()
	{
		// operators '-' and '&' are ambiguous in their TokenKind so we expect based on value instead

		if (token()->Value != "!" && token()->Value != "-" && token()->Value != "&")
		{
			return parse_POSTFIX();
		}

		Token* pOpToken = nullptr;
		(void)expectValue({ "!", "-", "&" }, &pOpToken);

		EXPRESSION* pExpression = parse_POSTFIX();

		if (pExpression == nullptr)
		{
			return nullptr;
		}

		UNARY* pUnary = createNode(
			UNARY
			{
				.pOpToken = pOpToken,
				.pExpression = pExpression
			}
		);

		return createNode(EXPRESSION(pUnary));
	}

	EXPRESSION* NodeBuffer::parse_BINARY()
	{
		// handles *, / and %
		auto tryParseMultiplicativeExpression = [&]() -> EXPRESSION*
			{
				EXPRESSION* pLeft = parse_UNARY();

				if (pLeft == nullptr)
				{
					return nullptr;
				}

				while (token()->Value == "*" || token()->Value == "/" || token()->Value == "%")
				{
					Token* pOpToken = nullptr;
					(void)expectKind<TokenKind::binary_operator>(&pOpToken);

					EXPRESSION* pRight = parse_UNARY();

					if (pRight == nullptr)
					{
						return nullptr;
					}

					BINARY* pBinary = createNode(
						BINARY
						{
							.pOpToken = pOpToken,
							.pLeft = pLeft,
							.pRight = pRight
						}
					);

					pLeft = createNode(EXPRESSION(pBinary));
				}

				return pLeft;
			};

		// handles + and -
		auto tryParseAdditiveExpression = [&]() -> EXPRESSION*
			{
				EXPRESSION* pLeft = tryParseMultiplicativeExpression();

				if (pLeft == nullptr)
				{
					return nullptr;
				}

				while (token()->Value == "+" || token()->Value == "-")
				{
					Token* pOpToken = nullptr;
					(void)expectKind<TokenKind::binary_operator>(&pOpToken);

					EXPRESSION* pRight = tryParseMultiplicativeExpression();

					if (pRight == nullptr)
					{
						return nullptr;
					}

					BINARY* pBinary = createNode(
						BINARY
						{
							.pOpToken = pOpToken,
							.pLeft = pLeft,
							.pRight = pRight
						}
					);

					pLeft = createNode(EXPRESSION(pBinary));
				}

				return pLeft;
			};

		// handles ==, !=, <, <=, >, >=
		auto tryParseRelationalExpression = [&]() -> EXPRESSION*
			{
				EXPRESSION* pLeft = tryParseAdditiveExpression();

				if (pLeft == nullptr)
				{
					return nullptr;
				}

				while (token()->Value == "==" || token()->Value == "!=" || token()->Value == "<" || token()->Value == "<="
					|| token()->Value == ">" || token()->Value == ">=")
				{
					Token* pOpToken = nullptr;
					(void)expectKind<TokenKind::binary_operator>(&pOpToken);

					EXPRESSION* pRight = tryParseAdditiveExpression();

					if (pRight == nullptr)
					{
						return nullptr;
					}

					BINARY* pBinary = createNode(
						BINARY
						{
							.pOpToken = pOpToken,
							.pLeft = pLeft,
							.pRight = pRight
						}
					);

					pLeft = createNode(EXPRESSION(pBinary));
				}

				return pLeft;
			};

		// handles && and ||
		auto tryParseLogicalExpression = [&]() -> EXPRESSION*
			{
				EXPRESSION* pLeft = tryParseRelationalExpression();

				if (pLeft == nullptr)
				{
					return nullptr;
				}

				while (token()->Value == "&&" || token()->Value == "||")
				{
					Token* pOpToken = nullptr;
					(void)expectKind<TokenKind::binary_operator>(&pOpToken);

					EXPRESSION* pRight = tryParseRelationalExpression();

					if (pRight == nullptr)
					{
						return nullptr;
					}

					BINARY* pBinary = createNode(
						BINARY
						{
							.pOpToken = pOpToken,
							.pLeft = pLeft,
							.pRight = pRight
						}
					);

					pLeft = createNode(EXPRESSION(pBinary));
				}

				return pLeft;
			};

		// handles =
		auto tryParseAssignmentExpression = [&]() -> EXPRESSION*
			{
				EXPRESSION* pLeft = tryParseLogicalExpression();

				if (pLeft == nullptr)
				{
					return nullptr;
				}

				while (token()->Value == "=")
				{
					(void)eat();

					EXPRESSION* pRight = tryParseLogicalExpression();

					if (pRight == nullptr)
					{
						return nullptr;
					}

					ASSIGNMENT* pAssignment = createNode(
						ASSIGNMENT
						{
							.pVariable = pLeft,
							.pValue = pRight
						}
					);

					pLeft = createNode(EXPRESSION(pAssignment));
				}

				return pLeft;
			};

		return tryParseAssignmentExpression();
	}

	template<>
	ASSIGNMENT* NodeBuffer::parse()
	{
		EXPRESSION* pAssignment = parse<EXPRESSION>();

		if (pAssignment == nullptr)
		{
			return nullptr;
		}

		if (!pAssignment->Is<ASSIGNMENT>())
		{
			logErrorAtCurrentPosition("Expected an assignment.");
			return nullptr;
		}

		return pAssignment->Get<ASSIGNMENT>();
	}

	template<>
	EXPRESSION* NodeBuffer::parse()
	{
		return parse_BINARY();
	}

#pragma endregion

#pragma region Statements

	template<>
	STATEMENT* NodeBuffer::parse();

	template<>
	FOR_LOOP* NodeBuffer::parse()
	{
		if (expectKind<TokenKind::for_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		if (expectKind<TokenKind::open_paren>() != SUCCESS)
		{
			return nullptr;
		}

		EXPRESSION* pInitialization = parse<EXPRESSION>();

		if (pInitialization == nullptr)
		{
			return nullptr;
		}

		if (expectKind<TokenKind::semicolon>() != SUCCESS)
		{
			return nullptr;
		}

		EXPRESSION* pCondition = parse<EXPRESSION>();

		if (pCondition == nullptr)
		{
			return nullptr;
		}

		if (expectKind<TokenKind::semicolon>() != SUCCESS)
		{
			return nullptr;
		}

		EXPRESSION* pIncrement = parse<EXPRESSION>();

		if (pIncrement == nullptr)
		{
			return nullptr;
		}

		if (expectKind<TokenKind::close_paren>() != SUCCESS)
		{
			return nullptr;
		}

		STATEMENT* pBody = parse<STATEMENT>();

		if (pBody == nullptr)
		{
			return nullptr;
		}

		return createNode(
			FOR_LOOP
			{
				.pInitialization = pInitialization,
				.pCondition = pCondition,
				.pIncrement = pIncrement,
				.pBody = pBody
			}
		);
	}

	template<>
	WHILE_LOOP* NodeBuffer::parse()
	{
		if (expectKind<TokenKind::while_keyword>() != SUCCESS)
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

		return createNode(
			WHILE_LOOP
			{
				.pCondition = pCondition->pExpression,
				.pStatement = pStatement,
			}
		);
	}

	template<>
	IF_STATEMENT* NodeBuffer::parse()
	{
		if (expectKind<TokenKind::if_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		ENCLOSED_EXPRESSION* pCondition = parse<ENCLOSED_EXPRESSION>();

		if (pCondition == nullptr)
		{
			return nullptr;
		}

		Token* pCaptureNameToken = nullptr;
		if (token()->Kind == TokenKind::arrow)
		{
			(void)eat();

			if (expectKind<TokenKind::identifier>(&pCaptureNameToken) != SUCCESS)
			{
				return nullptr;
			}
		}

		STATEMENT* pStatement = parse<STATEMENT>();

		if (pStatement == nullptr)
		{
			return nullptr;
		}

		STATEMENT* pElseStatement = nullptr;
		if (token()->Kind == TokenKind::else_keyword)
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
				.pElseStatement = pElseStatement,
				.pCaptureNameToken = pCaptureNameToken
			}
		);
	}

	
	template<>
	SWITCH_STATEMENT* NodeBuffer::parse()
	{
		if (expectKind<TokenKind::switch_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		ENCLOSED_EXPRESSION* pSwitchValue = parse<ENCLOSED_EXPRESSION>();

		if (pSwitchValue == nullptr)
		{
			return nullptr;
		}

		if (expectKind<TokenKind::open_brace>() != SUCCESS)
		{
			return nullptr;
		}

		std::vector<std::tuple<EXPRESSION*, Token*, STATEMENT*>> cases;
		while (token()->Kind != TokenKind::close_brace)
		{
			if (expectKind<TokenKind::case_keyword>() != SUCCESS)
			{
				return nullptr;
			}

			ENCLOSED_EXPRESSION* pCaseValue = parse<ENCLOSED_EXPRESSION>();

			if (pCaseValue == nullptr)
			{
				return nullptr;
			}

			Token* pCaptureNameToken = nullptr;
			if (token()->Kind == TokenKind::arrow)
			{
				(void)eat();

				if (expectKind<TokenKind::identifier>(&pCaptureNameToken) != SUCCESS)
				{
					return nullptr;
				}
			}

			STATEMENT* pStatement = parse<STATEMENT>();

			if (pStatement == nullptr)
			{
				return nullptr;
			}

			cases.push_back({ pCaseValue->pExpression, pCaptureNameToken, pStatement });
		}

		(void)eat();

		return createNode(
			SWITCH_STATEMENT
			{
				.pSwitchValue = pSwitchValue->pExpression,
				.Cases = std::move(cases)
			}
		);
	}

	template<>
	STATEMENT_BLOCK* NodeBuffer::parse()
	{
		if (expectKind<TokenKind::open_brace>() != SUCCESS)
		{
			return nullptr;
		}

		std::vector<STATEMENT*> statements;
		while (token()->Kind != TokenKind::close_brace)
		{
			STATEMENT* pStatement = parse<STATEMENT>();

			if (pStatement == nullptr)
			{
				return nullptr;
			}

			statements.push_back(pStatement);
		}

		(void)eat();

		return createNode(
			STATEMENT_BLOCK
			{
				.Statements = std::move(statements)
			}
		);
	}

	template<>
	RETURN* NodeBuffer::parse()
	{
		if (expectKind<TokenKind::return_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		EXPRESSION* pExpression = nullptr;
		if (token()->Kind != TokenKind::semicolon)
		{
			pExpression = parse<EXPRESSION>();

			if (pExpression == nullptr)
			{
				return nullptr;
			}
		}

		if (expectKind<TokenKind::semicolon>() != SUCCESS)
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

	template<>
	STATEMENT* NodeBuffer::parse()
	{
		switch (token()->Kind)
		{
		case TokenKind::variable_keyword:
		case TokenKind::constant_keyword:
		{
			VARIABLE_DEFINITION* pVariableDefinition = parse<VARIABLE_DEFINITION>();
			if (pVariableDefinition == nullptr)
			{
				return nullptr;
			}

			if (expectKind<TokenKind::semicolon>() != SUCCESS)
			{
				return nullptr;
			}

			return createNode(STATEMENT(pVariableDefinition));
		}

		case TokenKind::identifier:
		case TokenKind::long_identifier:
		{
			STATEMENT* pStatement = nullptr;

			// handle function call
			if (peekToken()->Kind == TokenKind::open_paren || peekToken()->Kind == TokenKind::colon)
			{
				FUNCTION_CALL* pFunctionCall = parse<FUNCTION_CALL>();
				if (pFunctionCall == nullptr)
				{
					return nullptr;
				}

				pStatement = createNode(STATEMENT(pFunctionCall));
			}
			// handle assignment
			else
			{
				ASSIGNMENT* pAssignment = parse<ASSIGNMENT>();
				if (pAssignment == nullptr)
				{
					return nullptr;
				}

				pStatement = createNode(STATEMENT(pAssignment));
			}

			if (expectKind<TokenKind::semicolon>() != SUCCESS)
			{
				return nullptr;
			}

			return pStatement;
		}

		case TokenKind::for_keyword:
		{
			FOR_LOOP* pForLoop = parse<FOR_LOOP>();
			if (pForLoop == nullptr)
			{
				return nullptr;
			}

			return createNode(STATEMENT(pForLoop));
		}

		case TokenKind::while_keyword:
		{
			WHILE_LOOP* pWhileLoop = parse<WHILE_LOOP>();
			if (pWhileLoop == nullptr)
			{
				return nullptr;
			}

			return createNode(STATEMENT(pWhileLoop));
		}

		case TokenKind::if_keyword:
		{
			IF_STATEMENT* pIfStatement = parse<IF_STATEMENT>();
			if (pIfStatement == nullptr)
			{
				return nullptr;
			}

			return createNode(STATEMENT(pIfStatement));
		}

		case TokenKind::switch_keyword:
		{
			SWITCH_STATEMENT* pSwitchStatement = parse<SWITCH_STATEMENT>();
			if (pSwitchStatement == nullptr)
			{
				return nullptr;
			}

			return createNode(STATEMENT(pSwitchStatement));
		}

		case TokenKind::open_brace:
		{
			STATEMENT_BLOCK* pStatementBlock = parse<STATEMENT_BLOCK>();
			if (pStatementBlock == nullptr)
			{
				return nullptr;
			}

			return createNode(STATEMENT(pStatementBlock));
		}

		case TokenKind::return_keyword:
		{
			RETURN* pReturn = parse<RETURN>();
			if (pReturn == nullptr)
			{
				return nullptr;
			}

			return createNode(STATEMENT(pReturn));
		}

		default:
			expectKind<TokenKind::variable_keyword
				, TokenKind::constant_keyword
				, TokenKind::identifier
				, TokenKind::for_keyword
				, TokenKind::while_keyword
				, TokenKind::if_keyword
				, TokenKind::open_brace
				, TokenKind::return_keyword>();
			return nullptr;
		}
	}

#pragma endregion

#pragma region Annotations

	NodeBuffer::Result NodeBuffer::getAnnotation()
	{
		Result result;

		result = expectKind<TokenKind::pound>();
		if (result != SUCCESS)
		{
			return result;
		}

		result = expectKind<TokenKind::open_bracket>();
		if (result != SUCCESS)
		{
			return result;
		}

		Token* pNameToken = nullptr;
		result = expectKind<TokenKind::identifier>(&pNameToken);
		if (result != SUCCESS)
		{
			return result;
		}

		auto it = ANNOTATION_NAMES.find(pNameToken->Value);
		if (it == ANNOTATION_NAMES.end())
		{
			logErrorAtPreviousPosition("Unknown annotation '{0}'.", pNameToken->Value);
			return UNKNOWN_ANNOTATION;
		}

		AnnotationArgs arguments;
		if (token()->Kind == TokenKind::open_paren)
		{
			(void)eat();

			while (token()->Kind != TokenKind::close_paren)
			{
				Token* pArgumentToken = nullptr;
				result = expectKind<TokenKind::identifier>(&pArgumentToken);
				if (result != SUCCESS)
				{
					return result;
				}

				arguments.push_back(pArgumentToken);

				if (token()->Kind == TokenKind::comma)
				{
					(void)eat();
				}
			}

			(void)eat();
		}

		if (it->second == AnnotationKind::NoArgs && arguments.size() != 0)
		{
			logErrorAtCurrentPosition("Annotation '{0}' does not take any arguments.", pNameToken->Value);
			return INVALID_ANNOTATION;
		}

		if (it->second == AnnotationKind::SingleArg && arguments.size() != 1)
		{
			logErrorAtCurrentPosition("Annotation '{0}' only takes one argument.", pNameToken->Value);
			return INVALID_ANNOTATION;
		}

		if (it->second == AnnotationKind::MultiArgs && arguments.size() < 1)
		{
			logErrorAtCurrentPosition("Annotation '{0}' must have at least one argument.", pNameToken->Value);
			return INVALID_ANNOTATION;
		}

		if (m_CurrentAnnotations.contains(pNameToken->Value))
		{
			logErrorAtCurrentPosition("Duplicate annotation '{0}'.", pNameToken->Value);
			return DUPLICATE_ANNOTATION;
		}

		result = expectKind<TokenKind::close_bracket>();
		if (result != SUCCESS)
		{
			return result;
		}

		m_CurrentAnnotations[pNameToken->Value] = arguments;

		return SUCCESS;
	}

#pragma endregion

#pragma region Modules

	NodeBuffer::Result NodeBuffer::getImport()
	{
		Result result;

		result = expectKind<TokenKind::import_keyword>();
		if (result != SUCCESS)
		{
			return result;
		}

		Token* pIdentifierToken = nullptr;
		result = expectKind<TokenKind::identifier, TokenKind::long_identifier>(&pIdentifierToken);
		if (result != SUCCESS)
		{
			return result;
		}

		result = expectKind<TokenKind::semicolon>();
		if (result != SUCCESS)
		{
			return result;
		}

		m_ImportedModules.push_back(pIdentifierToken->Value);

		return result;
	}

#pragma endregion


	NodeBuffer::NodeBuffer(const Source& source, TokenBuffer& tokenBuffer)
		: m_Source(source)
		, m_TokenBuffer(tokenBuffer)
		, m_Allocator()
		, m_CurrentTokenIndex(0)
	{
	}

	bool NodeBuffer::Parse()
	{
		m_Allocator.Reset(m_TokenBuffer.NumTokens() * 80);	// TODO: remove magic number

		using enum TokenKind;

		while (!isEOF())
		{
			// check for annotation
			if (token()->Kind == pound)
			{
				if (getAnnotation() != SUCCESS)
				{
					return false;
				}
			}

			// check for import
			if (token()->Kind == import_keyword)
			{
				if (getImport() != SUCCESS)
				{
					return false;
				}

				continue;
			}

			// check for visibility modifier
			Visibility visibility = Visibility::Private;

			if (token()->Kind == public_keyword)
			{
				visibility = Visibility::Public;
				(void)eat();
			}
			else if (token()->Kind == export_keyword)
			{
				visibility = Visibility::Export;
				(void)eat();
			}

			switch (token()->Kind)
			{
			case variable_keyword:
			case constant_keyword:
			{
				VARIABLE_DEFINITION* pVariableDefinition = parse<VARIABLE_DEFINITION>();

				if (pVariableDefinition == nullptr)
				{
					return false;
				}

				if (m_AllSymbolNames.contains(pVariableDefinition->pDeclaration->pNameToken->Value))
				{
					logErrorAtToken(pVariableDefinition->pDeclaration->pNameToken, "Symbol with name '{0}' already exists in this module.", pVariableDefinition->pDeclaration->pNameToken->Value);
					return false;
				}

				m_GlobalVariableDefinitions[std::string(pVariableDefinition->pDeclaration->pNameToken->Value)] = Definition<VARIABLE_DEFINITION>(visibility, pVariableDefinition);
				m_AllSymbolNames.insert(pVariableDefinition->pDeclaration->pNameToken->Value);

				break;
			}

			case macro_keyword:
			{
				MACRO_DEFINITION* pMacroDefinition = parse<MACRO_DEFINITION>();

				if (pMacroDefinition == nullptr)
				{
					return false;
				}

				if (m_AllSymbolNames.contains(pMacroDefinition->pNameToken->Value))
				{
					logErrorAtToken(pMacroDefinition->pNameToken, "Symbol with name '{0}' already exists in this module.", pMacroDefinition->pNameToken->Value);
					return false;
				}

				m_MacroDefinitions[std::string(pMacroDefinition->pNameToken->Value)] = Definition<MACRO_DEFINITION>(visibility, pMacroDefinition);
				m_AllSymbolNames.insert(pMacroDefinition->pNameToken->Value);

				break;
			}

			case type_keyword:
			{
				TYPE_DEFINITION* pTypeDefinition = parse<TYPE_DEFINITION>();

				if (pTypeDefinition == nullptr)
				{
					return false;
				}

				if (m_AllSymbolNames.contains(pTypeDefinition->pTypeIdentifier->pNameToken->Value))
				{
					logErrorAtToken(pTypeDefinition->pTypeIdentifier->pNameToken,
						"Symbol with name '{0}' already exists in this module.", pTypeDefinition->pTypeIdentifier->pNameToken->Value);
					return false;
				}

				m_TypeDefinitions[std::string(pTypeDefinition->pTypeIdentifier->pNameToken->Value)] = Definition<TYPE_DEFINITION>(visibility, pTypeDefinition);
				m_AllSymbolNames.insert(pTypeDefinition->pTypeIdentifier->pNameToken->Value);

				break;
			}

			case function_keyword:
			case extern_keyword:
			{
				FUNCTION_DEFINITION* pFunctionDefinition = parse<FUNCTION_DEFINITION>();

				if (pFunctionDefinition == nullptr)
				{
					return false;
				}

				Token* pFunctionNameToken = pFunctionDefinition->pFunctionNameToken;

				if (pFunctionDefinition->pTypeIdentifier != nullptr)
				{
					Token* pTypeNameToken = pFunctionDefinition->pTypeIdentifier->pNameToken;
					const std::string fullFunctionName = GetMangledName("", pTypeNameToken, pFunctionNameToken);

					if (m_FunctionDefinitions.contains(fullFunctionName))
					{
						logErrorAtToken(pFunctionNameToken, "Member function '{0}' is already defined for type '{1}'.",
							pFunctionNameToken->Value, pTypeNameToken->Value);
						return false;
					}

					m_FunctionDefinitions[fullFunctionName] = Definition<FUNCTION_DEFINITION>(visibility, pFunctionDefinition);
					// not added to AllSymbolNames since member function names cannot clash with other node names
				}

				else
				{
					if (m_AllSymbolNames.contains(pFunctionNameToken->Value))
					{
						logErrorAtToken(pFunctionNameToken, "Symbol with name '{0}' already exists in this module.", pFunctionNameToken->Value);
						return false;
					}

					m_FunctionDefinitions[std::string(pFunctionNameToken->Value)] = Definition<FUNCTION_DEFINITION>(visibility, pFunctionDefinition);
					m_AllSymbolNames.insert(pFunctionNameToken->Value);
				}

				break;
			}

			default:
			{
				(void)expectKind<macro_keyword, type_keyword, function_keyword, extern_keyword>();
				(void)eat();
				return false;
			}
			}
		}

		return true;
	}
}