#pragma once

class GameObject {
public:
	GameObject() = default;
	virtual ~GameObject() = default;

	virtual void Init() = 0;
	virtual void Update() = 0;
	virtual void Draw() = 0;
	virtual void UnInit() = 0;

	bool GetIsDestroy() const { return m_isDestroy; }
	void SetDestroy(bool destroy) { m_isDestroy = destroy; }

private:
	bool m_isDestroy = false;
};