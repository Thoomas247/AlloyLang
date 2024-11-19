//
// The AlloyType class enhances llvm::Type and adds functionality such as:
// - Type names as defined by the Alloy language in addition to the llvm type names
// - Differentiantion betwwen signed and unsigned integers
// - For pointer types, store the pointed to Type
//

#pragma once

namespace AlloyCompiler
{
    class AlloyType
    {
    public:
        llvm::Type* llvmType;

        // for pointer and reference types, llvm does not store the type pointed to by the pointer
        // we have to keep track of it ourselves
        // containedType is null if this is not a pointer nor a reference
        AlloyType* containedType = nullptr;

        static AlloyType* getIntType(llvm::LLVMContext& llvmContext, unsigned bits, bool signedInt, const std::string& typeName);

        static inline AlloyType* get(const std::string& typeName) { return AlloyTypeMap.at(typeName); }

        // pointer type creation methods
        static AlloyType* getPointerType(const std::string& containedType);
        static AlloyType* getPointerType(llvm::Type* containedType);
        static AlloyType* getPointerType(AlloyType* containedType);

        // accessors
        inline const std::string& name() { ASSERT(!alloyTypeName.empty(), "Alloy types should always have a name!"); return alloyTypeName; }
        inline const llvm::Type::TypeID getTypeID() { return llvmType->getTypeID(); }

        // check if this AlloyType is of the same type as the "right" parameter
        bool operator==(const llvm::Type* right) {
            return (llvmType == right);
        }

        inline bool isIntegerTy() const { return llvmType->isIntegerTy(); }
        inline int getIntegerBitWidth() const { return llvmType->getIntegerBitWidth(); }

        // get an AlloyType from an llvm Type
        static AlloyType* get(llvm::Type* llvmType);

        // clear all types before exiting the application
        static void ClearAlloyTypes();

    protected:
        AlloyType(llvm::LLVMContext& llvmContext, llvm::Type* llvmType)
            : llvmContext(llvmContext), llvmType(llvmType) {}

    private:            
        static std::unordered_map<std::string, AlloyType*> AlloyTypeMap;
        static std::unordered_map<llvm::Type*, AlloyType*> AlloyTypeIdMap;
        
        llvm::LLVMContext& llvmContext;
        std::string alloyTypeName;
        bool isSigned;        
    };

}
