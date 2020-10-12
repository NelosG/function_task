#pragma once

#include "type_descriptor.h"


template<typename R, typename... Args>
struct storage {
    storage() noexcept
            : buffer(),
              descriptor(empty_descriptor<R, Args...>()) {}

    template<typename T>
    storage(T val)
            :  buffer(),
               descriptor(get_descriptor<T, R, Args...>()) {
        if constexpr (small_storage<T>) {
            new(&(buffer)) T(std::move(val));
        } else {
            reinterpret_cast<void *&>(buffer) = new T(std::move(val));
        }
    }

    storage(storage const &other) {
        other.descriptor->copy(this, &other);
    }

    storage(storage &&other) noexcept {
        other.descriptor->move(this, &other);
    }

    storage &operator=(storage const &other) {
        if (this != &other) {
            storage temp(other);
            std::swap(buffer, temp.buffer);
            std::swap(descriptor, temp.descriptor);
        }
        return *this;
    }

    storage &operator=(storage &&other) noexcept {
        if (this != &other) {
            descriptor->destroy(this);
            other.descriptor->move(this, &other);
        }
        return *this;
    }


    ~storage() {
        descriptor->destroy(this);
    }

    explicit operator bool() const noexcept {
        return descriptor != empty_descriptor<R, Args...>();
    }

    R invoke(Args... args) const {
        return (*descriptor->invoke)(this, std::forward<Args>(args)...);
    }

    template<typename T>
    T *target() noexcept {
        return const_cast<T *>(get_target<T>());
    }

    template<typename T>
    T const *target() const noexcept {
        return get_target<T>();
    }


private:
    template<typename T>
    T const *get_target() const noexcept {
        if (get_descriptor<T, R, Args...>() != descriptor) return nullptr;
        if constexpr (small_storage<T>) {
            return reinterpret_cast<T const *>(&buffer);
        } else {
            return *reinterpret_cast<T *const *>(&buffer);
        }
    }


    template<typename RO, typename... ArgsO>
    friend type_descriptor<RO, ArgsO...> const *empty_descriptor();


    template<typename U, typename RO, typename... ArgsO>
    friend type_descriptor<RO, ArgsO...> const *get_descriptor() noexcept;

    std::aligned_storage_t<sizeof(void *), alignof(void *)> buffer;
    type_descriptor<R, Args...> const *descriptor;
};
