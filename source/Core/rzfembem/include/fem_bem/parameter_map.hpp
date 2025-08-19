#pragma once
#include <cstring>
#include <initializer_list>
#include <utility>

namespace USTC_CG {
namespace fem_bem {

    // Stack-based parameter map using fixed-size array
    // Ultra-lightweight optimized for small parameter lists with no dynamic memory allocation
    template<
        typename T,
        std::size_t MaxSize = 8,
        std::size_t NameBufferSize = 4>
    class ParameterMap {
       private:
        // Ultra-compact entry layout optimized for cache lines
        struct Entry {
            char name[NameBufferSize];
            T value;
            
            // No occupied flag - use name[0] == '\0' to indicate empty
            Entry() : value{}
            {
                name[0] = '\0';
            }
            
            bool is_empty() const { return name[0] == '\0'; }
            void mark_empty() { name[0] = '\0'; }
        };

        Entry entries_[MaxSize];
        std::size_t size_;

       public:
        ParameterMap() : size_(0) {}

        // Constructor from initializer list
        ParameterMap(std::initializer_list<std::pair<const char*, T>> init)
            : size_(0)
        {
            for (const auto& pair : init) {
                insert_or_assign(pair.first, pair.second);
            }
        }

        // Ultra-fast insert or update - optimized for hot path
        inline void insert_or_assign(const char* name, const T& value)
        {
            // Fast path for single character names (most common case)
            if (name[1] == '\0') {
                const char ch = name[0];
                // Check existing entries first (likely to be small number)
                for (std::size_t i = 0; i < size_; ++i) {
                    Entry& entry = entries_[i];
                    if (entry.name[0] == ch && entry.name[1] == '\0') {
                        entry.value = value;
                        return;
                    }
                }
                
                // Insert new single-char entry
                if (size_ < MaxSize) {
                    Entry& entry = entries_[size_];
                    entry.name[0] = ch;
                    entry.name[1] = '\0';
                    entry.value = value;
                    ++size_;
                    return;
                }
            }

            // Multi-character path with efficient string comparison
            const std::size_t name_len = std::strlen(name);
            if (name_len >= NameBufferSize) return; // Name too long
            
            // Check existing entries
            for (std::size_t i = 0; i < size_; ++i) {
                Entry& entry = entries_[i];
                if (entry.name[0] == name[0]) {
                    // Quick memcmp for exact match (faster than strcmp for short strings)
                    if (std::memcmp(entry.name, name, name_len + 1) == 0) {
                        entry.value = value;
                        return;
                    }
                }
            }

            // Insert new entry
            if (size_ < MaxSize) {
                Entry& entry = entries_[size_];
                std::memcpy(entry.name, name, name_len + 1);
                entry.value = value;
                ++size_;
            }
        }

        // Ultra-fast find for const access
        inline const T* find(const char* name) const
        {
            // Fast path for single character names
            if (name[1] == '\0') {
                const char ch = name[0];
                for (std::size_t i = 0; i < size_; ++i) {
                    const Entry& entry = entries_[i];
                    if (entry.name[0] == ch && entry.name[1] == '\0') {
                        return &entry.value;
                    }
                }
                return nullptr;
            }

            // Multi-character search with optimized comparison
            const std::size_t name_len = std::strlen(name);
            for (std::size_t i = 0; i < size_; ++i) {
                const Entry& entry = entries_[i];
                if (entry.name[0] == name[0]) {
                    // Quick memcmp for exact match
                    if (std::memcmp(entry.name, name, name_len + 1) == 0) {
                        return &entry.value;
                    }
                }
            }
            return nullptr;
        }

        // Ultra-fast find for non-const access
        inline T* find(const char* name)
        {
            // Fast path for single character names
            if (name[1] == '\0') {
                const char ch = name[0];
                for (std::size_t i = 0; i < size_; ++i) {
                    Entry& entry = entries_[i];
                    if (entry.name[0] == ch && entry.name[1] == '\0') {
                        return &entry.value;
                    }
                }
                return nullptr;
            }

            // Multi-character search with optimized comparison
            const std::size_t name_len = std::strlen(name);
            for (std::size_t i = 0; i < size_; ++i) {
                Entry& entry = entries_[i];
                if (entry.name[0] == name[0]) {
                    // Quick memcmp for exact match
                    if (std::memcmp(entry.name, name, name_len + 1) == 0) {
                        return &entry.value;
                    }
                }
            }
            return nullptr;
        }

        // Check if key exists
        bool contains(const char* name) const
        {
            return find(name) != nullptr;
        }

        // Get size
        std::size_t size() const
        {
            return size_;
        }

        // Check if empty
        bool empty() const
        {
            return size_ == 0;
        }

        // Clear all entries
        void clear()
        {
            size_ = 0;
            // Simply reset size, entries will be overwritten
        }

        // Index-based access for performance (replaces iterator interface)
        const char* get_name_at(std::size_t index) const
        {
            return index < size_ ? entries_[index].name : nullptr;
        }

        const T& get_value_at(std::size_t index) const
        {
            return entries_[index].value;
        }

        T& get_value_at(std::size_t index)
        {
            return entries_[index].value;
        }
    };

    // Type aliases for common use cases
    using ParameterMapD = ParameterMap<double>;
    using ParameterMapF = ParameterMap<float>;

}  // namespace fem_bem
}  // namespace USTC_CG