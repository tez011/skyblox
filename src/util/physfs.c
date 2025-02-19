#include <physfs.h>
#include <SDL.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "stb_image.h"

void *read_physfs_file(const char *path, size_t *length)
{
	if (PHYSFS_exists(path) == 0)
		return NULL;

	if (length)
		*length = 0;

	PHYSFS_file *f = PHYSFS_openRead(path);
	PHYSFS_sint64 fsz = PHYSFS_fileLength(f);
	char *s = NULL;
	if (fsz < 0)
		goto done;
	if ((s = malloc(fsz + (length == NULL ? 1 : 0))) == NULL)
		goto done;
	if (PHYSFS_readBytes(f, s, fsz) == -1) {
		free(s);
		s = NULL;
	} else if (length)
		*length = fsz;
	else
		s[fsz] = 0;
done:
	PHYSFS_close(f);
	return s;
}

static int stbi_physfs_read_cb(void *user, char *data, int size)
{
	return PHYSFS_readBytes(user, data, size);
}

static void stbi_physfs_skip_cb(void *user, int n)
{
	PHYSFS_sint64 fpos = PHYSFS_tell(user);
	PHYSFS_seek(user, fpos + n);
}

static int stbi_physfs_eof_cb(void *user)
{
	return PHYSFS_eof(user);
}

static const stbi_io_callbacks stbi_physfs = { .read = stbi_physfs_read_cb, .skip = stbi_physfs_skip_cb, .eof = stbi_physfs_eof_cb };

SDL_Surface *load_physfs_image(const char *path)
{
	if (PHYSFS_exists(path) == 0)
		return NULL;

	int width, height;
	PHYSFS_file *f = PHYSFS_openRead(path);
	stbi_uc *img_data = NULL;
	SDL_Surface *surf0 = NULL, *surf = NULL;

	img_data = stbi_load_from_callbacks(&stbi_physfs, f, &width, &height, NULL, STBI_rgb_alpha);
	PHYSFS_close(f);
	if (img_data == NULL) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load image %s\n", path);
		goto done;
	}

	surf0 = SDL_CreateRGBSurfaceWithFormatFrom(img_data, width, height, 32, 4 * width, SDL_PIXELFORMAT_RGBA32);
	if (surf0 == NULL) {
		SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Failed to create pixel buffer for image %s\n", path);
		goto done;
	}

	surf = SDL_CreateRGBSurface(0, width, height, 32, 0xFF << 24, 0xFF << 16, 0xFF << 8, 0xFF << 0);
	if (surf)
		SDL_BlitSurface(surf0, NULL, surf, NULL);

done:
	if (surf0)
		SDL_FreeSurface(surf0);
	if (img_data)
		stbi_image_free(img_data);

	return surf;
}
