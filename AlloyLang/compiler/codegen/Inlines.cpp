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

	llvm::Function* generateCastFunction(LLVMState& state, const std::string_view& mangledName, const std::string_view& fromName, const std::string_view& toName)
	{
		llvm::Type* fromType = state.NamedValues.GetType(fromName);
		llvm::Type* toType = state.NamedValues.GetType(toName);

		ASSERT(fromType != nullptr && toType != nullptr, "Invalid type names!");

		llvm::Type* returnType = toType;
		std::vector<llvm::Type*> paramTypes = { fromType };
		llvm::FunctionType* functionType = llvm::FunctionType::get(returnType, paramTypes, false);

		llvm::Function* function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, mangledName, *state.Module);

		ASSERT(function != nullptr, "Failed to generate cast function!");

		// force inlining of Cast functions
		function->addFnAttr(llvm::Attribute::AlwaysInline);

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

	llvm::Function* generateBitCastFunction(LLVMState& state, const std::string_view& mangledName, const std::string_view& fromName, const std::string_view& toName)
	{
		llvm::Type* fromType = state.NamedValues.GetType(fromName);
		llvm::Type* toType = state.NamedValues.GetType(toName);

		ASSERT(fromType != nullptr && toType != nullptr, "Invalid type names!");

		llvm::Type* returnType = toType;
		std::vector<llvm::Type*> paramTypes = { fromType };
		llvm::FunctionType* functionType = llvm::FunctionType::get(returnType, paramTypes, false);

		llvm::Function* function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, mangledName, *state.Module);

		ASSERT(function != nullptr, "Failed to generate bit cast function!");

		// force inlining of BitCast functions
		function->addFnAttr(llvm::Attribute::AlwaysInline);

		llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(*state.Context, "entry", function);
		state.Builder->SetInsertPoint(entryBlock);

		llvm::Value* fromValue = function->getArg(0);
		llvm::Value* castValue = nullptr;

		TypeSizePair fromInfo = getTypeSizePair(fromName);
		TypeSizePair toInfo = getTypeSizePair(toName);

		if (fromInfo.Type == TypeSizePair::Float)
		{
			// float to float
			if (toInfo.Type == TypeSizePair::Float)
			{
				castValue = state.Builder->CreateFPCast(fromValue, toType);
			}

			// float to int
			else
			{
				// convert float to int of same size

				if (toInfo.Type == TypeSizePair::Signed)
				{
					if (fromInfo.Size == 64)
					{
						fromValue = state.Builder->CreateFPToSI(fromValue, llvm::IntegerType::getInt64Ty(*state.Context));
					}

					else if (fromInfo.Size == 32)
					{
						fromValue = state.Builder->CreateFPToSI(fromValue, llvm::IntegerType::getInt32Ty(*state.Context));
					}
				}

				else if (toInfo.Type == TypeSizePair::Unsigned)
				{
					if (fromInfo.Size == 64)
					{
						fromValue = state.Builder->CreateFPToUI(fromValue, llvm::IntegerType::getInt64Ty(*state.Context));
					}

					else if (fromInfo.Size == 32)
					{
						fromValue = state.Builder->CreateFPToUI(fromValue, llvm::IntegerType::getInt32Ty(*state.Context));
					}
				}

				// castValue is now an integer
				// from big int to small int, just truncate; from small int to big int, zero-extend
				castValue = state.Builder->CreateZExtOrTrunc(fromValue, toType);
			}
		}

		else
		{
			// int to float
			if (toInfo.Type == TypeSizePair::Float)
			{
				
				if (fromInfo.Type == TypeSizePair::Signed)
				{
					castValue = state.Builder->CreateSIToFP(fromValue, toType);
				}

				else if (fromInfo.Type == TypeSizePair::Unsigned)
				{
					castValue = state.Builder->CreateUIToFP(fromValue, toType);
				}
				
			}

			// int to int
			else
			{
				castValue = state.Builder->CreateZExtOrTrunc(fromValue, toType);
			}
		}
		

		ASSERT(castValue != nullptr, "Invalid cast operation!");

		state.Builder->CreateRet(castValue);

		return function;
	}

	llvm::Function* generateBuiltInFunction(LLVMState& state, const std::string_view& mangledName)
	{
		llvm::Function* function = state.Module->getFunction(mangledName);
		if (function != nullptr)
		{
			return function;
		}

		// currently, all built-in functions are casts, which must be member functions and have one generic parameter
		// therefore, we assume the mangled name matches the format "type@function@param"
		const size_t firstSeparator = mangledName.find('@');
		const size_t secondSeparator = mangledName.find('@', firstSeparator + 1);

		ASSERT(firstSeparator != std::string_view::npos && secondSeparator != std::string_view::npos, "Invalid built-in function name!");

		const std::string_view& fromName = mangledName.substr(0, firstSeparator);
		const std::string_view& funcName = mangledName.substr(firstSeparator + 1, secondSeparator - firstSeparator);
		const std::string_view& toName = mangledName.substr(secondSeparator + 1, mangledName.size() - secondSeparator);

		llvm::Function* result = nullptr;

		if (funcName == CAST_FUNCTION_NAME)
		{
			result = generateCastFunction(state, mangledName, fromName, toName);
		}
		else if (funcName == BIT_CAST_FUNCTION_NAME)
		{
			result = generateBitCastFunction(state, mangledName, fromName, toName);
		}

		return result;
	}
}