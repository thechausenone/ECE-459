#pragma once

#include <cassert>
#include <deque>

#ifdef USE_FAST
    template<typename T>
    class Container
    {
        std::deque<T> storage;
        int len = 0;

        void deepCopy(const Container &other) {
            storage.clear();
            len = other.len;
            if (!other.storage.empty()) {
                storage = std::deque<T>(other.storage);
            }
        }

    public:
        Container() {
            // nop
        }

        ~Container() {
            clear();
        }

        Container(const Container &other) {
            deepCopy(other);
        }

        Container& operator=(const Container &other) {
            if (this != &other) {
                deepCopy(other);
            }

            return *this;
        }

        int size() const {
            return len;
        }

        T operator[](int idx) const {
            if (!(0 <= idx && idx < len)) {
                assert(false && "Accessing index out of range");
            }

            return storage[idx];
        }

        void clear() {
            storage.clear();
            len = storage.size();
        }

        T front() const {
            return storage.front();
        }

        void pushFront(T item) {
            storage.push_front(item);
            len = storage.size();
        }

        void pushBack(T item) {
            storage.push_back(item);
            len = storage.size();
        }

        T popFront() {
            T item = storage.front();
            storage.pop_front();
            len = storage.size();
            return item;
        }

        void push(T item) {
            pushBack(item);
        }

        T pop() {
            return popFront();
        }
    };
#else
    template<typename T>
    class Container
    {
        T* storage = nullptr;
        int len = 0;

        void deepCopy(const Container &other) {
            len = other.len;

            delete [] storage;
            storage = nullptr;

            if (other.storage) {
                storage = new T[len];
                for (int i = 0; i < len; i++) {
                    storage[i] = other.storage[i];
                }
            }
        }

    public:
        Container() {
            // nop
        }

        ~Container() {
            clear();
        }

        Container(const Container &other) {
            deepCopy(other);
        }

        Container& operator=(const Container &other) {
            if (this != &other) {
                deepCopy(other);
            }

            return *this;
        }

        int size() const {
            return len;
        }

        T operator[](int idx) const {
            if (!(0 <= idx && idx < len)) {
                assert(false && "Accessing index out of range");
            }

            return storage[idx];
        }

        void clear() {
            delete [] storage;
            storage = nullptr;
            len = 0;
        }

        T front() const {
            return storage[0];
        }

        void pushFront(T item) {
            T* oldStorage = storage;

            len += 1;
            storage = new T[len];

            storage[0] = item;
            for (int i = 1; i < len; i++) {
                storage[i] = oldStorage[i - 1];
            }

            delete [] oldStorage;
        }

        void pushBack(T item) {
            T* oldStorage = storage;

            len += 1;
            storage = new T[len];

            storage[len - 1] = item;
            for (int i = 0; i < len - 1; i++) {
                storage[i] = oldStorage[i];
            }

            delete [] oldStorage;
        }

        T popFront() {
            if (len < 1) {
                assert(false && "Trying to pop an empty Container");
            }

            T* oldStorage = storage;
            T front = oldStorage[0];

            len -= 1;

            if (len == 0) {
                storage = nullptr;
            } else {
                storage = new T[len];
                for (int i = 0; i < len; i++) {
                    storage[i] = oldStorage[i + 1];
                }
            }

            delete [] oldStorage;
            return front;
        }

        void push(T item) {
            pushBack(item);
        }

        T pop() {
            return popFront();
        }
    };
#endif