# Boost.Spirit.X4 — API Documentation

Boost.Spirit.X4 is a modern, header-only PEG (Parsing Expression Grammar) parser combinator library for C++23. Grammars are expressed directly as C++ expressions, and parsing is deterministic, top-down, and greedy.

---

## Table of Contents

1. [Including the Library](#1-including-the-library)
2. [Entry Points — `x4::parse`](#2-entry-points--x4parse)
3. [`parse_result`](#3-parse_result)
4. [Primitive (Auxiliary) Parsers](#4-primitive-auxiliary-parsers)
5. [Character Parsers](#5-character-parsers)
6. [String Parsers](#6-string-parsers)
7. [Numeric Parsers](#7-numeric-parsers)
8. [Boolean Parsers](#8-boolean-parsers)
9. [Symbol Table Parsers](#9-symbol-table-parsers)
10. [Operators / Combinators](#10-operators--combinators)
11. [Directives](#11-directives)
12. [Rules](#12-rules)
13. [Semantic Actions](#13-semantic-actions)
14. [Context System](#14-context-system)
15. [Error Handling](#15-error-handling)
16. [AST Position Tagging](#16-ast-position-tagging)
17. [Compile-time Configuration Macros](#17-compile-time-configuration-macros)

---

## 1. Including the Library

The convenience header pulls in all public components:

```cpp
#include <boost/spirit/x4.hpp>
```

Individual feature headers are also available and can be included on their own. All public symbols live in the namespace `boost::spirit::x4`, abbreviated as `x4` throughout this document.

---

## 2. Entry Points — `x4::parse`

**Header:** `<boost/spirit/x4/parse.hpp>`

`x4::parse` is a function-object (CPO) exposed in `boost::spirit::x4::cpos`. It is the single entry point for all parsing operations. Two families of overloads exist: range-based and iterator-pair-based, and within each family there is a plain-parse overload and a phrase-parse overload (with a skipper).

> **Note:** `phrase_parse` is a deprecated alias for `parse` — all overloads are unified under `x4::parse`.

### 2.1 Plain parse (no skipper)

```cpp
// Range-based
auto result = x4::parse(range, parser, attr);

// Iterator-pair-based
auto result = x4::parse(first, last, parser, attr);
```

- `range` — any `std::ranges::forward_range` (e.g. `std::string`, `std::string_view`, `std::vector<char>`). Raw string literals are treated as `std::basic_string_view`.
- `first`, `last` — a forward iterator and its sentinel.
- `parser` — any X4 parser expression.
- `attr` — a reference to the attribute variable that receives the parsed value. Pass `x4::unused` if you do not need the attribute.

Returns a `parse_result` (see §3).

An in-place overload accepts an existing result object as the first argument to avoid re-allocating the `expectation_failure` string:

```cpp
x4::parse(result, range, parser, attr);
```

### 2.2 Phrase parse (with skipper)

```cpp
// Range-based phrase parse
auto result = x4::parse(range, parser, skipper, attr);
auto result = x4::parse(range, parser, skipper, attr, flag);

// Iterator-pair phrase parse
auto result = x4::parse(first, last, parser, skipper, attr);
auto result = x4::parse(first, last, parser, skipper, attr, flag);
```

- `skipper` — a parser that matches tokens to skip (e.g. whitespace).
- `flag` — `x4::root_skipper_flag::do_post_skip` (default) or `x4::root_skipper_flag::dont_post_skip`.

`do_post_skip` causes one final skip after the main parser succeeds. `dont_post_skip` inhibits it.

### 2.3 `root_skipper_flag`

```cpp
enum class root_skipper_flag : char {
    do_post_skip,    // apply the skipper once after the main parser
    dont_post_skip   // inhibit the post-skip
};
```

### 2.4 Context helpers for `BOOST_SPIRIT_X4_INSTANTIATE`

```cpp
// Determine context type for parse() / phrase_parse()
template<class ItOrRange, class = void>
using parse_context_for = /* ... */;

template<class Skipper, class ItOrRange, class SeOrRange = ItOrRange>
using phrase_parse_context_for = /* ... */;
```

These type aliases are used when explicitly instantiating rules (see §12.4).

---

## 3. `parse_result`

**Header:** `<boost/spirit/x4/parse_result.hpp>`

```cpp
template<std::forward_iterator It, std::sentinel_for<It> Se = It>
struct parse_result {
    bool ok = false;
    expectation_failure<It> expect_failure;
    std::ranges::subrange<It, Se> remainder;

    // Convenient string view of remainder (available when the value_type is a character)
    std::basic_string_view<...> remainder_str() const;

    // true  ↔  ok && !remainder.empty()  (partial match)
    bool is_partial_match() const noexcept;

    // true  ↔  ok && remainder.empty()  (consumed all input and succeeded)
    bool completed() const noexcept;

    // Equivalent to completed()
    explicit operator bool() const noexcept;
};
```

- `ok` — `true` if all sub-parsers reported success.
- `expect_failure` — populated when an `expect` directive or `>` operator fails.
- `remainder` — the portion of the input that was **not** consumed.
- `completed()` / `operator bool` — `true` only when parsing succeeded *and* consumed the entire input.

A convenience alias for ranges:

```cpp
template<std::ranges::forward_range R>
using parse_result_for = parse_result<iterator_t<R const>, sentinel_t<R const>>;
```

---

## 4. Primitive (Auxiliary) Parsers

**Header:** `<boost/spirit/x4/auxiliary.hpp>` (or individual files below)

### `eps`

```cpp
inline constexpr eps_parser eps{};
```

Always succeeds without consuming input. With a boolean argument it acts as a semantic predicate:

```cpp
eps         // always succeeds
eps(true)   // succeeds
eps(false)  // fails
```

### `eoi`

```cpp
inline constexpr eoi_parser eoi{};
```

Succeeds only when the iterator has reached the end of input (after applying the active skipper).

### `eol`

```cpp
inline constexpr eol_parser eol{};
```

Matches a line ending: `\r`, `\n`, or `\r\n`. Succeeds if at least one newline character is consumed.

### `attr(value)`

```cpp
inline constexpr detail::attr_gen attr{};
// Usage:
attr(42)           // synthesises an int attribute with value 42
attr("hello")      // synthesises a std::string attribute
```

Does not consume input. Copies `value` into the attribute. Attribute type is deduced from the argument.

---

## 5. Character Parsers

**Headers:** `<boost/spirit/x4/char.hpp>` and files under `x4/char/`

### 5.1 `char_` — any character

```cpp
// In namespace x4::standard
inline constexpr any_char<char_encoding::standard> char_{};

// In namespace x4::standard_wide (if BOOST_SPIRIT_X4_NO_STANDARD_WIDE is not defined)
inline constexpr any_char<char_encoding::standard_wide> char_{};

// In namespace x4::unicode (if BOOST_SPIRIT_X4_UNICODE is defined)
// (analogous)
```

`char_` matches any single character and puts it into the attribute.

`char_` can also be called as a function to produce more specific parsers:

```cpp
char_('a')          // matches exactly 'a'
char_("abc")        // matches any character in the set {'a','b','c'}
char_('a','z')      // matches characters in the range ['a','z']  (two-arg form via char_set/char_range)
```

When called with a two-character literal array (`char const (&)[2]`) it behaves like `char_('x')`.

### 5.2 `lit(ch)` — literal character (no attribute)

```cpp
lit('a')   // matches 'a', produces no attribute
lit(L'a')  // wide character variant
```

`lit` is a function in `x4::standard` (and `x4::standard_wide`) namespaces. It also accepts single-element string literals.

### 5.3 Character classes

All character class parsers live in the `x4::standard` and `x4::standard_wide` namespaces (also accessible via `x4::parsers::standard` / `x4::parsers::standard_wide`).

| Parser       | Matches                        |
|--------------|-------------------------------|
| `alnum`      | alphanumeric                   |
| `alpha`      | alphabetic                     |
| `blank`      | space or tab                   |
| `cntrl`      | control characters             |
| `digit`      | decimal digit                  |
| `graph`      | printable non-space            |
| `lower`      | lowercase letter               |
| `print`      | printable character            |
| `punct`      | punctuation                    |
| `space`      | whitespace                     |
| `upper`      | uppercase letter               |
| `xdigit`     | hexadecimal digit              |

Example:

```cpp
using namespace x4::standard;
auto result = x4::parse("  hello", +alpha, attr);
```

### 5.4 Character ranges and sets

`char_range<Encoding>` and `char_set<Encoding>` are produced by calling `char_`:

```cpp
char_('a', 'z')      // char_range: ['a','z']
char_("aeiou")       // char_set: matches any vowel
```

### 5.5 Negated character parser (`~`)

Applying `operator~` to any character parser produces a `negated_char_parser` that matches any character *not* matched by the original:

```cpp
~char_('a')          // any character except 'a'
~alnum               // any non-alphanumeric character
~char_("aeiou")      // any non-vowel
```

---

## 6. String Parsers

**Header:** `<boost/spirit/x4/string.hpp>` (or files under `x4/string/`)

### `lit(str)` — literal string (no attribute)

```cpp
lit("hello")    // matches the literal string "hello", produces no attribute
lit(L"hello")   // wide variant
```

### `string(str)` — literal string with attribute

```cpp
string("hello")  // matches "hello", attribute is std::string
```

Both `lit` and `string` are functions in `x4::standard` / `x4::standard_wide` inline namespaces.

When using `operator>>` with string literals, they are implicitly converted to `lit`:

```cpp
"hello" >> "world"   // fine: literal strings become lit parsers automatically
```

---

## 7. Numeric Parsers

**Header:** `<boost/spirit/x4/numeric.hpp>` (or individual files under `x4/numeric/`)

### 7.1 Signed integers

| Parser     | Attribute type  | Notes              |
|------------|-----------------|--------------------|
| `short_`   | `short`         | decimal            |
| `int_`     | `int`           | decimal            |
| `long_`    | `long`          | decimal            |
| `long_long`| `long long`     | decimal            |
| `int8`     | `int8_t`        | decimal            |
| `int16`    | `int16_t`       | decimal            |
| `int32`    | `int32_t`       | decimal            |
| `int64`    | `int64_t`       | decimal            |

Custom signed integer parser:

```cpp
template<class T, unsigned Radix = 10, unsigned MinDigits = 1, int MaxDigits = -1>
struct int_parser;
```

Supported radixes: 2, 8, 10, 16. `MaxDigits = -1` means unlimited.

### 7.2 Unsigned integers

| Parser      | Attribute type       | Notes         |
|-------------|----------------------|---------------|
| `ushort_`   | `unsigned short`     | decimal       |
| `uint_`     | `unsigned int`       | decimal       |
| `ulong_`    | `unsigned long`      | decimal       |
| `ulong_long`| `unsigned long long` | decimal       |
| `uint8`     | `uint8_t`            | decimal       |
| `uint16`    | `uint16_t`           | decimal       |
| `uint32`    | `uint32_t`           | decimal       |
| `uint64`    | `uint64_t`           | decimal       |
| `bin`       | `unsigned`           | binary (base 2) |
| `oct`       | `unsigned`           | octal (base 8)  |
| `hex`       | `unsigned`           | hexadecimal     |

Custom unsigned integer parser:

```cpp
template<class T, unsigned Radix = 10, unsigned MinDigits = 1, int MaxDigits = -1>
struct uint_parser;
// Radix: 2–36
```

### 7.3 Real numbers

```cpp
template<class T, class Policy = real_policies<T>>
struct real_parser;

// Pre-defined instances (in x4::standard):
inline constexpr real_parser<float>       float_{};
inline constexpr real_parser<double>      double_{};
inline constexpr real_parser<long double> long_double{};
```

**Policy classes** control parsing behaviour. The built-in ones are:

| Policy                  | Description                            |
|-------------------------|----------------------------------------|
| `ureal_policies<T>`     | Unsigned real, no sign parsing         |
| `real_policies<T>`      | Signed real (default)                  |
| `strict_ureal_policies<T>` | Unsigned, requires decimal point    |
| `strict_real_policies<T>`  | Signed, requires decimal point      |

A custom policy must provide static methods:
`parse_sign`, `parse_n`, `parse_dot`, `parse_frac_n`, `parse_exp`, `parse_exp_n`, `parse_nan`, `parse_inf`, plus boolean constants `allow_leading_dot`, `allow_trailing_dot`, `expect_dot`.

The default policies recognise `nan`, `nan(...)`, `inf`, and `infinity` (case-insensitive).

---

## 8. Boolean Parsers

**Header:** `<boost/spirit/x4/numeric/bool.hpp>`

```cpp
// In x4::standard:
inline constexpr bool_parser<bool, char_encoding::standard> bool_{};
inline constexpr literal_bool_parser<bool, char_encoding::standard> true_{true};
inline constexpr literal_bool_parser<bool, char_encoding::standard> false_{false};
```

`bool_` parses `"true"` or `"false"` (case-sensitive by default) into a `bool` attribute.  
`true_` / `false_` parse the respective literal.

Custom policy via `bool_policies<T>`: provide static `parse_true` and `parse_false` functions.

---

## 9. Symbol Table Parsers

**Header:** `<boost/spirit/x4/symbols.hpp>`

Symbol table parsers match the longest key in a set of strings and optionally return an associated value.

### 9.1 `unique_symbols` (recommended)

```cpp
template<class T = unused_type>
using unique_symbols = unique_symbols_parser<char_encoding::standard, T>;
// Also available in x4::standard_wide and x4::unicode namespaces
```

Owns its lookup table uniquely (`std::unique_ptr`). Can be used with `constexpr`.

### 9.2 `shared_symbols` (shared ownership)

```cpp
template<class T = unused_type>
using shared_symbols = shared_symbols_parser<char_encoding::standard, T>;
```

Shares the lookup table via `std::shared_ptr`. Copies share the same underlying table.

> `symbols<T>` is a deprecated alias for `shared_symbols<T>`.

### 9.3 Construction and population

```cpp
x4::unique_symbols<int> keywords;
keywords.add("if", 1)("else", 2)("while", 3);

// Or initializer-list construction:
x4::unique_symbols<int> kw({ {"if",1}, {"else",2}, {"while",3} });

// Or from ranges:
x4::unique_symbols<int> kw(
    std::vector<std::string_view>{"if","else","while"},
    std::vector<int>{1, 2, 3}
);
```

### 9.4 Member functions

| Member      | Description                                  |
|-------------|----------------------------------------------|
| `add`       | Functor; call `add("key")` or `add("key", val)` |
| `remove`    | Functor; call `remove("key")` to delete entry |
| `name()`    | Returns the parser's debug name              |

---

## 10. Operators / Combinators

All operators are overloaded for parser expressions and produce new parser objects. They are found via ADL inside `namespace boost::spirit::x4`.

### 10.1 Sequence `>>`

```cpp
a >> b
```

Matches `a` then `b`. Both must succeed. Attributes are merged into a `std::tuple` (or flattened when they are compatible container/sequence types).

### 10.2 Expectation `>`

```cpp
a > b
```

Equivalent to `a >> expect[b]`. If `a` succeeds but `b` fails, an **expectation failure** is recorded and the parse does not backtrack. Used to produce good error messages.

### 10.3 Alternative `|`

```cpp
a | b
```

Tries `a`; if it fails (without an expectation failure), tries `b`. This is an **ordered choice** (PEG `/`).

### 10.4 Kleene star `*`

```cpp
*a
```

Matches zero or more repetitions of `a`. Always succeeds. Attribute is a container (e.g. `std::vector`).

### 10.5 Plus `+`

```cpp
+a
```

Matches one or more repetitions of `a`. Fails if `a` does not match at least once. Attribute is a container.

### 10.6 Optional `-`

```cpp
-a
```

Matches zero or one occurrence of `a`. Always succeeds. Attribute is `std::optional<Attr>` (or `Attr` for container attributes).

### 10.7 List `%`

```cpp
a % b
```

Matches `a` delimited by `b`: `a (b a)*`. Requires at least one `a`. Attribute is a container of `a`'s attributes.

### 10.8 Difference `-`

```cpp
a - b
```

Matches `a` but only if `b` does **not** match at the same position. Both are tried; `b`'s iterator is always rolled back.

### 10.9 And-predicate `&`

```cpp
&a
```

Succeeds if `a` would match without consuming input. The input position is not advanced.

### 10.10 Not-predicate `!`

```cpp
!a
```

Succeeds if `a` would **not** match, without consuming input.

### 10.11 Char negation `~`

```cpp
~cp
```

Negates a character parser (see §5.5). Only applicable to parsers that derive from `char_parser`.

---

## 11. Directives

Directives are parser combinators that modify how the subject parser behaves. They are called as `directive[subject_parser]`.

**Header:** `<boost/spirit/x4/directive.hpp>` (or individual files)

### `expect[p]`

```cpp
inline constexpr detail::expect_gen expect{};
```

Forces `p` to succeed; if it fails, records an **expectation failure** and prevents backtracking. Equivalent to the `>` operator's right-hand side.

### `lexeme[p]`

```cpp
inline constexpr detail::lexeme_gen lexeme{};
```

Disables the active skipper for the duration of `p`. Use to parse tokens that must not contain embedded whitespace.

### `skip(s)[p]`

```cpp
inline constexpr detail::skip_gen skip{};
inline constexpr detail::reskip_gen reskip{};
```

`skip(s)[p]` installs `s` as the skipper inside `p`.  
`reskip[p]` re-enables the enclosing skipper after it was disabled by `lexeme` or `no_skip`.

### `no_skip[p]`

```cpp
inline constexpr detail::no_skip_gen no_skip{};
```

Suppresses any active skipper for the duration of `p`, but unlike `lexeme`, does not inhibit the skipper from being re-enabled within `p` via `reskip`.

### `no_case[p]`

```cpp
inline constexpr detail::no_case_gen no_case{};
```

Makes character comparisons case-insensitive inside `p`. Works with character classes, character sets, ranges, and string literals.

### `omit[p]`

```cpp
inline constexpr detail::omit_gen omit{};
```

Runs `p` normally but discards its attribute (produces `unused`).

### `raw[p]`

```cpp
inline constexpr detail::raw_gen raw{};
```

Runs `p` and sets the attribute to a `std::ranges::subrange<It, It>` spanning the matched input (an iterator range over the raw input, not a copy of the matched text).

### `matches[p]`

```cpp
inline constexpr detail::matches_gen matches{};
```

Always succeeds. Attribute is `bool`: `true` if `p` matched, `false` otherwise.

### `repeat(n)[p]` / `repeat(min, max)[p]`

```cpp
inline constexpr detail::repeat_gen repeat{};
inline constexpr detail::repeat_inf_type inf{};
inline constexpr detail::repeat_inf_type repeat_inf{};
```

Repeats `p` a controlled number of times:

```cpp
repeat(3)[p]          // exactly 3 times
repeat(2, 5)[p]       // 2 to 5 times
repeat(2, inf)[p]     // 2 or more times (inf / repeat_inf as upper bound)
```

### `as<T>[p]`

```cpp
template<class T>
inline constexpr detail::as_fn<T> as{};
```

Forces the attribute type to `T`, regardless of what `p` would normally produce. Context accessor `_as_var(ctx)` inside a semantic action returns the `T&` attribute.

### `seek[p]`

```cpp
inline constexpr detail::seek_gen seek{};
```

Advances the input one character at a time until `p` matches (or until the end of input is reached without a match). Useful for error recovery.

### `with<ID>(val)[p]`

```cpp
template<class ID>
inline constexpr detail::with_fn<ID> with{};
```

Injects the value `val` into the parsing context under the tag `ID`. Inside `p` (and any rules it calls), `x4::get<ID>(ctx)` returns a reference to `val`.

Example:

```cpp
struct my_tag {};
int counter = 0;
x4::parse(input, x4::with<my_tag>(counter)[p], x4::unused);
```

### `with_local<ID, T>[p]`

```cpp
template<class ID, class T>
inline constexpr detail::with_local_gen<ID, T> with_local{};
```

Creates a default-constructed local variable of type `T` and binds it to the context under `ID` for the duration of `p`. Accessed inside `p` via `x4::get<ID>(ctx)` or the helper `_local_var(ctx)`.

```cpp
inline constexpr detail::local_var_fn _local_var{};
```

---

## 12. Rules

**Header:** `<boost/spirit/x4/rule.hpp>`

Rules are named, recursive grammar productions. They separate a rule's *declaration* (type and name) from its *definition* (the actual parser expression).

### 12.1 Declaring a rule

```cpp
template<class RuleID, class Attr = unused_type, bool ForceAttribute = false>
struct rule;
```

- `RuleID` — a unique tag type (a user-defined empty struct).
- `Attr` — the attribute type the rule exposes. Defaults to `unused_type` (no attribute).
- `ForceAttribute` — if `true`, the rule always synthesizes its attribute even if no output variable was supplied.

```cpp
struct my_int_class {};
x4::rule<my_int_class, int> my_int{"my_int"};
```

The `name` member (`std::string_view`) is used in debug output and error messages.

### 12.2 Defining a rule (inline, anonymous)

```cpp
auto def = (my_int = x4::int_);
```

`operator=` returns a `rule_definition` object that is a valid parser. Use `operator%=` to force attribute synthesis.

### 12.3 Defining a rule (separate translation unit)

Use the macros for rules whose definition must live in its own `.cpp` file.

**Declaration** (in a header):

```cpp
BOOST_SPIRIT_X4_DECLARE(my_int)
// or, to declare several at once:
BOOST_SPIRIT_X4_DECLARE(rule_a, rule_b, rule_c)
```

This generates:

```cpp
bool parse_rule(
    x4::detail::rule_id<MyIntClass>,
    It& first, Se const& last,
    Context const& ctx,
    Attr& attr
);
```

**Definition** (in a `.cpp` file):

```cpp
BOOST_SPIRIT_X4_DEFINE(my_int)
// or:
BOOST_SPIRIT_X4_DEFINE(rule_a, rule_b, rule_c)
```

This generates the corresponding `parse_rule` definition using the expression `my_int = <rhs>`.  
The rule variable itself must be defined alongside its RHS:

```cpp
// In the .cpp file:
auto const my_int_def = my_int = x4::int_;
BOOST_SPIRIT_X4_DEFINE(my_int)
```

### 12.4 Explicit template instantiation

When a rule is used across translation units, its `parse_rule` may be expensive to compile. Explicit instantiation reduces compile times:

```cpp
BOOST_SPIRIT_X4_INSTANTIATE(MyRule, It, Context)
BOOST_SPIRIT_X4_INSTANTIATE(MyRule, It, Se, Context)
```

Use `x4::parse_context_for<It>` or `x4::phrase_parse_context_for<Skipper, It>` (§2.4) to determine the correct `Context` type.

### 12.5 `on_success` and `on_error` hooks

A `RuleID` struct may define static member functions to hook into rule parsing:

```cpp
struct my_rule_id {
    // Called after the rule matches successfully.
    // `first` points to the start of the match, `last` to the end.
    template<class It, class Context, class Attr>
    static void on_success(It const& first, It const& last, Context const& ctx, Attr& attr);

    // Called when an expectation failure occurs inside the rule.
    template<class It, class Context>
    static void on_error(It const& first, It const& last, Context const& ctx,
                         x4::expectation_failure<It> const& failure);
};
```

`on_error` is only called when there is an `expectation_failure` context present.

---

## 13. Semantic Actions

**Header:** `<boost/spirit/x4/core/action.hpp>`

Attach a callable to a parser with `operator[]`:

```cpp
parser[callable]
```

The callable is invoked when `parser` matches. It may:

- Take no arguments: `[]{ /* ... */ }`
- Take the context by value or const reference: `[](auto&& ctx){ /* ... */ }`
- Return `void` (always succeeds) or `bool` (returning `false` causes the match to fail and restores the iterator).

### 13.1 Context accessors in semantic actions

Inside `[](auto&& ctx){ }`:

| Accessor       | Returns                                          |
|----------------|--------------------------------------------------|
| `_attr(ctx)`   | Reference to the attribute bound to the action   |
| `_rule_var(ctx)` | Reference to the current rule's attribute      |
| `_as_var(ctx)` | Reference to the `as<T>` wrapper attribute       |

> `_val` is a deprecated alias for `_rule_var`.  
> `_pass` and `_where` are obsolete and deleted; use `bool` return and `raw[...]` respectively.

Example:

```cpp
auto p = x4::int_[([](auto&& ctx){
    x4::_attr(ctx) *= 2;
})];
```

---

## 14. Context System

**Header:** `<boost/spirit/x4/core/context.hpp>`

The context is an immutable linked-list of `(ID, value&)` pairs threaded through every `.parse()` call.

### 14.1 `context<ID, T, Next>`

```cpp
template<class ID, class T, class Next = unused_type>
struct context;
```

Each node holds a reference to `T` and a (possibly value) tail `Next`.

### 14.2 `make_context<ID>(val)`

```cpp
template<class ID, class T>
context<ID, T> make_context(T& val) noexcept;

template<class ID, class T, class Next>
context<ID, T, canonical_context_t<Next>> make_context(T& val, Next&& next) noexcept;
```

Creates a context node binding `val` to `ID`. Thread an existing context as `next` to build a chain.

### 14.3 `x4::get<ID>(ctx)`

```cpp
template<class ID, class ContextT>
decltype(auto) get(ContextT const& ctx) noexcept;
```

Retrieves the value bound to `ID`. Returns `x4::unused` if `ID` is not present.

### 14.4 `has_context_v<Context, ID>`

```cpp
template<class Context, class ID>
constexpr bool has_context_v;
```

`true` if the context chain contains a node with the given `ID`.

### 14.5 Pre-defined context IDs

| ID Tag                              | Used for                                              |
|-------------------------------------|-------------------------------------------------------|
| `contexts::expectation_failure`     | Holds the `expectation_failure` object                |
| `contexts::skipper`                 | Holds the active skipper parser                       |
| `contexts::rule_var`                | Holds the current rule's attribute (`_rule_var`)      |
| `contexts::as_var`                  | Holds the `as<T>` attribute (`_as_var`)               |
| `contexts::attr`                    | Holds the semantic action attribute (`_attr`)         |
| `contexts::error_handler`           | Holds the `error_handler` object (unique)             |

---

## 15. Error Handling

**Header:** `<boost/spirit/x4/core/expectation.hpp>` and `<boost/spirit/x4/debug/error_reporting.hpp>`

### 15.1 `expectation_failure<It>`

```cpp
template<std::forward_iterator It>
struct expectation_failure {
    It const& where() const noexcept;     // iterator to the failure point
    std::string const& which() const noexcept; // description of what was expected
    bool has_value() const noexcept;
    explicit operator bool() const noexcept;
    void clear() noexcept;
};
```

Populated by `expect[p]` and `a > b` when the subject fails. Exposed through `parse_result::expect_failure`.

Free functions (require `contexts::expectation_failure` to be present in the context):

```cpp
bool has_expectation_failure(ctx);
decltype(auto) get_expectation_failure(ctx);
void clear_expectation_failure(ctx);
```

### 15.2 `error_handler<It>`

```cpp
template<std::forward_iterator It>
class error_handler {
public:
    error_handler(It first, It last,
                  std::ostream& err_out,
                  std::string file = "",
                  int tabs = 4);

    // Print error at a single position
    void operator()(It err_pos, std::string const& message) const;

    // Print error over a range
    void operator()(It err_first, It err_last, std::string const& message) const;

    // Print error for a position_tagged AST node
    void operator()(ast::position_tagged const& pos, std::string const& message) const;

    // Tag an attribute (for annotate_on_success)
    template<X4Attribute Attr>
    void tag(Attr& attr, It first, It last);

    // Retrieve iterator range for a position_tagged node
    std::ranges::subrange<It> position_of(ast::position_tagged const& pos) const;

    ast::position_cache<std::vector<It>> const& get_position_cache() const noexcept;
};
```

Inject into the parse via `x4::with<x4::contexts::error_handler>(handler)[grammar]`.

### 15.3 `annotate_on_success`

```cpp
struct annotate_on_success {
    template<class It, class Se, class Context, class Attr>
    void on_success(It const& first, Se const& last, Context const& ctx, Attr& attr);
};
```

Mix this into a `RuleID` struct (via inheritance) to automatically tag any `position_tagged` AST node with iterator positions when the rule matches. Requires `contexts::error_handler` to be in the context (provided by `x4::with`).

---

## 16. AST Position Tagging

**Header:** `<boost/spirit/x4/ast/position_tagged.hpp>`

### `ast::position_tagged`

```cpp
namespace ast {
struct position_tagged {
    int id_first = -1;
    int id_last  = -1;
};
}
```

Base class for AST node types that should carry source-position information. The `id_first` / `id_last` fields index into a `position_cache`.

### `ast::position_cache<Container>`

```cpp
namespace ast {
template<class Container>   // e.g. std::vector<It>
class position_cache {
public:
    position_cache(iterator_type first, iterator_type last);

    // Record the source range for an attribute; no-op if Attr does not inherit position_tagged
    template<X4Attribute Attr>
    void annotate(Attr& attr, iterator_type first, iterator_type last);

    // Retrieve the source range for a position_tagged node
    template<X4Attribute Attr>
    std::ranges::subrange<iterator_type> position_of(Attr const& attr) const;

    Container const& get_positions() const noexcept;
    iterator_type first() const;
    iterator_type last() const;
};
}
```

`error_handler` wraps a `position_cache` internally. Access it via `error_handler::get_position_cache()`.

---

## 17. Compile-time Configuration Macros

**Header:** `<boost/spirit/config.hpp>`

| Macro                              | Effect                                                                 |
|------------------------------------|------------------------------------------------------------------------|
| `BOOST_SPIRIT_X4_DEBUG`            | Enables debug tracing output (prints rule names and match status)       |
| `BOOST_SPIRIT_X4_UNICODE`          | Enables Unicode character encoding support (`x4::unicode` namespace)    |
| `BOOST_SPIRIT_X4_NO_STANDARD_WIDE` | Disables wide-character (`wchar_t`) encoding support                   |
| `BOOST_SPIRIT_X4_NO_RTTI`          | Disables RTTI-dependent features (affects rule `what()` information)   |

These must be defined **before** including any Spirit X4 header, typically in the build system or as compiler flags.

---

## Quick Reference: All Named Parsers

### Namespace `boost::spirit::x4` (top-level)

| Name          | Type family              |
|---------------|--------------------------|
| `eps`         | auxiliary                |
| `eoi`         | auxiliary                |
| `eol`         | auxiliary                |
| `attr`        | auxiliary (generator)    |
| `lit`         | string/char helper       |
| `string`      | string helper            |
| `int_`        | numeric (signed)         |
| `short_`      | numeric (signed)         |
| `long_`       | numeric (signed)         |
| `long_long`   | numeric (signed)         |
| `int8/16/32/64` | numeric (signed)       |
| `uint_`       | numeric (unsigned)       |
| `ushort_`     | numeric (unsigned)       |
| `ulong_`      | numeric (unsigned)       |
| `ulong_long`  | numeric (unsigned)       |
| `uint8/16/32/64` | numeric (unsigned)    |
| `bin`         | numeric (unsigned, base 2) |
| `oct`         | numeric (unsigned, base 8) |
| `hex`         | numeric (unsigned, base 16) |
| `float_`      | numeric (real)           |
| `double_`     | numeric (real)           |
| `long_double` | numeric (real)           |
| `bool_`       | boolean                  |
| `true_`       | boolean (literal)        |
| `false_`      | boolean (literal)        |
| `expect`      | directive                |
| `lexeme`      | directive                |
| `skip`        | directive                |
| `reskip`      | directive                |
| `no_skip`     | directive                |
| `no_case`     | directive                |
| `omit`        | directive                |
| `raw`         | directive                |
| `matches`     | directive                |
| `repeat`      | directive                |
| `seek`        | directive                |
| `as<T>`       | directive                |
| `with<ID>`    | directive                |
| `with_local<ID,T>` | directive           |
| `_local_var`  | context accessor         |

### Namespace `boost::spirit::x4::standard`

| Name     | Description                     |
|----------|---------------------------------|
| `char_`  | any `char`                      |
| `alnum`  | alphanumeric `char`             |
| `alpha`  | alphabetic `char`               |
| `blank`  | space/tab `char`                |
| `cntrl`  | control `char`                  |
| `digit`  | decimal digit `char`            |
| `graph`  | printable non-space `char`      |
| `lower`  | lowercase `char`                |
| `print`  | printable `char`                |
| `punct`  | punctuation `char`              |
| `space`  | whitespace `char`               |
| `upper`  | uppercase `char`                |
| `xdigit` | hexadecimal digit `char`        |

`x4::standard_wide` provides the same parsers for `wchar_t`. `x4::unicode` (requires `BOOST_SPIRIT_X4_UNICODE`) provides them for Unicode code points.

### Namespace `boost::spirit::x4::standard`

| Name             | Description                                    |
|------------------|------------------------------------------------|
| `unique_symbols<T>` | Symbol table with unique ownership          |
| `shared_symbols<T>` | Symbol table with shared ownership          |

---

## Minimal Example

```cpp
#include <boost/spirit/x4.hpp>
#include <string>
#include <iostream>

namespace x4 = boost::spirit::x4;
using namespace x4::standard;

int main()
{
    // Parse a comma-separated list of integers
    std::string input = "1, 2, 42, -7";
    std::vector<int> numbers;

    auto result = x4::parse(
        input,
        x4::int_ % ',',          // one or more ints separated by ','
        x4::standard::space,     // skip whitespace
        numbers
    );

    if (result.completed()) {
        for (int n : numbers) std::cout << n << '\n';
    } else {
        std::cerr << "Parse failed. Remaining: " << result.remainder_str() << '\n';
    }
}
```

---

## Recursive Grammar with Rules (Multi-TU)

**grammar.hpp**

```cpp
#pragma once
#include <boost/spirit/x4/rule.hpp>
#include <string>

struct expr_class {};
extern x4::rule<expr_class, int> const expr;

BOOST_SPIRIT_X4_DECLARE(expr)
```

**grammar.cpp**

```cpp
#include "grammar.hpp"
#include <boost/spirit/x4.hpp>

using namespace x4::standard;

x4::rule<expr_class, int> const expr{"expr"};

auto const expr_def =
    expr = x4::int_
         | ('(' >> expr >> ')');

BOOST_SPIRIT_X4_DEFINE(expr)
```

**main.cpp**

```cpp
#include "grammar.hpp"
#include <boost/spirit/x4/parse.hpp>

int main()
{
    int result = 0;
    auto r = x4::parse("(42)", expr, result);
    // r.completed() == true, result == 42
}
```

---

*Documentation generated from the header files in `include/boost/spirit/x4/`.*
