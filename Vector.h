#ifndef VECTOR_H
#define VECTOR_H

#include <Arduino.h>

template<typename T>
class Vector {
private:
    T* data;
    size_t capacity;
    size_t length;

public:
    // Constructor
    Vector()
        : data(nullptr), capacity(0), length(0) {}

    // Destructor
    ~Vector() {
        clear();
    }

    // Get size
    size_t size() const {
        return length;
    }

    // Get capacity
    size_t max_size() const {
        return capacity;
    }

    // Clear the vector
    void clear() {
        if (data != nullptr) {
            delete[] data;
            data = nullptr;
            capacity = 0;
            length = 0;
        }
    }

    // Push an element to the back of the vector
    void push_back(const T& value) {
        if (length >= capacity) {
            if (capacity == 0) {
                capacity = 1;
            } else {
                capacity *= 2;
            }
            T* newData = new T[capacity];
            if (newData == nullptr) {
                // Handle memory allocation failure
                Serial.println("Memory allocation failed!");
                return;
            }
            // Copy existing elements to the new array
            for (size_t i = 0; i < length; ++i) {
                newData[i] = data[i];
            }
            // Delete the old data
            delete[] data;
            // Update data pointer
            data = newData;
        }
        data[length++] = value;
    }

    // Pop an element from the back of the vector
    void pop_back() {
        if (length > 0) {
            --length;
        }
    }

    // Pop elements from the front of the vector
    void popFront(size_t count = 1) {
        if (count >= length) {
            clear(); // If count exceeds or equals the length, clear the vector
            return;
        }

        // Shift elements to the left by count positions
        for (size_t i = count; i < length; ++i) {
            data[i - count] = data[i];
        }

        // Update length
        length -= count;
    }

    // Access an element by index
    T& operator[](size_t index) {
        if (index >= length) {
            // Handle out-of-bounds access
            Serial.println("Index out of bounds!");
            // Return a reference to the first element for simplicity
            return data[0];
        }
        return data[index];
    }

    // Get iterator to the beginning of the vector
    T* begin() const {
        return data;
    }

    // Get iterator to the end of the vector
    T* end() const {
        return data + length;
    }

    // Copy constructor
    Vector(const Vector& other)
        : data(nullptr), capacity(0), length(0) {
        *this = other; // Use assignment operator for deep copy
    }

    // Assignment operator
    Vector& operator=(const Vector& other) {
        if (this != &other) { // Avoid self-assignment
            clear(); // Clear existing data

            // Allocate memory for new data
            capacity = other.capacity;
            length = other.length;
            data = new T[capacity];

            // Copy elements from the other vector
            for (size_t i = 0; i < length; ++i) {
                data[i] = other.data[i];
            }
        }
        return *this;
    }

    // Move constructor
    Vector(Vector&& other) noexcept
        : data(nullptr), capacity(0), length(0) {
        *this = std::move(other); // Use move assignment operator
    }

    // Move assignment operator
    Vector& operator=(Vector&& other) noexcept {
        if (this != &other) { // Avoid self-assignment
            clear(); // Clear existing data

            // Move data from the other vector
            data = other.data;
            capacity = other.capacity;
            length = other.length;

            // Reset the other vector
            other.data = nullptr;
            other.capacity = 0;
            other.length = 0;
        }
        return *this;
    }
};

#endif
