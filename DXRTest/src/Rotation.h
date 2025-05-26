#pragma once
#include "GameObjectUtils.h"

struct ORotation {
    union {
        XMFLOAT4 quaternion;
        struct {
            float qx, qy, qz, qw;
        };
    };

private:
    mutable XMFLOAT3 m_cachedEuler = { 0, 0, 0 };
    mutable bool m_eulerCacheValid = false;

    void InvalidateEulerCache() const {
        m_eulerCacheValid = false;
    }

    void UpdateEulerCache() const {
        if ( m_eulerCacheValid ) return;

        XMVECTOR q = XMQuaternionNormalize(XMLoadFloat4(&quaternion));
        XMFLOAT4 nq;
        XMStoreFloat4(&nq, q);

        float sinYaw = 2.0f * ( nq.w * nq.y + nq.z * nq.x );
        float cosYaw = 1.0f - 2.0f * ( nq.x * nq.x + nq.y * nq.y );
        m_cachedEuler.y = atan2f(sinYaw, cosYaw);

        float sinPitch = 2.0f * ( nq.w * nq.x - nq.y * nq.z );
        sinPitch = std::clamp(sinPitch, -1.0f, 1.0f);
        m_cachedEuler.x = asinf(sinPitch);

        float sinRoll = 2.0f * ( nq.w * nq.z + nq.x * nq.y );
        float cosRoll = 1.0f - 2.0f * ( nq.x * nq.x + nq.z * nq.z );
        m_cachedEuler.z = atan2f(sinRoll, cosRoll);

        m_eulerCacheValid = true;
    }

public:
    // プロパティアクセサ
    float GetPitchInternal() const {
        UpdateEulerCache();
        return m_cachedEuler.x;
    }
    void SetPitchInternal(float value) {
        UpdateEulerCache();
        m_cachedEuler.x = value;
        XMVECTOR q = XMQuaternionRotationRollPitchYaw(m_cachedEuler.x, m_cachedEuler.y, m_cachedEuler.z);
        q = XMQuaternionNormalize(q);
        XMStoreFloat4(&quaternion, q);
        m_eulerCacheValid = true;
    }

    float GetYawInternal() const {
        UpdateEulerCache();
        return m_cachedEuler.y;
    }
    void SetYawInternal(float value) {
        UpdateEulerCache();
        m_cachedEuler.y = value;
        XMVECTOR q = XMQuaternionRotationRollPitchYaw(m_cachedEuler.x, m_cachedEuler.y, m_cachedEuler.z);
        q = XMQuaternionNormalize(q);
        XMStoreFloat4(&quaternion, q);
        m_eulerCacheValid = true;
    }

    float GetRollInternal() const {
        UpdateEulerCache();
        return m_cachedEuler.z;
    }
    void SetRollInternal(float value) {
        UpdateEulerCache();
        m_cachedEuler.z = value;
        XMVECTOR q = XMQuaternionRotationRollPitchYaw(m_cachedEuler.x, m_cachedEuler.y, m_cachedEuler.z);
        q = XMQuaternionNormalize(q);
        XMStoreFloat4(&quaternion, q);
        m_eulerCacheValid = true;
    }

    XMFLOAT3 GetEulerAnglesInternal() const {
        UpdateEulerCache();
        return m_cachedEuler;
    }
    void SetEulerAnglesInternal(const XMFLOAT3& value) {
        XMVECTOR q = XMQuaternionRotationRollPitchYaw(value.x, value.y, value.z);
        q = XMQuaternionNormalize(q);
        XMStoreFloat4(&quaternion, q);
        m_cachedEuler = value;
        m_eulerCacheValid = true;
    }

    XMFLOAT3 GetEulerDegreesInternal() const {
        constexpr float radToDeg = 180.0f / XM_PI;
        UpdateEulerCache();
        return XMFLOAT3(
            m_cachedEuler.x * radToDeg,
            m_cachedEuler.y * radToDeg,
            m_cachedEuler.z * radToDeg
        );
    }
    void SetEulerDegreesInternal(const XMFLOAT3& value) {
        constexpr float degToRad = XM_PI / 180.0f;
        SetEulerAnglesInternal(XMFLOAT3(
            value.x * degToRad,
            value.y * degToRad,
            value.z * degToRad
        ));
    }

    XMFLOAT4 GetQuaternionInternal() const {
        return quaternion;
    }
    void SetQuaternionInternal(const XMFLOAT4& value) {
        XMVECTOR q = XMLoadFloat4(&value);
        q = XMQuaternionNormalize(q);
        XMStoreFloat4(&quaternion, q);
        InvalidateEulerCache();
    }

    // プロパティ定義
    __declspec( property( get = GetPitchInternal, put = SetPitchInternal ) ) float Pitch;
    __declspec( property( get = GetYawInternal, put = SetYawInternal ) ) float Yaw;
    __declspec( property( get = GetRollInternal, put = SetRollInternal ) ) float Roll;
    __declspec( property( get = GetEulerAnglesInternal, put = SetEulerAnglesInternal ) ) XMFLOAT3 EulerAngles;
    __declspec( property( get = GetEulerDegreesInternal, put = SetEulerDegreesInternal ) ) XMFLOAT3 EulerDegrees;
    __declspec( property( get = GetQuaternionInternal, put = SetQuaternionInternal ) ) XMFLOAT4 Quaternion;

    // コンストラクタ
    ORotation() noexcept {
        quaternion.x = quaternion.y = quaternion.z = 0.0f;
        quaternion.w = 1.0f;
        InvalidateEulerCache();
    }

    ORotation(float pitch, float yaw, float roll) noexcept {
        XMVECTOR q = XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);
        q = XMQuaternionNormalize(q);
        XMStoreFloat4(&quaternion, q);
        InvalidateEulerCache();
    }

    ORotation(const XMFLOAT3& euler) {
        XMVECTOR q = XMQuaternionRotationRollPitchYaw(euler.x, euler.y, euler.z);
        q = XMQuaternionNormalize(q);
        XMStoreFloat4(&quaternion, q);
        InvalidateEulerCache();
    }

    ORotation(const XMFLOAT4& quat) {
        XMVECTOR q = XMLoadFloat4(&quat);
        q = XMQuaternionNormalize(q);
        XMStoreFloat4(&quaternion, q);
        InvalidateEulerCache();
    }

    ORotation(float _qx, float _qy, float _qz, float _qw) noexcept {
        qx = _qx; qy = _qy; qz = _qz; qw = _qw;
        XMVECTOR q = XMLoadFloat4(&quaternion);
        q = XMQuaternionNormalize(q);
        XMStoreFloat4(&quaternion, q);
        InvalidateEulerCache();
    }

    explicit ORotation(_In_reads_(3) const float* pArray) {
        XMVECTOR q = XMQuaternionRotationRollPitchYaw(pArray[0], pArray[1], pArray[2]);
        q = XMQuaternionNormalize(q);
        XMStoreFloat4(&quaternion, q);
        InvalidateEulerCache();
    }

    // キャスト演算子
    operator XMFLOAT4() const { return quaternion; }
    operator XMFLOAT4* ( ) { return &quaternion; }
    operator const XMFLOAT4* ( ) const { return &quaternion; }

    operator XMFLOAT3() const {
        UpdateEulerCache();
        return m_cachedEuler;
    }

    // 算術演算子
    ORotation operator*(const ORotation& other) const {
        XMVECTOR q1 = XMLoadFloat4(&quaternion);
        XMVECTOR q2 = XMLoadFloat4(&other.quaternion);
        XMVECTOR result = XMQuaternionMultiply(q1, q2);

        ORotation rot;
        XMStoreFloat4(&rot.quaternion, result);
        rot.InvalidateEulerCache();
        return rot;
    }

    ORotation operator*(float scalar) const {
        return ORotation(qx * scalar, qy * scalar, qz * scalar, qw * scalar);
    }

    ORotation& operator*=(const ORotation& other) {
        XMVECTOR q1 = XMLoadFloat4(&quaternion);
        XMVECTOR q2 = XMLoadFloat4(&other.quaternion);
        XMVECTOR result = XMQuaternionMultiply(q1, q2);
        result = XMQuaternionNormalize(result);
        XMStoreFloat4(&quaternion, result);
        InvalidateEulerCache();
        return *this;
    }

    ORotation& operator*=(float scalar) {
        qx *= scalar; qy *= scalar; qz *= scalar; qw *= scalar;
        XMVECTOR q = XMQuaternionNormalize(XMLoadFloat4(&quaternion));
        XMStoreFloat4(&quaternion, q);
        InvalidateEulerCache();
        return *this;
    }

    // 比較演算子
    bool operator==(const ORotation& other) const {
        XMVECTOR q1 = XMLoadFloat4(&quaternion);
        XMVECTOR q2 = XMLoadFloat4(&other.quaternion);
        float dot = XMVectorGetX(XMQuaternionDot(q1, q2));
        return std::abs(std::abs(dot) - 1.0f) < FLOAT_EPSILON;
    }

    bool operator!=(const ORotation& other) const {
        return !( *this == other );
    }

    // 回転関連機能
    ORotation Normalize() const {
        XMVECTOR q = XMLoadFloat4(&quaternion);
        XMVECTOR normalized = XMQuaternionNormalize(q);
        ORotation result;
        XMStoreFloat4(&result.quaternion, normalized);
        result.InvalidateEulerCache();
        return result;
    }

    void NormalizeInPlace() {
        XMVECTOR q = XMLoadFloat4(&quaternion);
        XMVECTOR normalized = XMQuaternionNormalize(q);
        XMStoreFloat4(&quaternion, normalized);
        InvalidateEulerCache();
    }

    ORotation Inverse() const {
        XMVECTOR q = XMLoadFloat4(&quaternion);
        XMVECTOR inverted = XMQuaternionInverse(q);
        ORotation result;
        XMStoreFloat4(&result.quaternion, inverted);
        result.InvalidateEulerCache();
        return result;
    }

    XMVECTOR ToQuaternionVector() const {
        return XMLoadFloat4(&quaternion);
    }

    void SetQuaternionVector(XMVECTOR quat) {
        XMVECTOR normalized = XMQuaternionNormalize(quat);
        XMStoreFloat4(&quaternion, normalized);
        InvalidateEulerCache();
    }

    XMMATRIX ToMatrix() const {
        return XMMatrixRotationQuaternion(XMLoadFloat4(&quaternion));
    }

    OPosition RotateVector(const OPosition& vec) const {
        if ( std::abs(qw - 1.0f) < FLOAT_EPSILON && std::abs(qx) < FLOAT_EPSILON &&
            std::abs(qy) < FLOAT_EPSILON && std::abs(qz) < FLOAT_EPSILON ) {
            return vec;
        }

        XMVECTOR q = XMLoadFloat4(&quaternion);
        float length = XMVectorGetX(XMVector4Length(q));
        if ( length < FLOAT_EPSILON ) {
            return vec;
        }

        q = XMQuaternionNormalize(q);
        XMVECTOR v = vec.ToXMVector();
        XMVECTOR rotatedV = XMVector3Rotate(v, q);
        return OPosition::FromXMVector(rotatedV);
    }

    OPosition GetForwardVector() const {
        return RotateVector(OPosition::Forward());
    }

    OPosition GetRightVector() const {
        return RotateVector(OPosition::Right());
    }

    OPosition GetUpVector() const {
        return RotateVector(OPosition::Up());
    }

    // 角度の正規化
    void NormalizeAngles() {
        XMFLOAT3 euler = EulerAngles;

        auto normalizeAngle = [] (float angle) {
            while ( angle > XM_PI ) angle -= XM_2PI;
            while ( angle < -XM_PI ) angle += XM_2PI;
            return angle;
            };

        euler.x = normalizeAngle(euler.x);
        euler.y = normalizeAngle(euler.y);
        euler.z = normalizeAngle(euler.z);

        EulerAngles = euler;
    }

    // 2つのベクトル間の回転を作成
    static ORotation FromToRotation(const OPosition& from, const OPosition& to) {
        XMVECTOR fromVec = XMVector3Normalize(from.ToXMVector());
        XMVECTOR toVec = XMVector3Normalize(to.ToXMVector());

        float dot = XMVectorGetX(XMVector3Dot(fromVec, toVec));

        if ( dot >= 1.0f - FLOAT_EPSILON ) {
            return Identity();
        }

        if ( dot <= -1.0f + FLOAT_EPSILON ) {
            XMVECTOR axis = XMVector3Cross(fromVec, XMVectorSet(1, 0, 0, 0));
            if ( XMVectorGetX(XMVector3Length(axis)) < FLOAT_EPSILON ) {
                axis = XMVector3Cross(fromVec, XMVectorSet(0, 1, 0, 0));
            }
            axis = XMVector3Normalize(axis);
            XMVECTOR quat = XMQuaternionRotationAxis(axis, XM_PI);
            return FromQuaternionVector(quat);
        }

        XMVECTOR axis = XMVector3Cross(fromVec, toVec);
        float w = sqrtf(2.0f * ( 1.0f + dot ));
        XMVECTOR quat = XMVectorSet(
            XMVectorGetX(axis) / w,
            XMVectorGetY(axis) / w,
            XMVectorGetZ(axis) / w,
            w * 0.5f
        );
        quat = XMQuaternionNormalize(quat);
        return FromQuaternionVector(quat);
    }

    static ORotation FromMatrix(CXMMATRIX matrix) {
        XMVECTOR scale, rotation, translation;
        XMMatrixDecompose(&scale, &rotation, &translation, matrix);
        return FromQuaternionVector(rotation);
    }

    static ORotation FromEulerDegrees(float pitchDeg, float yawDeg, float rollDeg) {
        constexpr float degToRad = XM_PI / 180.0f;
        return FromEuler(pitchDeg * degToRad, yawDeg * degToRad, rollDeg * degToRad);
    }

    static ORotation FromEulerDegrees(const XMFLOAT3& eulerDegrees) {
        return FromEulerDegrees(eulerDegrees.x, eulerDegrees.y, eulerDegrees.z);
    }

    static ORotation LookAt(const OPosition& forward, const OPosition& up = OPosition::Up()) {
        XMVECTOR f = XMVector3Normalize(forward.ToXMVector());
        XMVECTOR u = XMVector3Normalize(up.ToXMVector());
        XMVECTOR r = XMVector3Cross(u, f);
        u = XMVector3Cross(f, r);

        XMMATRIX lookAtMatrix = XMMATRIX(
            r,
            u,
            f,
            XMVectorSet(0, 0, 0, 1)
        );

        return FromMatrix(lookAtMatrix);
    }

    // 静的ファクトリメソッド
    static ORotation FromQuaternionVector(XMVECTOR quat) {
        ORotation result;
        XMVECTOR normalized = XMQuaternionNormalize(quat);
        XMStoreFloat4(&result.quaternion, normalized);
        result.InvalidateEulerCache();
        return result;
    }

    static ORotation FromEuler(float pitch, float yaw, float roll) {
        return ORotation(pitch, yaw, roll);
    }

    static ORotation FromEuler(const XMFLOAT3& euler) {
        return ORotation(euler);
    }

    static ORotation AxisAngle(const OPosition& axis, float angle) {
        XMVECTOR axisVector = XMVector3Normalize(axis.ToXMVector());
        XMVECTOR quat = XMQuaternionRotationAxis(axisVector, angle);
        return FromQuaternionVector(quat);
    }

    static ORotation Slerp(const ORotation& a, const ORotation& b, float t) {
        XMVECTOR qa = XMLoadFloat4(&a.quaternion);
        XMVECTOR qb = XMLoadFloat4(&b.quaternion);
        XMVECTOR result = XMQuaternionSlerp(qa, qb, t);
        return FromQuaternionVector(result);
    }

    static const ORotation Identity() { return ORotation(); }

    XMVECTOR ToXMVector() const {
        return XMLoadFloat4(&quaternion);
    }

    static ORotation FromXMVector(XMVECTOR vec) {
        return FromQuaternionVector(vec);
    }
};

inline ORotation operator*(float scalar, const ORotation& rot) {
    return rot * scalar;
}