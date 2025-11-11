#pragma once
#include "GameObject.h"

struct Transform2D {
	XMFLOAT2 Position{};
	XMFLOAT2 Scale{ 1.0f,1.0f };
	XMFLOAT2 Rotation{};
};
class GameObject2D :public GameObject {
	virtual void Init()override = 0;
	virtual void Update() override = 0;
	virtual void Draw() override = 0;
	virtual void UnInit() override = 0;
	XMFLOAT2 GetPosition() { return m_Transform.Position; }

	void SetPosition(XMFLOAT2 pos) { m_Transform.Position = pos; };
protected:
	Transform2D m_Transform;
};