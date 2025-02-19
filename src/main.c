#include <assert.h>
#include "blox.h"
#include "ingame.h"
#include "nuklear_custom.h"
#include <physfs.h>
#include "render.h"
#include <stdio.h>
#include <SDL.h>
#include "world.h"

bool doQuit = false;
static SDL_Window *main_window;
static SDL_GLContext gl_ctx;
static struct nk_context *ui_ctx;

static int physfs_init(const char *argv0)
{
	PHYSFS_init(argv0);

	if (PHYSFS_mount(RESOURCE_PATH, "/resources", 0) == 0) {
		fprintf(stderr, "Error mounting resources at %s: %s\n", RESOURCE_PATH, PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
		return 1;
	}

	const char *pref_dir = PHYSFS_getPrefDir("trao1011", "blox");
	if (pref_dir == NULL || PHYSFS_mount(pref_dir, "/save", 0) == 0) {
		fprintf(stderr, "Error mounting saves at %s: %s\n", pref_dir, PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
		return 1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	const int initial_width = INITIAL_SCREEN_WIDTH, initial_height = INITIAL_SCREEN_HEIGHT;
	int rv;

	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		fprintf(stderr, "SDL_Init(SDL_INIT_EVERYTHING): %s\n", SDL_GetError());
		return 1;
	}

	main_window = SDL_CreateWindow("blox", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, initial_width, initial_height,
				       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	if (main_window == NULL) {
		fprintf(stderr, "SDL_CreateWindow(): %s\n", SDL_GetError());
		return 1;
	}

	if ((rv = physfs_init(argv[0])) != 0)
		return rv;
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	if ((gl_ctx = SDL_GL_CreateContext(main_window)) == NULL) {
		fprintf(stderr, "SDL_GL_CreateContext(): Error: %s\n", SDL_GetError());
		return 1;
	}
	if (gl3wInit() || !gl3wIsSupported(3, 3)) {
		fprintf(stderr, "Can't initialize OpenGL 3.3\n");
		return 1;
	}

	printf("Renderer: %s by %s\nOpenGL version %s with SL %s\n", glGetString(GL_RENDERER), glGetString(GL_VENDOR),
	       glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));

	UNUSED(argc);
	world_init();
	assert(ingame_init(initial_width, initial_height));

	/* Enable OpenGL features. */
	glBlendEquation(GL_FUNC_ADD);
	glEnable(GL_MULTISAMPLE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glFrontFace(GL_CCW);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, initial_width, initial_height);
	SDL_GL_SwapWindow(main_window);
	ui_ctx = nk_sdl_init(main_window);

	struct nk_font_atlas *atlas;
	nk_sdl_font_stash_begin(&atlas);
	nk_sdl_font_stash_end();

	Uint32 last_time = 0;
	int delta_ms = 0;
	while (!doQuit) {
		SDL_Event evt;
		Uint32 now_time = SDL_GetTicks();
		delta_ms = now_time - last_time;
		last_time = now_time;

		nk_input_begin(ui_ctx);
		while (SDL_PollEvent(&evt)) {
			if (evt.type == SDL_QUIT || (evt.type == SDL_WINDOWEVENT && evt.window.event == SDL_WINDOWEVENT_CLOSE && evt.window.windowID == SDL_GetWindowID(main_window)))
				doQuit = true;
			else if (evt.type == SDL_WINDOWEVENT && evt.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
				render_viewport_change(evt.window.data1, evt.window.data2);
			ingame_handle_input(&evt);
			nk_sdl_handle_event(&evt);
		}
		nk_input_end(ui_ctx);

		ingame_logic(delta_ms);
		render_main(main_window, ui_ctx);

		SDL_Delay(MAX((int32_t)(8 - delta_ms), 1));
	}

	nk_sdl_shutdown();
	SDL_GL_DeleteContext(gl_ctx);
	SDL_DestroyWindow(main_window);
	PHYSFS_deinit();
	SDL_Quit();

	return 0;
}
