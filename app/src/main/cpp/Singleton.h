//
// Created by zeng on 2025/11/28.
//

#ifndef SKIAOPENGLES_SINGLETON_H
#define SKIAOPENGLES_SINGLETON_H

template <typename T>
class Singleton {
public:
    static T& getInstance() {
        static T instance;
        return instance;
    }

    Singleton(const T&) = delete;
    void operator=(const T&) = delete;
    Singleton(T&&) = delete;

protected:
    Singleton() = default;
    virtual ~Singleton() = default;
};

#endif //SKIAOPENGLES_SINGLETON_H
