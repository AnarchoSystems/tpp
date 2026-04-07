#pragma once
#include <type_traits>

namespace tpp
{
    template <typename T, typename Enable = void>
    struct ArgType;

    // clang-format off
    
#define SCALAR_TYPE(FUN) \
    FUN(bool)            \
    FUN(long)            \
    FUN(double)          \
    FUN(float)           \
    FUN(char)

#define SCALAR_ARGTYPE(T) \
    template <>           \
    struct ArgType<T>     \
    {                     \
        using type = T;   \
    };

    // clang-format on

    SCALAR_TYPE(SCALAR_ARGTYPE)

    // enums are scalar as well
    template <typename T>
    struct ArgType<T, std::enable_if_t<std::is_enum_v<T>>>
    {
        using type = T;
    };

    // default: const T&
    template <typename T, typename Enable>
    struct ArgType
    {
        using type = const T &;
    };
}