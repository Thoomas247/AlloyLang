#pragma once
#include "parser/ASTNodes.hpp"

#include <memory>
#include <stack>

namespace AlloyCompiler
{
	// struct decl is encountered
	//	parse it
	//	look for type identifier (struct name) in the symbol table's current scope
	//		if it exists, error ("Type already defined within this scope! See previous definition:")
	//	create new TypeIdentifier node with type identifier
	//	add it to the symbol table's current scope
	//	create new scope
	//	parse members
	//	pop scope

	// enum decl is encountered
	//	parse it
	//	look for type identifier (enum name) in the symbol table's current scope
	//		if it exists, error ("Type already defined within this scope! See previous definition:")
	//	create new TypeIdentifier node with type identifier
	//	add it to the symbol table's current scope
	//	create new scope
	//	parse members
	//	pop scope

	// fn decl is encountered
	//	parse it
	//  look for argument and return types in the symbol table
	// 		if any doesn't exist, error ("Type must be defined before being used!")
	//	create mangled name (this is the function's identifier, eg. doSomething(args)(ret))
	//	look for mangled name in the symbol table's current scope
	//		if it exists, error ("Function already defined within this scope! See previous definition:")
	//	create new FunctionIdentifier node with mangled name
	//	add it to the symbol table's current scope
	//	create new scope
	//	add argument identifiers to the symbol table's current scope
	//	parse body
	//	pop scope

	// var/const decl is encountered (do not contain their own scope)
	//	parse it
	//	look for type identifier in the symbol table's current scope
	//		if it doesn't exist, error ("Type must be defined before being used!")
	//	look for identifier in the symbol table's current scope
	//		if it exists, error ("Duplicate variable name in this scope! See previous use:")
	//	create new VariableIdentifier node with type and identifier
	//	add it to the symbol table's current scope

}