#pragma once

#include <cassert>
#include <mutex>
#include <typeindex>
#include <unordered_map>

class SingletonFinalizer {
public:
    using FinalizerFunc = void(*)();
    static void Finalize();

    template <typename T>
    static void FinalizeClass() {
        std::type_index tmp_index = typeid(T);
        const auto& it = Lfinalizers.find(tmp_index);
        if (it == Lfinalizers.end())return;

        (*it->second)();
        Lfinalizers.erase(it);
    };

private:
    template<typename T>
    friend class Singleton;
    static void AddFinalizer(FinalizerFunc func, std::type_index type);

    static std::unordered_map<std::type_index, SingletonFinalizer::FinalizerFunc> Lfinalizers;
};

template <typename T>
class Singleton final {
public:
    static T& getInstance() {
        std::call_once(_initFlag, _Create);
        assert(_instance);
        return *_instance;
    }

private:
    static void _Create() {
        _instance = new T;
        SingletonFinalizer::AddFinalizer(&Singleton<T>::_Destroy, typeid(T));
    }

    static void _Destroy() {
        delete _instance;
        _instance = nullptr;
    }

    static std::once_flag _initFlag;
    static T* _instance;
};

template <typename T> std::once_flag Singleton<T>::_initFlag;
template <typename T> T* Singleton<T>::_instance = nullptr;