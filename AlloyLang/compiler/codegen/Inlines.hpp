#pragma once

#include "CodeGenerator.hpp"

constexpr auto CAST_FUNCTION_NAME = "cast";
constexpr auto BIT_CAST_FUNCTION_NAME = "bit_cast";

namespace AlloyCompiler
{
	/// <summary>
	/// Creates or returns the function needed to cast from one built-in number type to another.
	/// Allows casting from and to the same type.
	/// </summary>
	llvm::Function* generateCastFunction(LLVMState& state, const std::string_view& fromName, const std::string_view& toName);

	/// <summary>
	/// Creates or returns the function needed to bit cast from one built-in number type to another.
	/// Allows casting from and to the same type.
	/// </summary>
	llvm::Function* generateBitCastFunction(LLVMState& state, const std::string_view& fromName, const std::string_view& toName);

	/// <summary>
	/// Generates all combinations of casts for the built-in numeric types.
	/// </summary>
	void generateAllCastFunctions(ModuleTable& moduleTable, LLVMState& state);
}