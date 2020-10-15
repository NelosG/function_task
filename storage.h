#pragma once


struct bad_function_call : std::exception {
    char const *what() const noexcept override {
        return "bad function call";
    }
};

template<typename R, typename... Args>
struct storage;


constexpr static size_t INPLACE_BUFFER_SIZE = sizeof(void*);
constexpr static size_t INPLACE_BUFFER_ALIGNMENT = alignof(void*);
using buffer_t = std::aligned_storage_t<
        INPLACE_BUFFER_SIZE,
        INPLACE_BUFFER_ALIGNMENT>;

template<typename T>
constexpr static bool small_storage =
        sizeof(T) <= INPLACE_BUFFER_SIZE
        && std::is_nothrow_move_constructible<T>::value
        && INPLACE_BUFFER_ALIGNMENT % alignof(T) == 0;



template<typename R, typename... Args>
struct type_descriptor {
    using storage_t = storage<R, Args...>;

    void (*copy)(storage_t *dest, storage_t const *src);

    void (*move)(storage_t *dest, storage_t *src);

    R (*invoke)(storage_t const *src, Args...);

    void (*destroy)(storage_t *);
};




template<typename R, typename... Args>
struct storage {
    storage() noexcept
            : buffer(),
              descriptor(&empty_descriptor) {}

    template<typename T>
    storage(T val)
            :  buffer(),
               descriptor(&get_descriptor<T>) {
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
            descriptor->destroy(this);
            other.descriptor->move(this, &temp);
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
        return descriptor != &empty_descriptor;
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
        if (&get_descriptor<T> != descriptor) return nullptr;
        if constexpr (small_storage<T>) {
            return reinterpret_cast<T const *>(&buffer);
        } else {
            return *reinterpret_cast<T *const *>(&buffer);
        }
    }
    using storage_t = storage<R, Args...>;
    constexpr static type_descriptor<R, Args...> empty_descriptor =
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
    template<typename T>
    constexpr static type_descriptor<R, Args...> get_descriptor = {
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
                src->descriptor = &empty_descriptor;
            },
            // invoke
            [](storage_t const *src, Args... args) -> R {
                return (*src->template target<T>())(std::forward<Args>(args)...);
            },
            // destroy
            [](storage_t *src) {
                if constexpr (small_storage<T>) {
                    src->template target<T>()->~T();
                } else {
                    delete src->template target<T>();
                }
                src->descriptor = &empty_descriptor;
            }
    };

    buffer_t buffer;
    type_descriptor<R, Args...> const *descriptor;
};
