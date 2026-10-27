#pragma once
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace cmac {
// Small write-only JSON value; all user strings are escaped here.
class Json {
    using Array = std::vector<Json>;
    using Object = std::map<std::string, Json>;
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> value_;
public:
    Json() : value_(nullptr) {}
    Json(bool v) : value_(v) {}
    Json(int v) : value_(double(v)) {}
    Json(size_t v) : value_(double(v)) {}
    Json(double v) : value_(v) {}
    Json(const char* v) : value_(std::string(v)) {}
    Json(std::string v) : value_(std::move(v)) {}
    static Json array() { Json v; v.value_ = Array{}; return v; }
    static Json object() { Json v; v.value_ = Object{}; return v; }
    Json& operator[](const std::string& k) { return std::get<Object>(value_)[k]; }
    void push(Json v) { std::get<Array>(value_).push_back(std::move(v)); }
    void extend(const Json& other) {
        auto& target=std::get<Array>(value_);const auto& source=std::get<Array>(other.value_);
        target.insert(target.end(),source.begin(),source.end());
    }
    static std::string quote(const std::string& s) {
        std::ostringstream out; out << '"';
        for (unsigned char c : s) {
            switch (c) {
                case '"': out << "\\\""; break;
                case '\\': out << "\\\\"; break;
                case '\n': out << "\\n"; break;
                case '\r': out << "\\r"; break;
                case '\t': out << "\\t"; break;
                default:
                    if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(c) << std::dec;
                    else out << c;
            }
        }
        out << '"'; return out.str();
    }
    std::string dump() const {
        std::ostringstream out; out << std::setprecision(15);
        std::visit([&](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) out << "null";
            else if constexpr (std::is_same_v<T, bool>) out << (v ? "true" : "false");
            else if constexpr (std::is_same_v<T, double>) { if (std::isfinite(v)) out << v; else out << "null"; }
            else if constexpr (std::is_same_v<T, std::string>) out << quote(v);
            else if constexpr (std::is_same_v<T, Array>) {
                out << '['; bool first = true;
                for (const auto& e : v) { if (!first) out << ','; first = false; out << e.dump(); }
                out << ']';
            } else {
                out << '{'; bool first = true;
                for (const auto& [k,e] : v) { if (!first) out << ','; first = false; out << quote(k) << ':' << e.dump(); }
                out << '}';
            }
        }, value_);
        return out.str();
    }
};
}
