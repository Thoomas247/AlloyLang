#pragma once

namespace AlloyCompiler
{
	public ref class GlobalFunctions
	{
	public:
		ref struct CSharpToken
		{
			System::String^ Value;
			int LineStart;
			int Line;
			int Column;
			int Kind;
		};
		static System::Collections::Generic::List<CSharpToken^>^ Tokenize(System::String^ managedStr);

		enum class AlloyTokenTypes
		{
			none = 0, keyword = 1, basictype = 2, stringliteral = 3, varorconst = 4, comment = 5, enumeration = 6, structure = 7
		};
		// given a token kind returned by the parser, convert to to token type for syntax highlighting
		static AlloyTokenTypes GetTokenType(int TokenKind);

	};
}