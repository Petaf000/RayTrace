#pragma once
// 共通マクロ定義
#define FLOAT3_ZERO(x, y, z) x = y = z = 0.0f
#define FLOAT3_SET(x, y, z, val) x = y = z = val
#define FLOAT3_COPY(x, y, z, x2, y2, z2) x = x2; y = y2; z = z2
#define FLOAT3_ASSIGN_OP(x, y, z, x2, y2, z2, op) x op x2; y op y2; z op z2
#define FLOAT_EPSILON 1e-6f

// 演算子自動生成マクロ
#define DEFINE_FLOAT3_ARITHMETIC_OPERATORS(ClassName) \
    ClassName operator+(const ClassName& other) const { \
        return ClassName(x + other.x, y + other.y, z + other.z); \
    } \
    ClassName operator-(const ClassName& other) const { \
        return ClassName(x - other.x, y - other.y, z - other.z); \
    } \
    ClassName operator*(float scalar) const { \
        return ClassName(x * scalar, y * scalar, z * scalar); \
    } \
    ClassName operator/(float scalar) const { \
        float inv = 1.0f / scalar; \
        return ClassName(x * inv, y * inv, z * inv); \
    } \
    ClassName& operator+=(const ClassName& other) { \
        FLOAT3_ASSIGN_OP(x, y, z, other.x, other.y, other.z, +=); \
        return *this; \
    } \
    ClassName& operator-=(const ClassName& other) { \
        FLOAT3_ASSIGN_OP(x, y, z, other.x, other.y, other.z, -=); \
        return *this; \
    } \
    ClassName& operator*=(float scalar) { \
        FLOAT3_ASSIGN_OP(x, y, z, scalar, scalar, scalar, *=); \
        return *this; \
    } \
    ClassName& operator/=(float scalar) { \
        float inv = 1.0f / scalar; \
        FLOAT3_ASSIGN_OP(x, y, z, inv, inv, inv, *=); \
        return *this; \
    }

#define DEFINE_FLOAT3_COMPARISON_OPERATORS(ClassName) \
    bool operator==(const ClassName& other) const { \
        return TransformHelpers::Float3Equals(x, y, z, other.x, other.y, other.z); \
    } \
    bool operator!=(const ClassName& other) const { \
        return !(*this == other); \
    }

#define DEFINE_FLOAT3_CAST_OPERATORS() \
    operator XMFLOAT3() const { return value; } \
    operator XMFLOAT3*() { return &value; } \
    operator const XMFLOAT3*() const { return &value; }

#define DEFINE_SCALE_OPERATORS(ClassName) \
    ClassName operator*(const ClassName& other) const { \
        return ClassName(x * other.x, y * other.y, z * other.z); \
    } \
    ClassName operator/(const ClassName& other) const { \
        return ClassName(x / other.x, y / other.y, z / other.z); \
    } \
    ClassName& operator*=(const ClassName& other) { \
        FLOAT3_ASSIGN_OP(x, y, z, other.x, other.y, other.z, *=); \
        return *this; \
    } \
    ClassName& operator/=(const ClassName& other) { \
        FLOAT3_ASSIGN_OP(x, y, z, other.x, other.y, other.z, /=); \
        return *this; \
    }

// 共通ヘルパー関数
namespace TransformHelpers {
    inline bool FloatEquals(float a, float b, float epsilon = FLOAT_EPSILON) {
        return std::abs(a - b) < epsilon;
    }

    inline bool Float3Equals(float x1, float y1, float z1, float x2, float y2, float z2) {
        return FloatEquals(x1, x2) && FloatEquals(y1, y2) && FloatEquals(z1, z2);
    }

    inline XMVECTOR LoadFloat3(float x, float y, float z) {
        return XMVectorSet(x, y, z, 0.0f);
    }

    inline void StoreFloat3(XMVECTOR vec, float& x, float& y, float& z) {
        XMFLOAT3 temp;
        XMStoreFloat3(&temp, vec);
        FLOAT3_COPY(x, y, z, temp.x, temp.y, temp.z);
    }
}

// 共通のベクトル操作
template<typename Derived>
struct VectorOps {
    Derived& self() { return static_cast<Derived&>( *this ); }
    const Derived& self() const { return static_cast<const Derived&>( *this ); }

    float Length() const {
        const Derived& s = self();
        return std::sqrt(s.x * s.x + s.y * s.y + s.z * s.z);
    }

    float LengthSquared() const {
        const Derived& s = self();
        return s.x * s.x + s.y * s.y + s.z * s.z;
    }

    float Distance(const Derived& other) const {
        return ( self() - other ).Length();
    }

    float DistanceSquared(const Derived& other) const {
        return ( self() - other ).LengthSquared();
    }

    Derived Normalize() const {
        float len = Length();
        return len > FLOAT_EPSILON ? self() / len : Derived{};
    }

    void NormalizeInPlace() {
        float len = Length();
        if ( len > FLOAT_EPSILON ) {
            self() /= len;
        }
        else {
            Derived& s = self();
            FLOAT3_ZERO(s.x, s.y, s.z);
        }
    }

    float Dot(const Derived& other) const {
        const Derived& s = self();
        return s.x * other.x + s.y * other.y + s.z * other.z;
    }

    static Derived Lerp(const Derived& a, const Derived& b, float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return Derived(
            a.x + ( b.x - a.x ) * t,
            a.y + ( b.y - a.y ) * t,
            a.z + ( b.z - a.z ) * t
        );
    }

    XMVECTOR ToXMVector() const {
        const Derived& s = self();
        return TransformHelpers::LoadFloat3(s.x, s.y, s.z);
    }
};

// Float3の共通基底クラス
template<typename Derived>
struct Float3Base : VectorOps<Derived> {
    union {
        XMFLOAT3 value;
        struct {
            float x, y, z;
        };
    };

    // 共通コンストラクタ
    Float3Base() noexcept {
        FLOAT3_ZERO(x, y, z);
    }

    Float3Base(XMFLOAT3 val) noexcept : value(val) {}

    Float3Base(float _x, float _y, float _z) noexcept {
        FLOAT3_COPY(x, y, z, _x, _y, _z);
    }

    explicit Float3Base(_In_reads_(3) const float* pArray) noexcept {
        FLOAT3_COPY(x, y, z, pArray[0], pArray[1], pArray[2]);
    }

    DEFINE_FLOAT3_CAST_OPERATORS()
        DEFINE_FLOAT3_COMPARISON_OPERATORS(Derived)

        static Derived FromXMVector(XMVECTOR vec) {
        XMFLOAT3 result;
        XMStoreFloat3(&result, vec);
        return Derived(result);
    }
};