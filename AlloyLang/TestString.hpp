#pragma once

constexpr auto TestSrc2 = R"(
const res : i64 = 0;
var e : f32 = 1.0;

fn add (const a : &i64, const b : &i64) -> i64
{
	return (a + b);
}

fn mul (const c : &i64, const d : &i64) -> i64
{
	if (c == 1)
		return d;
	else {
		var val : i64 = 0;
		for (var i : i64 = 0; i < c; i = i + 1) {
			val = val + d;
		}	
		return val;
	}
}

fn main () -> i64
{
	return mul(3, 5);
};

)";

constexpr auto TestSrc1 = R"(
extern fn test (var a : i64);

// this is a sinlge line comment
struct Vector3
{
	var x : f32;
	var y : f32;
	var z : f32;
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