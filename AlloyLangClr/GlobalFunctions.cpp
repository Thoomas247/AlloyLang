#include <vcclr.h>
#include <msclr\marshal_cppstd.h>

#include "../AlloyLang/compiler/module/source/Source.hpp"
#include "../AlloyLang/compiler/module/token/TokenBuffer.hpp"
#include "GlobalFunctions.hpp"

namespace AlloyCompiler
{
	//
	// split a string into multiple tokens and return the token list to the calling .Net object
	//
	/* static*/
	System::Collections::Generic::List<GlobalFunctions::CSharpToken^>^ GlobalFunctions::Tokenize(System::String^ managedStr)
	{
		System::Collections::Generic::List<GlobalFunctions::CSharpToken^>^ list;
		if (!System::String::IsNullOrEmpty(managedStr))
		{
			msclr::interop::marshal_context context;
			AlloyCompiler::Source src(context.marshal_as<std::string>(managedStr));
			AlloyCompiler::TokenBuffer tok(src);
			tok.Tokenize();
			int count = (int)tok.NumTokens();
			list = gcnew System::Collections::Generic::List<GlobalFunctions::CSharpToken^>(count);
			for (int n = 0; n < count; n++)
			{
				// convert our C++ tokens into C# token and add it to the list
				Token* token = tok.GetToken(n);
				GlobalFunctions::CSharpToken^ cstoken = gcnew GlobalFunctions::CSharpToken();
				cstoken->Value = gcnew System::String(std::string(token->Value).data());
				cstoken->LineStart = token->Location.LineStart;
				cstoken->Line = token->Location.Line;
				cstoken->Column = token->Location.Column;
				cstoken->Kind = (int)token->Kind;
				list->Add(cstoken);
			}
		}
		return list;
	}

	/*static*/
	GlobalFunctions::AlloyTokenTypes GlobalFunctions::GetTokenType(int TokenKind)
	{
		//
		// given a token kind returned by the parser, convert to to token type for syntax highlighting
		//
		static const AlloyTokenTypes TokenKinds[] =
		{
			AlloyTokenTypes::none,       // none

			AlloyTokenTypes::none,       // end_of_file,

			AlloyTokenTypes::comment,     // comment

			AlloyTokenTypes::none,    // identifier,
			AlloyTokenTypes::none,    // long_identifier,

			AlloyTokenTypes::keyword,    // extern_keyword,
			AlloyTokenTypes::structure,  // struct_keyword,
			AlloyTokenTypes::enumeration,// enum_keyword,
			AlloyTokenTypes::keyword,    // function_keyword,
			AlloyTokenTypes::keyword,    // macro_keyword,

			AlloyTokenTypes::keyword,    // import_keyword,
			AlloyTokenTypes::keyword,    // as_keyword,

			AlloyTokenTypes::varorconst,    // public_keyword,
			AlloyTokenTypes::varorconst,    // export_keyword,

			AlloyTokenTypes::keyword,    // type_keyword,

			AlloyTokenTypes::varorconst,    // variable_keyword,
			AlloyTokenTypes::varorconst,    // constant_keyword,

			AlloyTokenTypes::keyword,    // for_keyword,
			AlloyTokenTypes::keyword,    // while_keyword,
			AlloyTokenTypes::keyword,    // if_keyword,
			AlloyTokenTypes::keyword,    // else_keyword,
			AlloyTokenTypes::keyword,    // switch_keyword,
			AlloyTokenTypes::keyword,    // case_keyword,
			AlloyTokenTypes::keyword,    // return_keyword,

			AlloyTokenTypes::keyword,    // new_keyword,
			AlloyTokenTypes::keyword,    // move_keyword,

			AlloyTokenTypes::none,       // pound,
			AlloyTokenTypes::none,       // at_symbol,

			AlloyTokenTypes::none,       // reference,

			AlloyTokenTypes::none,       // comma,
			AlloyTokenTypes::none,       // colon,
			AlloyTokenTypes::none,       // semicolon,
			AlloyTokenTypes::none,       // double_colon,
			AlloyTokenTypes::none,       // dot,
			AlloyTokenTypes::none,       // arrow,
			AlloyTokenTypes::none,       // ellipsis,

			AlloyTokenTypes::none,       // open_paren,
			AlloyTokenTypes::none,       // close_paren,
			AlloyTokenTypes::none,       // open_brace,
			AlloyTokenTypes::none,       // close_brace,
			AlloyTokenTypes::none,       // open_bracket,
			AlloyTokenTypes::none,       // close_bracket,

			AlloyTokenTypes::none,       // pipe_operator,

			AlloyTokenTypes::none,       // assignment_operator,

			AlloyTokenTypes::none,       // unary_operator,
			AlloyTokenTypes::none,       // binary_operator,

			AlloyTokenTypes::basictype,    // integer_literal,
			AlloyTokenTypes::basictype,    // float_literal,
			AlloyTokenTypes::basictype,    // boolean_literal,
			AlloyTokenTypes::stringliteral,   // string_literal,
			AlloyTokenTypes::stringliteral,    // character_literal
		};

		if (TokenKind < sizeof(TokenKinds) / sizeof(TokenKinds[0]))
			return TokenKinds[TokenKind];
		else
			return AlloyTokenTypes::none;
	}
}
