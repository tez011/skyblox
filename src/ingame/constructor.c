#include "ingame.h"
#include "render.h"
#include <string.h>
#include "util.h"

struct ingame_data_s igdt;
int chunk_render_radius = 1;

bool ingame_init(int initial_width, int initial_height)
{
	memset(&igdt, 0, sizeof(struct ingame_data_s));
	igdt.loc[0] = 5;
	igdt.loc[1] = 5;
	igdt.loc[2] = 5;
	igdt.held_block = 1;
	igdt.time_of_day = 7;
	memset(igdt.sun, 0, sizeof(igdt.sun));
	memset(igdt.moon, 0, sizeof(igdt.moon));

	return render_init(initial_width, initial_height);
}

inline void ingame_deinit(void)
{
	render_deinit();
}
