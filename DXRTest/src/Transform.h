#pragma once
#include "Position.h"
#include "Scale.h"
#include "Rotation.h"

struct Transform {
    OPosition Position;
    OScale Scale;
    ORotation Rotation;

    Transform() : Position(OPosition::Zero()), Scale(OScale::Identity()), Rotation(ORotation::Identity()) {}

    explicit Transform(OPosition pos, OScale scale, ORotation rot) noexcept
        : Position(pos), Scale(scale), Rotation(rot) {
    }

    explicit Transform(OPosition pos) noexcept
        : Position(pos), Scale(OScale::Identity()), Rotation(ORotation::Identity()) {
    }

    XMMATRIX GetWorldMatrix() const {
        XMMATRIX scaleMatrix = XMMatrixScaling(Scale.x, Scale.y, Scale.z);
        XMMATRIX rotationMatrix = Rotation.ToMatrix();
        XMMATRIX translationMatrix = XMMatrixTranslation(Position.x, Position.y, Position.z);
        return scaleMatrix * rotationMatrix * translationMatrix;
    }

    XMMATRIX GetInverseWorldMatrix() const {
        return XMMatrixInverse(nullptr, GetWorldMatrix());
    }

    static Transform FromMatrix(CXMMATRIX matrix) {
        XMVECTOR scale, rotation, translation;
        XMMatrixDecompose(&scale, &rotation, &translation, matrix);
        Transform result;
        result.Position = OPosition::FromXMVector(translation);
        result.Scale = OScale::FromXMVector(scale);
        result.Rotation = ORotation::FromQuaternionVector(rotation);
        return result;
    }

    // Transform�̍���
    Transform Combine(const Transform& other) const {
        XMMATRIX thisMatrix = GetWorldMatrix();
        XMMATRIX otherMatrix = other.GetWorldMatrix();
        XMMATRIX combined = thisMatrix * otherMatrix;
        return FromMatrix(combined);
    }

    // ����Transform�ithis����other�ւ̕ϊ��j
    Transform GetRelativeTo(const Transform& other) const {
        XMMATRIX thisMatrix = GetWorldMatrix();
        XMMATRIX otherMatrix = other.GetWorldMatrix();
        XMMATRIX relative = thisMatrix * XMMatrixInverse(nullptr, otherMatrix);
        return FromMatrix(relative);
    }

    // �_��ϊ�
    OPosition TransformPoint(const OPosition& point) const {
        XMVECTOR p = point.ToXMVector();
        XMMATRIX worldMatrix = GetWorldMatrix();
        XMVECTOR transformed = XMVector3Transform(p, worldMatrix);
        return OPosition::FromXMVector(transformed);
    }

    // �x�N�g����ϊ��i�ʒu���𖳎��j
    OPosition TransformVector(const OPosition& vector) const {
        XMVECTOR v = vector.ToXMVector();
        XMMATRIX worldMatrix = GetWorldMatrix();
        XMVECTOR transformed = XMVector3TransformNormal(v, worldMatrix);
        return OPosition::FromXMVector(transformed);
    }

    // �t�ϊ�
    Transform Inverse() const {
        return FromMatrix(GetInverseWorldMatrix());
    }

    // ���`���
    static Transform Lerp(const Transform& a, const Transform& b, float t) {
        return Transform(
            OPosition::Lerp(a.Position, b.Position, t),
            OScale::Lerp(a.Scale, b.Scale, t),
            ORotation::Slerp(a.Rotation, b.Rotation, t)
        );
    }

    // �O�����Ɉړ�
    void MoveForward(float distance) {
        OPosition forward = Rotation.GetForwardVector();
        Position += forward * distance;
    }

    // �E�����Ɉړ�
    void MoveRight(float distance) {
        OPosition right = Rotation.GetRightVector();
        Position += right * distance;
    }

    // ������Ɉړ�
    void MoveUp(float distance) {
        OPosition up = Rotation.GetUpVector();
        Position += up * distance;
    }

    // �w�肵������������
    void LookAt(const OPosition& target, const OPosition& up = OPosition::Up()) {
        OPosition direction = ( target - Position ).Normalize();
        Rotation = ORotation::LookAt(direction, up);
    }

    // �w�肵��������ŉ�]
    void RotateAroundAxis(const OPosition& axis, float angle) {
        ORotation axisRotation = ORotation::AxisAngle(axis, angle);
        Rotation *= axisRotation;
    }

    // ���[�J��������ŉ�]
    void RotateLocal(const ORotation& localRotation) {
        Rotation *= localRotation;
    }

    // ���[���h������ŉ�]
    void RotateWorld(const ORotation& worldRotation) {
        Rotation = worldRotation * Rotation;
    }

    // Transform�Ԃ̋���
    float DistanceTo(const Transform& other) const {
        return Position.Distance(other.Position);
    }

    // �w��ʒu�ւ̈ړ��i��ԁj
    void MoveTo(const OPosition& target, float speed) {
        OPosition direction = ( target - Position ).Normalize();
        Position += direction * speed;
    }

    // ���[�J�����W�n�ł̈ړ�
    void MoveLocal(const OPosition& localMovement) {
        OPosition worldMovement = Rotation.RotateVector(localMovement);
        Position += worldMovement;
    }

    // �X�P�[���̈�l�K�p
    void SetUniformScale(float scale) {
        Scale = OScale(scale);
    }

    // ���E�{�b�N�X�ϊ��i8�̊p��ϊ��j
    void TransformBounds(const OPosition& min, const OPosition& max,
        OPosition& outMin, OPosition& outMax) const {
        // 8�̊p���v�Z
        OPosition corners[8] = {
            OPosition(min.x, min.y, min.z),
            OPosition(max.x, min.y, min.z),
            OPosition(min.x, max.y, min.z),
            OPosition(max.x, max.y, min.z),
            OPosition(min.x, min.y, max.z),
            OPosition(max.x, min.y, max.z),
            OPosition(min.x, max.y, max.z),
            OPosition(max.x, max.y, max.z)
        };

        // �ŏ��̊p��ϊ����ď����l�Ƃ���
        OPosition transformed = TransformPoint(corners[0]);
        outMin = outMax = transformed;

        // �c��̊p��ϊ����čŏ��E�ő���X�V
        for ( int i = 1; i < 8; ++i ) {
            transformed = TransformPoint(corners[i]);
            outMin.x = std::min<float>(outMin.x, transformed.x);
            outMin.y = std::min<float>(outMin.y, transformed.y);
            outMin.z = std::min<float>(outMin.z, transformed.z);
            outMax.x = std::max<float>(outMax.x, transformed.x);
            outMax.y = std::max<float>(outMax.y, transformed.y);
            outMax.z = std::max<float>(outMax.z, transformed.z);
        }
    }

    // ��荂�x�ȕ�ԁi�X���[�Y�X�e�b�v�j
    static Transform SmoothStep(const Transform& a, const Transform& b, float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        t = t * t * ( 3.0f - 2.0f * t );
        return Transform(
            OPosition::Lerp(a.Position, b.Position, t),
            OScale::Lerp(a.Scale, b.Scale, t),
            ORotation::Slerp(a.Rotation, b.Rotation, t)
        );
    }

    bool operator==(const Transform& other) const {
        return Position == other.Position &&
            Scale == other.Scale &&
            Rotation == other.Rotation;
    }

    bool operator!=(const Transform& other) const {
        return !( *this == other );
    }

    static const Transform Identity() {
        return Transform(OPosition::Zero(), OScale::Identity(), ORotation::Identity());
    }
};