#pragma once

// マテリアルタイプの列挙型
enum class MaterialType {
    Lambert,    // 拡散反射（ランバート反射）
    Metal,      // 金属（鏡面反射）
    Dielectric, // 誘電体（透明/屈折）
    Emissive    // 発光体
};