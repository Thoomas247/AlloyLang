#include <vcclr.h>
#include <msclr\marshal_cppstd.h>

#include "../AlloyLang/compiler/module/source/Source.hpp"
#include "../AlloyLang/compiler/module/token/TokenBuffer.hpp"
#include "../AlloyLang/compiler/module/node/NodeBuffer.hpp"
#include "../AlloyLang/compiler/ModuleTable.hpp"
#include "GlobalFunctions.hpp"

namespace AlloyCompiler
{
	static GlobalFunctions::SemanticTokenType getIdentifierSemanticTokenType(const NodeBuffer& nodeBuffer, const std::string& identifier)
	{

		// built-in types
		for (auto& typeName : NUMERIC_TYPE_NAMES)
		{
			if (typeName == identifier)
			{
				return GlobalFunctions::SemanticTokenType::BuiltInTypeIdentifier;
			}
		}

		// struct and enum types
		{
			auto it = nodeBuffer.GetTypeDefinitions().find(identifier);
			if (it != nodeBuffer.GetTypeDefinitions().end())
			{
				TYPE_DEFINITION* pTypeDefinition = it->second.pDefinition;

				if (pTypeDefinition->pType->Type.Is<ENUM_TYPE>())
				{
					return GlobalFunctions::SemanticTokenType::EnumTypeIdentifier;
				}
				else
				{
					return GlobalFunctions::SemanticTokenType::StructTypeIdentifier;
				}
			}
		}

		// function names
		if (nodeBuffer.GetFunctionDefinitions().contains(identifier))
		{
			return GlobalFunctions::SemanticTokenType::FunctionIdentifier;
		}

		// global variable names
		if (nodeBuffer.GetGlobalVariableDefinitions().contains(identifier))
		{
			return GlobalFunctions::SemanticTokenType::GlobalIdentifier;
		}

		return GlobalFunctions::SemanticTokenType::None;

	}

	static GlobalFunctions::SemanticTokenType getSemanticTokenType(const NodeBuffer& nodeBuffer, Token* pToken)
	{
		switch (pToken->Kind)
		{
		case TokenKind::comment:
			return GlobalFunctions::SemanticTokenType::Comment;

		case TokenKind::identifier:
			return getIdentifierSemanticTokenType(nodeBuffer, std::string(pToken->Value));

		case TokenKind::long_identifier:
			return GlobalFunctions::SemanticTokenType::ModuleIdentifier;

		case TokenKind::extern_keyword:
		case TokenKind::struct_keyword:
		case TokenKind::enum_keyword:
		case TokenKind::function_keyword:
		case TokenKind::macro_keyword:
		case TokenKind::import_keyword:
		case TokenKind::as_keyword:
			return GlobalFunctions::SemanticTokenType::Keyword;

		case TokenKind::public_keyword:
		case TokenKind::export_keyword:
			return GlobalFunctions::SemanticTokenType::Label;

		case TokenKind::type_keyword:
		case TokenKind::variable_keyword:
		case TokenKind::constant_keyword:
			return GlobalFunctions::SemanticTokenType::Keyword;

		case TokenKind::for_keyword:
		case TokenKind::while_keyword:
		case TokenKind::if_keyword:
		case TokenKind::else_keyword:
		case TokenKind::switch_keyword:
		case TokenKind::case_keyword:
		case TokenKind::return_keyword:
			return GlobalFunctions::SemanticTokenType::Control;

		case TokenKind::new_keyword:
		case TokenKind::move_keyword:
			return GlobalFunctions::SemanticTokenType::Keyword;

		case TokenKind::integer_literal:
		case TokenKind::float_literal:
			return GlobalFunctions::SemanticTokenType::NumberLiteral;

		case TokenKind::boolean_literal:
			return GlobalFunctions::SemanticTokenType::BooleanLiteral;

		case TokenKind::string_literal:
		case TokenKind::character_literal:
			return GlobalFunctions::SemanticTokenType::StringLiteral;

		default:
			return GlobalFunctions::SemanticTokenType::None;
		}
	}

	/*static*/ System::Collections::Generic::List<GlobalFunctions::CSharpToken^>^ GlobalFunctions::Parse(System::String^ managedStr)
	{
		System::Collections::Generic::List<CSharpToken^>^ list;
		if (!System::String::IsNullOrEmpty(managedStr))
		{
			msclr::interop::marshal_context context;

			Source source(context.marshal_as<std::string>(managedStr));

			TokenBuffer tokenBuffer(source);
			tokenBuffer.Tokenize();

			NodeBuffer nodeBuffer(source, tokenBuffer);
			nodeBuffer.Parse();

			const int count = (int)tokenBuffer.NumTokens();
			list = gcnew System::Collections::Generic::List<CSharpToken^>(count);

			for (int n = 0; n < count; n++)
			{
				// convert our C++ tokens into C# token and add it to the list
				Token* token = tokenBuffer.GetToken(n);

				CSharpToken^ cstoken = gcnew CSharpToken();
				cstoken->Value = gcnew System::String(std::string(token->Value).data());
				cstoken->LineStart = token->Location.LineStart;
				cstoken->Line = token->Location.Line;
				cstoken->Column = token->Location.Column;
				cstoken->Kind = (int)getSemanticTokenType(nodeBuffer, token);

				list->Add(cstoken);
			}
		}
		return list;
	}

}
