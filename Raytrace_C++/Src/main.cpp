#include "Scene.h"

#include <Windows.h>

#pragma comment(lib, "winmm.lib")

int main(void) {
    int nx = 800;
    int ny = 400;
    int ns = 50;
    std::unique_ptr<Scene> scene(std::make_unique<Scene>("Output/40_Test.bmp", nx, ny, ns));
    scene->render();

    char command[256] = "start ";
	strcat(command, scene->getFilename());
    std::system(command);
    
    PlaySound(TEXT("Asset/Sound/coin.wav"), NULL, SND_FILENAME);
    return 0;
}