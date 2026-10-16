#pragma once

struct TGeneratedConstruction {
    template <class T>
    static T Blank()
    {
        if constexpr (requires { typename T::TUninitialized; }) {
            return T(typename T::TUninitialized{});
        } else {
            return T{};
        }
    }
};
