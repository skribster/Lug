---
title: Values
tags: Learn
layout: guide.njk
---

Lug is a dynamically typed language, which means you don't have to specify 
each type of a value yourself. Each value carries its own type.

There are four basic primitive types in Lug:
- Nil
- Number
- Boolean
- String

The other types, such as functions, lists, dictionaries, and things, 
are a little more complex, and I think they all deserve their own chapters.

Anyway, we'll go over the basic primitive types in order, starting off with...

## Nil
While every other type represents an actual value, nil is the opposite.
It means the absence of a value. As such, it's commonly used in places 
where you expect a value but return nothing. 

For instance, a function without any return statement returns 'nil' 
implicitly, because it didn't return any value. 
```
box doSomething <- function()
    print("I'm doing something!")
end

box returned <- doSomething()
print(returned) // nil
```

## Number

Numbers are... well, *numbers.* All number values represent a double-precision 
floating point number. Lug has no integer type.

Number literals are what you'd expect from any other scripting language. They can
have an optional decimal part, and even an optional exponent part. These are all
valid number literals in the Lug language:
```
5
-3.1415926
1.2e16
8e+23
2.61e-4
```

## Boolean

The boolean type represents two values: `true` and `false.` They come useful in
representing a two-state (true or false) value or make a yes/no decision. As we'll go
over later in another lesson, booleans play their part in control flow, where Lug checks
if the expression inside a conditional is 'true'.

```
box x <- 5

// Since x = 5 is true, then the inner body will run.
if x = 5 then
    ...
end
```

However, do note that any value can represent a condition! Lua recognizes all values
except false and nil as truthy, and everything else falsy. That means you can write
some weird stuff like this:
```
box a <- nil
if a then
    // won't run...
end

a <- 10
if a then
    // will run, because a is "truthy", neither false nor nil
end
```

## String

Strings are values that hold a piece of text information.

You can make a string literal by matching single or double quotes:
```
"this is a valid string literal"
'this is too!'
```

Strings also support C-like escape sequences. Let's say you have a
string literal where the literal itself has quotes inside of it. You can 
escape those quotes with backslashes:
```
"I said "Hello"" // invalid, the second quote will close the first one
"I said \"Hello\"" // valid!!!
```

How about a table? Here's all of the available escape sequences that Lug offers:
| Escape-sequence   | Meaning           |
| :---              | :---              |
| \0                | null-terminator   |
| \\                | backslash         |
| \a                | bell*             |
| \b                | backspace         |
| \e                | ESC character     |
| \f                | formfeed          |
| \n                | newline           |
| \r                | carriage return   |
| \t                | tab               |
| \v                | vertical tab      |

*<small><center>\*A lot of these are useless. What's the bell for????</center></small>*