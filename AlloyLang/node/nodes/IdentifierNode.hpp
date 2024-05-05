#pragma once
#include "../Node.hpp"

namespace AlloyCompiler
{
	class IdentifierBaseNode : public Node
	{
	public:
		IdentifierBaseNode() = default;
		~IdentifierBaseNode() override = 0;
	};

	class IdentifierNode : public IdentifierBaseNode
	{
	public:
		IdentifierNode(const std::string_view& identifier)
			: Identifier(identifier)
		{
		}

		~IdentifierNode() override = default;

	public:
		std::string_view Identifier;
	};

	class TypeIdentifierNode : public IdentifierBaseNode
	{
	public:
		enum class Modifier : uint8_t
		{
			None,
			Reference,
			Pointer,
		};

	public:
		TypeIdentifierNode(IdentifierBaseNode* pWrappedType, Modifier modifier, size_t arraySize)
			: pWrappedType(pWrappedType), TypeModifier(modifier), ArraySize(arraySize)
		{
		}

		~TypeIdentifierNode() override = default;

	public:
		IdentifierBaseNode* pWrappedType;
		Modifier TypeModifier;
		size_t ArraySize;
	};
}