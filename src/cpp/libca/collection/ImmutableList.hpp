#pragma once

#include <array>
#include <type_traits>

namespace ca {

template<typename T, size_t N>
class ImmutableList
{
private:
    std::array<T, N> data_;

public:
    template<typename... Args, typename = std::enable_if_t<sizeof...(Args) == N>>
    ImmutableList(Args... args)
        : data_{args...}
    {}

    template<typename... Args, typename = std::enable_if_t<sizeof...(Args) == N>>
    static ImmutableList of(Args... args)
    {
        return ImmutableList(args...);
    }

    T get(size_t index) const { return data_[index]; }

    size_t size() const { return N; }
};

}   // namespace ca
