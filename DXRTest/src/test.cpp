#include "Transform.h"
int main(void) {
	Transform player;
	player.MoveForward(5.0f);                    // 前進
	player.Rotation.Yaw+(XM_PIDIV4);           // ヨー回転追加
	player.Position += player.Rotation.GetRightVector() * 2.0f;  // 右移動

	// スケール操作
	OScale scale(2.0f);
	scale.MakeUniformFromMax();                  // 最大値で均等化

	// 回転操作
	ORotation rot = ORotation::FromEuler(45, 90, 0);  // 度数法から作成
	rot *= rot.AxisAngle(OPosition::Up(), XM_PI / 6.0f);  // 30度 Y軸周り回転
}