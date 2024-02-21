#include "llvm.hpp"
#include "LLVMCodeGenerator.hpp"
#include "NamedValues.hpp"

using namespace llvm;

namespace AlloyCompiler
{
	std::map<std::string, llvm::Type*> CGNamedValues::alloyToLlvmMap;

	CGNamedValues::CGNamedValues(LLVMContext& llvmContext, std::shared_ptr<CGNamedValues> Parent /*= nullptr*/) 
		: parent(Parent) {

		// first call, prefill static map with know types
		if (alloyToLlvmMap.size() == 0) {
			const std::string AlloyTypes[] = { "i64", "f32", "f64", "String" };
			llvm::Type* LLVMTypes[] = {
				llvm::Type::getInt64Ty(llvmContext),
				llvm::Type::getDoubleTy(llvmContext),
				llvm::Type::getDoubleTy(llvmContext),
				llvm::PointerType::get(llvm::IntegerType::get(llvmContext, 8), 0)		// convert string to uint_8*
			};

			// prefill the map with known types
			for (int t = 0; t < sizeof(AlloyTypes) / sizeof(AlloyTypes[0]); t++) {
				alloyToLlvmMap[AlloyTypes[t]] = LLVMTypes[t];
			}
		}
	}

	CGNamedValues::~CGNamedValues() {
	}

	AllocaInst* CGNamedValues::contains(const std::string& Name, bool checkParents /*= false*/) {
		AllocaInst* value = nullptr;
		if (values.contains(Name)) {
			value = values[Name];
		}
		else {
			if (checkParents && parent != nullptr) {
				value = parent->contains(Name, checkParents);
			}
		}
		return value;
	}

	/*static*/
	llvm::Type* CGNamedValues::AlloyToLLVMType(llvm::LLVMContext& llvmContext,
		const AlloyCompiler::NodeBuffers& NodeBuffers,
		const AlloyCompiler::TokenBuffers& TokenBuffers,
		AlloyCompiler::NodeID id) {
		//
		// get llvm type from AlloyLang types
		// the input node ID should be of type TYPE_IDENTIFIER
		//

		llvm::Type* llvmType = nullptr;
		std::string AlloyType;
		if (NodeBuffers.GetNode(id).Kind == AlloyCompiler::NodeKind::TYPE_IDENTIFIER) {
			const AlloyCompiler::TYPE_IDENTIFIER& ti = NodeBuffers.GetNode(id).TypeIdentifier;
			const AlloyCompiler::IDENTIFIER& i = NodeBuffers.GetNode(ti.IdentifierOrTypeIdentifierID).Identifier;
			AlloyType = TokenBuffers.GetValue(i.IdentifierTokenID).ToStringView();
		}
		else
		{
			if (NodeBuffers.GetNode(id).Kind == AlloyCompiler::NodeKind::IDENTIFIER) {
				const AlloyCompiler::IDENTIFIER& i = NodeBuffers.GetNode(id).Identifier;
				AlloyType = TokenBuffers.GetValue(i.IdentifierTokenID).ToStringView();
			}
			else {
				assert(false);
			}
		}

		llvmType = alloyToLlvmMap[AlloyType];

		assert(llvmType != nullptr);	// TBD: unknown type
		return llvmType;
	}

	/*static*/
	int CGNamedValues::CompareTypes(const llvm::Type* L, const llvm::Type* R) {
		//
		// given 2 LLVM types, we need to know which is higher, i.e. to which one we should convert the other type without losing precision
		// return 0 if the types are equal, 1 if L is greater than R, -1 if L is less than R, -2 if no conversion is possible (e.g. String)
		// TBD: define return values as an enum other than -2, -1, 0, 1
		// 
		llvm::Type::TypeID llvmTypes[] = { llvm::Type::TypeID::IntegerTyID, llvm::Type::TypeID::FloatTyID, llvm::Type::TypeID::DoubleTyID };
		llvm::Type::TypeID left = L->getTypeID();
		llvm::Type::TypeID right = R->getTypeID();
		if (left == right) {
			return 0;
		}
		else {
			int rankL = -1, rankR = -1;
			// search for the rank of the left and right types
			for (int r = 0; r < sizeof(llvmTypes) / sizeof(llvmTypes[0]); r++) {
				if (llvmTypes[r] == left) {
					rankL = r;
				}
				if (llvmTypes[r] == right) {
					rankR = r;
				}
			}
			if (rankL == -1 || rankR == -1) {
				return -2;	// no conversion is possible
			}
			return (rankL > rankR ? 1 : -1);
		}
	}

	/*static*/
	llvm::Type* CGNamedValues::MakeCompatible(llvm::Value*& L, llvm::Value*& R) {
		//
		// For binary operators, we need to the 2 operands to be of the same type, usually the one higher in ranking
		// E.g. to add a float to an int we convert the int value to float
		//
		llvm::Type* result = L->getType();
		if (L->getType() != R->getType()) {
			switch (CompareTypes(L->getType(), R->getType())) {

			case 1:
				// convert the 2 types to the type of the left operand
				// TBD ...
				break;

			case -1:
				// convert the 2 types to the type of the right operand
				result = R->getType();
				// TBD ...
				break;

			case -2:
				// cannot compare/convert the 2 types
				result = nullptr;
				break;

			default:
				assert(false);	// no other values are allowed
				result = nullptr;
				break;
			}
		}
		return result;
	}

}	// end of AlloyCompiler namespace
