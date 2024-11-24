#include "llvm/llvm.hpp"
#include "NamedValues.hpp"
#include "CodeGenerator.hpp"
#include "AlloyType.hpp"
#include "AlloyValue.hpp"
#include "LibraryFunctions.hpp"
#include "Inlines.hpp"
#include "SmartPointerClass.hpp"

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
		llvm::Type* fromType = state.NamedValues.GetType(fromName)->llvmType;
		llvm::Type* toType = state.NamedValues.GetType(toName)->llvmType;

		ASSERT(fromType != nullptr && toType != nullptr, "Invalid type names!");

		llvm::Type* returnType = toType;
#ifdef FIRST_PARAMETER_BYREF
		std::vector<llvm::Type*> paramTypes = { AlloyType::getPointerType(fromType) };
#else
		std::vector<llvm::Type*> paramTypes = { fromType };
#endif
		llvm::FunctionType* functionType = llvm::FunctionType::get(returnType, paramTypes, false);

		llvm::Function* function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, mangledName, *state.Module);

		ASSERT(function != nullptr, "Failed to generate cast function!");

		// force inlining of Cast functions
		function->addFnAttr(llvm::Attribute::AlwaysInline);
#ifdef FIRST_PARAMETER_BYREF
		// the first parameter is a reference to a variable and not a value
		function->addAttributeAtIndex(1, llvm::Attribute::getWithByRefType(*state.Context, fromType));
#endif
		llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(*state.Context, "entry", function);

		// create a new builder for this function, this will allow us to generate multiple functions in parallel
		llvm::IRBuilder<> builder(*state.Context);
		builder.SetInsertPoint(entryBlock);

#ifdef FIRST_PARAMETER_BYREF
		//
		// TBD: The first (and only) parameter to the cast functions is supposed to be &Self, but this creates and extra call to load the value from the pointer
		// We need to evaluate the impact on performance and on the inline optimizer of the extra call to "load"
		//
		llvm::Value* fromValue = builder.CreateLoad(fromType, function->getArg(0));
#else
		llvm::Value* fromValue = function->getArg(0);
#endif
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
				castValue = builder.CreateZExt(fromValue, toType);
			}

			else if (toInfo.Type == TypeSizePair::Float)
			{
				castValue = builder.CreateUIToFP(fromValue, toType);
			}
		}

		else if (fromInfo.Type == TypeSizePair::Signed)
		{
			if (toInfo.Type == TypeSizePair::Bool)
			{
				castValue = builder.CreateICmpNE(fromValue, builder.getInt(llvm::APInt(fromInfo.Size, 0, /*isSigned*/true)));
			}

			else if (toInfo.Type == TypeSizePair::Signed)
			{
				castValue = builder.CreateSExtOrTrunc(fromValue, toType);
			}

			else if (toInfo.Type == TypeSizePair::Unsigned)
			{
				castValue = builder.CreateZExtOrTrunc(fromValue, toType);
			}

			else if (toInfo.Type == TypeSizePair::Float)
			{
				castValue = builder.CreateSIToFP(fromValue, toType);
			}
		}

		else if (fromInfo.Type == TypeSizePair::Unsigned)
		{
			if (toInfo.Type == TypeSizePair::Bool)
			{
				castValue = builder.CreateICmpNE(fromValue, builder.getInt(llvm::APInt(fromInfo.Size, 0, /*isSigned*/false)));
			}

			else if (toInfo.Type == TypeSizePair::Signed)
			{
				// TODO: check this
				castValue = builder.CreateZExtOrTrunc(fromValue, toType);
			}

			else if (toInfo.Type == TypeSizePair::Unsigned)
			{
				// TODO: check this
				castValue = builder.CreateZExtOrTrunc(fromValue, toType);
			}

			else if (toInfo.Type == TypeSizePair::Float)
			{
				castValue = builder.CreateUIToFP(fromValue, toType);
			}
		}

		else if (fromInfo.Type == TypeSizePair::Float)
		{
			if (toInfo.Type == TypeSizePair::Bool)
			{
				castValue = builder.CreateFCmpONE(fromValue, llvm::ConstantFP::get(*state.Context, llvm::APFloat(0.0)));
			}

			else if (toInfo.Type == TypeSizePair::Signed)
			{
				castValue = builder.CreateFPToSI(fromValue, toType);
			}

			else if (toInfo.Type == TypeSizePair::Unsigned)
			{
				castValue = builder.CreateFPToUI(fromValue, toType);
			}

			else if (toInfo.Type == TypeSizePair::Float)
			{
				castValue = builder.CreateFPCast(fromValue, toType);
			}
		}

		ASSERT(castValue != nullptr, "Invalid cast operation!");

		builder.CreateRet(castValue);

		return function;
	}

	llvm::Function* generateBitCastFunction(LLVMState& state, const std::string_view& mangledName, const std::string_view& fromName, const std::string_view& toName)
	{
		llvm::Type* fromType = state.NamedValues.GetType(fromName)->llvmType;
		llvm::Type* toType = state.NamedValues.GetType(toName)->llvmType;

		ASSERT(fromType != nullptr && toType != nullptr, "Invalid type names!");

		llvm::Type* returnType = toType;
#ifdef FIRST_PARAMETER_BYREF
		std::vector<llvm::Type*> paramTypes = { AlloyType::getPointerType(fromType) };
#else
		std::vector<llvm::Type*> paramTypes = { fromType };
#endif
		llvm::FunctionType* functionType = llvm::FunctionType::get(returnType, paramTypes, false);

		llvm::Function* function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, mangledName, *state.Module);

		ASSERT(function != nullptr, "Failed to generate bit cast function!");

		// force inlining of BitCast functions
		function->addFnAttr(llvm::Attribute::AlwaysInline);
#ifdef FIRST_PARAMETER_BYREF
		// the first parameter is a reference to a variable and not a value
		function->addAttributeAtIndex(1, llvm::Attribute::getWithByRefType(*state.Context, fromType));
#endif

		llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(*state.Context, "entry", function);

		// create a new builder for this function, this will allow us to generate multiple functions in parallel
		llvm::IRBuilder<> builder(*state.Context);
		builder.SetInsertPoint(entryBlock);

#ifdef FIRST_PARAMETER_BYREF
		llvm::Value* fromValue = builder.CreateLoad(fromType, function->getArg(0));
#else
		llvm::Value* fromValue = function->getArg(0);
#endif
		llvm::Value* castValue = nullptr;

		TypeSizePair fromInfo = getTypeSizePair(fromName);
		TypeSizePair toInfo = getTypeSizePair(toName);

		if (fromInfo.Type == TypeSizePair::Float)
		{
			// float to float
			if (toInfo.Type == TypeSizePair::Float)
			{
				castValue = builder.CreateFPCast(fromValue, toType);
			}

			// float to int
			else
			{
				// convert float to int of same size

				if (toInfo.Type == TypeSizePair::Signed)
				{
					if (fromInfo.Size == 64)
					{
						fromValue = builder.CreateFPToSI(fromValue, llvm::IntegerType::getInt64Ty(*state.Context));
					}

					else if (fromInfo.Size == 32)
					{
						fromValue = builder.CreateFPToSI(fromValue, AlloyType::get("i32")->llvmType);
					}
				}

				else if (toInfo.Type == TypeSizePair::Unsigned)
				{
					if (fromInfo.Size == 64)
					{
						fromValue = builder.CreateFPToUI(fromValue, llvm::IntegerType::getInt64Ty(*state.Context));
					}

					else if (fromInfo.Size == 32)
					{
						fromValue = builder.CreateFPToUI(fromValue, AlloyType::get("i32")->llvmType);
					}
				}

				// castValue is now an integer
				// from big int to small int, just truncate; from small int to big int, zero-extend
				castValue = builder.CreateZExtOrTrunc(fromValue, toType);
			}
		}

		else
		{
			// int to float
			if (toInfo.Type == TypeSizePair::Float)
			{
				
				if (fromInfo.Type == TypeSizePair::Signed)
				{
					castValue = builder.CreateSIToFP(fromValue, toType);
				}

				else if (fromInfo.Type == TypeSizePair::Unsigned)
				{
					castValue = builder.CreateUIToFP(fromValue, toType);
				}
				
			}

			// int to int
			else
			{
				castValue = builder.CreateZExtOrTrunc(fromValue, toType);
			}
		}
		

		ASSERT(castValue != nullptr, "Invalid cast operation!");

		builder.CreateRet(castValue);

		return function;
	}

	llvm::Function* generateBuiltInFunction(LLVMState& state, const std::string_view& mangledName)
	{
		llvm::Function* function = state.Module->getFunction(mangledName);
		if (function != nullptr)
		{
			return function;
		}

		// currently, all built-in functions are casts, which must be member functions and have one generic parameter and the Self parameter
		// therefore, we assume the mangled name matches the format "type@function@param_type@self_type"
		std::vector<std::string> parts;
		size_t separator = 0, last = 0;
		std::string name(mangledName);
		std::replace(name.begin(), name.end(), NodeBuffer::GENERICS_SEPARATOR, NodeBuffer::TYPE_SEPARATOR);
		while (separator = name.find(NodeBuffer::TYPE_SEPARATOR, last)) {
			parts.push_back(name.substr(last, separator - last));
			if (separator == std::string_view::npos)
				break;
			last = separator + 1;
		}
		ASSERT(parts.size() == 4, "Invalid built-in function name!");

		llvm::Function* result = nullptr;

		if (parts[1] == CAST_FUNCTION_NAME)
		{
			result = generateCastFunction(state, mangledName, parts[0], parts[2]);
		}
		else if (parts[1] == BIT_CAST_FUNCTION_NAME)
		{
			result = generateBitCastFunction(state, mangledName, parts[0], parts[2]);
		}

		return result;
	}
}