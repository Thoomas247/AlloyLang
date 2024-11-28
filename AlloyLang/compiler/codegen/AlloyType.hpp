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

        static const AlloyType* getIntType(llvm::LLVMContext& llvmContext, unsigned bits, bool signedInt, const std::string& typeName);

        static inline const AlloyType* get(const std::string& typeName) { return AlloyTypeMap.at(typeName); }

        // pointer type creation methods
        static const AlloyType* getPointerType(const std::string& containedType);
        static const AlloyType* getPointerType(llvm::Type* containedType);
        static const AlloyType* getPointerType(const AlloyType* containedType);

        // create a combined Type/containedType type. Used for enumerators where we need to type of the enumerator and the type of the payload combined
        static const AlloyType* get(llvm::Type* llvmType, const AlloyType* containedType);

        // accessors
        inline const std::string& name() const { ASSERT(!alloyTypeName.empty(), "Alloy types should always have a name!"); return alloyTypeName; }
        inline const llvm::Type::TypeID getTypeID() const { return llvmType->getTypeID(); }

        // check if this AlloyType is of the same type as the "right" parameter
        bool operator==(const llvm::Type* right) const {
            return (llvmType == right);
        }

        inline bool isIntegerTy() const { return llvmType->isIntegerTy(); }
        inline int getIntegerBitWidth() const { return llvmType->getIntegerBitWidth(); }
        inline const AlloyType* getContainedType() const { return containedType; }

        // get an AlloyType from an llvm Type
        static const AlloyType* get(llvm::Type* llvmType);

        // clear all types before exiting the application
        static void ClearAlloyTypes();

    protected:
        AlloyType(llvm::LLVMContext& llvmContext, llvm::Type* llvmType)
            : llvmContext(llvmContext), llvmType(llvmType) {}

    private:            
        static std::unordered_map<std::string, const AlloyType*> AlloyTypeMap;
        static std::unordered_map<llvm::Type*, const AlloyType*> AlloyTypeIdMap;
        
        // for pointer and reference types, llvm does not store the type pointed to by the pointer
        // we have to keep track of it ourselves
        // containedType is null if this is not a pointer nor a reference
        const AlloyType* containedType = nullptr;

        llvm::LLVMContext& llvmContext;
        std::string alloyTypeName;
        bool isSigned;        
    };

}
