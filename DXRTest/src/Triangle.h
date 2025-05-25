#pragma once
#include <DirectXMath.h>

using namespace DirectX;

// 三角形構造体（3つの頂点で構成）
struct Triangle {
    XMFLOAT3 vertices[3];
    XMFLOAT3 normal;

    Triangle(const XMFLOAT3& v0, const XMFLOAT3& v1, const XMFLOAT3& v2) {
        vertices[0] = v0;
        vertices[1] = v1;
        vertices[2] = v2;
        CalculateNormal();
    }

    void CalculateNormal() {
        XMVECTOR v0 = XMLoadFloat3(&vertices[0]);
        XMVECTOR v1 = XMLoadFloat3(&vertices[1]);
        XMVECTOR v2 = XMLoadFloat3(&vertices[2]);

        XMVECTOR edge1 = XMVectorSubtract(v1, v0);
        XMVECTOR edge2 = XMVectorSubtract(v2, v0);
        XMVECTOR normalVec = XMVector3Normalize(XMVector3Cross(edge1, edge2));

        XMStoreFloat3(&normal, normalVec);
    }
};