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

	std::string Parser::stringVectorToString(const std::vector<std::string>& tokens) const
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

	bool Parser::isEOF() const
	{
		return m_CurrentTokenIndex >= (m_TokenBuffers.NumTokens() - 1);
	}

	bool Parser::hasNext() const
	{
		return !isEOF();
	}

	Token* Parser::token()
	{
		return m_TokenBuffers.GetToken(m_CurrentTokenIndex);
	}

	Token* Parser::peekToken(size_t offset)
	{
		return m_TokenBuffers.GetToken(m_CurrentTokenIndex + offset + 1);
	}

	Parser::Result Parser::eat()
	{
		if (isEOF())
		{
			logErrorAtCurrentPosition("Unexpected end of file.");
			return EOF_REACHED;
		}

		++m_CurrentTokenIndex;
		return SUCCESS;
	}

	Parser::Result Parser::expectValue(const std::vector<std::string>& values, Token** ppToken)
	{
		bool found = false;
		for (const std::string& value : values)
		{
			if (value == m_TokenBuffers.GetToken(m_CurrentTokenIndex)->Value)
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
			*ppToken = m_TokenBuffers.GetToken(m_CurrentTokenIndex);
		}

		++m_CurrentTokenIndex;
		return SUCCESS;
	}

	Parser::Result Parser::checkAnnotations(const std::unordered_set<std::string_view>& validAnnotations)
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

	bool Parser::consumeAnnotation(const std::string_view& name, AnnotationArgs* pArgs)
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

	size_t Parser::getOffsetToClosing(TokenKind closing)
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
	TYPE* Parser::parse();

	template<>
	TYPE_NAME* Parser::parse()
	{
		Token* pNameToken = nullptr;
		if (expectKind<TokenKind::identifier>(&pNameToken) != SUCCESS)
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
	VARIABLE_DECLARATION* Parser::parse<VARIABLE_DECLARATION>();

	template<>
	STRUCT_TYPE* Parser::parse()
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
	LITERAL* Parser::parse<LITERAL>();

	template<>
	ARRAY_TYPE* Parser::parse()
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
	MACRO_CALL* Parser::parse();

	template<>
	TYPE* Parser::parse()
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

		VariantNode<TYPE_NAME, STRUCT_TYPE, ARRAY_TYPE, MACRO_CALL> type;
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
	GENERIC_PARAMETER* Parser::parse();

	template<>
	TYPE_IDENTIFIER* Parser::parse()
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
	TYPE_DEFINITION* Parser::parse()
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

		if (m_NamedNodes.TypeDefinitions.contains(pTypeIdentifier->pNameToken->Value))
		{
			logErrorAtPreviousPosition("Type '{0}' is already defined.", pTypeIdentifier->pNameToken->Value);
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

		TYPE_DEFINITION* pTypeDefinition = createNode(
			TYPE_DEFINITION
			{
				.pTypeIdentifier = pTypeIdentifier,
				.pType = pType,
			}
		);

		m_NamedNodes.TypeDefinitions[pTypeIdentifier->pNameToken->Value] = pTypeDefinition;

		return pTypeDefinition;
	}

#pragma endregion

#pragma region Variables

	template<>
	VARIABLE* Parser::parse()
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
	VARIABLE_DECLARATION* Parser::parse()
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

		if (expectKind<TokenKind::colon>() != SUCCESS)
		{
			return nullptr;
		}

		TYPE* pType = parse<TYPE>();

		if (pType == nullptr)
		{
			return nullptr;
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
	EXPRESSION* Parser::parse<EXPRESSION>();

	template<>
	VARIABLE_DEFINITION* Parser::parse()
	{
		VARIABLE_DECLARATION* pVariableDeclaration = parse<VARIABLE_DECLARATION>();

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

#pragma region Annotations

	Parser::Result Parser::getAnnotation()
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

#pragma region Functions

	template<>
	FUNCTION_PARAMETER* Parser::parse()
	{
		switch (token()->Kind)
		{
		case TokenKind::variable_keyword:
		case TokenKind::constant_keyword:
		{
			VARIABLE_DECLARATION* pVariableDeclaration = parse<VARIABLE_DECLARATION>();

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
	RETURN_TYPE* Parser::parse()
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
	FUNCTION_TYPE* Parser::parse(bool allowVarArg, bool allowSelf)
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
	STATEMENT_BLOCK* Parser::parse<STATEMENT_BLOCK>();

	template<>
	FUNCTION_DEFINITION* Parser::parse()
	{
		if (checkAnnotations({ }) != SUCCESS)
		{
			return nullptr;
		}

		if (expectKind<TokenKind::function_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		TYPE_IDENTIFIER* pTypeIdentifier = nullptr;
		if (peekToken()->Kind == TokenKind::colon 
			|| peekToken(getOffsetToClosing(TokenKind::close_paren))->Kind == TokenKind::colon)
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

		// check for existing member function
		if (pTypeIdentifier != nullptr)
		{
			if (m_NamedNodes.MemberFunctionDefinitions.contains(pTypeIdentifier->pNameToken->Value)
				&& m_NamedNodes.MemberFunctionDefinitions[pTypeIdentifier->pNameToken->Value].contains(pFunctionNameToken->Value))
			{
				logErrorAtPreviousPosition("Member function '{0}:{1}' is already defined.", pTypeIdentifier->pNameToken->Value, pFunctionNameToken->Value);
				return nullptr;
			}
		}

		// check for existing function
		else if (m_NamedNodes.FunctionDefinitions.contains(pFunctionNameToken->Value))
		{
			logErrorAtPreviousPosition("Function '{0}' is already defined.", pFunctionNameToken->Value);
			return nullptr;
		}

		FUNCTION_TYPE* pFunctionType = parse<FUNCTION_TYPE>(/*allowVarArg*/false, /*allowSelf*/true);

		if (pFunctionType == nullptr)
		{
			return nullptr;
		}

		STATEMENT_BLOCK* pBody = parse<STATEMENT_BLOCK>();

		if (pBody == nullptr)
		{
			return nullptr;
		}

		FUNCTION_DEFINITION* pFunctionDefinition = createNode(
			FUNCTION_DEFINITION
			{
				.pTypeIdentifier = pTypeIdentifier,
				.pFunctionNameToken = pFunctionNameToken,
				.pFunctionType = pFunctionType,
				.pBody = pBody
			}
		);

		if (pTypeIdentifier != nullptr)
		{
			m_NamedNodes.MemberFunctionDefinitions[pTypeIdentifier->pNameToken->Value][pFunctionNameToken->Value] = pFunctionDefinition;
		}

		else
		{
			m_NamedNodes.FunctionDefinitions[pFunctionNameToken->Value] = pFunctionDefinition;
		}

		return pFunctionDefinition;
	}

	template<>
	EXTERN_DEFINITION* Parser::parse()
	{
		if (checkAnnotations({ }) != SUCCESS)
		{
			return nullptr;
		}

		if (expectKind<TokenKind::extern_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		Token* pFunctionNameToken = nullptr;
		if (expectKind<TokenKind::identifier>(&pFunctionNameToken) != SUCCESS)
		{
			return nullptr;
		}

		if (m_NamedNodes.ExternDefinitions.contains(pFunctionNameToken->Value))
		{
			logErrorAtPreviousPosition("Extern '{0}' is already defined!", pFunctionNameToken->Value);
			return nullptr;
		}

		FUNCTION_TYPE* pFunctionType = parse<FUNCTION_TYPE>(/*allowVarArg*/true, /*allowSelf*/false);

		if (pFunctionType == nullptr)
		{
			return nullptr;
		}

		if (expectKind<TokenKind::semicolon>() != SUCCESS)
		{
			return nullptr;
		}

		EXTERN_DEFINITION* pExternFunctionDefinition = createNode(
			EXTERN_DEFINITION
			{
				.pNameToken = pFunctionNameToken,
				.pFunctionType = pFunctionType
			}
		);

		m_NamedNodes.ExternDefinitions[pFunctionNameToken->Value] = pExternFunctionDefinition;

		return pExternFunctionDefinition;
	}

	template<>
	FUNCTION_CALL* Parser::parse()
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
		if (expectKind<TokenKind::identifier>(&pFunctionNameToken) != SUCCESS)
		{
			return nullptr;
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
	GENERIC_PARAMETER* Parser::parse()
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
	MACRO_STATEMENT* Parser::parse();

	template<>
	MACRO_VARIABLE_IDENTIFIER* Parser::parse()
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
	MACRO_CALL* Parser::parse()
	{
		if (expectKind<TokenKind::at_symbol>() != SUCCESS)
		{
			return nullptr;
		}

		Token* pMacroNameToken = nullptr;

		if (expectKind<TokenKind::identifier>(&pMacroNameToken) != SUCCESS)
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
	MACRO_DEFINITION* Parser::parse(bool isLocalMacro)
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

		if (!isLocalMacro)
		{
			if (m_NamedNodes.MacroDefinitions.contains(pNameToken->Value))
			{
				logErrorAtPreviousPosition("Macro '{0}' is already defined.", pNameToken->Value);
				return nullptr;
			}
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

		MACRO_DEFINITION* pMacroDefinition = createNode(
			MACRO_DEFINITION
			{
				.pNameToken = pNameToken,
				.Parameters = std::move(parameters),
				.ReturnType = returnType,
				.Body = std::move(macroStatements)
			}
		);

		if (!isLocalMacro)
		{
			m_NamedNodes.MacroDefinitions[pNameToken->Value] = pMacroDefinition;
		}

		return pMacroDefinition;
	}

	// MACRO_RETURN:	return_keyword ( MACRO_VARIABLE_IDENTIFIER | MACRO_CALL | TYPE ) semicolon ;
	template<>
	MACRO_RETURN* Parser::parse()
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
	MACRO_STATEMENT* Parser::parse()
	{
		switch (token()->Kind)
		{
		case TokenKind::macro_keyword:
		{
			MACRO_DEFINITION* pMacroDefinition = parse<MACRO_DEFINITION>(/*isLocalMacro*/true);

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
	EXPRESSION* Parser::parse();

	template<>
	LITERAL* Parser::parse()
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
	CONSTRUCTOR* Parser::parse()
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
	POINTER_INIT* Parser::parse()
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
	POINTER_MOVE* Parser::parse()
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
	INITIALIZER_LIST* Parser::parse()
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
	ENCLOSED_EXPRESSION* Parser::parse()
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

	EXPRESSION* Parser::parse_PRIMARY()
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

				else
				{
					goto parse_function_call;
				}
			}

			// FUNCTION_CALL
			if (peekToken()->Kind == colon)
			{
			parse_function_call:	// we jump here in the case where we are unsure if we have a function call or a constructor

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
			parse_constructor:		// we jump here in the case where we are unsure if we have a function call or a constructor

				CONSTRUCTOR* pConstructor = parse<CONSTRUCTOR>();

				if (pConstructor == nullptr)
				{
					return nullptr;
				}

				return createNode(EXPRESSION(createNode(PRIMARY(pConstructor))));
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

	EXPRESSION* Parser::parse_POSTFIX()
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

	EXPRESSION* Parser::parse_UNARY()
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

	EXPRESSION* Parser::parse_BINARY()
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
	ASSIGNMENT* Parser::parse()
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
	EXPRESSION* Parser::parse()
	{
		return parse_BINARY();
	}

#pragma endregion

#pragma region Statements

	template<>
	STATEMENT* Parser::parse();

	template<>
	FOR_LOOP* Parser::parse()
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
	WHILE_LOOP* Parser::parse()
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
	IF_STATEMENT* Parser::parse()
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
				.pElseStatement = pElseStatement
			}
		);
	}

	template<>
	STATEMENT_BLOCK* Parser::parse()
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
	RETURN* Parser::parse()
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
	STATEMENT* Parser::parse()
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

	NamedNodes Parser::Parse()
	{
		using enum TokenKind;

		while (!isEOF())
		{
			switch (token()->Kind)
			{
			case macro_keyword:
			{
				if (parse<MACRO_DEFINITION>(/*isLocalMacro*/false) == nullptr)
				{
					ASSERT(false, "");
				}
				break;
			}

			case pound:
				if (getAnnotation() != SUCCESS)
				{
					ASSERT(false, "");
				}
				break;

			case type_keyword:
				if (parse<TYPE_DEFINITION>() == nullptr)
				{
					ASSERT(false, "");
				}
				break;

			case function_keyword:
				if (parse<FUNCTION_DEFINITION>() == nullptr)
				{
					ASSERT(false, "");
				}
				break;

			case extern_keyword:
				if (parse<EXTERN_DEFINITION>() == nullptr)
				{
					ASSERT(false, "");
				}
				break;

			default:
				(void)expectKind<type_keyword, function_keyword, extern_keyword>();
				(void)eat();
				break;
			}
		}

		return std::move(m_NamedNodes);
	}
}