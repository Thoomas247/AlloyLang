#pragma once
#include "../Node.hpp"

#include "IdentifierNode.hpp"

namespace AlloyCompiler
{
	/// <summary>
	/// RETURN_TYPE_DECLARATION:	[ variable | constant ] TYPE_IDENTIFIER ;
	/// </summary>
	class ReturnTypeDeclarationNode : public Node
	{
	public:
		enum class Kind : uint8_t
		{
			Copy,
			Variable,
			Constant
		};

	public:
		ReturnTypeDeclarationNode(Kind kind, TypeIdentifierNode* pTypeIdentifier)
			: Kind(kind), pTypeIdentifier(pTypeIdentifier)
		{
		}

	public:
		Kind Kind;
		TypeIdentifierNode* pTypeIdentifier;
	};

	/// <summary>
	/// VALUE_DECLARATION:	( variable | constant ) IDENTIFIER colon TYPE_IDENTIFIER ;
	/// </summary>
	class ValueDeclarationNode : public Node
	{
	public:
		enum class Kind : uint8_t
		{
			Variable,
			Constant
		};

	public:
		ValueDeclarationNode(Kind kind, IdentifierNode* pIdentifier, TypeIdentifierNode* pTypeIdentifier)
			: Kind(kind), pIdentifier(pIdentifier), pTypeIdentifier(pTypeIdentifier)
		{
		}

	public:
		Kind Kind;
		IdentifierNode* pIdentifier;
		TypeIdentifierNode* pTypeIdentifier;
	};

	/// <summary>
	/// FUNCTION_DECLARATION:	function IDENTIFIER open_paren [ VALUE_DECLARATION { comma VALUE_DECLARATION } ] close_paren [ arrow RETURN_TYPE_DECLARATION ] ;
	/// </summary>
	class FunctionDeclarationNode : public Node
	{
	public:
		FunctionDeclarationNode(IdentifierNode* pIdentifier, std::vector<ValueDeclarationNode*> Parameters, ReturnTypeDeclarationNode* pReturnType, bool IsVariadic)
			: pIdentifier(pIdentifier), Parameters(Parameters), pReturnType(pReturnType), IsVariadic(IsVariadic)
		{
		}

	public:
		IdentifierNode* pIdentifier;
		std::vector<ValueDeclarationNode*> Parameters;
		ReturnTypeDeclarationNode* pReturnType;
		bool IsVariadic;
	};
}