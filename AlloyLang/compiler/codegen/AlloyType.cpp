#include "llvm/llvm.hpp"
#include "NamedValues.hpp"
#include "CodeGenerator.hpp"
#include "AlloyType.hpp"
#include "AlloyValue.hpp"

namespace AlloyCompiler
{
    /*static*/
    std::unordered_map<std::string, const AlloyType*> AlloyType::AlloyTypeMap;
    /*static*/
    std::unordered_map<llvm::Type*, const AlloyType*> AlloyType::AlloyTypeIdMap;

    /*static*/
    void AlloyType::ClearAlloyTypes()
    {
        for (auto alloyType = AlloyTypeMap.begin(); alloyType != AlloyTypeMap.end(); alloyType++)
        {
            delete alloyType->second;
        }

        AlloyTypeMap.clear();
        AlloyTypeIdMap.clear();
    }

    /*static*/
    const AlloyType* AlloyType::getIntType(llvm::LLVMContext& llvmContext, unsigned bits, bool signedInt, const std::string& typeName)
    {
        if (AlloyTypeMap.contains(typeName)) {
            return AlloyTypeMap.at(typeName);
        }
        else {
            llvm::Type* llvmType = llvm::IntegerType::get(llvmContext, bits);
            AlloyType* alloyType = new AlloyType(llvmContext, llvmType);
            alloyType->alloyTypeName = typeName;
            alloyType->isSigned = signedInt;
            AlloyTypeMap[typeName] = alloyType;
            AlloyTypeIdMap[llvmType] = alloyType;
            return alloyType;
        }   
    }

    /*static*/
    const AlloyType* AlloyType::get(llvm::Type* llvmType)
    {
        //
        // get an AlloyType from an llvm Type
        //
        const AlloyType* result = nullptr;

        if (AlloyTypeIdMap.contains(llvmType)) {
            result = AlloyTypeIdMap.at(llvmType);
        } 
        else {
            // retrieve the name of the llvm type
            std::string typeName;
            // structures usually have a name associated with them, try to retrieve that first
            if (isa<llvm::StructType>(llvmType)) 
                typeName = llvmType->getStructName().data();
            if (typeName.empty()) {
                llvm::raw_string_ostream str(typeName);
                llvmType->print(str);
            }

            // check if this type name is already in the map, otherwise create a new AlloyType
            if (!AlloyTypeMap.contains(typeName)) {
                AlloyType* alloyType = new AlloyType(llvmType->getContext(), llvmType);
                alloyType->alloyTypeName = typeName;
                alloyType->isSigned = true;
                AlloyTypeMap[typeName] = alloyType;
                AlloyTypeIdMap[llvmType] = alloyType;
                result = alloyType;
            }
            else {
                result = AlloyTypeMap.at(typeName);
                AlloyTypeIdMap[llvmType] = result;
            }
        }

        return result;
    }


    /*static*/
    const AlloyType* AlloyType::getPointerType(const std::string& containedType)
    {
        const AlloyType* result = get(containedType);

        if (result) {
            result = getPointerType(result);
        }

        ASSERT(result != nullptr, "Tried to retrieve pointer to unknown type!");

        return result;
    }

    /*static*/
    const AlloyType* AlloyType::getPointerType(llvm::Type* containedType)
    {
        return getPointerType(get(containedType));
    }

    /*static*/
    const AlloyType* AlloyType::getPointerType(const AlloyType* containedType)
    {
        std::string typeName = containedType->alloyTypeName + "*";

        if (AlloyTypeMap.contains(typeName)) {
            return AlloyTypeMap.at(typeName);
        }
        else {
            llvm::Type* llvmType = llvm::PointerType::get(containedType->llvmContext, 0);
            AlloyType* alloyType = new AlloyType(containedType->llvmContext, llvmType);
            alloyType->alloyTypeName = typeName;
            alloyType->containedType = containedType;
            alloyType->isSigned = false;
            AlloyTypeMap[typeName] = alloyType;
            AlloyTypeIdMap[llvmType] = alloyType;
            return alloyType;
        }
    }

    // create a combined Type/containedType type. Used for enumerators where we need to type of the enumerator and the type of the payload combined
    /*static*/
    const AlloyType* AlloyType::get(llvm::Type* llvmType, const AlloyType* containedType)
    {
        if (containedType == nullptr)
            return get(llvmType);
        else {
            if (llvmType->isPointerTy()) {
                return getPointerType(containedType);
            }
            else {
                std::string typeName = get(llvmType)->alloyTypeName + NodeBuffer::POINTER_SEPARATOR + containedType->alloyTypeName;

                if (AlloyTypeMap.contains(typeName)) {
                    return AlloyTypeMap.at(typeName);
                }
                else {
                    AlloyType* alloyType = new AlloyType(containedType->llvmContext, llvmType);
                    alloyType->alloyTypeName = typeName;
                    alloyType->containedType = containedType;
                    alloyType->isSigned = false;
                    AlloyTypeMap[typeName] = alloyType;
                    AlloyTypeIdMap[llvmType] = alloyType;
                    return alloyType;
                }
            }
        }
    }
}
