// BrazilMR — parser JSON mínimo, suficiente para o glTF (e perfis de VR Box).
// Sem alocação de strings intermediárias desnecessárias; sem dependências.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace brazilmr {

class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    JsonValue() : type_(Type::Null) {}
    explicit JsonValue(Type t) : type_(t) {}

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }
    bool isBool() const { return type_ == Type::Bool; }

    double asDouble(double def = 0.0) const { return isNumber() ? num_ : def; }
    float  asFloat(float def = 0.0f) const { return isNumber() ? static_cast<float>(num_) : def; }
    int    asInt(int def = 0) const { return isNumber() ? static_cast<int>(num_ + (num_ < 0 ? -0.5 : 0.5)) : def; }
    bool   asBool(bool def = false) const { return isBool() ? bool_ : def; }
    const std::string& asString() const { static const std::string empty; return isString() ? str_ : empty; }

    // arrays
    std::size_t size() const { return isArray() ? array_.size() : 0; }
    const JsonValue& at(std::size_t i) const {
        static const JsonValue null;
        if (isArray() && i < array_.size()) return array_[i];
        return null;
    }
    void push(const JsonValue& v) { array_.push_back(v); }

    // objetos (busca linear — glTF tem poucas chaves por objeto)
    const JsonValue& get(const char* key) const {
        static const JsonValue null;
        for (auto& kv : object_) if (kv.first == key) return kv.second;
        return null;
    }
    void put(const std::string& key, const JsonValue& v) { object_.emplace_back(key, v); }

    // construtores de conveniência
    static JsonValue makeNumber(double d) { JsonValue v(Type::Number); v.num_ = d; return v; }
    static JsonValue makeString(std::string s) { JsonValue v(Type::String); v.str_ = std::move(s); return v; }
    void setString(std::string s) { type_ = Type::String; str_ = std::move(s); }
    static JsonValue makeBool(bool b) { JsonValue v(Type::Bool); v.bool_ = b; return v; }
    static JsonValue makeArray() { return JsonValue(Type::Array); }
    static JsonValue makeObject() { return JsonValue(Type::Object); }

    void reserveArray(std::size_t n) { array_.reserve(n); }
    void reserveObject(std::size_t n) { object_.reserve(n); }

    // Serialização compacta (usado em depuração/perfis).
    std::string dump() const;

private:
    Type type_;
    double num_ = 0.0;
    bool bool_ = false;
    std::string str_;
    std::vector<JsonValue> array_;
    std::vector<std::pair<std::string, JsonValue>> object_;
};

// Faz o parse completo. Retorna false em caso de erro (mensagem em err).
bool jsonParse(const char* data, std::size_t length, JsonValue& out,
               std::string& err);

inline bool jsonParse(const std::string& s, JsonValue& out, std::string& err) {
    return jsonParse(s.data(), s.size(), out, err);
}

} // namespace brazilmr
