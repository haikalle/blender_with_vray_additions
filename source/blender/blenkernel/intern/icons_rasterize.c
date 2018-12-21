/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/blenkernel/intern/icons_rasterize.c
 *  \ingroup bke
 */
#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_bitmap_draw_2d.h"
#include "BLI_math_geom.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BKE_icons.h"

#include "BLI_strict_flags.h"

struct UserRasterInfo {
	int pt[3][2];
	const uint *color;
	/* only for smooth shading */
	struct {
		float pt_fl[3][2];
		uint color_u[3][4];
	} smooth;
	int   rect_size[2];
	uint *rect;
};

static void tri_fill_flat(int x, int x_end, int y, void *user_data)
{
	struct UserRasterInfo *data = user_data;
	uint *p = &data->rect[(y * data->rect_size[1]) + x];
	uint col = data->color[0];
	while (x++ != x_end) {
		*p++ = col;
	}
}

static void tri_fill_smooth(int x, int x_end, int y, void *user_data)
{
	struct UserRasterInfo *data = user_data;
	uint *p = &data->rect[(y * data->rect_size[1]) + x];
	float pt_step_fl[2] = {(float)x, (float)y};
	while (x++ != x_end) {
		float w[3];
		barycentric_weights_v2_clamped(UNPACK3(data->smooth.pt_fl), pt_step_fl, w);

		uint col_u[4] = {0, 0, 0, 0};
		for (uint corner = 0; corner < 3; corner++) {
			for (uint chan = 0; chan < 4; chan++) {
				col_u[chan] += data->smooth.color_u[corner][chan] * (uint)(w[corner] * 255.0f);
			}
		}
		union {
			uint  as_u32;
			uchar as_bytes[4];
		} col;
		col.as_bytes[0] = (uchar)(col_u[0] / 255);
		col.as_bytes[1] = (uchar)(col_u[1] / 255);
		col.as_bytes[2] = (uchar)(col_u[2] / 255);
		col.as_bytes[3] = (uchar)(col_u[3] / 255);
		*p++ = col.as_u32;

		pt_step_fl[0] += 1.0f;
	}
}

ImBuf *BKE_icon_geom_rasterize(
        const struct Icon_Geom *geom,
        const unsigned int size_x, const unsigned int size_y)
{
	const int coords_len = geom->coords_len;

	const uchar (*pos)[2] = geom->coords;
	const uint   *col = (void *)geom->colors;

	/* TODO(campbell): Currently rasterizes to fixed size, then scales.
	 * Should rasterize to double size for eg instead. */
	const int rect_size[2] = {max_ii(256, (int)size_x * 2), max_ii(256, (int)size_y * 2)};

	ImBuf *ibuf = IMB_allocImBuf((uint)rect_size[0], (uint)rect_size[1], 32, IB_rect);

	struct UserRasterInfo data;

	data.rect_size[0] = rect_size[0];
	data.rect_size[1] = rect_size[1];

	data.rect = ibuf->rect;

	float scale[2];
	const bool use_scale = (rect_size[0] != 256) || (rect_size[1] != 256);

	if (use_scale) {
		scale[0] = ((float)rect_size[0] / 256.0f);
		scale[1] = ((float)rect_size[1] / 256.0f);
	}

	for (int t = 0; t < coords_len; t += 1, pos += 3, col += 3) {
		if (use_scale) {
			ARRAY_SET_ITEMS(data.pt[0], (int)(pos[0][0] * scale[0]), (int)(pos[0][1] * scale[1]));
			ARRAY_SET_ITEMS(data.pt[1], (int)(pos[1][0] * scale[0]), (int)(pos[1][1] * scale[1]));
			ARRAY_SET_ITEMS(data.pt[2], (int)(pos[2][0] * scale[0]), (int)(pos[2][1] * scale[1]));
		}
		else {
			ARRAY_SET_ITEMS(data.pt[0], UNPACK2(pos[0]));
			ARRAY_SET_ITEMS(data.pt[1], UNPACK2(pos[1]));
			ARRAY_SET_ITEMS(data.pt[2], UNPACK2(pos[2]));
		}
		data.color = col;
		if ((col[0] == col[1]) && (col[0] == col[2])) {
			BLI_bitmap_draw_2d_tri_v2i(UNPACK3(data.pt), tri_fill_flat, &data);
		}
		else {
			ARRAY_SET_ITEMS(data.smooth.pt_fl[0], UNPACK2_EX((float), data.pt[0], ));
			ARRAY_SET_ITEMS(data.smooth.pt_fl[1], UNPACK2_EX((float), data.pt[1], ));
			ARRAY_SET_ITEMS(data.smooth.pt_fl[2], UNPACK2_EX((float), data.pt[2], ));
			ARRAY_SET_ITEMS(data.smooth.color_u[0], UNPACK4_EX((uint), ((uchar *)(col + 0)), ));
			ARRAY_SET_ITEMS(data.smooth.color_u[1], UNPACK4_EX((uint), ((uchar *)(col + 1)), ));
			ARRAY_SET_ITEMS(data.smooth.color_u[2], UNPACK4_EX((uint), ((uchar *)(col + 2)), ));
			BLI_bitmap_draw_2d_tri_v2i(UNPACK3(data.pt), tri_fill_smooth, &data);
		}
	}
	IMB_scaleImBuf(ibuf, size_x, size_y);
	return ibuf;
}
