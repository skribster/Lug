---
title: Configuration
layout: guide.njk
tags: embedding
---

Recall that Lug::VM takes in an optional VMConfig inside its constructor. There are quite a bit of settings that you can customize inside a VMConfig. Here's what the struct may look like:

```
struct VMConfig {
    ResolveModuleFn resolveModuleFn { DefaultResolveModule };
    LoadModuleFn loadModuleFn { DefaultFileLoader };
    WriteFn writeFn { DefaultWriter };
    ErrorFn errorFn { [](const Error& error) {} };
};

```