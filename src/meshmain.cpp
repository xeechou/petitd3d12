#include "mesh.h"
#include "window.h"

int main(int argc, char* argv[])
{
    Application::Create<MeshApp>();

    std::shared_ptr<Window> win = Application::Get().CreateRenderWindow("Mesh", 1280, 720);
    Application::Get().Run();
    Application::Destroy();

    return 0;
    return 0;
}
