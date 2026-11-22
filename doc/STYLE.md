# phiola Code Style

Developed through years of experience, these rules aim to reduce cognitive load, minimize mistakes, and simplify maintenance.

## General Principles

* Performance: optimize for memory efficiency and minimal CPU instructions where it doesn't hurt readability too much
	* Load modules on-demand: the fastest code is the code that is never loaded, so don't load (.so) modules unless necessary
	* Keep each module's private data within a single memory buffer to avoid unnecessary heap allocations
	* Keep 'hot' data fields near each other and align to a 64-byte boundary so they fit within a single CPU cache line
	* Amortize the costs of a single operation by performing the same operation for multiple objects in a loop, where applicable
* Maintainability: split the complex application logic into modules, keep the code concise, avoid unnecessary abstraction layers

## File Structure

* Header files (.h)
	* Header files are only for public APIs (internal-only declarations would pollute the global namespace)
	* `#pragma once` prevents a header from being parsed twice
	* Define small helper functions with `static inline` — this keeps them local to each C file and prevents 'unused function' compiler errors when they are not used
* Header and source files start with a header block to give the reader a clue about what this code is for:

	```c
	/** Project: Module: Short description
	Year, Author */
	```

	or:

	```c
	/** Project: Short description
	Year, Author */
	```

### Header Include Order

* Local/project headers first, followed by external/standard ones (i.e., ordered by scope) — this improves readability and makes missing includes in project headers easier to find:

	```c
	#include <module/hdr.h>
	#include <ffsys/hdr.h>
	#include <ffbase/hdr.h>
	#include <unistd.h>
	```

## Indentation & Spacing

* Tab indentation — easy to re-indent manually, has configurable length, eliminates all possible issues when using different text editors
* UNIX line endings

## Naming

* 'snake_case', e.g. `gui_core_task_uint()` or `GUI_EV_PLAY_PAUSED` — provides a more natural reading experience (compared to 'camelCase') and eliminates lower/upper-case decision issues entirely
* General rule: larger scope -> better naming
* Polished naming for public API; relaxed/short otherwise
* Prefer minimal names for local variables, e.g. `i` for loop iterators, `n` for number of elements, `s` for strings, `r` for result codes, etc

## Types

* `int64/uint64`
* `uint` or `unsigned`
* `ushort`
* `u_char`

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

## Functions

* Names start with a context; the verb comes last (e.g., `ctx_action()`) — this significantly improves readability compared to verb-first naming

	```c
	int file_hdr_read(file *f) {...}
	int file_hdr_write(file *f) {...}
	int file_body_read(file *f) {...}
	```

* If a function returns a status, use `int` (not `bool`): `0` on success, non-zero on error.  The return value can later be extended at almost no additional cost: `<0` for error codes, `>0` for special cases.

* Use `goto fin` (or `goto end`) when returning early and when you need to free resources

* Example:

	```c
	int ctx_action(ctx *c)
	{
		int rc = -1;
		...
		if (error)
			goto fin;
		rc = 0;
	fin:
		ffmem_free(...);
		return rc;
	}
	```

## Operator-First Multi-line Expressions

```c
// very long statement
int a = ...
	+ ...;
```

```c
// too many arguments
func(arg1, arg2, ...
	, arg10);
```

```c
// function calls on separate lines
if (func1()
	|| func2())
```

## Error Handling

* Value-first checks for function calls (with assignment to a variable):

	```c
	if (FFFILE_NULL == (fd = fffile_open()))
		...
	```

* Normal value-last checks otherwise

	```c
	if (fd == FFFILE_NULL)
		...
	```

* `goto err` for error paths when you need to free resources

	```c
	fffd func()
	{
		...
		if (fd == FFFILE_NULL)
			goto err;
		return fd;

	err:
		ffmem_free(...);
		return FFFILE_NULL;
	}
	```

## Documentation

* Use inline comments `// Do this` for code blocks with higher-than-average complexity; don't overload the function's body with comments
* Public functions:

	```c
	/** Short description.
	Longer description for important but not obvious function behavior.
	a: Argument 1
	b: Argument 2
	Return 0 on success; non-zero on error. */
	int func(int a, int b);
	```

## Git Commit Messages

Format: `[symbol] module: description`

* `+ module: new feature` — e.g., `+ core: add stuff`
* `* module: change/update` — e.g., `* core: improve stuff`
* `- module: bug fix` — e.g., `- core: stuff didn't work`
* `! module: breaking change` — e.g., `! core: remove feature`
* `module: minor changes` — something the users won't notice (refactoring), no symbol prefix

Rules:
* `module`: the module or subsystem being modified
* `description`: short, imperative, lowercase
* One line preferred; add a body paragraph for complex changes

## Disclaimer

These rules are guidelines, not laws; use judgement.
