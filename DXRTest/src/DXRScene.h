#pragma once

#include "Scene.h"
#include "DXRRenderer.h"
#include "DXRGameObjects.h"

// CPU���C�g����Scene�ɑΉ�����DXR�V�[��
class DXRScene : public Scene {
public:
    DXRScene();
    virtual ~DXRScene() = default;

    virtual void Init() override;
    virtual void UnInit() override;
    virtual void Update() override;
    virtual void Draw() override;

    // DXR�p���\�b�h
    void InitDXR(DXRRenderer* renderer);
    void BuildAccelerationStructures();
    void UpdateCamera();

    // �V�[���f�[�^�擾
    const std::vector<DXRMaterial>& GetMaterials() const { return m_materials; }
    const std::vector<DXRInstance>& GetInstances() const { return m_instances; }
    const DXRCameraData& GetCamera() const { return m_camera; }
    const DXRGlobalConstants& GetGlobalConstants() const { return m_globalConstants; }

    // DXR�I�u�W�F�N�g��p�ǉ����\�b�h
    template<typename T = DXRShape, typename... Args>
    T* AddDXRGameObject(Layer layer, Args&&... args) {
        T* obj = new T(std::forward<Args>(args)...);
        obj->Init();
        m_GameObject[layer].push_back(obj);

        // DXRShape�̏ꍇ�͊Ǘ����X�g�ɒǉ�
        DXRShape* shape = dynamic_cast<DXRShape*>( obj );
        if ( shape ) {
            m_dxrShapes.push_back(shape);
        }

        return obj;
    }

protected:
    // �R�[�l���{�b�N�X�V�[���\�z
    virtual void BuildCornellBoxScene();

    // �}�e���A���쐬
    void CreateMaterials();

    // �C���X�^���X�X�V
    void UpdateInstances();

    // �J�����X�V
    void UpdateCameraData();

protected:
    DXRRenderer* m_dxrRenderer;

    // DXR�V�[���f�[�^
    std::vector<DXRMaterial> m_materials;
    std::vector<DXRInstance> m_instances;
    DXRCameraData m_camera;
    DXRGlobalConstants m_globalConstants;

    // DXR�I�u�W�F�N�g�Ǘ�
    std::vector<DXRShape*> m_dxrShapes;

    // �J�����p�����[�^
    XMFLOAT3 m_cameraPosition;
    XMFLOAT3 m_cameraTarget;
    XMFLOAT3 m_cameraUp;
    float m_fov;
    float m_aspect;

    // �����_�����O�p�����[�^
    int m_maxDepth;
    int m_sampleCount;
    XMFLOAT4 m_backgroundColor;
    int m_frameIndex;
};

// ��̓I�ȃR�[�l���{�b�N�X�V�[������
class CornellBoxDXRScene : public DXRScene {
public:
    CornellBoxDXRScene();
    virtual ~CornellBoxDXRScene() = default;

    virtual void Init() override;

private:
    virtual void BuildCornellBoxScene() override;
};