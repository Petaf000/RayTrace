#pragma once
#include <list>
#include "GameObject.h"

enum Layer {
	PreLayer,
	Gameobject3D,
	Gameobject2D,
	LayerAll,
};

class Scene {
public:
	Scene() {};

	virtual ~Scene() = default;

	template<typename T = GameObject >
	T* AddGameObject(Layer layer) {
		T* tmp = new T();
		tmp->Init();
		m_GameObject[layer].push_back(tmp);
		return tmp;
	};

	template<typename T = GameObject, typename... Args >
	T* AddGameObject(Layer layer, Args&&... args) {
		T* tmp = new T(std::forward<Args>(args)...);
		tmp->Init();
		m_GameObject[layer].push_back(tmp);
		return tmp;
	};

	template<typename T>
	T* GetGameObject() {
		for ( int i = 0; i < Layer::LayerAll; i++ ) {
			for ( auto& obj : m_GameObject[i] ) {
				T* ret = dynamic_cast<T*>( obj );

				if ( ret != nullptr )
					return ret;
			}
		}
		return nullptr;
	}

	template<typename T>
	std::vector<T*> GetGameObjects() {
		std::vector<T*> tmp;
		for ( int i = 0; i < Layer::LayerAll; i++ ) {
			for ( auto& obj : m_GameObject[i] ) {
				T* ret = dynamic_cast<T*>( obj );

				if ( ret != nullptr )
					tmp.push_back(ret);
			}
		}
		return tmp;
	}

	virtual void Init() = 0;
	virtual void UnInit() {
		for ( int i = 0; i < Layer::LayerAll; i++ ) {
			for ( auto obj : m_GameObject[i] ) {
				obj->UnInit();
				delete obj;
			}
		}
	};

	virtual void Update() {
		for ( int i = 0; i < Layer::LayerAll; i++ ) {
			for ( auto obj : m_GameObject[i] ) {
				obj->Update();
			}
			m_GameObject[i].remove_if([] (GameObject* obj) {return obj->GetIsDestroy(); });
		}
	};
	virtual void Draw() {
		for ( int i = 0; i < Layer::LayerAll; i++ ) {
			for ( auto obj : m_GameObject[i] ) {
				obj->Draw();
			}
		}
	};
protected:
	std::list<GameObject*> m_GameObject[Layer::LayerAll];
};