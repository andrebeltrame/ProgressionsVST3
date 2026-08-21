#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace harmonia::json
{

/** A very small JSON value, enough to read and write the library index without
    pulling in a dependency. */
class Value
{
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Value() = default;
    Value(bool value) : type(Type::Bool), boolean(value) {}
    Value(double value) : type(Type::Number), number(value) {}
    Value(int value) : type(Type::Number), number(value) {}
    Value(const char* value) : type(Type::String), text(value) {}
    Value(std::string value) : type(Type::String), text(std::move(value)) {}

    static Value array() { Value v; v.type = Type::Array; return v; }
    static Value object() { Value v; v.type = Type::Object; return v; }

    Type kind() const noexcept { return type; }
    bool isNull() const noexcept { return type == Type::Null; }
    bool isArray() const noexcept { return type == Type::Array; }
    bool isObject() const noexcept { return type == Type::Object; }

    bool asBool(bool fallback = false) const { return type == Type::Bool ? boolean : fallback; }
    double asNumber(double fallback = 0.0) const { return type == Type::Number ? number : fallback; }
    int asInt(int fallback = 0) const { return type == Type::Number ? static_cast<int>(number) : fallback; }
    const std::string& asString() const { return text; }
    std::string asString(const std::string& fallback) const { return type == Type::String ? text : fallback; }

    const std::vector<Value>& items() const { return elements; }
    void push(Value value) { type = Type::Array; elements.push_back(std::move(value)); }

    /** Object access. Returns a null value when the key is missing. */
    const Value& operator[](const std::string& key) const;
    void set(const std::string& key, Value value);
    bool has(const std::string& key) const { return members.find(key) != members.end(); }
    const std::map<std::string, Value>& fields() const { return members; }

    std::string toString(int indent = 2) const;

private:
    void write(std::string& out, int indent, int depth) const;

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string text;
    std::vector<Value> elements;
    std::map<std::string, Value> members;
};

/** Returns false and fills `error` when the text is not valid JSON. */
bool parse(const std::string& text, Value& out, std::string& error);

} // namespace harmonia::json
