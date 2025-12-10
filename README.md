# `con`sole `co`mmands
C++ **header-only** library for creating REPLs (`R`ead-`E`val-`P`rint `L`oops) and simple text-based command interpreters.

It helps you automatically convert a line of text like this:

`"some_command_name 123 'Hello!' 456"`

... into actual C/C++ function call like this:
```cpp
void some_command_name(int a, std::string_view b, int c)
{
	// ...
}

some_command_name(123, "Hello!", 456); // <-- Executed internally by `conco`
```

In another words - `conco` is primarily a command dispatcher, designed to take a raw command line string and map it directly to an executable C or C++ function (a "command"). It automatically handles type deduction, argument parsing, error checking and result serialization (to a string buffer).

## Features

* **Header-only**: The library is header-only, so you just need to include `conco.hpp`
* **Simple**:  No dependencies, no memory allocations, no global state.
* **Text-to-function mapping**: Map text commands to C/C++ functions with an associated name and description.
* **Automatic argument parsing**: Arguments are automatically tokenized and converted from strings to the types required by the function parameters.
* **Return value handling**: Captures the return value of the executed function and can stringify it into a user-provided buffer.
* **Custom types**: User can define specialized conversion functions to enable argument parsing for custom types.
* **Default arguments**: Supports commands with default arguments.
* **Overloading**: Supports function overloading by allowing multiple functions with the same name, but different set of parameters.

## Basic Example

```cpp
void log_enable() { ... }

int sum(int a, int b) { return a + b; }

int main()
{
	// List of available commands
	const conco::command commands[] = {
		{ log_enable, "log.enable;Enable logging" },
		{ sum, "sum;Sum of two integers" }
	};

	// Calls `log_enable`
	conco::execute(commands, "log.enable");

	char buffer[256] = { 0 };

	// Calls `sum(123, 456)`, writes stringified result to `buffer`
	conco::execute(commands, "sum 123 456", buffer);
	std::println("{}", buffer); // Outputs: 579
}
```

## Passing `void *` user data

```cpp
int multiply(int a, int b, void *user_data)
{
	int factor = *static_cast<int *>(user_data);
	return (a * b) * factor;
}

int main()
{
	const conco::command commands[] = {
		{ multiply, "multiply" }
	};

	int factor = 2;
	char buffer[256] = { 0 };

	// Calls `multiply(3, 4, &factor)` and writes stringified result to `buffer`
	conco::execute(commands, "multiply 3 4", buffer, &factor);
	std::println("{}", buffer); // Outputs: 24
}
```

## Member function commands

```cpp
struct calculator
{
	int add( int x, int y ) { return x + y; }
	int sub( int x, int y ) { return x - y; }
};

int main()
{
	calculator calc;

	const conco::command commands[] = {
		conco::method<&calculator::add>{ calc, "add" },
		conco::method<&calculator::sub>{ calc, "sub" },
	};

	char buffer[256] = { 0 };

	// Calls `calc.add(10, 5)`
	conco::execute(commands, "add 10 5", buffer);
	std::println("{}", buffer); // Outputs: 15
	
	// Calls `calc.sub(10, 5)`
	conco::execute(commands, "sub 10 5", buffer);
	std::println("{}", buffer); // Outputs: 5
}
```

## Default arguments

```cpp
int sum_four(int a, int b, int c, int d) { return a + b + c + d; }

int main()
{
	const conco::command commands[] = {
		{ sum_four, "sum_four x=1 y=2 z=3 w=4;Sum of four integers" }
	};

	char buffer[256] = { 0 };

	// Calls `sum_four(1, 2, 3, 4)`
	conco::execute(commands, "sum_four", buffer);
	std::println("{}", buffer); // Outputs: 10

	// Calls `sum_four(10, 20, 3, 4)`
	conco::execute(commands, "sum_four 10 20 3 4", buffer);
	std::println("{}", buffer); // Outputs: 37

	// Calls `sum_four(10, 20, 30, 40)`
	conco::execute(commands, "sum_four 10 20 30 40", buffer);
	std::println("{}", buffer); // Outputs: 100
}
```

## Implemnting `help` command

You can access the list of available commands from within a command implementation by accepting a `conco::context` parameter.

```cpp
void help(const conco::context &ctx)
{
	for (const auto &cmd : ctx.commands)
	{
		// ... Print command name, description and other info
	}
}

int main()
{
	conco::command commands[] = {
		{ help, "help;Displays this help message" },
		// Other commands...
	};

	conco::execute(commands, "help");
}
```

## Overloading

This is a bit wonky, but you can overload commands by listing multiple functions with the same name in the command list. The library will call the first one that succeeds in parsing the arguments. Because of this, it is better to list the more specific overloads first and the more general ones last; see the example below.

```cpp
void log_enable_a() { ... }
void log_enable_b(int level) { ... }
void log_enable_c(int level, const char *filename) { ... }

int main()
{
	// List of available commands, note the reversed order
	conco::command commands[] = {
		{ log_enable_c, "log.enable" },
		{ log_enable_b, "log.enable" },
		{ log_enable_a, "log.enable" },
	};

	conco::execute(commands, "log.enable");           // Calls log_enable_a()
	conco::execute(commands, "log.enable 2");         // Calls log_enable_b()
	conco::execute(commands, "log.enable 3 log.txt"); // Calls log_enable_c()
}
```
