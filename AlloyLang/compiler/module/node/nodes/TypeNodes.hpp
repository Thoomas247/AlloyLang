#pragma once

#include "NodeCommon.hpp"
#include "VariantNodes.hpp"

namespace AlloyCompiler
{
	struct MACRO_CALL;

	struct TYPE_NAME
	{
		Token* pNameToken;
		std::vector<TYPE*> GenericArguments;
	};

	struct STRUCT_TYPE
	{
		Token* pErrorToken;
		std::vector<std::pair<Token*, TYPE*>> Members;
	};

	struct ENUM_TYPE
	{
		Token* pErrorToken;
		std::vector<std::pair<Token*, TYPE*>> Members;	// TYPE of member is optional
	};

	struct ARRAY_TYPE
	{
		TYPE* pElementType;
		LITERAL* pSizeLiteral;
	};

	struct TYPE
	{
		TypeModifier Modifier;
		VariantNode<TYPE_NAME, STRUCT_TYPE, ENUM_TYPE, ARRAY_TYPE, MACRO_CALL> Type;

		Token* GetErrorToken() const
		{
			if (Type.Is<TYPE_NAME>())	return Type.Get<TYPE_NAME>()->pNameToken;
			if (Type.Is<STRUCT_TYPE>()) return Type.Get<STRUCT_TYPE>()->pErrorToken;
			if (Type.Is<ENUM_TYPE>())	return Type.Get<ENUM_TYPE>()->pErrorToken;
			if (Type.Is<ARRAY_TYPE>())	return Type.Get<ARRAY_TYPE>()->pElementType->GetErrorToken();

			return nullptr;
			//if (Type.Is<MACRO_CALL>()) return Type.Get<MACRO_CALL>()->pMacroNameToken;
		}
	};

	struct TYPE_IDENTIFIER
	{
		Token* pNameToken;
		std::vector<GENERIC_PARAMETER*> GenericParameters;
	};

	struct TYPE_DEFINITION
	{
		TYPE_IDENTIFIER* pTypeIdentifier;
		TYPE* pType;
	};
}