#pragma once
#include <cstdint>

namespace AlloyCompiler
{
	enum class Visibility : uint8_t
	{
		Private,
		Public,
		Export
	};

	template <typename T>
	struct Definition
	{
		Visibility Access;
		T* pDefinition;

		Definition()
			: Access(Visibility::Private), pDefinition(nullptr)
		{}

		Definition(Visibility visibility, T* pDefinition)
			: Access(visibility), pDefinition(pDefinition)
		{}

		bool IsNull() const
		{
			return pDefinition == nullptr;
		}
	};
}