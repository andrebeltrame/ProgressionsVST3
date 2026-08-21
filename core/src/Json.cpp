#include "harmonia/Json.h"

#include <cmath>
#include <cstdlib>
#include <sstream>

namespace harmonia::json
{

namespace
{

const Value nullValue;

void writeEscaped(std::string& out, const std::string& text)
{
    out += '"';
    for (char c : text)
    {
        switch (c)
        {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20)
                {
                    char buffer[8];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                    out += buffer;
                }
                else
                {
                    out += c;
                }
        }
    }
    out += '"';
}

std::string formatNumber(double value)
{
    if (std::isfinite(value) && value == std::floor(value) && std::abs(value) < 1e15)
        return std::to_string(static_cast<long long>(value));

    std::ostringstream stream;
    stream.precision(6);
    stream << value;
    return stream.str();
}

struct Parser
{
    const std::string& text;
    size_t pos = 0;
    std::string error;

    explicit Parser(const std::string& source) : text(source) {}

    void skipSpace()
    {
        while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\n' || text[pos] == '\r'))
            ++pos;
    }

    bool fail(const std::string& message)
    {
        if (error.empty())
            error = message + " at offset " + std::to_string(pos);
        return false;
    }

    bool parseValue(Value& out)
    {
        skipSpace();
        if (pos >= text.size())
            return fail("unexpected end of input");

        switch (text[pos])
        {
            case '{': return parseObject(out);
            case '[': return parseArray(out);
            case '"':
            {
                std::string parsed;
                if (! parseString(parsed))
                    return false;
                out = Value(parsed);
                return true;
            }
            case 't':
                if (text.compare(pos, 4, "true") != 0)
                    return fail("expected true");
                pos += 4;
                out = Value(true);
                return true;
            case 'f':
                if (text.compare(pos, 5, "false") != 0)
                    return fail("expected false");
                pos += 5;
                out = Value(false);
                return true;
            case 'n':
                if (text.compare(pos, 4, "null") != 0)
                    return fail("expected null");
                pos += 4;
                out = Value();
                return true;
            default:
                return parseNumber(out);
        }
    }

    bool parseNumber(Value& out)
    {
        const size_t start = pos;
        if (pos < text.size() && (text[pos] == '-' || text[pos] == '+'))
            ++pos;
        while (pos < text.size() && (std::isdigit(static_cast<unsigned char>(text[pos])) || text[pos] == '.'
                                     || text[pos] == 'e' || text[pos] == 'E' || text[pos] == '-' || text[pos] == '+'))
            ++pos;

        if (pos == start)
            return fail("expected a value");

        out = Value(std::strtod(text.substr(start, pos - start).c_str(), nullptr));
        return true;
    }

    bool parseString(std::string& out)
    {
        if (text[pos] != '"')
            return fail("expected a string");
        ++pos;

        out.clear();
        while (pos < text.size())
        {
            const char c = text[pos++];
            if (c == '"')
                return true;

            if (c != '\\')
            {
                out += c;
                continue;
            }

            if (pos >= text.size())
                break;

            const char escape = text[pos++];
            switch (escape)
            {
                case '"':  out += '"'; break;
                case '\\': out += '\\'; break;
                case '/':  out += '/'; break;
                case 'b':  out += '\b'; break;
                case 'f':  out += '\f'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case 'u':
                {
                    if (pos + 4 > text.size())
                        return fail("truncated \\u escape");
                    const auto code = static_cast<unsigned>(std::strtoul(text.substr(pos, 4).c_str(), nullptr, 16));
                    pos += 4;
                    // Enough for the ASCII and Latin range the index actually uses.
                    if (code < 0x80)
                    {
                        out += static_cast<char>(code);
                    }
                    else if (code < 0x800)
                    {
                        out += static_cast<char>(0xc0 | (code >> 6));
                        out += static_cast<char>(0x80 | (code & 0x3f));
                    }
                    else
                    {
                        out += static_cast<char>(0xe0 | (code >> 12));
                        out += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
                        out += static_cast<char>(0x80 | (code & 0x3f));
                    }
                    break;
                }
                default:
                    return fail("unknown escape");
            }
        }
        return fail("unterminated string");
    }

    bool parseArray(Value& out)
    {
        out = Value::array();
        ++pos; // '['
        skipSpace();
        if (pos < text.size() && text[pos] == ']')
        {
            ++pos;
            return true;
        }

        while (pos < text.size())
        {
            Value element;
            if (! parseValue(element))
                return false;
            out.push(std::move(element));

            skipSpace();
            if (pos >= text.size())
                break;
            if (text[pos] == ',')
            {
                ++pos;
                continue;
            }
            if (text[pos] == ']')
            {
                ++pos;
                return true;
            }
            return fail("expected , or ]");
        }
        return fail("unterminated array");
    }

    bool parseObject(Value& out)
    {
        out = Value::object();
        ++pos; // '{'
        skipSpace();
        if (pos < text.size() && text[pos] == '}')
        {
            ++pos;
            return true;
        }

        while (pos < text.size())
        {
            skipSpace();
            std::string key;
            if (! parseString(key))
                return false;

            skipSpace();
            if (pos >= text.size() || text[pos] != ':')
                return fail("expected :");
            ++pos;

            Value value;
            if (! parseValue(value))
                return false;
            out.set(key, std::move(value));

            skipSpace();
            if (pos >= text.size())
                break;
            if (text[pos] == ',')
            {
                ++pos;
                continue;
            }
            if (text[pos] == '}')
            {
                ++pos;
                return true;
            }
            return fail("expected , or }");
        }
        return fail("unterminated object");
    }
};

} // namespace

const Value& Value::operator[](const std::string& key) const
{
    const auto it = members.find(key);
    return it != members.end() ? it->second : nullValue;
}

void Value::set(const std::string& key, Value value)
{
    type = Type::Object;
    members[key] = std::move(value);
}

void Value::write(std::string& out, int indent, int depth) const
{
    const std::string pad(static_cast<size_t>(indent * (depth + 1)), ' ');
    const std::string closePad(static_cast<size_t>(indent * depth), ' ');
    const char* newline = indent > 0 ? "\n" : "";

    switch (type)
    {
        case Type::Null:   out += "null"; break;
        case Type::Bool:   out += boolean ? "true" : "false"; break;
        case Type::Number: out += formatNumber(number); break;
        case Type::String: writeEscaped(out, text); break;

        case Type::Array:
            if (elements.empty())
            {
                out += "[]";
                break;
            }
            out += "[";
            out += newline;
            for (size_t i = 0; i < elements.size(); ++i)
            {
                out += pad;
                elements[i].write(out, indent, depth + 1);
                if (i + 1 < elements.size())
                    out += ",";
                out += newline;
            }
            out += closePad;
            out += "]";
            break;

        case Type::Object:
            if (members.empty())
            {
                out += "{}";
                break;
            }
            out += "{";
            out += newline;
            {
                size_t index = 0;
                for (const auto& [key, value] : members)
                {
                    out += pad;
                    writeEscaped(out, key);
                    out += indent > 0 ? ": " : ":";
                    value.write(out, indent, depth + 1);
                    if (++index < members.size())
                        out += ",";
                    out += newline;
                }
            }
            out += closePad;
            out += "}";
            break;
    }
}

std::string Value::toString(int indent) const
{
    std::string out;
    write(out, indent, 0);
    return out;
}

bool parse(const std::string& text, Value& out, std::string& error)
{
    Parser parser(text);
    if (! parser.parseValue(out))
    {
        error = parser.error;
        return false;
    }

    parser.skipSpace();
    if (parser.pos != text.size())
    {
        error = "trailing characters at offset " + std::to_string(parser.pos);
        return false;
    }
    error.clear();
    return true;
}

} // namespace harmonia::json
