/*=============================================================================
    Minimal XML Parser — Boost.Spirit.X4 Example
    =============================================
    Demonstrates:
      - x4::parse entry point with phrase-parse skipper
      - Primitive parsers: char_, alpha, digit, lit
      - lexeme directive (disable skipper inside a token)
      - omit directive (discard a sub-parser's attribute)
      - raw directive (capture matched input as iterator range)
      - Kleene (*), plus (+), optional (-), sequence (>>), alternative (|)
      - Named rules with BOOST_SPIRIT_X4_DEFINE for mutual recursion
      - Semantic actions via operator[](_attr, _rule_var)
      - Skipper combining whitespace, comments, and PIs

    XML subset:
      - Nested elements with attributes (double- or single-quoted values)
      - Text content (character data), including mixed text+child content
      - Self-closing tags (<br/>, <img src="x"/>)
      - Whitespace, XML comments (<!-- ... -->), and processing instructions
        (<?...?>) are all silently consumed by the skipper

    Build from the repo root (GCC 14 or Clang 18+):
        g++-14 -std=c++23 -I include -I /usr/include \
               examples/xml_parser/xml_parser.cpp -o xml_parser && ./xml_parser
=============================================================================*/

#include <boost/spirit/x4.hpp>

// boost::variant with recursive wrapper
#include <boost/variant.hpp>

// Boost.Fusion accessors for semantic action attributes
#include <boost/fusion/include/at_c.hpp>

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace x4 = boost::spirit::x4;

// ── AST ───────────────────────────────────────────────────────────────────────

struct xml_attribute {
    std::string name;
    std::string value;
};

struct xml_node;

// Content inside an element is either a text string or a child element.
// boost::recursive_wrapper<xml_node> breaks the circular struct dependency.
using xml_content = boost::variant<
    std::string,
    boost::recursive_wrapper<xml_node>
>;

struct xml_node {
    std::string                tag;
    std::vector<xml_attribute> attributes;
    std::vector<xml_content>   children;
};

// ── Pretty-printer ────────────────────────────────────────────────────────────

static void print_node(xml_node const& node, int indent = 0);

struct content_visitor : boost::static_visitor<>
{
    int indent;
    explicit content_visitor(int i) : indent(i) {}

    void operator()(std::string const& text) const
    {
        // Omit pure-whitespace text nodes from debug output.
        for (char c : text)
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                std::cout << std::string(size_t(indent) * 2, ' ')
                          << "TEXT: \"" << text << "\"\n";
                return;
            }
    }

    void operator()(xml_node const& node) const { print_node(node, indent); }
};

static void print_node(xml_node const& node, int indent)
{
    std::cout << std::string(size_t(indent) * 2, ' ') << '<' << node.tag;
    for (auto const& a : node.attributes)
        std::cout << ' ' << a.name << "=\"" << a.value << '"';
    std::cout << ">\n";

    content_visitor v{indent + 1};
    for (auto const& c : node.children)
        boost::apply_visitor(v, c);

    std::cout << std::string(size_t(indent) * 2, ' ') << "</" << node.tag << ">\n";
}

// ── Grammar ───────────────────────────────────────────────────────────────────

namespace xml_grammar {

using namespace x4::standard; // char_, alpha, digit, space, …

// ── Forward declarations ──────────────────────────────────────────────────────

struct element_class {};
struct content_class {};
struct attribute_class {};

// Rules with their attribute types:
struct xml_name_class  {};
struct attr_val_class  {};
x4::rule<xml_name_class,  std::string>   const xml_name  {"xml_name"};
x4::rule<attr_val_class,  std::string>   const attr_value{"attr_value"};
x4::rule<element_class,   xml_node>      const element   {"element"};
x4::rule<content_class,   xml_content>   const content   {"content"};
x4::rule<attribute_class, xml_attribute> const attribute {"attribute"};

// ── Character helpers ─────────────────────────────────────────────────────────

auto const name_start_char = alpha | char_('_') | char_(':');
auto const name_char        = name_start_char | digit | char_('-') | char_('.');

// XML Name — typed rule with attribute std::string.
// The rule's string attribute causes Spirit to flatten (char + vector<char>) → string.
// lexeme[] prevents the skipper from running inside the token.
auto const xml_name_def = xml_name = x4::lexeme[ name_start_char >> *name_char ];

BOOST_SPIRIT_X4_DEFINE(xml_name)

// ── Attribute value ───────────────────────────────────────────────────────────

// Capture all chars between the quotes into a std::string.
auto const dq_value = x4::lexeme[ '"'  >> *( ~char_('"')  ) >> '"'  ];
auto const sq_value = x4::lexeme[ '\'' >> *( ~char_('\'') ) >> '\'' ];
// Each produces std::string (kleene of char → string).
//
// The alternative `dq_value | sq_value` would produce variant<string,string>
// because Spirit always generates a variant for alternatives unless the
// destination is a single typed rule.  We use a typed rule here to force
// the result to be std::string.

auto const attr_value_def = attr_value = dq_value | sq_value;

BOOST_SPIRIT_X4_DEFINE(attr_value)

// ── Attribute rule ────────────────────────────────────────────────────────────
// Parses  name="value"  or  name='value'  and builds an xml_attribute
// via a semantic action.
//
// Because xml_name and attr_value are both typed rules producing std::string,
// _attr(ctx) is a Boost.Fusion sequence (string, string) with no variants.

auto const attribute_def = attribute =
    ( xml_name >> '=' >> attr_value )
    [([] (auto&& ctx) {
        auto& attr = x4::_attr(ctx);
        x4::_rule_var(ctx) = xml_attribute{
            boost::fusion::at_c<0>(attr),
            boost::fusion::at_c<1>(attr)
        };
    })];

BOOST_SPIRIT_X4_DEFINE(attribute)

// ── Skipper ───────────────────────────────────────────────────────────────────

// XML comment: <!-- ... -->
auto const comment_skip = x4::omit[
    x4::lit("<!--")
    >> *( ~char_('-') | ('-' >> ~char_('-')) )
    >> x4::lit("-->")
];

// XML processing instruction (and declaration): <?...?>
auto const pi_skip = x4::omit[
    x4::lit("<?")
    >> *( ~char_('?') | ('?' >> ~char_('>')) )
    >> x4::lit("?>")
];

// Active skipper: whitespace OR comments OR PIs.
auto const skipper = space | comment_skip | pi_skip;

// ── Text character data ───────────────────────────────────────────────────────

// Capture a non-empty run of non-'<' characters as a std::string.
auto const char_data = x4::lexeme[ +( ~char_('<') ) ];
// lexeme[] keeps the skipper from eating spaces inside text nodes.

// ── Content: either text OR a child element ───────────────────────────────────

// The semantic action maps the parsed sub-type into xml_content
// (a boost::variant<string, recursive_wrapper<xml_node>>).

auto const content_def = content =
    char_data [([] (auto&& ctx) {
        x4::_rule_var(ctx) = xml_content{std::string(x4::_attr(ctx))};
    })]
    | element [([] (auto&& ctx) {
        x4::_rule_var(ctx) = xml_content{x4::_attr(ctx)};
    })];

BOOST_SPIRIT_X4_DEFINE(content)

// ── Element rule ──────────────────────────────────────────────────────────────
//
// Two forms, both resolved to xml_node via semantic actions:
//
//   Full:         <tag attr…>  content*  </tag>
//   Self-closing: <tag attr…/>
//
// The semantic action for each form builds an xml_node and writes it into
// _rule_var(ctx).  We use separate parsing expressions to keep each
// semantic action's _attr(ctx) type well-defined.

// Helper: parses a sequence of zero or more attributes.
auto const attrs_parser = *attribute;

// ── Full element ──
struct full_elem_class {};
x4::rule<full_elem_class, xml_node> const full_elem{"full_elem"};

// Parses:  <tag *attribute> *content </tag>
// _attr(ctx) = Boost.Fusion sequence (string, vector<xml_attribute>, vector<xml_content>)
auto const full_elem_def = full_elem =
    ( '<' >> xml_name >> attrs_parser >> '>'
      >> *content
      >> x4::lit("</") >> x4::omit[xml_name] >> '>' )
    [([] (auto&& ctx) {
        auto& attr = x4::_attr(ctx);
        x4::_rule_var(ctx) = xml_node{
            std::string(boost::fusion::at_c<0>(attr)),
            std::move(boost::fusion::at_c<1>(attr)),
            std::move(boost::fusion::at_c<2>(attr))
        };
    })];

BOOST_SPIRIT_X4_DEFINE(full_elem)

// ── Self-closing element ──
struct self_closing_class {};
x4::rule<self_closing_class, xml_node> const self_closing{"self_closing"};

// Parses:  <tag *attribute/>
// _attr(ctx) = Boost.Fusion sequence (string, vector<xml_attribute>)
auto const self_closing_def = self_closing =
    ( '<' >> xml_name >> attrs_parser >> x4::lit("/>") )
    [([] (auto&& ctx) {
        auto& attr = x4::_attr(ctx);
        x4::_rule_var(ctx) = xml_node{
            std::string(boost::fusion::at_c<0>(attr)),
            std::move(boost::fusion::at_c<1>(attr)),
            {}   // no children
        };
    })];

BOOST_SPIRIT_X4_DEFINE(self_closing)

// ── Combined element rule ─────────────────────────────────────────────────────
//
// Both full_elem and self_closing produce xml_node, so their alternative
// cleanly produces xml_node.  No semantic action needed here.

auto const element_def = element = full_elem | self_closing;

BOOST_SPIRIT_X4_DEFINE(element)

// ── Document ──────────────────────────────────────────────────────────────────
// An XML document is (optionally: XML declaration / PIs, consumed by skipper)
// followed by the root element.

auto const document = element;

} // namespace xml_grammar

// ── Test harness ──────────────────────────────────────────────────────────────

static int g_failures = 0;

static void run_test(std::string_view test_name,
                     std::string_view xml_input,
                     bool             expect_success)
{
    xml_node root{};
    auto result = x4::parse(xml_input,
                             xml_grammar::document,
                             xml_grammar::skipper,
                             root);

    bool ok   = result.completed();
    bool pass = (ok == expect_success);

    std::cout << (pass ? "PASS" : "FAIL") << "  [" << test_name << "]\n";

    if (ok) {
        print_node(root, 1);
    } else if (expect_success) {
        if (result.expect_failure)
            std::cout << "      expectation failure: "
                      << result.expect_failure.which() << '\n';
        else
            std::cout << "      remainder: \"" << result.remainder_str() << "\"\n";
    }
    std::cout << '\n';

    if (!pass) ++g_failures;
}

int main()
{
    // ── Test 1: simple element with text ─────────────────────────────────────
    run_test(
        "simple text element",
        "<greeting>Hello, World!</greeting>",
        true
    );

    // ── Test 2: self-closing element with attributes ──────────────────────────
    run_test(
        "self-closing with attributes",
        R"(<person name="Alice" age="30"/>)",
        true
    );

    // ── Test 3: nested elements ───────────────────────────────────────────────
    run_test(
        "nested elements",
        R"(
        <library>
          <book id="1">
            <title>The C++ Programming Language</title>
            <author>Bjarne Stroustrup</author>
          </book>
          <book id="2">
            <title>Effective C++</title>
            <author>Scott Meyers</author>
          </book>
        </library>
        )",
        true
    );

    // ── Test 4: XML declaration + comment (consumed by skipper) ───────────────
    run_test(
        "XML declaration and comment",
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <!-- A simple note -->
        <note>
          <to>John</to>
          <from>Jane</from>
          <body>Meeting at 3pm</body>
        </note>)",
        true
    );

    // ── Test 5: mixed content (text and child elements) ───────────────────────
    run_test(
        "mixed content",
        "<p>Hello <em>world</em>!</p>",
        true
    );

    // ── Test 6: attribute with single quotes ──────────────────────────────────
    run_test(
        "single-quote attributes",
        "<img src='photo.jpg' alt='A photo'/>",
        true
    );

    // ── Test 7: deeply nested elements ───────────────────────────────────────
    run_test(
        "deeply nested",
        "<root><a><b><c>deep</c></b></a></root>",
        true
    );

    // ── Test 8: multiple attributes ───────────────────────────────────────────
    run_test(
        "multiple attributes",
        R"(<rect x="10" y="20" width="100" height="50" fill="blue"/>)",
        true
    );

    // ── Test 9: should FAIL — incomplete input ────────────────────────────────
    run_test(
        "incomplete element (should fail)",
        "<unclosed>",
        false
    );

    // ── Test 10: empty self-closing element ───────────────────────────────────
    run_test(
        "empty self-closing",
        "<br/>",
        true
    );

    // ── Summary ───────────────────────────────────────────────────────────────
    std::cout << "----------------------------------------\n";
    if (g_failures == 0)
        std::cout << "All tests PASSED.\n";
    else
        std::cout << g_failures << " test(s) FAILED.\n";

    return g_failures == 0 ? 0 : 1;
}
