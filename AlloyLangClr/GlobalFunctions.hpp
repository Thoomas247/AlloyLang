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

		enum class SemanticTokenType
		{
			None = 0,

			Keyword,
			Label,
			Control,

			ModuleIdentifier,
			BuiltInTypeIdentifier,
			EnumTypeIdentifier,
			StructTypeIdentifier,
			FunctionIdentifier,
			VariableIdentifier,
			GlobalIdentifier,

			StringLiteral,
			NumberLiteral,
			BooleanLiteral,

			Comment
		};

		static System::Collections::Generic::List<CSharpToken^>^ Parse(System::String^ managedStr);
	};
}