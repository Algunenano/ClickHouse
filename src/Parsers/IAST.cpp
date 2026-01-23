#include <Parsers/IAST.h>

#include <IO/Operators.h>
#include <IO/WriteBufferFromString.h>
#include <IO/WriteHelpers.h>
#include <Parsers/CommonParsers.h>
#include <Parsers/IdentifierQuotingStyle.h>
#include <Poco/String.h>
#include <Common/SensitiveDataMasker.h>
#include <Common/SipHash.h>
#include <Common/StringUtils.h>

#include <algorithm>

namespace DB
{

namespace ErrorCodes
{
    extern const int TOO_BIG_AST;
    extern const int TOO_DEEP_AST;
    extern const int UNKNOWN_ELEMENT_IN_AST;
    extern const int BAD_ARGUMENTS;
}


IAST::~IAST()
{
    /** Thread-local stack to collect children for iterative deletion.
      * Avoids stack overflow on deeply nested ASTs without per-node overhead.
      */
    static thread_local std::vector<ASTPtr> delete_stack;
    static thread_local bool processing = false;

    /// Move children with use_count==1 onto the stack for deferred deletion
    for (auto & child : children)
    {
        /** If two threads remove ASTPtr concurrently,
          * it is possible that neither thread will see use_count == 1.
          * It is ok. Will need one more extra stack frame in this case.
          */
        if (child.use_count() == 1)
            delete_stack.push_back(std::move(child));
    }

    /// If we're in a nested destructor call (destroying a node from the stack), just return.
    /// Our children are now on the stack and will be processed by the top-level loop.
    if (processing)
        return;

    /// Top-level destructor: process the stack iteratively
    processing = true;
    while (!delete_stack.empty())
    {
        /// Pop and destroy. The destructor of `to_delete` will push its children onto delete_stack and return early.
        ASTPtr to_delete = std::move(delete_stack.back());
        delete_stack.pop_back();
    }
    processing = false;
}

size_t IAST::size() const
{
    size_t res = 1;
    for (const auto & child : children)
        res += child->size();

    return res;
}

size_t IAST::checkSize(size_t max_size) const
{
    size_t res = 1;
    for (const auto & child : children)
        res += child->checkSize(max_size);

    if (res > max_size)
        throw Exception(ErrorCodes::TOO_BIG_AST, "AST is too big. Maximum: {}", max_size);

    return res;
}


IASTHash IAST::getTreeHash(bool ignore_aliases) const
{
    SipHash hash_state;
    updateTreeHash(hash_state, ignore_aliases);
    return getSipHash128AsPair(hash_state);
}


void IAST::updateTreeHash(SipHash & hash_state, bool ignore_aliases) const
{
    updateTreeHashImpl(hash_state, ignore_aliases);
    hash_state.update(children.size());
    for (const auto & child : children)
        child->updateTreeHash(hash_state, ignore_aliases);
}


void IAST::updateTreeHashImpl(SipHash & hash_state, bool /*ignore_aliases*/) const
{
    auto id = getID();
    hash_state.update(id.data(), id.size());
}


size_t IAST::checkDepthImpl(size_t max_depth) const
{
    std::vector<std::pair<ASTPtr, size_t>> stack;
    stack.reserve(children.size());

    for (const auto & i: children)
        stack.push_back({i, 1});

    size_t res = 0;

    while (!stack.empty())
    {
        auto top = stack.back();
        stack.pop_back();

        if (top.second >= max_depth)
            throw Exception(ErrorCodes::TOO_DEEP_AST, "AST is too deep. Maximum: {}", max_depth);

        res = std::max(res, top.second);

        for (const auto & i: top.first->children)
            stack.push_back({i, top.second + 1});
    }

    return res;
}

String IAST::formatWithPossiblyHidingSensitiveData(
    size_t max_length,
    bool one_line,
    bool show_secrets,
    bool print_pretty_type_names,
    IdentifierQuotingRule identifier_quoting_rule,
    IdentifierQuotingStyle identifier_quoting_style) const
{
    WriteBufferFromOwnString buf;
    FormatSettings settings(one_line);
    settings.show_secrets = show_secrets;
    settings.print_pretty_type_names = print_pretty_type_names;
    settings.identifier_quoting_rule = identifier_quoting_rule;
    settings.identifier_quoting_style = identifier_quoting_style;
    format(buf, settings);
    return wipeSensitiveDataAndCutToLength(buf.str(), max_length, !show_secrets);
}

String IAST::formatForLogging(size_t max_length) const
{
    return formatWithPossiblyHidingSensitiveData(
        /*max_length=*/max_length,
        /*one_line=*/true,
        /*show_secrets=*/false,
        /*print_pretty_type_names=*/false,
        /*identifier_quoting_rule=*/IdentifierQuotingRule::WhenNecessary,
        /*identifier_quoting_style=*/IdentifierQuotingStyle::Backticks);
}

String IAST::formatForErrorMessage() const
{
    return formatWithPossiblyHidingSensitiveData(
        /*max_length=*/0,
        /*one_line=*/true,
        /*show_secrets=*/false,
        /*print_pretty_type_names=*/false,
        /*identifier_quoting_rule=*/IdentifierQuotingRule::WhenNecessary,
        /*identifier_quoting_style=*/IdentifierQuotingStyle::Backticks);
}

String IAST::formatWithSecretsOneLine() const
{
    return formatWithPossiblyHidingSensitiveData(
        /*max_length=*/0,
        /*one_line=*/true,
        /*show_secrets=*/true,
        /*print_pretty_type_names=*/false,
        /*identifier_quoting_rule=*/IdentifierQuotingRule::WhenNecessary,
        /*identifier_quoting_style=*/IdentifierQuotingStyle::Backticks);
}

String IAST::formatWithSecretsMultiLine() const
{
    return formatWithPossiblyHidingSensitiveData(
        /*max_length=*/0,
        /*one_line=*/false,
        /*show_secrets=*/true,
        /*print_pretty_type_names=*/false,
        /*identifier_quoting_rule=*/IdentifierQuotingRule::WhenNecessary,
        /*identifier_quoting_style=*/IdentifierQuotingStyle::Backticks);
}

bool IAST::childrenHaveSecretParts() const
{
    for (const auto & child : children)
    {
        if (child->hasSecretParts())
            return true;
    }
    return false;
}

void IAST::cloneChildren()
{
    for (auto & child : children)
        child = child->clone();
}


String IAST::getColumnName() const
{
    WriteBufferFromOwnString write_buffer;
    appendColumnName(write_buffer);
    return write_buffer.str();
}


String IAST::getColumnNameWithoutAlias() const
{
    WriteBufferFromOwnString write_buffer;
    appendColumnNameWithoutAlias(write_buffer);
    return write_buffer.str();
}


void IAST::FormatSettings::writeIdentifier(WriteBuffer & ostr, const String & name, bool ambiguous) const
{
    checkIdentifier(name);
    bool must_quote
        = (identifier_quoting_rule == IdentifierQuotingRule::Always
           || (ambiguous && identifier_quoting_rule == IdentifierQuotingRule::WhenNecessary));

    if (identifier_quoting_rule == IdentifierQuotingRule::UserDisplay && !must_quote)
    {
        // Quote `name` if it is one of the keywords when `identifier_quoting_rule` is `IdentifierQuotingRule::UserDisplay`
        const auto & keyword_set = getKeyWordSet();
        must_quote = keyword_set.contains(Poco::toUpper(name));
    }

    switch (identifier_quoting_style)
    {
        case IdentifierQuotingStyle::Backticks:
        {
            if (must_quote)
                writeBackQuotedString(name, ostr);
            else
                writeProbablyBackQuotedString(name, ostr);
            break;
        }
        case IdentifierQuotingStyle::DoubleQuotes:
        {
            if (must_quote)
                writeDoubleQuotedString(name, ostr);
            else
                writeProbablyDoubleQuotedString(name, ostr);
            break;
        }
        case IdentifierQuotingStyle::BackticksMySQL:
        {
            if (must_quote)
                writeBackQuotedStringMySQL(name, ostr);
            else
                writeProbablyBackQuotedStringMySQL(name, ostr);
            break;
        }
    }
}

void IAST::FormatSettings::checkIdentifier(const String & name) const
{
    if (enforce_strict_identifier_format)
    {
        bool is_word_char_identifier = std::all_of(name.begin(), name.end(), isWordCharASCII);
        if (!is_word_char_identifier)
        {
            throw Exception(
                ErrorCodes::BAD_ARGUMENTS,
                "Identifier '{}' contains characters other than alphanumeric and cannot be when enforce_strict_identifier_format is enabled",
                name);
        }
    }
}

void IAST::dumpTree(WriteBuffer & ostr, size_t indent) const
{
    String indent_str(indent, '-');
    ostr << indent_str << getID() << ", ";
    writePointerHex(this, ostr);
    writeChar('\n', ostr);
    for (const auto & child : children)
    {
        if (!child)
            throw Exception(ErrorCodes::UNKNOWN_ELEMENT_IN_AST, "Can't dump a nullptr child");
        child->dumpTree(ostr, indent + 1);
    }
}

std::string IAST::dumpTree(size_t indent) const
{
    WriteBufferFromOwnString wb;
    dumpTree(wb, indent);
    return wb.str();
}

}
