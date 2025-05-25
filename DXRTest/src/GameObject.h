#pragma once
struct OPosition {
	union {
		XMFLOAT3 value;
		struct {
			float x, y, z;
		};
	};

	OPosition() noexcept = default;
	constexpr OPosition(XMFLOAT3 pos) noexcept :value(pos) {}

	// NOTE: à¯êîÇ™ÉÅÉìÉoïœêîÇ∆îÌÇÈÇÃÇ≈_ïtÇØÇƒÇ‹Ç∑
	constexpr OPosition(float _x, float _y, float _z) noexcept : x(_x), y(_y), z(_z) {}
	explicit OPosition(_In_reads_(3) const float* pArray) noexcept : x(pArray[0]), y(pArray[1]), z(pArray[2]) {}

	operator XMFLOAT3() const { return value; }
	operator XMFLOAT3*() { return &value; }
    
};

struct OScale {
	union {
		XMFLOAT3 value;
		struct {
			float x, y, z;
		};
	};

	OScale() noexcept = default;
	constexpr OScale(XMFLOAT3 pos) noexcept :value(pos) {}

	constexpr OScale(float _x, float _y, float _z) noexcept : x(_x), y(_y), z(_z) {}
	explicit OScale(_In_reads_(3) const float* pArray) noexcept : x(pArray[0]), y(pArray[1]), z(pArray[2]) {}

	operator XMFLOAT3() const { return value; }
	operator XMFLOAT3* () { return &value; }
};

struct ORotation {
	union {
		XMFLOAT3 value;
		struct {
			float x, y, z;
		};
	};

	ORotation() noexcept = default;
	constexpr ORotation(XMFLOAT3 rot) noexcept :value(rot) {}

	constexpr ORotation(float _x, float _y, float _z) noexcept : x(_x), y(_y), z(_z) {}
	explicit ORotation(_In_reads_(3) const float* pArray) noexcept : x(pArray[0]), y(pArray[1]), z(pArray[2]) {}

	operator XMFLOAT3() const { return value; }
	operator XMFLOAT3* () { return &value; }
};
struct Transform
{
	OPosition Position{};
	OScale Scale{};
	ORotation Rotation{};

	Transform() = default;
	explicit Transform(OPosition pos, OScale scale, ORotation rot ) noexcept : Position(pos), Scale(scale), Rotation(rot) {}
	explicit Transform(_In_reads_(3) const XMFLOAT3* pArray) noexcept : Position(pArray[0]), Scale(pArray[1]), Rotation(pArray[2]) {}
};

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
		rotationMatrix = XMMatrixRotationRollPitchYaw(m_Transform.Rotation.x, m_Transform.Rotation.x, m_Transform.Rotation.x);

		XMFLOAT3 forward;
		XMStoreFloat3(&forward, rotationMatrix.r[2]);
		return forward;
	}
protected:
	Transform m_Transform;

private:
	bool m_isDestroy = false;
};