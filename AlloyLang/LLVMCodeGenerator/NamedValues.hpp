#pragma once

#include <string>
#include <map>

class llvm::AllocaInst;

static int objects_created = 0;

class CGNamedValues
{
public:

	CGNamedValues(std::shared_ptr<CGNamedValues> Parent = nullptr) : parent(Parent) { /*objects_created++; printf_s("Number of CGNamedValues : %d\n", objects_created);*/ }
	~CGNamedValues() { /*objects_created--; printf_s("Number of CGNamedValues : %d\n", objects_created);*/ }

	llvm::AllocaInst* contains(const std::string& Name, bool checkParents = false) {
		llvm::AllocaInst* value = nullptr;
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
	void insert(const std::string& Name, llvm::AllocaInst* value) { values[Name] = value; }
	void clear() { values.clear(); }

	std::shared_ptr<CGNamedValues>& getParent() { return parent; }

	static llvm::Type* AlloyToLLVMType(llvm::LLVMContext& llvmContext,
		const AlloyCompiler::NodeBuffers& NodeBuffers,
		const AlloyCompiler::TokenBuffers& TokenBuffers,
		AlloyCompiler::NodeID id) {
		//
		// get llvm type from AlloyLang types
		// the input node ID should be of type TYPE_IDENTIFIER
		//
		// TBD: this method is called frequently, we need to use a map for faster lookups
		// We also need to dynamically add new types to the map, e.g. structures
		//
		const char* AlloyTypes[] = { "i64", "F32", "F64", "String", "Vector3" };	// Vector3 is only for testing, we need to be able to add this dynamically
		llvm::Type* LLVMTypes[] = {
			llvm::Type::getInt64Ty(llvmContext),
			llvm::Type::getPrimitiveType(llvmContext, llvm::Type::TypeID::FloatTyID),
			llvm::Type::getPrimitiveType(llvmContext, llvm::Type::TypeID::DoubleTyID),
			llvm::PointerType::get(llvm::IntegerType::get(llvmContext, 8), 0),		// convert string to uint_8*
			llvm::StructType::getTypeByName(llvmContext, "Vector3")					// this is how structure types should be added
		};
		llvm::Type* llvmType = nullptr;

		assert(NodeBuffers.GetNode(id).Kind == AlloyCompiler::NodeKind::TYPE_IDENTIFIER);
		const AlloyCompiler::TYPE_IDENTIFIER& ti = NodeBuffers.GetNode(id).TypeIdentifier;
		const AlloyCompiler::IDENTIFIER& i = NodeBuffers.GetNode(ti.IdentifierOrTypeIdentifierID).Identifier;
		std::string AlloyType(TokenBuffers.GetValue(i.IdentifierTokenID).ToStringView());

		for (int t = 0; t < sizeof(AlloyTypes) / sizeof(AlloyTypes[0]); t++) {
			if (AlloyType == AlloyTypes[t]) {
				llvmType = LLVMTypes[t];
				break;
			}
		}

		return llvmType;
	}

	static int CompareTypes(const llvm::Type* L, const llvm::Type* R) {
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

	static llvm::Type* MakeCompatible(llvm::Value*& L, llvm::Value*& R) {
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

private:
	std::shared_ptr<CGNamedValues> parent;
	std::map<std::string, llvm::AllocaInst*> values;
};
