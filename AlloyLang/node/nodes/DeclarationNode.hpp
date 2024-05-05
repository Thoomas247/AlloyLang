#pragma once
#include "../Node.hpp"

#include "IdentifierNode.hpp"

namespace AlloyCompiler
{
	class DeclarationNode : public Node
	{
	public:
		DeclarationNode() = default;
		virtual ~DeclarationNode() = 0;
	};

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

	class FunctionDeclarationNode : public Node
	{

	};
}