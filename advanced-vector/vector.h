#pragma once
#include <iostream>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>


// Класс для работы с сырой памятью и обращения к ней
// Хранит буфер, вмещающий заданное количество элементов и предоставляет доступ к элементам по индексу
/*
Конструктор копирования и копирующий оператор присваивания в классе RawMemory должны быть запрещены, так как не имеют смысла.
*/
template <typename T>
class RawMemory {
public:

    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    // Конструктор копирования и копирующий оператор присваивания в классе RawMemory не имеют смысла,
    // поэтому запрещены во избежание их случайного вызова. 
    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    // Перемещающий конструктор 
    RawMemory(RawMemory&& other) noexcept {
        Swap(other);
    }
    
    // Оператор присваивания-перемещения 
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this->buffer_ != rhs.buffer_) {
            Swap(rhs);
        }
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
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
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};


// Класс вектора, работающий с сырой памятью через класс RawMemory
template <typename T>
class Vector {
public:
    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
        /* оператор const_cast используется, чтобы снять константность 
        с ссылки на текущий объект и вызвать неконстантную версию оператора [] */
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

private:

    // Вызывает деструктор объекта по адресу buf
    // деструктор шаблона T yt должен выбрасывать исключений
    static void Destroy(T* buf) noexcept {
        buf->~T();
    }

    // Вызывает деструкторы n объектов массива по адресу buf
    static void DestroyN(T* buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }

    // Создаёт копию объекта elem в сырой памяти по адресу buf
    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    inline constexpr void SafeCopyingOrMoving (T* from, size_t n_el, T* to) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) { 
            // копировать нельзя или при перемещении нет исключений
            std::uninitialized_move_n(from, n_el, to);
        } else {
            // если возможны исключения при перемещении
            std::uninitialized_copy_n(from, n_el, to);
        }
    }


public:
    // Конструктор по умолчанию
    // инициализирует вектор нулевого размера и вместимости, данные которого указывают на nullptr
    Vector() = default;

    // Конструктор, создающий вектор заданного размера
    explicit Vector(size_t size)
        : data_(size)  // память выделяется в конструкторе класса RawMemory, объектом которого является поле data_
        , size_(size)  // capacity теперь является свойством data_
    {
        // Создание + отлов исключений + разрушение созданных объектов при выбросе исключения 
        std::uninitialized_value_construct_n(data_.GetAddress()/* куда */, size_/* сколько */);
    }

    // Копирующий конструктор
    Vector(const Vector& other)
        : data_(other.size_)  // выделяем память такого же размера, как копируемый вектор
        , size_(other.size_)  
        /* независимо от вместимости оригинального вектора копия будет занимать столько 
        памяти, сколько нужно для хранения его элементов*/
    {
        std::uninitialized_copy_n(other.data_.GetAddress() /*std::static_cast<void*>(std::addressof(other[0]))*/ /*откуда*/, size_ /* сколько элементов */, data_.GetAddress()/* куда */);        
    } 

    // Перемещающий конструктор
    Vector(Vector&& other) noexcept {
        // Меняем местами
        Swap(other);
        // Удаляем исходный
    }

    // Деструктор
    ~Vector() {
        std::destroy_n(data_.GetAddress() /* откуда удалять */, size_/* сколько элементов */);
    }

    // Копирующий оператор присванивания 
    Vector& operator=(const Vector& rhs) {
        // Применяем copy-and-swap только когда вместимости вектора-приёмника не хватает, 
        // чтобы вместить все элементы вектора-источника:
        if (this != &rhs) {
            if (data_.Capacity() < rhs.size_) {
                /* copy-and-swap */
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
                // ~rhs_copy;
            } else {
                /* Скопировать элементы из rhs, создав при необходимости новые
                   или удалив существующие */
                if (size_ < rhs.size_) {
                    // копируем элементы в уже проинициализированные ячейки 
                    for (size_t i = 0; i < size_; i++) {
                        data_[i] = rhs[i];
                    }
                    // остальные элементы создаем на месте непроинициализированных
                    std::uninitialized_copy_n(rhs.data_ + size_, (rhs.size_ - size_), data_ + size_);
                }
                else {
                    // копируем элементы по меньшему размеру  
                    for (size_t i = 0; i < rhs.size_; i++) {
                        data_[i] = rhs[i];
                    }
                    // остальные удаляем
                    std::destroy_n(data_ + rhs.size_ /* откуда удалять */, (size_ - rhs.size_)/* сколько элементов */);
                }                
            }
            size_ = rhs.size_;
        }
        return *this;
    }


    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
            // ~rhs;
        }
        return *this;
    }

    void Swap(Vector& other) noexcept {
        // просто swap параметров вектора
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);      
    }


    // Резервирование памяти под элементы вектора
    /* Если требуемая вместимость больше текущей, Reserve выделяет нужный объём сырой памяти.
    */
    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        // Вариант 3 - адаптивный под шаблонный тип
        // Проверяется наличие оператора копирования и отсутствие исключений (noexcept) при перемещении/. 
        // И всё это во время компиляции, а не при выполнении программы => то есть скомпилируется только одна ветка if
        // constexpr оператор if будет вычислен во время компиляции
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) { 
            // копировать нельзя или при перемещении нет исключений
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            // если возможны исключения при перемещении
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        } 
        // Избавляемся от старой сырой памяти, обменивая её на новую
        data_.Swap(new_data);
        // Разрушаем старый объект
        std::destroy_n(new_data.GetAddress(), size_);
        // При выходе из метода старая память будет возвращена в кучу
    }

    void Resize(size_t new_size) {
        if (new_size < size_) {
            // удаляем элементы, не вписывающиеся в ноовый размер
            std::destroy_n(data_ + new_size, (size_-new_size));
        }
        else {
            // гарантируем достаточность места в векторе 
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_ + size_, (new_size - size_));
        }
        // актуализируем размер
        size_ = new_size;
    }


    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        /*
           Реализация похожа на PushBack, только вместо копирования или перемещения
           переданного элемента, он конструируется путём передачи параметров метода конструктору T
        */
        // создаем новый элемент
        if (size_ == Capacity()) {
            // Создаем новый вектор
            size_t new_capacity = size_ == 0 ? 1 : size_ * 2;
            RawMemory<T> new_data(new_capacity);
            // перемещаем в конец новый элемент
            new (new_data + size_) T(std::forward<Args>(args)...);
            // копируем или перемещаем все элементы исходного вектора в новую область
            SafeCopyingOrMoving(data_.GetAddress(), size_, new_data.GetAddress());
            data_.Swap(new_data);
            std::destroy_n(new_data.GetAddress(), size_);
        }
        else {
            // просто перемещаем новый элемент в конец
            new (data_ + size_) T(std::forward<Args>(args)...);
        }
        // ! Обновить размер !
        ++size_;
        return data_[size_ - 1];
    }

    // Копирующий push_back
    // Вариант 2 - через EmplaceBack 
    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    // Перемещающий push_back
    // работает эффективнее для временных объектов 
    // Вариант 2 - через EmplaceBack
    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }



    void PopBack() noexcept {
        // удаляем один элемент
        std::destroy_n(data_ + size_ - 1, 1);
        --size_;
    }

public:
// Методы, использующие итераторы
    using iterator = T*;
    using const_iterator = const T*;
    
    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return (data_ + size_);
    }

    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept {
        return (data_ + size_);
    }

    const_iterator cbegin() const noexcept {
        return const_cast<const_iterator>(data_.GetAddress());
    }

    const_iterator cend() const noexcept {
        return const_cast<const_iterator>(data_ + size_);
    }


    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        size_t pos_index = std::distance(cbegin(), pos);
        if (size_ == Capacity()) {
            // Создаем новый вектор
            size_t new_capacity = size_ == 0 ? 1 : size_ * 2;
            RawMemory<T> new_data(new_capacity);
            
            // конструируем новый элемент на заданном месте
            new (new_data + pos_index) T(std::forward<Args>(args)...);
            
            // копируем/перемещаем элементы исходного вектора до позиции вставки
            try {
                // to do: +1 ??
                SafeCopyingOrMoving(begin(), (pos_index), new_data.GetAddress());
            }
            catch (...) {
                std::destroy_at(new_data + pos_index);
            }

            // копируем элементы исходного вектора после позиции вставки
            try {
                // to do: +1 ??
                SafeCopyingOrMoving(begin() + pos_index, (size_ - pos_index), (new_data + pos_index + 1));
            }
            catch (...) {
                std::destroy(new_data.GetAddress(), new_data + pos_index);
            }
            // Заменяем data_ на новый созданный вектор
            data_.Swap(new_data);
            // удаляем элементы исходного вектора
            std::destroy_n(new_data.GetAddress(), size_);
            // !!! обновить размер вектора
            ++size_;

        }
        else {
            // Частный случай - пустой вектор - нужно просто вставить элемент
            if (size_ == 0) {
                // конструируем новый элемент в заданной позиции
                new (data_ + pos_index) T(std::forward<Args>(args)...);
                ++size_;
                return data_ + pos_index;
            }
            // Переместить последний элемент 
            std::move(end() - 1, end(), end());
            ++size_;
            // создаем временную копию
            T new_el(std::forward<Args>(args)...);

            // сдвигаем элементы от старого end до pos, двигаясь вправо
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) { 
                // копировать нельзя или при перемещении нет исключений
                std::move_backward(begin() + pos_index, end() - 2, end() - 1);
            } else {
                // если возможны исключения при перемещении
                std::copy_backward(begin() + pos_index, end() - 2, end() - 1);
            }

            // конструируем новый элемент в заданной позиции
            new (data_ + pos_index) T(std::move(new_el));

        }
        
        // вернуть итератор на вставленный элемент
        return data_ + pos_index;
    }

    // Вставка нового элемента в указанную позицию
    // Вариант 2 - через Emplace
    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }


    // Вставка нового элемента в указанную позицию (эффективнее для временных объектов)
    // Вариант 2 - через Emplace
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    } 
    

    iterator Erase(const_iterator pos) {
        size_t pos_index = std::distance(cbegin(), pos);
        // Перемещаем элементы, следующие за удаляемым
        std::move(begin() + pos_index + 1, end(), begin() + pos_index);
        // Разрушаем последний элемент, содержимое которого уже перемещено
        std::destroy_at(end() - 1);
        // !! Обновляем размер
        --size_;
        return (begin() + pos_index);
    }

};