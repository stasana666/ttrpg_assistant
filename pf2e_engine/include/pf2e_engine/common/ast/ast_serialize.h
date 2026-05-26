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

// AstSerialize(value) -> std::string. Stable, deterministic representation
// of a value-typed field. Type tag is prefixed so distinct types with the
// same numeric content do not collide (e.g. int 0 vs bool false).
//
// Add overloads in this header for primitive / std types.
// For project-specific value types (TPosition, TPlayerId, etc.) add
// overloads in the type's own header, next to the type definition.

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

// Enums: serialize by underlying integer value. Resolves ambiguity with
// the int overload by SFINAE on is_enum.
template <class T, class = std::enable_if_t<std::is_enum_v<T>>>
std::string AstSerialize(T v)
{
    return "enum:" + std::to_string(static_cast<std::int64_t>(v));
}

// TValueId: use the global name table so the serialization is human-readable
// AND stable across runs (the id integer is allocation-order-dependent).
template <class Tag>
std::string AstSerialize(const TValueId<Tag>& id)
{
    return "TValueId:" + std::string(TValueIdManager<Tag>::Instance().Name(id));
}

// std::monostate (empty variant alternative).
inline std::string AstSerialize(std::monostate)
{
    return "monostate";
}

// std::variant: recurse into the held alternative.
template <class... Ts>
std::string AstSerialize(const std::variant<Ts...>& v)
{
    return "variant:" + std::visit(
        [](const auto& x) -> std::string { return AstSerialize(x); }, v);
}

// std::vector<T>: serialize each element. Caller must ensure deterministic
// element order (this is true for ordered containers; for unordered ones
// pre-sort before calling).
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

// std::array<T, N>: serialize each element in index order.
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
