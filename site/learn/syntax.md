---
title: Syntax
layout: guide.njk
tags: Learn
---

Lug's syntax aims for simplicity, and should feel 
familiar to langauges like Ruby or Lua.

## Comments
You can make comments on code with double slashes. `(//)`
```
// This is a comment!
```

There are also block comments which span multiple lines. Begin
a block comment with `/*` and end in `*/` .
```
/*
    This comment 
    spans multiple 
    lines...

    /*
        They can also be
        nested too!
    */
*/
```

## Whitespace

Tabs and newlines have no effect on the code flow at all. 
Theoretically, you could have an entire program listed in one
giant line like this:
```
box a <- 10 print(a) box add <- function(x, y) return x + y end print(add(4, 5))
```

Obviously, that's ugly. Which is why it's best practice to 
separate statements with newlines in your code.
```
box a <- 10
print(a)

box add <- function(x, y)
    return x + y
end

print(add(4, 5))
```

Or if you really have to, I like using semicolons to denote the separation of
two statements in the same line. 
```
box a <- 2; print(a)
```
It's optional, however, and semicolons really don't have an effect on the flow
of code other than that.

## Grammar

For the nerdier people who are more experienced in reading formal grammar, 
here's the full grammar of Lug written in Extended 
 [Backus-Naur Form.](https://en.wikipedia.org/wiki/Backus%E2%80%93Naur_form)

```
/* Statements */
statement           ::= box
                      | print
                      | break
                      | continue
                      | block
                      | if
                      | while
                      | for
                      | count

box                 ::= 'box' identifier_list '<-' expression_list
break               ::= 'break'
continue            ::= 'continue'
block               ::= statement* ( 'return' expression )?
if                  ::= 'if' expression 'then' block ('elseif' expression 'then' block)* ('else' block)? 'end'
while               ::= 'while' expression 'do' block 'end'
for                 ::= 'foreach' IDENTIFIER ( 'at' IDENTIFIER )? 'in' expression 'do' block 'end'
count               ::= 'count' IDENTIFIER 'from' expression '...' expression 'by' expression 'do' block 'end'
inspect             ::= "inspect" expression ( "when" expression ( "when" expression )* "do" block "end" )* ( "else" block "end" )? "end"

/* Expressions */
expression          ::= ternary
ternary             ::= logic_or '?' expression ':' ternary
logic_or            ::= logic_and ('or' logic_and)*
logic_and           ::= equality ('and' equality)*
equality            ::= comparison ( ('!=' | '=') comparison )* 
comparison          ::= term ( ('>' | '>=' | '<' | '<=') term )*
term                ::= factor ( ('-' | '+') factor )*
factor              ::= unary ( ('*' | '/') unary )*
unary               ::= ( ('share' | 'not' | '-') unary ) | primary
postfix             ::= primary ( "[" expression "]" | "." IDENTIFIER | "(" arguments? ")" )*
primary             ::= 'null' 
                      | BOOLEAN
                      | NUMBER
                      | STRING
                      | IDENTIFIER
                      | '(' expression ')'
                      | function
                      | list
                      | dictionary
                      | thing

function            ::= 'function' '(' parameters? ')' block 'end'
list                ::= '[' expression_list? ']'
dictionary          ::= '{' keyvalue_list? '}'
thing               ::= 'thing' ( 'of' expression )?

/* Utility rules */
arguments           ::= expression ( ',' expression )*
parameters          ::= parameter ( ',' parameter )*
parameter           ::= 'shared'? IDENTIFIER ('<-' expression)?

expression_list     ::= expression (',' expression)*
identifier_list     ::= IDENTIFIER (',' IDENTIFIER)*
keyvalue_list       ::= keyvalue_pair (',' keyvalue_pair)*
keyvalue_pair       ::= (primary '<-' expression) | ('[' expression ']' '<-' expression)

NUMBER              ::= DIGIT+ ( '.' DIGIT+ )? ;
STRING              ::= ('"' ALPHA* '"') | ("'" ALPHA* "'")
IDENTIFIER          ::= ALPHA ( ALPHA | DIGIT )* 
ALPHA               ::= 'a' ... 'z' | 'A' ... 'Z' | '_'
DIGIT               ::= '0' ... '9'
BOOLEAN             ::= 'true' | 'false'
```