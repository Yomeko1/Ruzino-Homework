#pragma once
#include <cstring>
#include <initializer_list>
#include <utility>

namespace USTC_CG {
namespace fem_bem {

    // Stack-based parameter map using fixed-size array
    // Optimized for small parameter lists with no dynamic memory allocation
    template<
        typename T,
        std::size_t MaxSize = 8,
        std::size_t NameBufferSize = 4>
    class ParameterMap {
       private:
        struct Entry {
            char name[NameBufferSize];
            T value;
            bool occupied;

            Entry() : value{}, occupied(false)
            {
                name[0] = '\0';
            }
        };

        Entry entries_[MaxSize];
        std::size_t size_;

       public:
        ParameterMap() : size_(0)
        {
        }

        // Constructor from initializer list
        ParameterMap(std::initializer_list<std::pair<const char*, T>> init)
            : size_(0)
        {
            for (const auto& pair : init) {
                insert_or_assign(pair.first, pair.second);
            }
        }

        // Insert or update a value
        void insert_or_assign(const char* name, const T& value)
        {
            // First check if key already exists
            for (std::size_t i = 0; i < size_; ++i) {
                if (entries_[i].occupied && entries_[i].name[0] == name[0] &&
                    std::strcmp(entries_[i].name, name) == 0) {
                    entries_[i].value = value;
                    return;
                }
            }

            // Insert new entry if there's space
            if (size_ < MaxSize) {
                Entry& entry = entries_[size_];

                // Optimized string copy - handle common single character case
                // first
                if (name[1] == '\0') {
                    // Single character optimization
                    entry.name[0] = name[0];
                    entry.name[1] = '\0';
                }
                else {
                    // Fast manual copy for longer strings
                    const char* src = name;
                    char* dst = entry.name;
                    std::size_t i = 0;
                    while (i < NameBufferSize - 1 && *src != '\0') {
                        *dst++ = *src++;
                        ++i;
                    }
                    *dst = '\0';
                }

                entry.value = value;
                entry.occupied = true;
                ++size_;
            }
        }

        // Access by key (const version) - optimized for single character names
        const T* find(const char* name) const
        {
            // Fast path for single character names
            if (name[1] == '\0') {
                const char ch = name[0];
                for (std::size_t i = 0; i < size_; ++i) {
                    if (entries_[i].occupied && entries_[i].name[0] == ch &&
                        entries_[i].name[1] == '\0') {
                        return &entries_[i].value;
                    }
                }
            }
            else {
                // Slower path for multi-character names
                for (std::size_t i = 0; i < size_; ++i) {
                    if (entries_[i].occupied &&
                        std::strcmp(entries_[i].name, name) == 0) {
                        return &entries_[i].value;
                    }
                }
            }
            return nullptr;
        }

        // Access by key (non-const version) - optimized for single character
        // names
        T* find(const char* name)
        {
            // Fast path for single character names
            if (name[1] == '\0') {
                const char ch = name[0];
                for (std::size_t i = 0; i < size_; ++i) {
                    if (entries_[i].occupied && entries_[i].name[0] == ch &&
                        entries_[i].name[1] == '\0') {
                        return &entries_[i].value;
                    }
                }
            }
            else {
                // Slower path for multi-character names
                for (std::size_t i = 0; i < size_; ++i) {
                    if (entries_[i].occupied &&
                        std::strcmp(entries_[i].name, name) == 0) {
                        return &entries_[i].value;
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
            for (std::size_t i = 0; i < MaxSize; ++i) {
                entries_[i].occupied = false;
            }
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