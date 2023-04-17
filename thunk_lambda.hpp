/*
    Written by T. P. K. Healy in April 2023, adapted from code shared by:
        Alf P. Steinbach  alf.p.steinbach@gmail.com
        Edward Catmur     ecatmur@googlemail.com
*/

#ifndef HEADER_INCLUSION_GUARD_THUNK_LAMBDA_HPP
#define HEADER_INCLUSION_GUARD_THUNK_LAMBDA_HPP

#include <cassert>      // assert
#include <cstddef>      // size_t
#include <cstdlib>      // abort (if mutex fails to lock)
#include <mutex>        // mutex, lock_guard
#include <utility>      // index_sequence, make_index_sequence, forward, declval

// The next three templates: 'ToFuncPtr', 'ToReturnType', 'IsNoExcept'
// are just helpers to make this all possible. You can scroll past them
// to Line #66

namespace detail {

// The following template turns a member function pointer into a
// normal function pointer, but we only want it to use decltype on it
template<typename ReturnType, class ClassType, bool b_noexcept, typename... Params>
ReturnType (*ToFuncPtr(ReturnType (ClassType::*)(Params...) const noexcept(b_noexcept)))(Params...) noexcept(b_noexcept)
{
    return nullptr;  // suppress compiler warning
}
// and also the non-const version for non-const member functions (or mutable lambdas):
template<typename ReturnType, class ClassType, bool b_noexcept, typename... Params>
ReturnType (*ToFuncPtr(ReturnType (ClassType::*)(Params...) noexcept(b_noexcept)))(Params...) noexcept(b_noexcept)
{
    return nullptr;  // suppress compiler warning
}

// The following template isolates the return type of a member function pointer,
// but we only want it to use decltype on it. I tried using 'std::result_of_t'
// instead but I get a compiler error for the lambda being an incomplete type
template <typename ReturnType, class ClassType, typename... Params>
ReturnType ToReturnType(ReturnType (ClassType::*)(Params...) const)
{
    return std::declval<ReturnType>();  // suppress compiler warning
}
// and also the non-const version for non-const member functions (or mutable lambdas):
template <typename ReturnType, class ClassType, typename... Params>
ReturnType ToReturnType(ReturnType (ClassType::*)(Params...))
{
    return std::declval<ReturnType>();  // suppress compiler warning
}

// The following template determines whether a non-static member function is noexcept
template<typename ReturnType, class ClassType, bool b_noexcept, typename... Params>
consteval bool IsNoExcept(ReturnType (ClassType::*)(Params...) const noexcept(b_noexcept))
{
    return b_noexcept;
}
// and also the non-const version for non-const member functions (or mutable lambdas):
template<typename ReturnType, class ClassType, bool b_noexcept, typename... Params>
consteval bool IsNoExcept(ReturnType (ClassType::*)(Params...) noexcept(b_noexcept))
{
    return b_noexcept;
}

}  // close namespace 'detail'

template<typename LambdaType,std::size_t N = 32u>
class thunk {
protected:
    using size_t  = std::size_t;
    using R       = decltype(detail::ToReturnType(&LambdaType::operator()));
    using FuncPtr = decltype(detail::ToFuncPtr   (&LambdaType::operator()));  // preserves the 'noexcept'

public:
    explicit thunk(LambdaType &arg) noexcept : index(acquire(arg)) {
        data[index] = &arg;
    }
    ~thunk() noexcept { release(index); }
    FuncPtr get() const noexcept {
        FuncPtr ret;
        [this,&ret]<size_t... I>(std::index_sequence<I...>) {
            ((I == index ? ret = &invoke<I> : ret), ...);
        }(std::make_index_sequence<N>());
        return ret;
    }
    operator FuncPtr() const noexcept { return this->get(); }

protected:
    static size_t acquire(LambdaType &arg) noexcept {
        try {
            std::lock_guard lck(mut);  // might throw std::system_error
            for (size_t i = 0; i != N; ++i)
                if (nullptr == data[i])
                    return data[i] = &arg, i;
            assert(nullptr == "thunk pool exhausted");
            std::abort();  // ifdef NDEBUG
        }
        catch(...) {
            assert(nullptr == "failed to lock mutex");
            std::abort();  // ifdef NDEBUG
        }
    }
    static void release(size_t const i) noexcept {
        try {
            std::lock_guard lck(mut);  // might throw std::system_error
            data[i] = nullptr;
        }
        catch(...) {
            assert(nullptr == "failed to lock mutex");
            std::abort();  // ifdef NDEBUG
        }
    }
    inline static LambdaType *data[N] = {};  // starts off all nullptr's
    inline static std::mutex mut;
    size_t const index;

    template<size_t I, typename... A> static R invoke(A... a) noexcept(detail::IsNoExcept(&LambdaType::operator()))
    {
        assert(nullptr != data[I]);
        return (*data[I])(std::forward<A>(a)...);
    }

    thunk(void)                        = delete;
    thunk(thunk const & )              = delete;
    thunk(thunk       &&)              = delete;
    thunk &operator=(thunk const & )   = delete;
    thunk &operator=(thunk       &&)   = delete;
    thunk const *operator&(void) const = delete;  // just to avoid confusion
    thunk       *operator&(void)       = delete;  // just to avoid confusion
};

#endif  // ifndef HEADER_INCLUSION_GUARD_THUNK_LAMBDA_HPP
