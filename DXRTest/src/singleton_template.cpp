#include "singleton_template.h"

//���������P�[�W
namespace {
    constexpr int kMaxFinalizersSize = 256;
}

std::unordered_map<std::type_index, SingletonFinalizer::FinalizerFunc> SingletonFinalizer::Lfinalizers;

//Singleton�N���X����݂̂̃A�N�Z�X�B�O������͎g�p���Ȃ��ł��������B
void SingletonFinalizer::AddFinalizer(FinalizerFunc func, std::type_index type) {
    assert(Lfinalizers.size() < kMaxFinalizersSize);
    Lfinalizers.insert(std::make_pair(type, func));
}

//mozc���̃V���O���g���Ő��������C���X�^���X�̉���B���ׂẴC���X�^���X��������܂��B
void SingletonFinalizer::Finalize() {
    for (auto& finalize : Lfinalizers) {
        (*finalize.second)();
    }
}