#pragma once

#include <string>
#include <map>

class llvm::AllocaInst;

static int objects_created = 0;

class CGNamedValues
{
public:

	CGNamedValues(std::shared_ptr<CGNamedValues> Parent = nullptr) : parent(Parent) { objects_created++; printf_s("Number of CGNamedValues : %d\n", objects_created); }
	~CGNamedValues() { objects_created--; printf_s("Number of CGNamedValues : %d\n", objects_created); }

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

private:
	std::shared_ptr<CGNamedValues> parent;
	std::map<std::string, llvm::AllocaInst*> values;
};
