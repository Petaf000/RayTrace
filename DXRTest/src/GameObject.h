#pragma once
#include "Transform.h"

class GameObject
{
public:
	GameObject() {};
	GameObject(OPosition pos) { m_Transform.Position = pos; };
	GameObject(OScale scale) { m_Transform.Scale = scale; };
	GameObject(ORotation rotation) { m_Transform.Rotation = rotation; };
	GameObject(OPosition pos, OScale scale) { m_Transform.Position = pos; m_Transform.Scale = scale; };
	GameObject(OPosition pos, ORotation rotation) { m_Transform.Position = pos; m_Transform.Rotation = rotation; };
	GameObject(OScale scale, ORotation rotation) { m_Transform.Scale = scale; m_Transform.Rotation = rotation; };
	GameObject(OPosition pos, OScale scale, ORotation rotation) :m_Transform(pos, scale, rotation) {};
	GameObject(Transform transform):m_Transform(transform) {};
	virtual ~GameObject() = default;
	
	virtual void Init() = 0;
	virtual void UnInit() = 0;
	virtual void Draw() = 0;
	virtual void Update() = 0;

	XMFLOAT3 GetPosition() { return m_Transform.Position; }

	void SetPosition(XMFLOAT3 pos) { m_Transform.Position = pos; };
	void SetScale(XMFLOAT3 scale) { m_Transform.Scale = scale; }

	void DestroyActor() { m_isDestroy = true; };

	bool GetIsDestroy() { return m_isDestroy; }

	XMFLOAT3 GetForward() {
		XMMATRIX rotationMatrix;
		rotationMatrix = XMMatrixRotationRollPitchYaw(m_Transform.Rotation.EulerAngles.x, m_Transform.Rotation.EulerAngles.x, m_Transform.Rotation.EulerAngles.x);

		XMFLOAT3 forward;
		XMStoreFloat3(&forward, rotationMatrix.r[2]);
		return forward;
	}
protected:
	Transform m_Transform;

private:
	bool m_isDestroy = false;
};