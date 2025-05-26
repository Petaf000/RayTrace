#pragma once
#include "GameObjectUtils.h"

struct OScale : Float3Base<OScale> {
    // 継承コンストラクタ
    using Float3Base::Float3Base;

    // Scale固有のコンストラクタ
    OScale() noexcept {
        FLOAT3_SET(x, y, z, 1.0f);
    }

    OScale(float uniform) noexcept {
        FLOAT3_SET(x, y, z, uniform);
    }

    DEFINE_FLOAT3_ARITHMETIC_OPERATORS(OScale)
        DEFINE_SCALE_OPERATORS(OScale)

        // Scale固有の機能
        float Volume() const {
        return x * y * z;
    }

    float MaxComponent() const {
        return std::max<float>({ x, y, z });
    }

    float MinComponent() const {
        return std::min<float>({ x, y, z });
    }

    bool IsUniform() const {
        return TransformHelpers::FloatEquals(x, y) && TransformHelpers::FloatEquals(y, z);
    }

    float GetUniformScale() const {
        return ( x + y + z ) / 3.0f;
    }

    OScale Inverse() const {
        return OScale(1.0f / x, 1.0f / y, 1.0f / z);
    }

    void MakeUniform(float scale) {
        FLOAT3_SET(x, y, z, scale);
    }

    void MakeUniformFromMax() {
        float maxScale = MaxComponent();
        FLOAT3_SET(x, y, z, maxScale);
    }

    void MakeUniformFromMin() {
        float minScale = MinComponent();
        FLOAT3_SET(x, y, z, minScale);
    }

    void MakeUniformFromAverage() {
        float avgScale = GetUniformScale();
        FLOAT3_SET(x, y, z, avgScale);
    }

    // スケールの補間（各軸を個別に補間）
    static OScale SmoothStep(const OScale& a, const OScale& b, float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        t = t * t * ( 3.0f - 2.0f * t ); // Smooth step function
        return Lerp(a, b, t);
    }

    // 制限（最小・最大値でクランプ）
    OScale Clamp(const OScale& min, const OScale& max) const {
        return OScale(
            std::clamp(x, min.x, max.x),
            std::clamp(y, min.y, max.y),
            std::clamp(z, min.z, max.z)
        );
    }

    // 絶対値
    OScale Abs() const {
        return OScale(std::abs(x), std::abs(y), std::abs(z));
    }

    OPosition ApplyToPosition(const OPosition& pos) const {
        return OPosition(pos.x * x, pos.y * y, pos.z * z);
    }

    // 静的定数
    static const OScale Identity() { return OScale(1, 1, 1); }
    static const OScale Zero() { return OScale(0, 0, 0); }
    static const OScale Half() { return OScale(0.5f, 0.5f, 0.5f); }
    static const OScale Double() { return OScale(2, 2, 2); }
};

inline OScale operator*(float scalar, const OScale& scale) {
    return scale * scalar;
}