#include "Parser.hpp"

namespace AlloyCompiler
{
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
		return m_CurrentTokenID >= m_TokenBuffers.LastTokenID();
	}

	bool Parser::hasNext() const
	{
		return !isEOF();
	}

	TokenKind Parser::kind() const
	{
		return m_TokenBuffers.GetKind(m_CurrentTokenID);
	}

	TokenKind Parser::peek() const
	{
		if (isEOF())
		{
			return TokenKind::end_of_file;
		}

		return m_TokenBuffers.GetKind(m_CurrentTokenID + 1);
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

	Parser::Result Parser::expectValue(const std::vector<std::string>& values, std::string_view* pValueStringView)
	{
		bool found = false;
		for (const std::string& value : values)
		{
			if (value == m_TokenBuffers.GetValue(m_CurrentTokenID))
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			logErrorAtPosition("Expected {0}.", stringVectorToString(values));
			return UNEXPECTED_VALUE;
		}

		if (pValueStringView != nullptr)
		{
			*pValueStringView = m_TokenBuffers.GetValue(m_CurrentTokenID);
		}

		++m_CurrentTokenID;
		return SUCCESS;
	}

	Parser::Result Parser::checkAnnotations(const std::unordered_set<std::string_view>& validAnnotations)
	{
		Result result = SUCCESS;

		for (auto& [name, args] : m_CurrentAnnotations)
		{
			if (!validAnnotations.contains(name))
			{
				logErrorAtPosition("Annotation '{0}' is not valid here.", name);
				result = INVALID_ANNOTATION;
			}
		}

		return result;
	}

#pragma endregion

#pragma region Types

	template<>
	TYPE* Parser::parse();

	template<>
	NAMED_TYPE* Parser::parse()
	{
		if (checkAnnotations({ }) != SUCCESS)
		{
			return nullptr;
		}

		std::string_view name;
		if (expect<TokenKind::identifier>(&name) != SUCCESS)
		{
			return nullptr;
		}

		return createNode(
			NAMED_TYPE
			{
				.Name = name
			}
		);
	
	}

	template<>
	NAMED_VARIABLE_DECLARATION* Parser::parse<NAMED_VARIABLE_DECLARATION>();

	template<>
	STRUCT_TYPE* Parser::parse()
	{
		if (expect<TokenKind::struct_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		if (expect<TokenKind::open_brace>() != SUCCESS)
		{
			return nullptr;
		}

		std::vector<NAMED_VARIABLE_DECLARATION*> members;
		while (kind() != TokenKind::close_brace)
		{
			NAMED_VARIABLE_DECLARATION* pMember = parse<NAMED_VARIABLE_DECLARATION>();

			if (pMember == nullptr)
			{
				return nullptr;
			}

			members.push_back(pMember);

			if (kind() == TokenKind::comma)
			{
				(void)eat();
			}
			else if (kind() != TokenKind::close_brace)
			{
				(void)expect<TokenKind::comma, TokenKind::close_brace>();
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
		if (expect<TokenKind::open_bracket>() != SUCCESS)
		{
			return nullptr;
		}

		TYPE* pType = parse<TYPE>();

		if (pType == nullptr)
		{
			return nullptr;
		}

		LITERAL* pSizeLiteral = nullptr;
		if (kind() == TokenKind::semicolon)
		{
			(void)eat();

			pSizeLiteral = parse<LITERAL>();

			if (pSizeLiteral == nullptr)
			{
				return nullptr;
			}

			if (pSizeLiteral->Type != LiteralType::Integer)
			{
				logErrorAtPosition("Array size must be an integer.");
				return nullptr;
			}
		}

		if (expect<TokenKind::close_bracket>() != SUCCESS)
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
	TYPE* Parser::parse()
	{
		TypeModifier modifier = TypeModifier::None;

		if (value() == "&")
		{
			modifier = TypeModifier::Reference;
		}
		else if (value() == "*")
		{
			modifier = TypeModifier::Pointer;
		}

		if (modifier != TypeModifier::None)
		{
			(void)eat();
		}

		using enum TokenKind;

		VariantNode<NAMED_TYPE, STRUCT_TYPE, ARRAY_TYPE> type;
		switch (kind())
		{
		case identifier:
		{
			NAMED_TYPE* pNamedType = parse<NAMED_TYPE>();

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
			(void)expect<identifier, struct_keyword, open_bracket>();
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
	NAMED_TYPE_DEFINITION* Parser::parse()
	{
		if (expect<TokenKind::type_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		std::string_view name;
		if (expect<TokenKind::identifier>(&name) != SUCCESS)
		{
			return nullptr;
		}

		if (namedNodeExists<NAMED_TYPE_DEFINITION>(name))
		{
			logErrorAtPosition("Type '{0}' is already defined.", name);
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

		NAMED_TYPE_DEFINITION* pTypeDefinition = createNode(
			NAMED_TYPE_DEFINITION
			{
				.Name = name,
				.pType = pType
			}
		);

		addNamedNode(name, pTypeDefinition);

		return pTypeDefinition;
	}

#pragma endregion

#pragma region Variables

	template<>
	NAMED_VARIABLE* Parser::parse()
	{
		std::string_view name;
		if (expect<TokenKind::identifier>(&name) != SUCCESS)
		{
			return nullptr;
		}

		return createNode(
			NAMED_VARIABLE
			{
				.Name = name
			}
		);
	}

	template<>
	NAMED_VARIABLE_DECLARATION* Parser::parse()
	{
		TokenKind tokenKind;
		if (expect<TokenKind::variable_keyword, TokenKind::constant_keyword>(&tokenKind) != SUCCESS)
		{
			return nullptr;
		}

		VariableType varType;

		if (tokenKind == TokenKind::variable_keyword)
		{
			varType = VariableType::Variable;
		}
		else
		{
			varType = VariableType::Constant;
		}

		std::string_view name;
		if (expect<TokenKind::identifier>(&name) != SUCCESS)
		{
			return nullptr;
		}

		if (expect<TokenKind::colon>() != SUCCESS)
		{
			return nullptr;
		}

		TYPE* pType = parse<TYPE>();
		
		if (pType == nullptr)
		{
			return nullptr;
		}

		return createNode(
			NAMED_VARIABLE_DECLARATION
			{
				.VarType = varType,
				.Name = name,
				.pType = pType
			}
		);
	}

	template<>
	EXPRESSION* Parser::parse<EXPRESSION>();

	template<>
	NAMED_VARIABLE_DEFINITION* Parser::parse()
	{
		NAMED_VARIABLE_DECLARATION* pVariableDeclaration = parse<NAMED_VARIABLE_DECLARATION>();

		if (pVariableDeclaration == nullptr)
		{
			return nullptr;
		}

		if (expect<TokenKind::assignment_operator>() != SUCCESS)
		{
			return nullptr;
		}

		EXPRESSION* pValue = parse<EXPRESSION>();

		if (pValue == nullptr)
		{
			return nullptr;
		}

		return createNode(
			NAMED_VARIABLE_DEFINITION
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

		result = expect<TokenKind::pound>();
		if (result != SUCCESS)
		{
			return result;
		}

		result = expect<TokenKind::open_bracket>();
		if (result != SUCCESS)
		{
			return result;
		}

		std::string_view name;
		result = expect<TokenKind::identifier>(&name);
		if (result != SUCCESS)
		{
			return result;
		}

		auto it = ANNOTATION_NAMES.find(name);
		if (it == ANNOTATION_NAMES.end())
		{
			logErrorAtPosition("Unknown annotation '{0}'.", name);
			return UNKNOWN_ANNOTATION;
		}

		AnnotationArgs arguments;
		if (kind() == TokenKind::open_paren)
		{
			(void)eat();

			while (kind() != TokenKind::close_paren)
			{
				std::string_view argument;
				result = expect<TokenKind::identifier>(&argument);
				if (result != SUCCESS)
				{
					return result;
				}

				arguments.push_back(argument);

				if (kind() == TokenKind::comma)
				{
					(void)eat();
				}
			}

			(void)eat();
		}

		if (it->second == AnnotationKind::NoArgs && arguments.size() != 0)
		{
			logErrorAtPosition("Annotation '{0}' does not take any arguments.", name);
			return INVALID_ANNOTATION;
		}

		if (it->second == AnnotationKind::SingleArg && arguments.size() != 1)
		{
			logErrorAtPosition("Annotation '{0}' only takes one argument.", name);
			return INVALID_ANNOTATION;
		}

		if (it->second == AnnotationKind::MultiArgs && arguments.size() == 1)
		{
			logErrorAtPosition("Annotation '{0}' must have at least one argument.", name);
			return INVALID_ANNOTATION;
		}

		if (m_CurrentAnnotations.contains(name))
		{
			logErrorAtPosition("Duplicate annotation '{0}'.", name);
			return DUPLICATE_ANNOTATION;
		}

		m_CurrentAnnotations[name] = arguments;

		return SUCCESS;
	}

#pragma endregion

#pragma region Functions

	template<>
	RETURN_TYPE* Parser::parse()
	{
		ReturnType retType = ReturnType::Copy;

		if (kind() == TokenKind::variable_keyword)
		{
			retType = ReturnType::Variable;
		}
		else if (kind() == TokenKind::constant_keyword)
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
	FUNCTION_SIGNATURE* Parser::parse(bool allowVarArg)
	{
		if (expect<TokenKind::function_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		std::string_view name;
		if (expect<TokenKind::identifier>(&name))
		{
			return nullptr;
		}

		if (expect<TokenKind::open_paren>())
		{
			return nullptr;
		}

		bool isVarArg = false;
		std::vector<NAMED_VARIABLE_DECLARATION*> parameters;
		while (kind() != TokenKind::close_paren)
		{
			if (kind() == TokenKind::ellipsis)
			{
				(void)eat();

				if (!allowVarArg)
				{
					logErrorAtPosition("Only extern functions can have variable arguments.");
					return nullptr;
				}

				isVarArg = true;

				if (kind() != TokenKind::close_paren)
				{
					logErrorAtPosition("Variable argument specifier '...' must be the last parameter.");
					return nullptr;
				}

				break;
			}

			NAMED_VARIABLE_DECLARATION* pVariableDeclaration = parse<NAMED_VARIABLE_DECLARATION>();

			if (pVariableDeclaration == nullptr)
			{
				return nullptr;
			}

			parameters.push_back(pVariableDeclaration);

			if (kind() == TokenKind::comma)
			{
				(void)eat();
			}
		}

		(void)eat();

		RETURN_TYPE* pReturnType = nullptr;
		if (kind() == TokenKind::arrow)
		{
			(void)eat();

			pReturnType = parse<RETURN_TYPE>();

			if (pReturnType == nullptr)
			{
				return nullptr;
			}
		}

		return createNode(
			FUNCTION_SIGNATURE
			{
				.Name = name,
				.Parameters = parameters,
				.pReturnType = pReturnType
			}
		);
	}

	template<>
	STATEMENT_BLOCK* Parser::parse<STATEMENT_BLOCK>();

	template<>
	NAMED_FUNCTION_DEFINITION* Parser::parse()
	{
		if (checkAnnotations({ }) != SUCCESS)
		{
			return nullptr;
		}

		FUNCTION_SIGNATURE* pFunctionSignature = parse<FUNCTION_SIGNATURE>(/*allowVarArg*/false);

		if (pFunctionSignature == nullptr)
		{
			return nullptr;
		}

		if (namedNodeExists<NAMED_FUNCTION_DEFINITION>(pFunctionSignature->Name))
		{
			if (namedNodeExists<EXTERN_FUNCTION_DEFINITION>(pFunctionSignature->Name))
			{
				logErrorAtPosition("Function '{0}' is already defined.", pFunctionSignature->Name);
				return nullptr;
			}
		}

		STATEMENT_BLOCK* pBody = parse<STATEMENT_BLOCK>();

		if (pBody == nullptr)
		{
			return nullptr;
		}

		NAMED_FUNCTION_DEFINITION* pFunctionDefinition = createNode(
			NAMED_FUNCTION_DEFINITION
			{
				.pSignature = pFunctionSignature,
				.pBody = pBody
			}
		);

		addNamedNode(pFunctionSignature->Name, pFunctionDefinition);

		return pFunctionDefinition;
	}

	template<>
	EXTERN_FUNCTION_DEFINITION* Parser::parse()
	{
		if (checkAnnotations({ }) != SUCCESS)
		{
			return nullptr;
		}

		if (expect<TokenKind::extern_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		FUNCTION_SIGNATURE* pFunctionSignature = parse<FUNCTION_SIGNATURE>(/*allowVarArg*/true);

		if (pFunctionSignature == nullptr)
		{
			return nullptr;
		}

		if (namedNodeExists<EXTERN_FUNCTION_DEFINITION>(pFunctionSignature->Name))
		{
			logErrorAtPosition("Extern function '{0}' is already defined.", pFunctionSignature->Name);
			return nullptr;
		}

		if (expect<TokenKind::semicolon>() != SUCCESS)
		{
			return nullptr;
		}

		EXTERN_FUNCTION_DEFINITION* pExternFunctionDefinition = createNode(
			EXTERN_FUNCTION_DEFINITION
			{
				.pSignature = pFunctionSignature
			}
		);

		addNamedNode(pFunctionSignature->Name, pExternFunctionDefinition);

		return pExternFunctionDefinition;
	}

	template<>
	FUNCTION_CALL* Parser::parse()
	{
		std::string_view functionName;
		if (expect<TokenKind::identifier>(&functionName) != SUCCESS)
		{
			return nullptr;
		}

		if (expect<TokenKind::open_paren>() != SUCCESS)
		{
			return nullptr;
		}

		std::vector<EXPRESSION*> arguments;
		while (kind() != TokenKind::close_paren)
		{
			EXPRESSION* pArgument = parse<EXPRESSION>();

			if (pArgument == nullptr)
			{
				return nullptr;
			}

			arguments.push_back(pArgument);

			if (kind() == TokenKind::comma)
			{
				(void)eat();
			}
		}

		(void)eat();

		return createNode(
			FUNCTION_CALL
			{
				.Function = functionName,
				.Arguments = arguments
			}
		);
	}

#pragma endregion

#pragma region Expressions

	template<>
	EXPRESSION* Parser::parse();

	template<>
	LITERAL* Parser::parse()
	{
		using enum TokenKind;

		TokenKind kind;
		std::string_view value;
		if (expect<integer_literal,
			float_literal,
			boolean_literal,
			string_literal,
			character_literal>(&kind, &value) != SUCCESS)
		{
			return nullptr;
		}

		LiteralType literalType;
		switch (kind)
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
		}

		return createNode(
			LITERAL
			{
				.Type = literalType,
				.Value = value
			}
		);
	}

	template<>
	CONSTRUCTOR* Parser::parse()
	{
		NAMED_TYPE* pNamedType = parse<NAMED_TYPE>();

		if (pNamedType == nullptr)
		{
			return nullptr;
		}

		if (expect<TokenKind::open_brace>() != SUCCESS)
		{
			return nullptr;
		}

		std::vector<std::pair<std::string_view, EXPRESSION*>> arguments;
		while (kind() != TokenKind::close_brace)
		{
			std::string_view memberName;
			if (expect<TokenKind::identifier>(&memberName) != SUCCESS)
			{
				return nullptr;
			}

			if (expect<TokenKind::assignment_operator>() != SUCCESS)
			{
				return nullptr;
			}

			EXPRESSION* pValue = parse<EXPRESSION>();

			if (pValue == nullptr)
			{
				return nullptr;
			}

			arguments.push_back({ memberName, pValue });

			if (kind() == TokenKind::comma)
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
		if (expect<TokenKind::new_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		EXPRESSION* pValue = nullptr;
		EXPRESSION* pSize = nullptr;

		// handle array allocation
		if (kind() == TokenKind::open_bracket)
		{
			(void)eat();

			pValue = parse<EXPRESSION>();

			if (pValue == nullptr)
			{
				return nullptr;
			}

			if (expect<TokenKind::semicolon>() != SUCCESS)
			{
				return nullptr;
			}

			pSize = parse<EXPRESSION>();

			if (pSize == nullptr)
			{
				return nullptr;
			}

			if (expect<TokenKind::close_bracket>() != SUCCESS)
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
		if (expect<TokenKind::move_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		// TODO: pointer could be struct member, array member, ... need to fix!
		NAMED_VARIABLE* pNamedVariable = parse<NAMED_VARIABLE>();

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
		if (expect<TokenKind::open_brace>() != SUCCESS)
		{
			return nullptr;
		}

		std::vector<EXPRESSION*> values;
		while (kind() != TokenKind::close_brace)
		{
			EXPRESSION* pExpression = parse<EXPRESSION>();

			if (pExpression == nullptr)
			{
				return nullptr;
			}

			values.push_back(pExpression);

			if (kind() == TokenKind::comma)
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
		if (expect<TokenKind::open_paren>() != SUCCESS)
		{
			return nullptr;
		}

		EXPRESSION* pExpression = parse<EXPRESSION>();

		if (pExpression == nullptr)
		{
			return nullptr;
		}

		if (expect<TokenKind::close_paren>() != SUCCESS)
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
		EXPRESSION expression;

		using enum TokenKind;

		switch (kind())
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

			expression.Set(createNode(PRIMARY(pLiteral)));
			break;
		}

		case identifier:
		{
			// NAMED_VARIABLE_DEFINITION
			if (peek() == variable_keyword || peek() == constant_keyword)
			{
				NAMED_VARIABLE_DEFINITION* pVariableDefinition = parse<NAMED_VARIABLE_DEFINITION>();

				if (pVariableDefinition == nullptr)
				{
					return nullptr;
				}

				expression.Set(createNode(PRIMARY(pVariableDefinition)));
				break;
			}

			// FUNCTION_CALL
			if (peek() == open_paren)
			{
				FUNCTION_CALL* pFunctionCall = parse<FUNCTION_CALL>();

				if (pFunctionCall == nullptr)
				{
					return nullptr;
				}

				expression.Set(createNode(PRIMARY(pFunctionCall)));
			}

			// CONSTRUCTOR
			if (peek() == open_brace)
			{
				CONSTRUCTOR* pConstructor = parse<CONSTRUCTOR>();

				if (pConstructor == nullptr)
				{
					return nullptr;
				}

				expression.Set(createNode(PRIMARY(pConstructor)));
				break;
			}

			// NAMED_VARIABLE
			NAMED_VARIABLE* pNamedVariable = parse<NAMED_VARIABLE>();

			if (pNamedVariable == nullptr)
			{
				return nullptr;
			}

			expression.Set(createNode(PRIMARY(pNamedVariable)));
			break;
		}

		case new_keyword:
		{
			POINTER_INIT* pPointerInit = parse<POINTER_INIT>();

			if (pPointerInit == nullptr)
			{
				return nullptr;
			}

			expression.Set(createNode(PRIMARY(pPointerInit)));
			break;
		}

		case move_keyword:
		{
			POINTER_MOVE* pPointerMove = parse<POINTER_MOVE>();

			if (pPointerMove == nullptr)
			{
				return nullptr;
			}

			expression.Set(createNode(PRIMARY(pPointerMove)));
			break;
		}

		case open_brace:
		{
			INITIALIZER_LIST* pInitializerList = parse<INITIALIZER_LIST>();

			if (pInitializerList == nullptr)
			{
				return nullptr;
			}

			expression.Set(createNode(PRIMARY(pInitializerList)));
			break;
		}

		case open_paren:
		{
			ENCLOSED_EXPRESSION* pEnclosedExpression = parse<ENCLOSED_EXPRESSION>();

			if (pEnclosedExpression == nullptr)
			{
				return nullptr;
			}

			expression.Set(createNode(PRIMARY(pEnclosedExpression)));
			break;
		}

		default:
		{
			(void)expect<integer_literal, float_literal, boolean_literal, string_literal, character_literal,
				identifier, new_keyword, move_keyword, open_brace, open_paren>();
			return nullptr;
		}
		}

		return createNode(expression);
	}

	EXPRESSION* Parser::parse_POSTFIX()
	{
		// even though left size can be any expression in the syntax, due to the order we parse expressions in
		// we can only have a primary expression at this point
		EXPRESSION* pLeft = parse_PRIMARY();

		if (pLeft == nullptr)
		{
			return nullptr;
		}

		while (kind() == TokenKind::open_bracket || kind() == TokenKind::dot)
		{
			// POSTFIX node to insert either ARRAY_ACCESS or MEMBER_ACCESS into
			POSTFIX* pPostfix = createNode(POSTFIX());

			if (kind() == TokenKind::open_bracket)
			{
				(void)expect<TokenKind::open_bracket>();

				EXPRESSION* pRight = parse<EXPRESSION>();

				if (pRight != nullptr)
				{
					return nullptr;
				}

				if (expect<TokenKind::close_bracket>() != SUCCESS)
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

			else
			{
				(void)expect<TokenKind::dot>();

				std::string_view memberName;
				if (expect<TokenKind::identifier>(&memberName) != SUCCESS)
				{
					return nullptr;
				}

				// set the POSTFIX expression to an MEMBER_ACCESS
				pPostfix->Set(
					createNode(MEMBER_ACCESS
						{
							.pObject = pLeft,
							.MemberName = memberName
						}
					)
				);
			}

			// set the left to our newly-parsed POSTFIX expression
			pLeft->Set(pPostfix);
		}

		return pLeft;	// if no POSTFIX was found, this returns a PRIMARY wrapped in an EXPRESSION as expected
	}

	EXPRESSION* Parser::parse_UNARY()
	{
		// operators '-' and '&' are ambiguous so we expect based on value instead of kind
		
		if (value() != "!" && value() != "-" && value() != "&")
		{
			return parse_POSTFIX();
		}

		std::string_view op;
		(void)expectValue({ "!", "-", "&" }, &op);
	
		EXPRESSION* pExpression = parse_POSTFIX();
	
		if (pExpression == nullptr)
		{
			return nullptr;
		}

		UNARY* pUnary = createNode(
			UNARY
			{
				.Operator = op,
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

				while (value() == "*" || value() == "/" || value() == "%")
				{
					std::string_view op;
					(void)expect<TokenKind::binary_operator>(&op);

					EXPRESSION* pRight = parse_UNARY();

					if (pRight == nullptr)
					{
						return nullptr;
					}

					BINARY* pBinary = createNode(
						BINARY
						{
							.Operator = op,
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

				while (value() == "+" || value() == "-")
				{
					std::string_view op;
					(void)expect<TokenKind::binary_operator>(&op);

					EXPRESSION* pRight = tryParseMultiplicativeExpression();

					if (pRight == nullptr)
					{
						return nullptr;
					}

					BINARY* pBinary = createNode(
						BINARY
						{
							.Operator = op,
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

				while (value() == "==" || value() == "!=" || value() == "<" || value() == "<=" 
					|| value() == ">" || value() == ">=")
				{
					std::string_view op;
					(void)expect<TokenKind::binary_operator>(&op);

					EXPRESSION* pRight = tryParseAdditiveExpression();

					if (pRight == nullptr)
					{
						return nullptr;
					}

					BINARY* pBinary = createNode(
						BINARY
						{
							.Operator = op,
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

				while (value() == "&&" || value() == "||")
				{
					std::string_view op;
					(void)expect<TokenKind::binary_operator>(&op);

					EXPRESSION* pRight = tryParseRelationalExpression();

					if (pRight == nullptr)
					{
						return nullptr;
					}

					BINARY* pBinary = createNode(
						BINARY
						{
							.Operator = op,
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

				while (value() == "=")
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

		if (!pAssignment->Is<ASSIGNMENT>())
		{
			logErrorAtPosition("Expected an assignment.");
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
		if (expect<TokenKind::for_keyword>() != SUCCESS)
		{
			return nullptr;
		}

		if (expect<TokenKind::open_paren>() != SUCCESS)
		{
			return nullptr;
		}

		EXPRESSION* pInitialization = parse<EXPRESSION>();

		if (pInitialization == nullptr)
		{
			return nullptr;
		}

		if (expect<TokenKind::semicolon>() != SUCCESS)
		{
			return nullptr;
		}

		EXPRESSION* pCondition = parse<EXPRESSION>();

		if (pCondition == nullptr)
		{
			return nullptr;
		}

		if (expect<TokenKind::semicolon>() != SUCCESS)
		{
			return nullptr;
		}

		EXPRESSION* pIncrement = parse<EXPRESSION>();

		if (pIncrement == nullptr)
		{
			return nullptr;
		}

		if (expect<TokenKind::close_paren>() != SUCCESS)
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
		if (expect<TokenKind::while_keyword>() != SUCCESS)
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

	template<>
	STATEMENT* Parser::parse()
	{
		switch (kind())
		{
		case TokenKind::variable_keyword:
		case TokenKind::constant_keyword:
		{
			NAMED_VARIABLE_DEFINITION* pVariableDefinition = parse<NAMED_VARIABLE_DEFINITION>();
			if (pVariableDefinition == nullptr)
			{
				return nullptr;
			}

			if (expect<TokenKind::semicolon>() != SUCCESS)
			{
				return nullptr;
			}

			return createNode(STATEMENT(pVariableDefinition));
		}

		case TokenKind::identifier:
		{
			STATEMENT* pStatement = nullptr;

			// handle function call
			if (peek() == TokenKind::open_paren)
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

			if (expect<TokenKind::semicolon>() != SUCCESS)
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
			expect<TokenKind::variable_keyword
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

#pragma region ECS Constructs

	template<>
	NAMED_COMPONENT_DEFINITION* Parser::parse()
	{
		if (checkAnnotations({ "exclude" }) != SUCCESS)
		{
			return nullptr;
		}

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
		if (checkAnnotations({ }) != SUCCESS)
		{
			return nullptr;
		}

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
		if (checkAnnotations({ }) != SUCCESS)
		{
			return nullptr;
		}

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
		if (checkAnnotations({ }) != SUCCESS)
		{
			return nullptr;
		}

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
		if (checkAnnotations({ }) != SUCCESS)
		{
			return nullptr;
		}

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
		if (checkAnnotations({ }) != SUCCESS)
		{
			return nullptr;
		}

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
		using enum TokenKind;

		while (!isEOF())
		{
			switch (kind())
			{
			case pound:
				(void)getAnnotation();
				break;

			case type_keyword:
				(void)parse<NAMED_TYPE_DEFINITION>();
				break;

			case function_keyword:
				(void)parse<NAMED_FUNCTION_DEFINITION>();
				break;

			case extern_keyword:
				(void)parse<EXTERN_FUNCTION_DEFINITION>();
				break;

			case component_keyword:
				(void)parse<NAMED_COMPONENT_DEFINITION>();
				break;

			case resource_keyword:
				(void)parse<NAMED_RESOURCE_DEFINITION>();
				break;

			case query_keyword:
				(void)parse<NAMED_QUERY_DEFINITION>();
				break;

			case system_keyword:
				(void)parse<NAMED_SYSTEM_DEFINITION>();
				break;

			case group_keyword:
				(void)parse<NAMED_GROUP_DEFINITION>();
				break;

			case application_keyword:
				(void)parse<APPLICATION_DEFINITION>();
				break;

			default:
				(void)expect<type_keyword, function_keyword, extern_keyword, 
					component_keyword, resource_keyword, query_keyword, system_keyword, 
					group_keyword, application_keyword>();
				(void)eat();
				break;
			}
		}

		return std::move(m_NamedNodes);
	}

}