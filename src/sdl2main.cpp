#include "helpers.h"
#include "renderer1.h"
#include <SDL.h>


int main(int argc, char* args[])
{
    SDL_Window* window =
        SDL_CreateWindow("MainWindow", SDL_WINDOWPOS_CENTERED,
                         SDL_WINDOWPOS_CENTERED, 1280, 720, 0);

	Renderer renderer(window);
    
    while (true)
    {
        bool shouldRender = true;
        SDL_Event event;
        if (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT) break;

			if (event.type = SDL_WINDOWEVENT)
			{
				switch (event.window.event)
				{
                case SDL_WINDOWEVENT_RESIZED:
                    renderer.resize(event.window.data1, event.window.data2);
                    shouldRender = false;
				}
			}
        }
        if (shouldRender)
        {
            renderer.render();
        }
    }

	return 0;
}
