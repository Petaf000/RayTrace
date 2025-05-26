#pragma once
#include "GameObjectUtils.h"

struct OPosition : Float3Base<OPosition> {
    // �p���R���X�g���N�^
    using Float3Base::Float3Base;

    DEFINE_FLOAT3_ARITHMETIC_OPERATORS(OPosition)

        // Position�ŗL�̋@�\
        OPosition Cross(const OPosition& other) const {
        return OPosition(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }

    // ���ˁi�@���x�N�g���ɑ΂��āj
    OPosition Reflect(const OPosition& normal) const {
        OPosition n = normal.Normalize();
        return *this - n * ( 2.0f * this->Dot(n) );
    }

    // ���e�i�x�N�g���ɑ΂��āj
    OPosition Project(const OPosition& onto) const {
        float dot = this->Dot(onto);
        float lengthSq = onto.LengthSquared();
        if ( lengthSq < FLOAT_EPSILON ) return OPosition::Zero();
        return onto * ( dot / lengthSq );
    }

    // �p�x�v�Z�i���W�A���j
    float AngleTo(const OPosition& other) const {
        OPosition a = this->Normalize();
        OPosition b = other.Normalize();
        float dot = std::clamp(a.Dot(b), -1.0f, 1.0f);
        return acosf(dot);
    }

    // ���ʂւ̓��e�i���ʂ̖@�����w��j
    OPosition ProjectOnPlane(const OPosition& planeNormal) const {
        OPosition n = planeNormal.Normalize();
        return *this - n * this->Dot(n);
    }

    // ���ʐ��`��ԁi�����x�N�g���p�j
    static OPosition Slerp(const OPosition& a, const OPosition& b, float t) {
        OPosition na = a.Normalize();
        OPosition nb = b.Normalize();
        float dot = std::clamp(na.Dot(nb), -1.0f, 1.0f);

        if ( std::abs(dot) >= 1.0f - FLOAT_EPSILON ) {
            return Lerp(na, nb, t);
        }

        float theta = acosf(std::abs(dot));
        float sinTheta = sinf(theta);
        float a_coeff = sinf(( 1.0f - t ) * theta) / sinTheta;
        float b_coeff = sinf(t * theta) / sinTheta;

        if ( dot < 0.0f ) b_coeff = -b_coeff;

        return na * a_coeff + nb * b_coeff;
    }

    static OPosition FromDirection(const OPosition& direction, float distance) {
        return direction.Normalize() * distance;
    }

    // �ÓI�萔
    static const OPosition Zero() { return OPosition(0, 0, 0); }
    static const OPosition One() { return OPosition(1, 1, 1); }
    static const OPosition Forward() { return OPosition(0, 0, 1); }
    static const OPosition Back() { return OPosition(0, 0, -1); }
    static const OPosition Up() { return OPosition(0, 1, 0); }
    static const OPosition Down() { return OPosition(0, -1, 0); }
    static const OPosition Right() { return OPosition(1, 0, 0); }
    static const OPosition Left() { return OPosition(-1, 0, 0); }
};

// �O���[�o�����Z�q
inline OPosition operator*(float scalar, const OPosition& pos) {
    return pos * scalar;
}