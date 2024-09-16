#include "Inlines.hpp"

namespace AlloyCompiler
{

	struct TypeSizePair
	{
		enum Type
		{
			Bool, Signed, Unsigned, Float
		};

		Type Type;
		size_t Size;
	};

	static TypeSizePair getTypeSizePair(const std::string_view& name)
	{
		TypeSizePair result{};

		switch (name[0])
		{
		case 'b':
			result.Type = TypeSizePair::Bool;
			break;

		case 'i':
			result.Type = TypeSizePair::Signed;
			break;

		case 'u':
			result.Type = TypeSizePair::Unsigned;
			break;

		case 'f':
			result.Type = TypeSizePair::Float;
			break;

		default:
			std::unreachable();
		}

		std::string_view sizeString = name.substr(1);

		(void)std::from_chars(&name[1], &name.back() + 1, result.Size);

		return result;
	}

	llvm::Function* generateCastFunction(LLVMState& state, const std::string_view& fromName, const std::string_view& toName)
	{
		const std::string functionName = std::string(fromName) + ":" + CAST_FUNCTION_NAME + "@" + std::string(toName);

		llvm::Function* function = state.Module->getFunction(functionName);
		if (function != nullptr)
		{
			return function;
		}

		llvm::Type* fromType = state.NamedValues.GetType(fromName);
		llvm::Type* toType = state.NamedValues.GetType(toName);

		ASSERT(fromType != nullptr && toType != nullptr, "Invalid type names!");

		llvm::Type* returnType = toType;
		std::vector<llvm::Type*> paramTypes = { fromType };
		llvm::FunctionType* functionType = llvm::FunctionType::get(returnType, paramTypes, false);

		function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, functionName, *state.Module);

		ASSERT(function != nullptr, "Failed to generate cast function!");

		llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(*state.Context, "entry", function);
		state.Builder->SetInsertPoint(entryBlock);

		llvm::Value* fromValue = function->getArg(0);
		llvm::Value* castValue = nullptr;

		TypeSizePair fromInfo = getTypeSizePair(fromName);
		TypeSizePair toInfo = getTypeSizePair(toName);

		if (fromInfo.Type == TypeSizePair::Bool)
		{
			if (toInfo.Type == TypeSizePair::Bool)
			{
				castValue = fromValue;
			}

			else if (toInfo.Type == TypeSizePair::Signed || toInfo.Type == TypeSizePair::Unsigned)
			{
				castValue = state.Builder->CreateZExt(fromValue, toType);
			}

			else if (toInfo.Type == TypeSizePair::Float)
			{
				castValue = state.Builder->CreateUIToFP(fromValue, toType);
			}
		}

		else if (fromInfo.Type == TypeSizePair::Signed)
		{
			if (toInfo.Type == TypeSizePair::Bool)
			{
				castValue = state.Builder->CreateICmpNE(fromValue, state.Builder->getInt(llvm::APInt(fromInfo.Size, 0, /*isSigned*/true)));
			}

			else if (toInfo.Type == TypeSizePair::Signed)
			{
				castValue = state.Builder->CreateSExtOrTrunc(fromValue, toType);
			}

			else if (toInfo.Type == TypeSizePair::Unsigned)
			{
				castValue = state.Builder->CreateZExtOrTrunc(fromValue, toType);
			}

			else if (toInfo.Type == TypeSizePair::Float)
			{
				castValue = state.Builder->CreateSIToFP(fromValue, toType);
			}
		}

		else if (fromInfo.Type == TypeSizePair::Unsigned)
		{
			if (toInfo.Type == TypeSizePair::Bool)
			{
				castValue = state.Builder->CreateICmpNE(fromValue, state.Builder->getInt(llvm::APInt(fromInfo.Size, 0, /*isSigned*/false)));
			}

			else if (toInfo.Type == TypeSizePair::Signed)
			{
				// TODO: check this
				castValue = state.Builder->CreateZExtOrTrunc(fromValue, toType);
			}

			else if (toInfo.Type == TypeSizePair::Unsigned)
			{
				// TODO: check this
				castValue = state.Builder->CreateZExtOrTrunc(fromValue, toType);
			}

			else if (toInfo.Type == TypeSizePair::Float)
			{
				castValue = state.Builder->CreateUIToFP(fromValue, toType);
			}
		}

		else if (fromInfo.Type == TypeSizePair::Float)
		{
			if (toInfo.Type == TypeSizePair::Bool)
			{
				castValue = state.Builder->CreateFCmpONE(fromValue, llvm::ConstantFP::get(*state.Context, llvm::APFloat(0.0)));
			}

			else if (toInfo.Type == TypeSizePair::Signed)
			{
				castValue = state.Builder->CreateFPToSI(fromValue, toType);
			}

			else if (toInfo.Type == TypeSizePair::Unsigned)
			{
				castValue = state.Builder->CreateFPToUI(fromValue, toType);
			}

			else if (toInfo.Type == TypeSizePair::Float)
			{
				castValue = state.Builder->CreateFPCast(fromValue, toType);
			}
		}

		ASSERT(castValue != nullptr, "Invalid cast operation!");

		state.Builder->CreateRet(castValue);

		return function;
	}

	llvm::Function* generateBitCastFunction(LLVMState& state, const std::string_view& fromName, const std::string_view& toName)
	{
		const std::string functionName = std::string(fromName) + ":" + BIT_CAST_FUNCTION_NAME + "@" + std::string(toName);

		llvm::Function* function = state.Module->getFunction(functionName);
		if (function != nullptr)
		{
			return function;
		}

		llvm::Type* fromType = state.NamedValues.GetType(fromName);
		llvm::Type* toType = state.NamedValues.GetType(toName);

		ASSERT(fromType != nullptr && toType != nullptr, "Invalid type names!");

		llvm::Type* returnType = toType;
		std::vector<llvm::Type*> paramTypes = { fromType };
		llvm::FunctionType* functionType = llvm::FunctionType::get(returnType, paramTypes, false);

		function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, functionName, *state.Module);

		ASSERT(function != nullptr, "Failed to generate bit cast function!");

		llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(*state.Context, "entry", function);
		state.Builder->SetInsertPoint(entryBlock);

		llvm::Value* castValue = state.Builder->CreateTruncOrBitCast(function->getArg(0), toType);

		ASSERT(castValue != nullptr, "Invalid cast operation!");

		state.Builder->CreateRet(castValue);

		return function;
	}

	void generateAllCastFunctions(LLVMState& state)
	{
		const std::array<std::string, 10> numericTypeNames = {
			"i8", "i16", "i32", "i64",
			"u8", "u16", "u32", "u64",
			"f32", "f64"
		};

		for (auto& to : numericTypeNames)
		{
			for (auto& from : numericTypeNames)
			{
				generateCastFunction(state, from, to);
				//generateBitCastFunction(state, from, to);
			}
		}
	}
}