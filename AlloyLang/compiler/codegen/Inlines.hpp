#pragma once

constexpr auto CAST_FUNCTION_NAME = "cast";
constexpr auto BIT_CAST_FUNCTION_NAME = "bit_cast";

namespace AlloyCompiler
{
	/// <summary>
	/// Creates or returns the function needed to cast from one built-in number type to another.
	/// Allows casting from and to the same type.
	/// </summary>
	llvm::Function* generateCastFunction(LLVMState& state, const std::string_view& mangledName, const std::string_view& fromName, const std::string_view& toName);

	/// <summary>
	/// Creates or returns the function needed to bit cast from one built-in number type to another.
	/// Allows casting from and to the same type.
	/// </summary>
	llvm::Function* generateBitCastFunction(LLVMState& state, const std::string_view& mangledName, const std::string_view& fromName, const std::string_view& toName);

	/// <summary>
	/// Generates or returns the built-in function which corresponds to the given mangled name.
	/// The mangled name must include generic parameters.
	/// </summary>
	llvm::Function* generateBuiltInFunction(LLVMState& state, const std::string_view& mangledName);
}