# Coding Rules for Contributors

## Architecture Overview

It is important to understand how phiola's main components (Executor, Core, Tracks, and Filters) interact; it's explained in the README file: [How to Use API](README.md#how-to-use-api).

## General Principles

* Performance: optimize for memory efficiency and minimal CPU instructions where it doesn't hurt readability too much
* Maintainability: split the complex application logic into modules, keep the code concise, avoid unnecessary abstraction layers

## File Structure

* Header file (.h)
	* Create header files only for public APIs. Internal-only code should use a single .c file with static declarations.
	* `#pragma once`
	* `static inline` helper functions
* Start header and source files like this (may omit `Module:`):
```c
/** Project: Module: Short description
Year, Author */
```

### Header Include Order

* Local/project headers first (`#include <hdr.h>`) followed by external/standard ones

## Indentation & Spacing

* Tab indentation
* UNIX line endings

## Naming

* `snake_case`
* General rule: larger scope -> better naming.
* Polished naming for public API; relaxed/short otherwise.
* Prefer minimal names for local variables, e.g. `i` for loop iterators, `n` for number of elements, `s` for strings, `r` for result codes, etc.

## Types

* `unsigned` or `uint` for non-negative values
* `void *ptr`
* type casting with pointers: `char *s = (char*)ptr`

## Sentinels

* `~0U` for invalid unsigned values
* `(void*)-1` for invalid pointers

## Functions

* `ctx_action()`, e.g. `file_hdr_read()`
* Use `int` type for status-returning functions: return `0` on success, non-zero on error

## Braces

* Opening brace on a new line for functions:
	```c
	int func()
	{
	```

* Opening brace on the same line for control flow:
	```c
	if (...) {
		...
	} else {
		...
	}
	```

## Operator-First Multi-line Expressions

```c
func(arg1, arg2, ...
	, arg10);
```

```c
if (func1()
	|| func2())
```

## Error Handling

* Use `if (!func())` and `if (func())` for zero/non-zero checks. Use explicit comparisons when checking for exact `0` or `1` values.
* Value-first checks for function calls: `if (E_BUSY == func())`
* Value-first checks for function calls with assignment to a variable: `if (E_BUSY == (e = func()))`
* Normal value-last checks otherwise: `if (e == E_BUSY)`
* `goto err` for error paths; `goto fin` for finalization.

## Documentation

* Inline comments `// Do this` for the code blocks with higher-than-average complexity
* Public functions

	```c
	/** Short description.
	Longer description for important but not obvious function behavior.
	a: Argument 1.
	b: Argument 2.
	Return 0 on success; non-zero on error. */
	int func(int a, int b);
	```

## Git Commit Messages

Format: `[symbol] module: description`

* `+ module: new feature` — e.g., `+ core: add stuff`
* `* module: change/update` — e.g., `* core: improve stuff`
* `- module: bug fix` — e.g., `- core: stuff didn't work`
* `module: minor changes` — something the users won't notice (refactoring), no symbol prefix

Rules:
* `module`: the module or subsystem being modified
* `description`: short, imperative, lowercase
* One line preferred; add body paragraph for complex changes

## Disclaimer

These rules are guidelines, not laws; use judgement.
