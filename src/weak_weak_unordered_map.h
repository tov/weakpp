#pragma once

#include "weak_hash_table_base.h"
#include "weak_traits.h"
#include "weak_weak_pair.h"

#include <functional>

namespace weak {

/// A map whose keys and range values are both stored by `std::weak_ptr`s.
///
/// When a pointer to a key or a range value expires, that association of the
/// map is lazily removed.
template<
        class Key, class T,
        class Hash = std::hash<Key>,
        class KeyEqual = std::equal_to<>,
        class Allocator = std::allocator<weak_weak_pair<Key, T>>>
class weak_weak_unordered_map
        : public weak_hash_table_base<weak_weak_pair<Key, T>,
                                              Hash, KeyEqual, Allocator>
{
    using BaseClass = weak_hash_table_base<weak_weak_pair<Key, T>,
                                                   Hash, KeyEqual, Allocator>;
    using Bucket = typename BaseClass::Bucket;
public:
    using BaseClass::weak_hash_table_base;

    /// Proxy class returned by `operator[](const std::shared_ptr<const Key>&)`.
    ///
    /// Assigning to this proxy will assign to the value pointer in the map.
    class proxy
    {
    public:
        /// Copy constructor.
        proxy(const proxy&) noexcept = default;
        /// Move constructor.
        proxy(proxy&&) noexcept = default;

        /// No copy-assignment.
        proxy& operator=(const proxy&) = delete;
        /// No move-assignment.
        proxy& operator=(proxy&&) = delete;

        /// Dereferencing the proxy acts like dereferencing the `shared_ptr`.
        T& operator*() const noexcept
        {
            return *value_ptr_;
        }

        /// The proxy stands for a `shared_ptr`.
        std::shared_ptr<T> operator->() const noexcept
        {
            return value_ptr_;
        }

        /// The proxy is coercible to a `shared_ptr`.
        operator std::shared_ptr<T>() const noexcept
        {
            return value_ptr_;
        }

        /// Assigning a `shared_ptr` to the proxy assigns into the map.
        proxy& operator=(const std::shared_ptr<T>& value)
        {
            bucket_.value().second = value_ptr_ = value;
            return *this;
        }

        /// Assigning a `shared_ptr` to the proxy assigns into the map.
        proxy& operator=(std::shared_ptr<T>&& value)
        {
            bucket_.value().second = value_ptr_ = std::move(value);
            return *this;
        }

    private:
        explicit proxy(Bucket& bucket)
                : bucket_(bucket)
                , value_ptr_(bucket_.value().second.lock())
        { }

        Bucket& bucket_;
        std::shared_ptr<T> value_ptr_;

        friend class weak_weak_unordered_map;
    };

    /// Looks up the given key in the hash table, returning a proxy for
    /// the `shared_ptr` to the value.
    ///
    /// If the key doesn't exist then it is inserted and temporarily mapped
    /// to an expired pointer. However, assigning a `shared_ptr` to the proxy
    /// will stored the `shared_ptr` in the map instead.
    proxy operator[](const std::shared_ptr<const Key>& key)
    {
        Bucket* result_bucket;

        BaseClass::insert_helper_(
                *key,
                [&](Bucket& bucket) {
                    BaseClass::construct_bucket_(bucket, key,
                                                 std::shared_ptr<T>{});
                    result_bucket = &bucket;
                },
                [&](Bucket& bucket) {
                    bucket.value().first = key;
                    bucket.value().second = std::shared_ptr<T>{};
                    result_bucket = &bucket;
                },
                [&](Bucket& bucket) {
                    result_bucket = &bucket;
                });

        return proxy(*result_bucket);
    }
};

/// Swaps two `weak_weak_unordered_map`s in constant time.
template<class Key, class T, class Hash, class KeyEqual, class Allocator>
void swap(weak_weak_unordered_map<Key, T, Hash, KeyEqual, Allocator>& a,
          weak_weak_unordered_map<Key, T, Hash, KeyEqual, Allocator>& b)
{
    a.swap(b);
}

/// Is `a` a submap of `b`?
///
/// That is, are all the keys of `a` keys of `b`, and all the values equal
/// according to `compare`. Function `compare` defaults to equality, but
/// other relations are possible.
template <class Key, class T, class Hash, class KeyEqual, class Allocator,
          class TEqual = std::equal_to<>>
bool submap(
        const weak_weak_unordered_map<Key, T, Hash, KeyEqual, Allocator>& a,
        const weak_weak_unordered_map<Key, T, Hash, KeyEqual, Allocator>& b,
        TEqual compare = TEqual())
{
    for (const auto& elem : a) {
        auto iter = b.find(*elem.first);
        if (iter == b.end() || !compare(*(*iter).second, *elem.second))
            return false;
    }

    return true;
}

/// Are the keys of `a` a subset of the keys of `b`?
template <class Key, class T, class Hash, class KeyEqual, class Allocator>
bool keys_subset(
        const weak_weak_unordered_map<Key, T, Hash, KeyEqual, Allocator>& a,
        const weak_weak_unordered_map<Key, T, Hash, KeyEqual, Allocator>& b)
{
    submap(a, b, [](const auto&, const auto&) { return true; });
}

/// Are the given maps equal?
template <class Key, class T, class Hash, class KeyEqual, class Allocator>
bool operator==(const weak_weak_unordered_map<Key, T, Hash, KeyEqual, Allocator>& a,
                const weak_weak_unordered_map<Key, T, Hash, KeyEqual, Allocator>& b)
{
    return submap(a, b) && keys_subset(b, a);
}

/// Are the given maps unequal?
template <class Key, class T, class Hash, class KeyEqual, class Allocator>
bool operator!=(const weak_weak_unordered_map<Key, T, Hash, KeyEqual, Allocator>& a,
                const weak_weak_unordered_map<Key, T, Hash, KeyEqual, Allocator>& b)
{
    return !(a == b);
}

} // end namespace weak
