#pragma once

#include <exception>

template<typename R, typename... Args>
struct storage;

struct bad_function_call : std::exception {
    char const *what() const noexcept override {
        return "bad function call";
    }
};

template<typename T>
constexpr static bool small_storage =
        sizeof(T) <= sizeof(void *)
        && std::is_nothrow_move_constructible<T>::value
        && (alignof(void *) % alignof(T) == 0 || alignof(void *) <= alignof(T));

template<typename R, typename... Args>
struct type_descriptor {
    using storage_t = storage<R, Args...>;

    void (*copy)(storage_t *dest, storage_t const *src);

    void (*move)(storage_t *dest, storage_t *src);

    R (*invoke)(storage_t const *src, Args...);

    void (*destroy)(storage_t *);
};

template<typename R, typename... Args>
type_descriptor<R, Args...> const *empty_descriptor() {
    using storage_t = storage<R, Args...>;

    constexpr static type_descriptor<R, Args...> impl =
            {
                    [](storage_t *dest, storage_t const *src) {
                        dest->descriptor = src->descriptor;
                    },
                    [](storage_t *dest, storage_t *src) {
                        dest->descriptor = std::move(src->descriptor);
                    },
                    [](storage_t const *src, Args...) -> R {
                        throw bad_function_call();
                    },
                    [](storage_t *) {}
            };

    return &impl;
}


template<typename T, typename R, typename... Args>
type_descriptor<R, Args...> const *get_descriptor() noexcept {
    using storage_t = storage<R, Args...>;

    constexpr static type_descriptor<R, Args...> impl = {
            // copy
            [](storage_t *dest, storage_t const *src) {
                if constexpr (small_storage<T>) {
                    new(&dest->buffer) T(*src->template target<T>());
                } else {
                    reinterpret_cast<void *&>(dest->buffer) = (new T(*src->template target<T>()));
                }
                dest->descriptor = src->descriptor;
            },
            // move
            [](storage_t *dest, storage_t *src) noexcept {
                if constexpr (small_storage<T>) {
                    new(&dest->buffer) T(std::move(*src->template target<T>()));
                } else {
                    reinterpret_cast<void *&>(dest->buffer) = (src->template target<T>());
                }
                dest->descriptor = src->descriptor;
                src->descriptor = empty_descriptor<R, Args...>();
            },
            // invoke
            [](storage_t const *src, Args... args) -> R {
                return (*src->template target<T>())(std::forward<Args>(args)...);
            },
            // destroy
            [](storage_t *src) {
                src->template target<T>()->~T();
                src->descriptor = empty_descriptor<R, Args...>();
            }
    };

    return &impl;
}

