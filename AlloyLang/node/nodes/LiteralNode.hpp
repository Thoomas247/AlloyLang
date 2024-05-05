#pragma once
#include "../Node.hpp"

namespace AlloyCompiler
{
	class LiteralNode : public Node
	{
	public:
		LiteralNode() = default;
		~LiteralNode() override = 0;
	};

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