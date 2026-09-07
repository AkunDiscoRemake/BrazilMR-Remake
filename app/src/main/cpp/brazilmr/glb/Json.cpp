#include "Json.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace brazilmr {

namespace {

class Parser {
public:
    Parser(const char* data, std::size_t len)
        : d_(data), len_(len), pos_(0) {}

    bool parse(JsonValue& out, std::string& err) {
        skipWs();
        if (!parseValue(out)) { err = err_; return false; }
        skipWs();
        if (pos_ != len_) { err = "conteúdo após o documento JSON"; return false; }
        return true;
    }

private:
    void skipWs() {
        while (pos_ < len_) {
            char c = d_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos_;
            else break;
        }
    }

    bool peek(char& c) const {
        if (pos_ >= len_) return false;
        c = d_[pos_];
        return true;
    }

    void fail(const char* msg) { if (err_.empty()) err_ = msg; }

    bool parseValue(JsonValue& out) {
        skipWs();
        char c;
        if (!peek(c)) { fail("fim inesperado"); return false; }
        switch (c) {
            case '{': return parseObject(out);
            case '[': return parseArray(out);
            case '"': out = JsonValue::makeString("");
                      if (!parseStringInto(out)) return false;
                      return true;
            case 't': return parseLit(out, "true", true);
            case 'f': return parseLit(out, "false", false);
            case 'n': return parseLit(out, "null", false);
            default:  return parseNumber(out);
        }
    }

    bool parseLit(JsonValue& out, const char* lit, bool isTrue) {
        std::size_t n = 0;
        while (lit[n]) ++n;
        if (pos_ + n > len_) { fail("literal inválido"); return false; }
        for (std::size_t i = 0; i < n; ++i)
            if (d_[pos_ + i] != lit[i]) { fail("literal inválido"); return false; }
        pos_ += n;
        out = lit[0] == 'n' ? JsonValue() : JsonValue::makeBool(isTrue);
        return true;
    }

    bool parseObject(JsonValue& out) {
        out = JsonValue::makeObject();
        ++pos_; // {
        skipWs();
        char c;
        if (!peek(c)) { fail("objeto sem fechamento"); return false; }
        if (c == '}') { ++pos_; return true; }
        while (true) {
            skipWs();
            if (!peek(c) || c != '"') { fail("chave esperada"); return false; }
            std::string key;
            if (!parseString(key)) return false;
            skipWs();
            if (!peek(c) || c != ':') { fail("':' esperado"); return false; }
            ++pos_;
            JsonValue val;
            if (!parseValue(val)) return false;
            out.put(key, val);
            skipWs();
            if (!peek(c)) { fail("objeto sem fechamento"); return false; }
            if (c == ',') { ++pos_; continue; }
            if (c == '}') { ++pos_; return true; }
            fail("',' ou '}' esperado");
            return false;
        }
    }

    bool parseArray(JsonValue& out) {
        out = JsonValue::makeArray();
        ++pos_; // [
        skipWs();
        char c;
        if (!peek(c)) { fail("array sem fechamento"); return false; }
        if (c == ']') { ++pos_; return true; }
        while (true) {
            JsonValue val;
            if (!parseValue(val)) return false;
            out.push(val);
            skipWs();
            if (!peek(c)) { fail("array sem fechamento"); return false; }
            if (c == ',') { ++pos_; continue; }
            if (c == ']') { ++pos_; return true; }
            fail("',' ou ']' esperado");
            return false;
        }
    }

    bool parseStringInto(JsonValue& out) {
        std::string s;
        if (!parseString(s)) return false;
        out.setString(std::move(s));
        return true;
    }

    bool parseString(std::string& out) {
        ++pos_; // "
        out.clear();
        while (pos_ < len_) {
            unsigned char c = static_cast<unsigned char>(d_[pos_]);
            if (c == '"') { ++pos_; return true; }
            if (c == '\\') {
                ++pos_;
                if (pos_ >= len_) break;
                char e = d_[pos_];
                switch (e) {
                    case '"': out += '"'; ++pos_; break;
                    case '\\': out += '\\'; ++pos_; break;
                    case '/': out += '/'; ++pos_; break;
                    case 'b': out += '\b'; ++pos_; break;
                    case 'f': out += '\f'; ++pos_; break;
                    case 'n': out += '\n'; ++pos_; break;
                    case 'r': out += '\r'; ++pos_; break;
                    case 't': out += '\t'; ++pos_; break;
                    case 'u': {
                        if (pos_ + 4 >= len_) { fail("unicode inválido"); return false; }
                        unsigned code = 0;
                        for (int i = 1; i <= 4; ++i) {
                            char h = d_[pos_ + i];
                            code <<= 4;
                            if (h >= '0' && h <= '9') code |= static_cast<unsigned>(h - '0');
                            else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned>(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned>(h - 'A' + 10);
                            else { fail("unicode inválido"); return false; }
                        }
                        pos_ += 5;
                        // UTF-8 (BMP; pares surrogate são raros em glTF)
                        if (code < 0x80) {
                            out += static_cast<char>(code);
                        } else if (code < 0x800) {
                            out += static_cast<char>(0xC0 | (code >> 6));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        } else {
                            out += static_cast<char>(0xE0 | (code >> 12));
                            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        }
                        break;
                    }
                    default: fail("escape inválido"); return false;
                }
            } else {
                out += static_cast<char>(c);
                ++pos_;
            }
        }
        fail("string sem fechamento");
        return false;
    }

    bool parseNumber(JsonValue& out) {
        std::size_t start = pos_;
        if (pos_ < len_ && (d_[pos_] == '-' || d_[pos_] == '+')) ++pos_;
        bool any = false;
        while (pos_ < len_) {
            char c = d_[pos_];
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' ||
                c == '-' || c == '+') {
                ++pos_;
                any = true;
            } else break;
        }
        if (!any) { fail("número inválido"); return false; }
        char* endPtr = nullptr;
        double v = std::strtod(d_ + start, &endPtr);
        out = JsonValue::makeNumber(v);
        return true;
    }

    const char* d_;
    std::size_t len_;
    std::size_t pos_;
    std::string err_;
};

} // namespace

bool jsonParse(const char* data, std::size_t length, JsonValue& out,
               std::string& err) {
    Parser p(data, length);
    return p.parse(out, err);
}

std::string JsonValue::dump() const {
    switch (type_) {
        case Type::Null: return "null";
        case Type::Bool: return bool_ ? "true" : "false";
        case Type::Number: {
            char buf[32];
            double v = num_;
            if (v == std::floor(v) && std::fabs(v) < 1e15)
                std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
            else
                std::snprintf(buf, sizeof(buf), "%g", v);
            return buf;
        }
        case Type::String: {
            std::string s = "\"";
            for (char c : str_) {
                if (c == '"' || c == '\\') { s += '\\'; s += c; }
                else if (c == '\n') s += "\\n";
                else s += c;
            }
            return s + "\"";
        }
        case Type::Array: {
            std::string s = "[";
            for (std::size_t i = 0; i < array_.size(); ++i) {
                if (i) s += ",";
                s += array_[i].dump();
            }
            return s + "]";
        }
        case Type::Object: {
            std::string s = "{";
            for (std::size_t i = 0; i < object_.size(); ++i) {
                if (i) s += ",";
                s += "\"" + object_[i].first + "\":" + object_[i].second.dump();
            }
            return s + "}";
        }
    }
    return "null";
}

} // namespace brazilmr
