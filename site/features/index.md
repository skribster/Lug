---
title: Features
layout: guide.njk
---

## Primitives
```
// number
print(123)         

// string
print("double quoted") 
print('single quoted') 

// boolean
print(true)
print(false)

// nil
print(nil)
```

## Variables

```
box a <- 10
print(a) // 10

box b <- a
print(b) // 10
```

```
// Lexical scoping works too.

box a <- "outer"
print(a) // outer

do
    box a <- "inner"
    print(a) // inner
end
```

## Control flow

### If

```
box money <- 10
box cost <- 5

if money < cost then
    print("You don't have enough to buy this item.")
elseif money >= cost then
    print("Bought item!")
    money <- money - cost
end
```

### Count

```
count incrementer2 from 0 to 10 do
    if incrementer2 = 5 then
        continue
    end
    
    print(incrementer2)
end
```

### Foreach

```
box letters <- ['a', 'b', 'c', 'd', 'e']
foreach letter at index in letters do
    print(index, letter)
end
```

### Inspect
```
box PlayerState <- category
    IDLE,
    WALK,
    RUN,
    MIDAIR,
    DASH
end

box state <- PlayerState.RUN // expect: Player is moving.

inspect state
    when PlayerState.IDLE do
        print("Player is not moving.")
    end

    when PlayerState.WALK
    when PlayerState.RUN
    when PlayerState.DASH do
        print("Player is moving.")
    end

    when PlayerState.MIDAIR do
        print("Player is midair.")
    end
end
```

## Functions
```
box add <- function(a, b)
    return a + b
end

print(add(1, 2))
```

```
box fib <- function(n)
    if n < 2 then return n end
    return fib(n - 2) + fib(n - 1)
end

print(fib(35))
```

## Collections

### Lists
```
box list <- [1, 2, 3, 4, 5]
box matrix <- [
    [1, 2],
    [3, 4],
    [5, 6]
]

print(list[0]) // 1
print(list[2]) // 3
print(list[-1]) // 5

list[1] <- "Two"
print(list[1]) // Two
```

### Dictionaries
```
box dict <- {
    "a" <- 1,
    "b" <- 2,
    "c" <- 3,
    "d" <- 4
}


print(dict["a"]) // 1
print(dict["b"]) // 2
dict["e"] <- 5
```

## Categories
```
box Fruit <- category 
    BANANA,
    APPLE,
    ORANGE,
    STRAWBERRY,
    BLUEBERRY,
    MANGO
end

// Categories behave as numbers
print(Fruit.BANANA)     // expect: 0
print(Fruit.APPLE)      // expect: 1
print(Fruit.ORANGE)     // expect: 2
print(Fruit.STRAWBERRY) // expect: 3
print(Fruit.BLUEBERRY)  // expect: 4
print(Fruit.MANGO)      // expect: 5
```

```
box Level <- category
    LOW <- 10,
    LOW_2,
    LOW_3,
    MEDIUM <- 20,
    MEDIUM_2,
    MEDIUM_3,
    HIGH <- 30,
    HIGH_2,
    HIGH_3
end

print(Level.LOW)        // expect: 10
print(Level.LOW_2)      // expect: 11
print(Level.LOW_3)      // expect: 12
print(Level.MEDIUM)     // expect: 20
print(Level.MEDIUM_2)   // expect: 21
print(Level.MEDIUM_3)   // expect: 22
print(Level.HIGH)       // expect: 30
print(Level.HIGH_2)     // expect: 31
print(Level.HIGH_3)     // expect: 32
```

## Things
```
box cat <- thing
cat.name <- "Whiskers"
cat.age <- 5
print(cat.name) // Whiskers

cat.meow <- function(self)
    print(self.name + " meows :-)")
end

cat.meow(cat)
// passes itself as the first parameter
cat:meow() 
```

## Pass-by-value Semantics
```
box numberList <- [1, 2, 3, 4, 5]

// numberList copies here
box numberListCopy <- numberList
numberListCopy:append(6)
print(numberList) // prints [1, 2, 3, 4, 5]
```

## Sharing
```
// You can share an object's reference and store it
box numberListRef <- share numberList
numberListRef:append(6)
print(numberList) // prints [1, 2, 3, 4, 5, 6]
```

```
// Functions can also take in refs
box appendList <- function(list)
    list:append("appended")
end

box testList <- ["a", "b", "c", "d", "e"]
appendList(testList)
print(testList) // ["a", "b", "c", "d", "e"]
appendList(share testList) // share the value.
print(testList) // ["a", "b", "c", "d", "e", "appended"]
```

```
// Functions can force the user to pass a shareable
box popList <- function(shared list)
    list:pop()
end

box testList2 <- ["a", "b", "c", "d", "e"]
popList(share testList2)
popList(testList2) // runtime error: expected shared argument at argument 1
```

## Variadic arguments
```
box sum <- function(args...) // All variadic args get packed into one list.
    print(type(args)) // list

    box total <- 0
    foreach value at index in args do
        total <- total + value
    end

    return total
end

print(sum(10, 20, 30)) // 60
```

## Default arguments
```
// default args must be at the end 
box greet <- function(name, greeting <- "Hello")
    print(greeting + " " + name)
end

greet("Jason") // Hello Jason
greet("Bob", "Hi") // Hi Bob
print("Charlie", "Welcome") // Welcome Charlie
```

## Metaprogramming
```
// 'thing's can have dunder methods that affect
// its behavior in the code.

box Animal <- thing
Animal.name <- "unknown"
Animal.age <- 0

box Dog <- thing

// lookup chain, if Dog.__proto != nil and we try
// to access an attribute that Dog doesn't have, it
// tries looking it up in its proto (Animal).
Dog.__proto__ <- share Animal
print(Dog.name) // "default"
Dog.name <- "Rex"
print(Dog.name) // "Rex"

// Shorthand syntax for __proto__:
box Dog <- thing of Animal
print(Dog.name) // "default"
```
```
// Other dunder examples:

// {} is syntactic sugar for setting attributes
box Vec3 <- thing {
    x <- 0,
    y <- 0,
    z <- 0
}


Vec3.new <- function(x, y, z)
    return thing of Vec3 {
        x <- x,
        y <- y,
        z <- z
    }
end


Vec3.__add__ <- function(shared self, shared other)
    if type(other) != "thing" or other.__proto__ != Vec3 then   
        error("can't add Vec3 with " + type(other))
    end
    
    return Vec3.new(self.x + other.x, self.y + other.y, self.z + other.z)
end

Vec3.__subtract__ <- function(shared self, shared other)
    if type(other) != "thing" or other.__proto__ != Vec3 then   
        error("can't add Vec3 with " + type(other))
    end
    
    return Vec3.new(self.x - other.x, self.y - other.y, self.z - other.z)
end

Vec3.__multiply__ <- function(shared self, shared other)
    if type(other) != "thing" or other.__proto__ != Vec3 then   
        error("can't add Vec3 with " + type(other))
    end
    
    return Vec3.new(self.x * other.x, self.y * other.y, self.z * other.z)
end

Vec3.__divide__ <- function(shared self, shared other)
    if type(other) != "thing" or other.__proto__ != Vec3 then   
        error("can't add Vec3 with " + type(other))
    end
    
    return Vec3.new(self.x / other.x, self.y / other.y, self.z / other.z)
end


Vec3.__tostring__ <- function(shared self)
    return "Vec3(" + string(self.x) + ", " + string(self.y) + ", " + string(self.z) + ")"
end


box a <- Vec3.new(1, 4, 9)
box b <- Vec3.new(4, 2, 1)

print(a) // Vec3(1, 4, 9)
print(b) // Vec3(4, 2, 1)
print(a + b) // expect: Vec3(5, 6, 10)
print(a - b) // expect: Vec3(-3, 2, 8)
print(a * b) // expect: Vec3(4, 8, 9)
print(a / b) // expect: Vec3(0.25, 2, 9)
```

## Including Files

```
box MathUtils <- thing

MathUtils.add <- function(a, b)
    return a + b
end

return MathUtils
```

```
// in other file...
box MathUtils <- unbox("path/to/MathUtils.lug")
print(MathUtils.add(1, 2)) // 3
```