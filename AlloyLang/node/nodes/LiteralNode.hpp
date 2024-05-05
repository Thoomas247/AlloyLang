#pragma once
#include "../Node.hpp"

namespace AlloyCompiler
{

	/// <summary>
	/// LITERAL:	INTEGER_LITERAL | FLOAT_LITERAL | BOOLEAN_LITERAL | STRING_LITERAL | CHARACTER_LITERAL ;
	/// </summary>
	class LiteralNode : public Node
	{
	public:
		LiteralNode() = default;
		~LiteralNode() override = 0;
	};

	/// <summary>
	/// INTEGER_LITERAL:	integer_literal ;
	/// </summary>
	class IntegerLiteralNode : public LiteralNode
	{
	public:
		IntegerLiteralNode(int64_t value)
			: SignedValue(value)
		{
		}

		IntegerLiteralNode(uint64_t value)
			: UnsignedValue(value)
		{
		}

		~IntegerLiteralNode() override = default;

		int64_t GetSignedValue() const
		{
			return SignedValue;
		}

		uint64_t GetUnsignedValue() const
		{
			return UnsignedValue;
		}

	public:
		union
		{
			uint64_t UnsignedValue;
			int64_t SignedValue;
		};
	};

	/// <summary>
	/// FLOAT_LITERAL:		float_literal;
	/// </summary>
	class FloatLiteralNode : public LiteralNode
	{
	public:
		FloatLiteralNode(double value)
			: Value(value)
		{
		}

		~FloatLiteralNode() override = default;

	public:
		double Value;
	};

	/// <summary>
	/// BOOLEAN_LITERAL:	boolean_literal ;
	/// </summary>
	class BooleanLiteralNode : public LiteralNode
	{
	public:
		BooleanLiteralNode(bool value)
			: Value(value)
		{
		}

		~BooleanLiteralNode() override = default;

	public:
		bool Value;
	};

	/// <summary>
	/// STRING_LITERAL:		string_literal ;
	/// </summary>
	class StringLiteralNode : public LiteralNode
	{
	public:
		StringLiteralNode(const std::string_view& value)
			: Value(value)
		{
		}

		~StringLiteralNode() override = default;

	public:
		std::string_view Value;
	};

	/// <summary>
	/// CHARACTER_LITERAL:	character_literal ;
	/// </summary>
	class CharacterLiteralNode : public LiteralNode
	{
	public:
		CharacterLiteralNode(char value)
			: Value(value)
		{
		}

	~CharacterLiteralNode() override = default;

	public:
		char Value;
	};
}