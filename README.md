# `con`sole `co`mmands
C++ **header-only** library for creating simple text-based command interpreters or REPL/Quake-like consoles.

It helps you automatically convert a line of text like this:

`some_command_name 123 'Hello!' 456`

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
* **Simple**: No dependencies, no memory allocations, no global state.
* **Automatic argument parsing**: Arguments are automatically tokenized and converted from strings to the types required by the function parameters.
* **Return value handling**: Captures the return value of the executed function and can stringify it into a user-provided buffer.
* **Custom types**: User can define specialized conversion functions to enable argument parsing for own types.
* **Member functions**: Supports binding member functions to commands, allowing using methods of classes/struct as commands.
* **Default arguments & overloading**: Multiple commands with the same name, but different parameter sets can be defined, as well as default argument values.

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

## Member function commands

You can bind member functions of a class/struct as commands using `conco::method` helper template:

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

## Passing `void *` user data

Your command functions can accept an optional `(const) void *` parameter, which will be passed as a user-defined pointer when executing the command. This is mostly a fallback mechanism for easier integration with existing plain C APIs that use `void *` user data pointers. You should prefer member functions for more idiomatic C++ code.

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

## Default arguments

Commands can have list of argument names with optional default values specified as part of the command name. During execution, if the user omits some arguments, the default values will be used - just like you would expect from normal C++ function default arguments.

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

You can access the list of available commands from within a command implementation by accepting a `const conco::context &` parameter. From there, you can iterate over `ctx.commands` to get the list of all registered commands along with their metadata.

```cpp
void help(const conco::context &ctx)
{
	for (const auto &cmd : ctx.commands)
	{
		// Print command name, argument list and other info
		// Use `conco::command::desc` field to access reflection metadata
	}
}

int main()
{
	const conco::command commands[] = {
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

## Custom complex types

When adding custom types, you need to provide following functions:
* `type_name(conco::tag<T>)` - returns human readable name of the type
* `from_string(conco::tag<T>, std::string_view)` - converts a string token to the custom type
* `to_chars(conco::tag<T>, std::span<char>, T)` - converts the custom type to a string representation

Providing `type_name` function is mandatory, while `from_string` and `to_chars` are only needed if you want to use the type as a command argument or return value.

The extension mechanism relies on ADL (argument-dependent lookup) with `conco::tag<T>` wrapper type to allow defining the functions in the same namespace as the custom type.

See the example below:

```cpp
struct point
{
	int x;
	int y;
};

point add_points(const point &a, const point &b)
{
	return { a.x + b.x, a.y + b.y };
}

// Must be `constexpr`
constexpr std::string_view type_name( conco::tag<point> ) noexcept { return "point"; }

// Conversion from string to `point`. The string will be in format "x y", e.g. "10 20"
std::optional<point> from_string( conco::tag<point>, std::string_view str ) noexcept
{
	conco::tokenizer tokenizer{ str }; // Let's use `conco`'s built-in tokenizer

	auto tx = tokenizer.next(); // Get first token (x)
	auto ty = tokenizer.next(); // Get second token (y)
	if ( !tx || !ty )
		return std::nullopt; // Not enough tokens

	auto ox = conco::from_string( conco::tag<decltype( point::x )>{}, *tx ); // Convert x token to int
	auto oy = conco::from_string( conco::tag<decltype( point::y )>{}, *ty ); // Convert y token to int
	if ( !ox || !oy )
		return std::nullopt; // Conversion failed

	return point{ *ox, *oy }; // Return the parsed point
}

// Conversion from `point` to string. The output format will be "{x y}", e.g. "{10 20}"
bool to_chars( conco::tag<point>, std::span<char> buffer, point p ) noexcept
{
	auto r = std::format_to_n( buffer.data(), buffer.size(), "{{{} {}}}", p.x, p.y );
	*r.out = '\0';
	return true;
}

int main()
{
	const conco::command commands[] = {
		{ add_points, "add_points a b={0 0};Add two points" } // `b` has a default value of `{0 0}`
	};

	char buffer[256] = { 0 };

	// Calls `add_points({10, 20}, {30, 40})`
	conco::execute(commands, "add_points {10 20} {30 40}", buffer);
	std::println("{}", buffer); // Outputs: {40 60}

	// Calls `add_points({10, 20}, {0, 0})`, uses default value for `b`
	conco::execute(commands, "add_points {10 20}", buffer);
	std::println("{}", buffer); // Outputs: {10 20}
}
```

## Tokenization rules

The library splits input command lines into tokens using the following rules:
* Tokens are separated by whitespace (spaces, tabs) or commas
* Tokens can be enclosed in single (`'`) or double (`"`) quotes to include whitespace or special characters
* Tokens can be enclosed in curly braces (`{}`) for building complex types (e.g. `{10 20}` for a point) - nesting is supported
* Backslash (`\`) can be used in front of special characters to escape them (e.g. `\"` for a double quote character inside a double-quoted token)
* Equal sign (`=`) is a token
* Semicolon (`;`) is a terminating charater, tokenization stops when it is encountered

Since the whole library is zero-copy and does not allocate memory, tokens are represented as `std::string_view` slices of the original input string. This means that escaped characters are not really unescaped in the tokens and user code must handle that if needed.
