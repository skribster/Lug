// Components ----------------------------------

const userInput = document.querySelector("#playground-input");
const output = document.querySelector("#playground-output");
const runButton = document.querySelector("#playground-run");
const examples = document.querySelectorAll(".playground-example")

// Example codes ----------------------------------

let exampleCode = new Map();
exampleCode.set("Hello World", `print("Hello, world!")`)
exampleCode.set("Loop", `count counter from 1 to 10 by 1 do
    print("Count: " + string(counter))
end`)
exampleCode.set("Fibonacci", `box fib <- function(n)
    if n < 2 then
        return n
    end
    return fib(n - 1) + fib(n - 2)
end

box start <- clock()
print(fib(35))
print(clock() - start)`)

exampleCode.set("Fizzbuzz", `box fizzBuzz <- function(length)
    box res <- []
    count i from 1 to length do
        if i % 3 = 0 and i % 5 = 0 then
            res:append("fizzbuzz")
        elseif i % 3 = 0 then
            res:append("fizz")
        elseif i % 5 = 0 then
            res:append("buzz")
        else
            res:append(string(i))
        end
    end

    return res
end

box res <- fizzBuzz(20)

foreach s in res do
    print(s)
end`)
exampleCode.set("Sieve of Eratosthenes", `box sieve <- function(n)
    box flags <- []
    count i from 0 to n do
        flags:append(true)
    end

    box found <- 0
    box j <- 0
    count k from 2 to n do
        if flags[k] then
            found <- found + 1
            j <- k * k
            while j <= n do
                flags[j] <- false
                j <- j + k
            end
        end
    end
    return found
end

box start <- clock()
print(sieve(200000))
print(clock() - start)`)

exampleCode.set("OOP Example", `box Dog <- thing // Create a new thing and stuff it inside the container Dog
// User-defined constructor
Dog.new <- function(name, age, sound)
    /* 
         'thing of Dog' is syntactic sugar for:
        box self <- thing
        self.__proto__ <- share Dog
        where __proto__ is another 'thing' to
        check when lookup fails
    */
    box self <- thing of Dog {
        /* 
            Syntactic sugar for setting each attribute
            by hand. (i.e. self.name <- name self.age <- age etc...)
        */
        name <- name,
        age <- age,
        sound <- sound
    }
    
    return self
end

Dog.birthday <- function(shared self)
    self.age <- self.age + 1
    print(self.name + " is turning " + string(self.age))
end

Dog.speak <- function(shared self)
    print(self.name + " says " + self.sound + "!")
end

Dog.__tostring__ <- function(shared self)
    return "<dog " + self.name + ">"
end

// Let's create a dog!
box buddy <- Dog.new("Buddy", 4, "woof")
buddy:birthday() // expect: Buddy is turning 5
buddy:birthday() // expect: Buddy is turning 6`)


// Main code ----------------------------------

let modulePromise;

runButton.addEventListener("click", async () => {
    runButton.disabled = true;
    
    try {
        modulePromise ??= import("./lug-website.js")
            .then(({ default: createLugModule }) => createLugModule());
        const lug = await modulePromise;
        output.textContent = lug.run(userInput.value);
    } catch (error) {
        modulePromise = undefined;
        output.textContent = "";
    } finally {
        runButton.disabled = false;
    }
});


userInput.addEventListener('keydown', function (e) {
    if (e.key === 'Escape') {
        this.blur(); // Releases focus so keyboard users can tab away
        return;
    }

    if (e.key === 'Tab') {
        e.preventDefault();

        const start = this.selectionStart;
        const end = this.selectionEnd;

        this.value = this.value.substring(0, start) + '\t' + this.value.substring(end);
        this.selectionStart = this.selectionEnd = start + 1;
    }
});

examples.forEach(link => {
    link.addEventListener("click", (event) => {
        event.preventDefault();
        let code = exampleCode.get(link.textContent);
        if (code === undefined) {
            userInput.value = "Unknown error";
            return;
        }

        userInput.value = code;
    })
});