#pragma once
#include "Scene.h"
#include "DXRData.h"
#include "DXRShape.h"

class DXRScene : public Scene {
public:
    DXRScene() = default;
    virtual ~DXRScene() = default;

    virtual void Init() override = 0;
    virtual void UnInit() override;
    virtual void Update() override;
    virtual void Draw() override;

    TLASData GetTLASData() const;
    std::vector<DXRShape*> GetDXRShapes() const;

    struct CameraData {
        XMFLOAT3 position;
        XMFLOAT3 target;
        XMFLOAT3 up;
        float fov;
        float aspect;
    };

    void SetCamera(const CameraData& camera) { m_cameraData = camera; }
    CameraData GetCamera() const { return m_cameraData; }
	const std::vector<DXRMaterialData>& GetUniqueMaterials() const { return m_uniqueMaterials; }
protected:
    CameraData m_cameraData;
    std::vector<DXRMaterialData> m_uniqueMaterials;
};