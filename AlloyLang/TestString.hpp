#pragma once

constexpr auto TestSrc1 = R"(
extern fn printf (const str : String) -> i32;

fn main () -> i64
{
	printf("Basic");
	return 1;
}
)";

constexpr auto TestSrc2 = R"(
extern fn test (var a : i64);
extern fn printf (const format : String, ...);

// array of i64
exp var array : [i64; 10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

// array of references to i64
pub var refArray : [&i64; 3] = { &array[0], &array[1], &array[2] };

// array of pointers to i64
const ptrArray : [*i64; 3] = { new array[0], new 0, new 0 };

// array of i64 from ptrArray
const arr : [i64; 3] = { @ptrArray[0], @ptrArray[1], @ptrArray[2] };

// pointer to array of i64
const ptr : *[i64; 3] = new { 1, 2, 3 };

// reference to array of i64
const ref : &[i64; 3] = &array;

// pointer to array of dynamic size, initialized to 0 (has to be pointer)
const dynArray : *[i64] = new [0; array[4]];

// this is a sinlge line comment
struct Vector3
{
	var x : f32;
	var y : f32;
	var z : f32;
}

struct Transform
{
	var position : Vector3;
	var rotation : Vector3;
	var scale : Vector3;
}

/*
* This is a multi line comment.
/* Nested multi line comment. */
*/
enum TokenTypes
{
	Identifier,
	Number,
	Operator,
	Keyword,
	Comment,
	Whitespace,
	Newline,
	Unknown
}

fn struct_constructor () -> Vector3
{
	return Vector3 
	{ 
		x = 32.0, 
		y = 64.0, 
		z = 0.0 
	};
}

fn struct_ptr_constructor () -> var *Vector3
{
	return new Vector3 
	{ 
		x = 32.0,
		y = 64.0,
		z = 0.0,	// trailing comma is allowed
	};
}

fn member_assignment (var t : &Transform, const value : f32)
{
	t.rotation.x = value;
}

fn member_access (const t : &Transform) -> f32
{
	return t.rotation.x;
}

fn do_nothing (var a : i64, const b : &bool)
{}

fn add (const a : &i64, const b : &i64) -> i64
{
	return a + b;
}

fn sub (const a : &i64, const b : &i64) -> i64
{
	return a - b;
}

fn mul (const a : &i64, const b : &i64) -> i64
{
	return a * b;
}

fn div (const a : &i64, const b : &i64) -> i64
{
	return a / b;
}

fn factorial (const a : &i64) -> i64
{
	if (a <= 1)
	{
		return 1;
	}
	else
	{
		return a * factorial(a - 1);
	}
}

fn loop (const a : &i64) -> i64
{
	var res : i64 = 0;
	for (var i : i64 = 0; i < a; i = i + 1)
	{
		res = res + i;
	}
	return res;
}

fn while_loop (const a : &i64) -> i64
{
	var res : i64 = 0;
	var i : i64 = 0;
	while (i < a)
	{
		res = res + i;
		i = i + 1;
	}
	return res;
}

fn anon_block (const a : &i64) -> i64
{
	{
		return a;
	}
}

fn if_else (const a : &i64) -> i64
{
	if (a < 0)
	{
		return -1;
	}
	else if (a == 0)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

fn fn_call (const a : &i64) -> i64
{
	return add(a, 1);
}

fn void_fn_call (const a : &i64)
{
	do_nothing(a, true);
}

exp var a : i64 = 2;
pub var c : char = 'A';

const res : i64 = div(a, 200);

var s : String = "ABC";
const d : char = '\0';
var e : f32 = 1.01;

const num : i32 = a + e / 50;

const b : i8 = 0;

)";