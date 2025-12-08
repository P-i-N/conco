#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>

#include "conco_tokenizer.hpp"

namespace conco {

enum class result
{
	success,                // Command executed successfully
	command_not_found,      // No command with the given name was found in provided list
	argument_parsing_error, // One or more arguments could not be parsed
	not_enough_arguments,   // Not enough arguments were provided for the command
	no_matching_overload,   // Multiple overloads found, but none could be executed due to argument parsing errors
};

/**
 * Executes a command from the given command list based on the provided command line string.
 *
 * The function intentionally accepts non-const `cmd_line` pointer because the tokenizer will
 * modify it and put in-place null-terminators between tokens. Expect the command line string
 * to be modified after this call!
 */
result execute( std::span<const struct command> commands,
                char *cmd_line,
                struct output &out,
                void *user_data = nullptr );

/**
 * Same as above, but creates an `output` structure internally.
 */
result execute( std::span<const struct command> commands,
                char *cmd_line,
                std::span<char> output_buffer = {},
                void *user_data = nullptr );

/**
 * Represents a single executable command.
 *
 * Commands can be created for free functions, lambdas or member functions (methods).
 * Everything is type-erased, runtime information about arguments, return type and
 * invocation are stored separately under `desc` member.
 *
 * The whole structure fits nicely into 24 bytes on 64-bit systems or 12 bytes on 32-bit systems.
 */
struct command final
{
	// Type-erased pointer to a function or a object instance for method commands
	void *target = nullptr;

	// Runtime info about arguments and invocation function
	const struct descriptor &desc;

	// Command name with optional arg. names ("sum x y"), followed by optional description block
	// separated by semicolon ("sum x y;Sum two integers")
	const char *name_and_desc = nullptr;

	// Constructors for function commands. For method commands, use `method<>()` helper function.
	template <typename F>
	  requires std::is_function_v<std::remove_pointer_t<F>>
	command( F func, const char *n );

	// Temporaries are not allowed
	template <typename C> command( const C &&, const descriptor &, const char * ) = delete;

	// Compares only the command name part, ignoring optional argument names and description
	bool operator==( token name ) const noexcept
	{
		size_t i = 0;
		while ( i < name.size() && name[i] && name[i] == name_and_desc[i] )
			++i;

		return i == ( name.size() - 1 ) && tokenizer::is_ident_term( name_and_desc[i] );
	}

	// Extracts command name (without argument names and description)
	std::string_view name() const noexcept
	{
		size_t i = 0;
		while ( name_and_desc[i] && !tokenizer::is_ident_term( name_and_desc[i] ) )
			++i;

		return std::string_view( name_and_desc, i );
	}

	// Extracts command description (text after semicolon)
	std::string_view description() const noexcept
	{
		const char *sep = std::strchr( name_and_desc, ';' );
		if ( !sep )
			return {};

		return std::string_view( sep + 1 );
	}

private:
	template <typename C>
	command( const C &ctx, const descriptor &d, const char *n )
	  : target( const_cast<C *>( &ctx ) ), desc( d ), name_and_desc( n )
	{}

	template <auto M, typename C> friend command method( C &ctx, const char *n );
	template <auto M, typename C> friend command method( const C &ctx, const char *n );
};

/**
 * Holds the result of a command execution with detailed error information.
 *
 * You only provide the buffer for storing stringified command result, the rest is filled
 * by the execution function and invokers.
 */
struct output
{
	std::span<char> buffer;                // Buffer for stringified command result
	const command *cmd = nullptr;          // Executed command, `nullptr` = not found
	uint32_t arg_error_mask : 30 = 0;      // Bitmask of argument parsing errors
	bool not_enough_arguments : 1 = false; // Whether there were not enough arguments
	bool result_error : 1 = false;         // Result stringification failed (does not mean execution failed!)

	void reset() noexcept
	{
		cmd = nullptr;
		arg_error_mask = 0;
		not_enough_arguments = false;
		result_error = false;
	}

	bool has_error() const noexcept { return arg_error_mask || not_enough_arguments || result_error; }
};

/**
 * Encapsulates all the context needed for command execution.
 *
 * This structure is created by the `execute()` function and passed down to invokers and
 * argument parsers. You can also access it from your own command functions by using
 * `const context &` as one of your argument types.
 */
struct context final
{
	std::span<const command> commands; // All available commands (as provided to `execute()`)
	std::span<char> raw_command_line;  // Command line text buffer from the user
	tokenizer args;                    // Tokenizer for command arguments
	token command_name;                // Command name (first token)
	output &out;                       // Result of the command execution
	void *user_data;                   // User data, will be passed to `void*` args in command functions

	context( std::span<const command> cmds, char *cmd_line, output &res, void *ud = nullptr ) noexcept
	  : commands( cmds ),
	    raw_command_line( cmd_line, cmd_line ? ( std::strlen( cmd_line ) + 1 ) : 0 ),
	    out( res ),
	    user_data( ud )
	{
		reset();
	}

	void reset()
	{
		args = tokenizer{ raw_command_line };
		command_name = args.next().value_or( {} );
	}
};

/**
 * Holds runtime information about the command function or method.
 *
 * All the fields are filled from function/method signature traits at compile-time.
 * This is a "poor man's" reflection for command functions. You can use this to inspect
 * commands at runtime, generate help texts about arguments, etc.
 */
struct descriptor final
{
	using invoker_func_t = bool ( * )( struct context & );
	invoker_func_t invoker = nullptr;

	// Names of argument types for this command. Hidden args have empty names.
	std::span<const std::string_view> arg_type_names;

	std::string_view result_type_name;

	uint8_t arg_count = 0;         // Number of real (program) arguments
	uint8_t command_arg_count = 0; // Number of textual arguments, minimum required
	bool has_tail_args = false;    // Whether the last argument is a tokenizer for variadic tail arguments
	bool has_result = false;       // Command has a non-void return type

	template <typename I, typename Traits = typename I::traits> static const descriptor &get()
	{
		static const descriptor desc = { .invoker = &I::call,
			                               // Explicitly construct span with `arg_count` to trim the dummy element
			                               .arg_type_names = { Traits::arg_type_names, Traits::arg_count },
			                               .result_type_name = Traits::result_type_name,
			                               .arg_count = Traits::arg_count,
			                               .command_arg_count = Traits::command_arg_count,
			                               .has_tail_args = Traits::has_tail_args,
			                               .has_result = Traits::has_result };

		return desc;
	}
};

/**
 * Template for type parsers for converting C++ types to/from strings.
 *
 * Specialized variants must provide:
 *   static constexpr std::string_view name;
 *   static std::optional<T> from_chars( std::span<char> ) { ... }
 *   static bool to_chars( std::span<char>, T ) { ... }
 *
 * The `name` should contain human-readable type name, e.g. "int", "float", "string", etc.
 *
 * The `from_chars()` function should attempt to parse the given string span into the desired type.
 * It should return `std::nullopt` on failure. The input string span is ALWAYS null-terminated.
 *
 * The `to_chars()` function should attempt to convert the given value into string form.
 * It should return `true` on success, `false` on failure (e.g. buffer too small).
 *
 * The default template is a fallback for types without a specialized parser.
 */
template <typename T> struct type_parser
{
	static constexpr bool not_specialized = true; // Marker to indicate no specialization exists
	static constexpr std::string_view name = {};  // Empty name for unknown types
};

} // namespace conco

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* Here be dragons... */

namespace conco::detail {

template <typename T> struct type_parser_helper
{
	using arg_tuple_type = std::remove_cvref_t<T>; // How the parsed argument will appear in the args tuple
	static constexpr size_t command_arg_count = 1; // Number of command arguments this type consumes
};

template <typename T>
  requires requires { type_parser<T>::not_specialized; }
struct type_parser_helper<T>
{
	using arg_tuple_type = T;                      // As-is, does not appear in args tuple anyway
	static constexpr size_t command_arg_count = 0; // Not specialized, does not consume
};

template <typename F> struct command_traits;

// Reflects function/method signatures into information about arguments and return type
template <typename RT, typename... Args> struct command_traits<RT( Args... )>
{
	static_assert( sizeof...( Args ) <= 30, "Too many command arguments!" );
	static_assert( ( !std::is_rvalue_reference_v<Args> && ... ), "Command arguments cannot be r-value references (&&)!" );
	static_assert( ( std::is_default_constructible_v<std::remove_cvref_t<Args>> && ... ),
	               "Command arguments must be default-constructible!" );

	using return_type = RT;

	static constexpr size_t arg_count = sizeof...( Args );
	static constexpr size_t command_arg_count = ( ( type_parser_helper<Args>::command_arg_count ) + ... + 0 );
	static constexpr bool has_tail_args = ( std::is_same_v<std::remove_cvref_t<Args>, tokenizer> || ... || false );
	static constexpr bool has_result = !std::is_void_v<RT>;

	static constexpr std::string_view arg_type_names[sizeof...( Args ) + 1] = {
		( type_parser<std::remove_cvref_t<Args>>::name )..., {} // Dummy +1 so the array is not empty
	};

	static constexpr std::string_view result_type_name = type_parser<RT>::name;

	static_assert( command_arg_count <= arg_count,
	               "Number of command arguments cannot be greater than total number of arguments!" );
};

/**
 * For generic types, extracts next argument from the command line and attempts to parse it
 * into the desired type. Some special "built-in" types are returned directly without parsing.
 */
template <typename T> static T parse_arg( context &ctx )
{
	// C-style `void*` user data pointer passed as-is
	if constexpr ( std::is_same_v<T, void *> || std::is_same_v<T, const void *> )
		return ctx.user_data;
	// `tokenizer` only allowed as whatever, it will only see remaining tail arguments
	else if constexpr ( std::is_same_v<std::remove_cvref_t<T>, tokenizer> )
		return ctx.args;
	// `output` only allowed as ref, because user *IS* expected to modify it
	else if constexpr ( std::is_same_v<T, output &> )
		return ctx.out;
	// `context` only allowed as const ref, so the user can't modify it from inside command functions
	else if constexpr ( std::is_same_v<T, const context &> )
		return ctx;
	// Everything else is parsed and converted from string into the desired type
	else
	{
		if ( auto token_opt = ctx.args.next(); token_opt )
		{
			if ( auto parsed_opt = type_parser<T>::from_chars( *token_opt ); parsed_opt )
				return *parsed_opt;

			size_t current_cmd_arg_index = ctx.args.count - 2; // +1 for cmd name token, +1 for last `next`
			ctx.out.arg_error_mask |= static_cast<uint32_t>( 1u << current_cmd_arg_index );
		}
		else
			ctx.out.not_enough_arguments = true;

		return T{};
	}
}

/**
 * Creates a tuple of parsed arguments for the given command function/method.
 *
 * For example, for a function `void foo(int, float, std::string)`, this will return
 * a tuple: `std::tuple<int, float, std::string>` with all arguments parsed from the
 * command context.
 */
template <typename... Args> auto make_args_tuple( context &ctx )
{
	using result_t = std::tuple<typename type_parser_helper<Args>::arg_tuple_type...>;

	// Using brace initialization to guarantee left-to-right evaluation order or `parse_arg<>()` calls
	return result_t{ ( parse_arg<Args>( ctx ) )... };
}

// Applies given tuple of arguments to the callable (function/method) and handles result stringification
template <typename RT> static bool apply( context &ctx, auto &&callable, auto &&args_tuple )
{
	if constexpr ( std::is_void_v<RT> )
	{
		ctx.out.result_error = false;
		std::apply( callable, args_tuple );
		if ( !ctx.out.buffer.empty() )
			ctx.out.buffer[0] = '\0';
	}
	else
	{
		auto r = std::apply( callable, args_tuple );
		if ( !ctx.out.buffer.empty() )
			ctx.out.result_error = !type_parser<RT>::to_chars( ctx.out.buffer, r );
	}

	return true;
}

template <typename F> struct function_invoker;

template <typename RT, typename... Args> struct function_invoker<RT ( * )( Args... )> : command_traits<RT( Args... )>
{
	using traits = command_traits<RT( Args... )>;

	static bool call( context &ctx )
	{
		auto args_tuple = make_args_tuple<Args...>( ctx );
		if ( ctx.out.has_error() )
			return false;

		auto callable = static_cast<RT ( * )( Args... )>( ctx.out.cmd->target );
		return apply<RT>( ctx, callable, args_tuple );
	}
};

template <typename C, auto M, typename F> struct method_invoker;

template <typename C, auto M, typename RT, typename... Args>
struct method_invoker<C, M, RT ( C::* )( Args... )> : command_traits<RT( Args... )>
{
	using traits = command_traits<RT( Args... )>;

	static bool call( context &ctx )
	{
		auto args_tuple = make_args_tuple<Args...>( ctx );
		if ( ctx.out.has_error() )
			return false;

		auto *obj = static_cast<C *>( ctx.out.cmd->target );
		auto callable = [obj]( auto &&...args ) -> RT { return ( obj->*M )( std::forward<decltype( args )>( args )... ); };
		return apply<RT>( ctx, callable, args_tuple );
	}
};

template <typename C, auto M, typename RT, typename... Args>
struct method_invoker<const C, M, RT ( C::* )( Args... ) const> : method_invoker<C, M, RT ( C::* )( Args... )>
{};

template <typename C, auto M, typename RT, typename... Args>
struct method_invoker<C, M, RT ( C::* )( Args... ) const> : method_invoker<C, M, RT ( C::* )( Args... )>
{};

template <typename C, auto M, typename RT, typename... Args>
struct method_invoker<const C, M, RT ( C::* )( Args... )> : method_invoker<C, M, RT ( C::* )( Args... )>
{
	// You are probably trying to create `command` for a const method, but used non-const object instance!
	static_assert( false, "Non-const method invoker instantiated for const method pointer!" );
};

} // namespace conco::detail

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace conco {

template <typename F>
  requires std::is_function_v<std::remove_pointer_t<F>>
inline command::command( F func, const char *n )
  : target( func ), desc( descriptor::get<detail::function_invoker<F>>() ), name_and_desc( n )
{}

template <auto M, typename C> command method( C &ctx, const char *n )
{
	return { ctx, descriptor::get<detail::method_invoker<C, M, decltype( M )>>(), n };
}

template <auto M, typename C> command method( const C &ctx, const char *n )
{
	return { ctx, descriptor::get<detail::method_invoker<const C, M, decltype( M )>>(), n };
}

inline result execute( std::span<const command> commands, char *cmd_line, output &out, void *user_data )
{
	context ctx = { commands, cmd_line, out, user_data };
	size_t overload_count = 0;

	auto cmd_iter = std::ranges::find_if( commands, [&]( const command &cmd ) { return cmd == ctx.command_name; } );
	while ( cmd_iter != commands.end() && *cmd_iter == ctx.command_name )
	{
		++overload_count;

		out.reset();
		out.cmd = &*cmd_iter;

		if ( out.cmd->desc.invoker( ctx ) )
			return result::success;

		ctx.reset();
		++cmd_iter;
	}

	if ( overload_count == 1 )
		return out.not_enough_arguments ? result::not_enough_arguments : result::argument_parsing_error;

	return overload_count > 1 ? result::no_matching_overload : result::command_not_found;
}

inline result execute( std::span<const command> commands,
                       char *cmd_line,
                       std::span<char> output_buffer,
                       void *user_data )
{
	output out = { output_buffer };
	return execute( commands, cmd_line, out, user_data );
}

} // namespace conco

#include "conco_type_parsers.hpp"
