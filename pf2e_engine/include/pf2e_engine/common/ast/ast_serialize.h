#pragma once

#include <pf2e_engine/common/value_id.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

// Stringify value-typed fields for the AST. Type tag is prefixed so two
// different types with the same numeric content do not collide. Extend
// with overloads as needed; project-specific types put theirs next to the
// type definition.

inline std::string AstSerialize(bool v)
{
    return std::string("bool:") + (v ? "1" : "0");
}

inline std::string AstSerialize(int v)
{
    return "int:" + std::to_string(v);
}

inline std::string AstSerialize(unsigned int v)
{
    return "uint:" + std::to_string(v);
}

inline std::string AstSerialize(std::int64_t v)
{
    return "i64:" + std::to_string(v);
}

inline std::string AstSerialize(std::uint64_t v)
{
    return "u64:" + std::to_string(v);
}

inline std::string AstSerialize(float v)
{
    return "float:" + std::to_string(v);
}

inline std::string AstSerialize(double v)
{
    return "double:" + std::to_string(v);
}

inline std::string AstSerialize(const std::string& v)
{
    return "string:" + v;
}

inline std::string AstSerialize(std::string_view v)
{
    return "string:" + std::string(v);
}

inline std::string AstSerialize(const char* v)
{
    return "string:" + std::string(v);
}

inline std::string AstSerialize(const std::filesystem::path& v)
{
    return "path:" + v.string();
}

template <class T, class = std::enable_if_t<std::is_enum_v<T>>>
std::string AstSerialize(T v)
{
    return "enum:" + std::to_string(static_cast<std::int64_t>(v));
}

template <class Tag>
std::string AstSerialize(const TValueId<Tag>& id)
{
    return "TValueId:" + std::string(TValueIdManager<Tag>::Instance().Name(id));
}

inline std::string AstSerialize(std::monostate)
{
    return "monostate";
}

template <class... Ts>
std::string AstSerialize(const std::variant<Ts...>& v)
{
    return "variant:" + std::visit(
        [](const auto& x) -> std::string { return AstSerialize(x); }, v);
}

// Caller must ensure deterministic element order for unordered containers.
template <class T>
std::string AstSerialize(const std::vector<T>& v)
{
    std::ostringstream oss;
    oss << "vector[";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << AstSerialize(v[i]);
    }
    oss << "]";
    return oss.str();
}

template <class T, std::size_t N>
std::string AstSerialize(const std::array<T, N>& a)
{
    std::ostringstream oss;
    oss << "array[";
    for (std::size_t i = 0; i < N; ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << AstSerialize(a[i]);
    }
    oss << "]";
    return oss.str();
}
