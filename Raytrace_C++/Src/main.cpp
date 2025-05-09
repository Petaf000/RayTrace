#include "Scene.h"

int main(void) {
    int nx = 800;
    int ny = 400;
    int ns = 100;
    std::unique_ptr<Scene> scene(std::make_unique<Scene>("Output/20_ColorTexture.bmp", nx, ny, ns));
    scene->render();

    char command[256] = "start ";
	strcat(command, scene->getFilename());
    std::system(command);
    return 0;
}