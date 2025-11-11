#include "singleton_template.h"

//内部リンケージ
namespace {
    constexpr int kMaxFinalizersSize = 256;
}

std::unordered_map<std::type_index, SingletonFinalizer::FinalizerFunc> SingletonFinalizer::Lfinalizers;

//Singletonクラスからのみのアクセス。外部からは使用しないでください。
void SingletonFinalizer::AddFinalizer(FinalizerFunc func, std::type_index type) {
    assert(Lfinalizers.size() < kMaxFinalizersSize);
    Lfinalizers.insert(std::make_pair(type, func));
}

//mozc式のシングルトンで生成したインスタンスの解放。すべてのインスタンスを解放します。
void SingletonFinalizer::Finalize() {
    for (auto& finalize : Lfinalizers) {
        (*finalize.second)();
    }
}