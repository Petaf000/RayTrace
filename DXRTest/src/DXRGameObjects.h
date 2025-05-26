#pragma once

#include "GameObject3D.h"
#include "DXRRenderer.h"

// CPUレイトレのShapeに対応するDXRオブジェクト基底クラス
class DXRShape : public GameObject3D {
public:
    DXRShape() = default;
    virtual ~DXRShape() = default;

    virtual void Init() override = 0;
    virtual void Update() override = 0;
    virtual void Draw() override = 0;
    virtual void UnInit() override = 0;

    // DXR用メソッド
    virtual void BuildBLAS(ID3D12Device5* device) = 0;
    virtual const std::vector<DXRVertex>& GetVertices() const = 0;
    virtual const std::vector<uint32_t>& GetIndices() const = 0;
    virtual int GetMaterialIndex() const { return m_materialIndex; }
    virtual void SetMaterialIndex(int index) { m_materialIndex = index; }

    ComPtr<ID3D12Resource> GetBLAS() const { return m_bottomLevelAS; }

protected:
    ComPtr<ID3D12Resource> m_bottomLevelAS;
    ComPtr<ID3D12Resource> m_vertexBuffer;
    ComPtr<ID3D12Resource> m_indexBuffer;
    std::vector<DXRVertex> m_vertices;
    std::vector<uint32_t> m_indices;
    int m_materialIndex = 0;

    // BLAS構築ヘルパー
    bool CreateBLASBuffers(ID3D12Device5* device);
};

// DXRSphere - CPUレイトレのSphereに対応
class DXRSphere : public DXRShape {
public:
    DXRSphere(float radius = 1.0f, int segments = 32);
    virtual ~DXRSphere() = default;

    virtual void Init() override;
    virtual void Update() override;
    virtual void Draw() override;
    virtual void UnInit() override;

    virtual void BuildBLAS(ID3D12Device5* device) override;
    virtual const std::vector<DXRVertex>& GetVertices() const override { return m_vertices; }
    virtual const std::vector<uint32_t>& GetIndices() const override { return m_indices; }

    void SetRadius(float radius) { m_radius = radius; CreateSphereGeometry(); }
    float GetRadius() const { return m_radius; }

private:
    void CreateSphereGeometry();

    float m_radius;
    int m_segments;
};

// DXRBox - CPUレイトレのBoxに対応
class DXRBox : public DXRShape {
public:
    DXRBox(const XMFLOAT3& size = XMFLOAT3(1.0f, 1.0f, 1.0f));
    virtual ~DXRBox() = default;

    virtual void Init() override;
    virtual void Update() override;
    virtual void Draw() override;
    virtual void UnInit() override;

    virtual void BuildBLAS(ID3D12Device5* device) override;
    virtual const std::vector<DXRVertex>& GetVertices() const override { return m_vertices; }
    virtual const std::vector<uint32_t>& GetIndices() const override { return m_indices; }

    void SetSize(const XMFLOAT3& size) { m_size = size; CreateBoxGeometry(); }
    XMFLOAT3 GetSize() const { return m_size; }

private:
    void CreateBoxGeometry();

    XMFLOAT3 m_size;
};

// DXRRect - CPUレイトレのRectに対応  
class DXRRect : public DXRShape {
public:
    enum AxisType {
        kXY = 0,
        kXZ,
        kYZ
    };

    DXRRect(float x0, float x1, float y0, float y1, float k, AxisType axis);
    virtual ~DXRRect() = default;

    virtual void Init() override;
    virtual void Update() override;
    virtual void Draw() override;
    virtual void UnInit() override;

    virtual void BuildBLAS(ID3D12Device5* device) override;
    virtual const std::vector<DXRVertex>& GetVertices() const override { return m_vertices; }
    virtual const std::vector<uint32_t>& GetIndices() const override { return m_indices; }

private:
    void CreateRectGeometry();

    float m_x0, m_x1, m_y0, m_y1, m_k;
    AxisType m_axis;
};

// DXRLight - レイトレ用のライト
class DXRLight : public DXRRect {
public:
    DXRLight(float x0, float x1, float y0, float y1, float k, AxisType axis, const XMFLOAT3& emission);
    virtual ~DXRLight() = default;

    virtual void Init() override;
    void SetEmission(const XMFLOAT3& emission) { m_emission = emission; }
    XMFLOAT3 GetEmission() const { return m_emission; }

private:
    XMFLOAT3 m_emission;
};