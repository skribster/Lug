const userInput = document.querySelector("#playground-input");
const output = document.querySelector("#playground-output");
const runButton = document.querySelector("#playground-run");

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