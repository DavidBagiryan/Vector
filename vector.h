#pragma once

#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }
    
    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept 
    {
        Swap(other);
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    T* buffer_ = nullptr;
    size_t capacity_ = 0;
    
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }
};

template <typename T>
class Vector {
public:
    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)  
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }
    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_) 
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }
    
    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }
    
    Vector(Vector&& other) noexcept
            : data_(std::move(other.data_))
            , size_(other.size_)
    {
        other.size_ = 0;
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                if (rhs.size_ < size_) {
                    std::copy_n(rhs.data_.GetAddress(), rhs.size_, data_.GetAddress());
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                }
                else {
                    std::copy_n(rhs.data_.GetAddress(), size_, data_.GetAddress());
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }
    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            data_ = std::move(rhs.data_);
            size_ = rhs.size_;
            rhs.size_ = 0;
        }
        return *this;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }
    
    size_t Size() const noexcept {
        return size_;
    }
    
    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }
    
    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        FullSafeMemoryTransfer(data_.GetAddress(), Size(), new_data, new_data.GetAddress());
    }
    
    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, Size() - new_size);
        } 
        else if (new_size > size_) {
            size_t capacity = Size() == 0 ? 1 : Capacity();
            while (new_size > Capacity()) {
                Reserve(capacity * 2);
            }
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        size_ = new_size;
    }
    
    template <typename F> // Forwarding reference
    void PushBack(F&& value) {
        EmplaceBack(std::forward<F>(value));
    }
    
    void PopBack() /* noexcept */ {
        std::destroy_at(data_.GetAddress() + size_);
        --size_;
    }
    
    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (Size() < Capacity()) {
            new (data_ + Size()) T(std::forward<Args>(args)...);          
        }
        else {
            RawMemory<T> new_data(size_ == 0 ? 1 : Capacity() * 2);
            new (new_data + Size()) T(std::forward<Args>(args)...);
            FullSafeMemoryTransfer(data_.GetAddress(), Size(), new_data, new_data.GetAddress());
        }
        ++size_;
        return data_[size_ - 1];
    }
    
    using iterator = T*;
    using const_iterator = const T*;
    
    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_.GetAddress() + Size();
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return data_.GetAddress() + Size();
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_.GetAddress() + Size();
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        assert(pos >= cbegin() && pos <= cend());
        size_t pos_ind = pos - cbegin();
        if (Size() < Capacity()) {
            new (end()) T(std::forward<T>(*(end() - 1)));
            std::move_backward(begin() + pos_ind, end() - 1, end());
            data_[pos_ind] = T(std::forward<Args>(args)...);
        }
        else {
            RawMemory<T> new_data(Size() == 0 ? 1 : Capacity() * 2);
            new (new_data.GetAddress() + pos_ind) T(std::forward<Args>(args)...);
            try {
                SafeMemoryTransfer(begin(), pos_ind, new_data.GetAddress());
            }
            catch (...) {
                std::destroy_at(new_data.GetAddress() + pos_ind);
                throw;
            }
            try {
                SafeMemoryTransfer(begin() + pos_ind, end() - begin() - pos_ind, new_data.GetAddress() + pos_ind + 1);
            }
            catch (...) {
                std::destroy_n(new_data.GetAddress(), pos_ind + 1);
                throw;
            }
            std::destroy_n(begin(), Size());
            data_.Swap(new_data);
        }
        ++size_;
        return begin() + pos_ind;
    }
    
    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/ {
        assert(pos >= cbegin() && pos < cend());
        size_t i = pos - cbegin();
        std::move(begin() + i + 1, end(), begin() + i);
        std::destroy_at(data_.GetAddress() + Size() - 1);
        --size_;
        return begin() + i;
    }
    
    template <typename F>
    iterator Insert(const_iterator pos, F&& value) {
        return Emplace(pos, std::forward<F>(value));
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
    
    void FullSafeMemoryTransfer(iterator begin, size_t size, RawMemory<T>& new_data, T* new_address_begin) {
        SafeMemoryTransfer(begin, size, new_address_begin);
        std::destroy_n(begin, size);
        data_.Swap(new_data);
    }
    
    void SafeMemoryTransfer(iterator begin, size_t size, T* new_address_begin) {
        // constexpr оператор if будет вычислен во время компиляции
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(begin, size, new_address_begin);
        } else {
            std::uninitialized_copy_n(begin, size, new_address_begin);
        }
    }
};
