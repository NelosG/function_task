#pragma once

#include "storage.h"

template<typename F>
struct function;

template<typename R, typename... Args>
struct function<R(Args...)> {
    function() noexcept = default;

    function(function const& other) = default;
    function(function&& other) noexcept = default;

    function& operator=(function const& rhs) = default;
    function& operator=(function&& rhs) noexcept = default;

    template<typename T>
    function(T val)
            : fStorage(std::move(val)) {}

    ~function() = default;

    explicit operator bool() const noexcept {
        return static_cast<bool>(fStorage);
    }

    R operator()(Args... args) const {
        return fStorage.invoke(std::forward<Args>(args)...);
    }

    template<typename T>
    T* target() noexcept {
        return fStorage.template target<T>();
    }

    template<typename T>
    T const* target() const noexcept {
        return fStorage.template target<T>();
    }

private:
    storage<R, Args...> fStorage;
};
