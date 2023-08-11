#include <SDL.h>
#include <memory>
#include "SDL_events.h"
#include "application.h"
#include "window.h"
#include "clock.h"
#include "cube.h"

int main(int argc, char* argv[])
{
    Application::Create<CubeApp>();

    std::shared_ptr<Window> win = Application::Get().CreateRenderWindow("Cube", 1280, 720);
    Application::Get().Run();
    Application::Destroy();

    return 0;
}
