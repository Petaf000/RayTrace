#pragma once
#include "GameObject.h"

#include "Transform.h"

class GameObject3D :public GameObject {
public:
	GameObject3D() {};
	GameObject3D(OPosition pos) { m_Transform.Position = pos; };
	GameObject3D(OScale scale) { m_Transform.Scale = scale; };
	GameObject3D(ORotation rotation) { m_Transform.Rotation = rotation; };
	GameObject3D(OPosition pos, OScale scale) { m_Transform.Position = pos; m_Transform.Scale = scale; };
	GameObject3D(OPosition pos, ORotation rotation) { m_Transform.Position = pos; m_Transform.Rotation = rotation; };
	GameObject3D(OScale scale, ORotation rotation) { m_Transform.Scale = scale; m_Transform.Rotation = rotation; };
	GameObject3D(OPosition pos, OScale scale, ORotation rotation) :m_Transform(pos, scale, rotation) {};
	GameObject3D(Transform transform) :m_Transform(transform) {};
	virtual ~GameObject3D() = default;

	virtual void Init() = 0;
	virtual void UnInit() = 0;
	virtual void Draw() = 0;
	virtual void Update() = 0;

	XMFLOAT3 GetPosition() const { return m_Transform.Position; }
	void SetPosition(const XMFLOAT3& pos) { m_Transform.Position = pos; }

	XMFLOAT3 GetScale() const { return m_Transform.Scale; }
	void SetScale(const XMFLOAT3& scale) { m_Transform.Scale = scale; }

	XMFLOAT3 GetRotation() const { return m_Transform.Rotation; }
	void SetRotation(const XMFLOAT3& rot) { m_Transform.Rotation = rot; }

	XMFLOAT3 GetForward() {
		XMMATRIX rotationMatrix;
		rotationMatrix = XMMatrixRotationRollPitchYaw(m_Transform.Rotation.EulerAngles.x, m_Transform.Rotation.EulerAngles.x, m_Transform.Rotation.EulerAngles.x);

		XMFLOAT3 forward;
		XMStoreFloat3(&forward, rotationMatrix.r[2]);
		return forward;
	}

	XMMATRIX GetWorldMatrix() const {
		XMMATRIX scaleMatrix = XMMatrixScaling(m_Transform.Scale.x, m_Transform.Scale.y, m_Transform.Scale.z);
		XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(m_Transform.Rotation.EulerAngles.x, m_Transform.Rotation.EulerAngles.y, m_Transform.Rotation.EulerAngles.z);
		XMMATRIX translationMatrix = XMMatrixTranslation(m_Transform.Position.x, m_Transform.Position.y, m_Transform.Position.z);

		return scaleMatrix * rotationMatrix * translationMatrix;
	};
protected:
	Transform m_Transform;
};