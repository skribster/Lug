---
title: Functions
tags: Learn
layout: guide.njk
---

In Lug, functions act like any other basic value! Like numbers, strings,
lists, dicts, etc. they have their own literal syntax and can be 
passed around in boxes.

An example of a function expression that, when called, returns the sum of 
two numbers is seen below:
```
function(x, y) 
    return x + y
end
```

Note that in order to actually make a function useful, per se, it's often 
necessary to store it in a name.
```
box add <- function(x, y)
    return x + y
end
```

There is no special syntax for function declarations, it's merely just 
storing a function value inside a variable. Really, you will see this 
pattern come up very often in Lug, where everything revolves around this
universal 'box' concept that can hold any value. Consistent, yes?

Now the box named 'add' stores the value of our function. We can print,
pass around, or interact with this function like any other Lug value. 
```
box add2 <- add
print(add2) // <fn anonymous>

box sum <- add(1, 2)
print(sum) // 3
```