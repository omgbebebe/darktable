/*
    This file is part of darktable,
    copyright (c) 2009--2012 johannes hanika.

    darktable is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    darktable is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with darktable.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
// our includes go first:
#include "common/imageio.h"
#include "common/image_cache.h"
#include "common/tags.h"
#include "common/collection.h"
#include "develop/imageop.h"
#include "bauhaus/bauhaus.h"
#include "gui/gtk.h"

#include <gtk/gtk.h>
#include <stdlib.h>
#include <math.h>

// this is the version of the modules parameters,
// and includes version information about compile-time dt
DT_MODULE_INTROSPECTION(1, dt_iop_clut_params_t)

#define CUBEROOT(X) (int)floor(pow(X,1/3.) + 0.5)
#define CUBE(X) (int)floor(pow(X,3.))
// TODO: some build system to support dt-less compilation and translation!

gboolean is_valid_clut(const dt_image_t*);
int find_cluts(const char*);

typedef struct dt_iop_clut_params_t
{
  // self->params
  int imgid;
} dt_iop_clut_params_t;

typedef struct dt_iop_clut_gui_data_t
{
  // self->gui_data
  GtkWidget *scale; // this is needed by gui_update
} dt_iop_clut_gui_data_t;

typedef struct dt_iop_clut_global_data_t
{
  int clut_size;
  int clut_bpp;
  int clut_level;
  int clut_data_size;
  void *clut_data;
} dt_iop_clut_global_data_t;

typedef struct dt_iop_clut_data_t
{
  // self->data
  int clut_size;
  int clut_bpp;
  int clut_level;
  int clut_data_size;
  void *clut_data;
} dt_iop_clut_data_t;

// this returns a translatable name
const char *name()
{
  // make sure you put all your translatable strings into _() !
  return _("HaldCLUT");
}

// some additional flags (self explanatory i think):
int flags()
{
  return IOP_FLAGS_INCLUDE_IN_STYLES | IOP_FLAGS_SUPPORTS_BLENDING;
}

// where does it appear in the gui?
int groups()
{
  return IOP_GROUP_COLOR;
}

/** process, all real work is done here. */
void process(struct dt_iop_module_t *self, dt_dev_pixelpipe_iop_t *piece, void *i, void *o,
             const dt_iop_roi_t *roi_in, const dt_iop_roi_t *roi_out)
{
  const dt_iop_clut_data_t *const d = (dt_iop_clut_data_t *)piece->data;
  fprintf(stderr,"[process]: level=%d\n", d->clut_level);
  fprintf(stderr,"[process]: size=%d\n", d->clut_size);
  fprintf(stderr,"[process]: bpp=%d\n", d->clut_bpp);
  fprintf(stderr,"[process]: data_size=%d\n", d->clut_data_size);

  const float *clut = (float *)d->clut_data;
  /* for (int i=0; i < d->clut_data_size; i++) */
  /* { */
  /*   if(clut_data[i] != 0) fprintf(stderr,"%d ", clut_data[i]); */
  /* } */
  /* fprintf(stderr,"\n"); */

  const int ch = piece->colors;
  const int width = roi_out->width;
  const int height = roi_out->height;
  int c,r;
  r = c = 0;

#ifdef _OPENMP
#pragma omp parallel for default(none) schedule(static) shared(i, o, roi_in, roi_out, clut)
#endif
  for(int k = 0; k < height; k++)
  {
    float *in = ((float *)i) + (size_t)k * ch * width;
    float *out = ((float *)o) + (size_t)k * ch * width;
//    float *cl = ((float *)clut) + (size_t)k * ch * width;
    
    for(int j = 0; j < width; j++, in += ch, out += ch)
    {
      out[0] = clut[c + r*width];
      out[1] = 0.0;
      out[2] = 0.0;
      c++;
    }
    r++;
  }
//  for(int k = 0; k < 3; k++) piece->pipe->processed_maximum[k] = 1.0f;
}


void commit_params(struct dt_iop_module_t *self, dt_iop_params_t *p1, dt_dev_pixelpipe_t *pipe,
                   dt_dev_pixelpipe_iop_t *piece)
{
  dt_iop_clut_params_t *p = (dt_iop_clut_params_t *)p1;
  if(0)
  {
    p = (dt_iop_clut_params_t *)self->default_params;
    fprintf(stderr,"[commit_params]: imgid=%d\n", p->imgid);
  }
  
  dt_iop_clut_data_t *d = (dt_iop_clut_data_t *)piece->data;
  dt_iop_clut_global_data_t *gd = (dt_iop_clut_global_data_t *)self->data;

  /* gd->clut_bpp = 123; */
  /* gd->clut_data = calloc(64,sizeof(uint8_t)); */
  /* memset(gd->clut_data, 12, 64); */

  d->clut_bpp = gd->clut_bpp;
  d->clut_size = gd->clut_size;
  d->clut_level = gd->clut_level;
  d->clut_data_size = gd->clut_data_size;
  d->clut_data = gd->clut_data;
}

void init_pipe(struct dt_iop_module_t *self, dt_dev_pixelpipe_t *pipe, dt_dev_pixelpipe_iop_t *piece)
{
  piece->data = calloc(1, sizeof(dt_iop_clut_data_t));
  self->commit_params(self, self->default_params, pipe, piece);
}

void cleanup_pipe(struct dt_iop_module_t *self, dt_dev_pixelpipe_t *pipe, dt_dev_pixelpipe_iop_t *piece)
{
//  dt_iop_clut_data_t *d = (dt_iop_clut_data_t *)piece->data;
  free(piece->data);
  piece->data = NULL;
}

void init_global(dt_iop_module_so_t *module)
{
  dt_iop_clut_global_data_t *gd
    = (dt_iop_clut_global_data_t *)calloc(1, sizeof(dt_iop_clut_global_data_t));
  module->data = gd;
}

void reload_defaults(dt_iop_module_t *module)
{
  dt_iop_clut_params_t tmp = { -1 };
  if(!module || !module->dev) goto end;

  dt_iop_clut_global_data_t *gd = (dt_iop_clut_global_data_t *)module->data;
  if (gd->clut_data)
  {
    free(gd->clut_data);
    gd->clut_data = NULL;
  }

  if(!gd) goto end;

  gchar *qin = "select distinct id from images where   (flags & 256) != 256 and ((id in (select imgid from tagged_images as a join tags as b on a.tagid = b.id where name like 'HaldCLUT BW'))) order by filename, version limit ?1, ?2";

  int imgid = -1;
  sqlite3_stmt *stmt;
  DT_DEBUG_SQLITE3_PREPARE_V2(dt_database_get(darktable.db), qin, -1, &stmt, NULL);
  DT_DEBUG_SQLITE3_BIND_INT(stmt, 1, 0);
  DT_DEBUG_SQLITE3_BIND_INT(stmt, 2, 1);
  if(sqlite3_step(stmt) == SQLITE_ROW)
  {
    imgid = sqlite3_column_int(stmt, 0);
  }
  sqlite3_finalize(stmt);
  fprintf(stderr,"[reload_defaults]: imgid: %d\n", imgid);
  tmp.imgid = imgid;

  if (imgid == -1) goto end;

  dt_mipmap_buffer_t buf;
  dt_mipmap_cache_get(darktable.mipmap_cache, &buf, imgid, DT_MIPMAP_FULL,DT_MIPMAP_BLOCKING, 'r');
  const dt_image_t *img = dt_image_cache_get(darktable.image_cache, imgid, 'r');
  dt_image_t image = *img;
  dt_image_cache_read_release(darktable.image_cache, img);
  if(!buf.buf)
  {
    dt_control_log(_("failed to get raw buffer from image `%s'"), image.filename);
    dt_mipmap_cache_release(darktable.mipmap_cache, &buf);
    goto end;
  }
   
  uint32_t data_size = image.width * image.height * sizeof(float) * 3;
  fprintf(stderr,"[reload_defaults]: data_size: %d\n", data_size);
  fprintf(stderr,"[reload_defaults]: buf_width: %d\n", buf.width);
  fprintf(stderr,"[reload_defaults]: img_width: %d\n", image.width);
  gd->clut_size = image.width;
  gd->clut_bpp = image.bpp;
  gd->clut_level = CUBEROOT(image.width);
  gd->clut_data = malloc(data_size);
  gd->clut_data_size = data_size;
  memcpy(gd->clut_data, buf.buf, data_size);

  fprintf(stderr,"[reload_defaults]: sizeof(float): %lu\n", sizeof(float));
  fprintf(stderr,"[reload_defaults]: cpp: %d\n", image.cpp);
  
  /* uint8_t *dd = (uint8_t*)gd->clut_data; */
  /* for (int i=0; i < data_size; i++) */
  /* { */
  /*   if(dd[i] != 0) fprintf(stderr,"%d ", dd[i]); */
  /* } */
  /* fprintf(stderr,"\n"); */


  dt_mipmap_cache_release(darktable.mipmap_cache, &buf);

end:
  memcpy(module->params, &tmp, sizeof(dt_iop_clut_params_t));
  memcpy(module->default_params, &tmp, sizeof(dt_iop_clut_params_t));
  module->default_enabled = 0;
}

void init(dt_iop_module_t *module)
{
  module->params = malloc(sizeof(dt_iop_clut_params_t));
  module->default_params = malloc(sizeof(dt_iop_clut_params_t));
  module->default_enabled = 0;
  module->params_size = sizeof(dt_iop_clut_params_t);
  module->gui_data = NULL;
  module->priority = 901; // module order created by iop_dependencies.py, do not edit!
}

void cleanup(dt_iop_module_t *module)
{
  free(module->gui_data);
  module->gui_data = NULL;
  free(module->params);
  module->params = NULL;
}

void cleanup_global(dt_iop_module_so_t *module)
{
  dt_iop_clut_global_data_t *gd = (dt_iop_clut_global_data_t *)module->data;
  fprintf(stderr, "[cleanup_global]: bpp=%d\n", gd->clut_bpp);
  free(module->data);
  module->data = NULL;
}




/** put your local callbacks here, be sure to make them static so they won't be visible outside this file! */
static void slider_callback(GtkWidget *w, dt_iop_module_t *self)
{
  // this is important to avoid cycles!
  if(darktable.gui->reset) return;
  //dt_iop_useless_params_t *p = (dt_iop_useless_params_t *)self->params;
  //p->checker_scale = dt_bauhaus_slider_get(w);
  // let core know of the changes
  dt_dev_add_history_item(darktable.develop, self, TRUE);
}

/** gui callbacks, these are needed. */
void gui_update(dt_iop_module_t *self)
{
  // let gui slider match current parameters:
  //dt_iop_useless_gui_data_t *g = (dt_iop_useless_gui_data_t *)self->gui_data;
  //dt_iop_useless_params_t *p = (dt_iop_useless_params_t *)self->params;
  //dt_bauhaus_slider_set(g->scale, p->checker_scale);
}

void gui_init(dt_iop_module_t *self)
{
  // init the slider (more sophisticated layouts are possible with gtk tables and boxes):
  self->gui_data = malloc(sizeof(dt_iop_clut_gui_data_t));
  dt_iop_clut_gui_data_t *g = (dt_iop_clut_gui_data_t *)self->gui_data;
  g->scale = dt_bauhaus_slider_new_with_range(self, 1, 100, 1, 50, 0);
  self->widget = g->scale;
  g_signal_connect(G_OBJECT(g->scale), "value-changed", G_CALLBACK(slider_callback), self);
}

void gui_cleanup(dt_iop_module_t *self)
{
  // nothing else necessary, gtk will clean up the slider.
  free(self->gui_data);
  self->gui_data = NULL;
}

gboolean is_valid_clut(const dt_image_t *img)
{
  // check if size is quad
  if (img->width != img->height)
    return FALSE;
  // check if size is cubic
  if (img->width != CUBE(CUBEROOT(img->width)))
    return FALSE;
  return TRUE;
}

int find_cluts(const char *tag)
{
  fprintf(stderr,"[find_clut]: entry\n");
  gchar *qin = "select distinct id from images where   (flags & 256) != 256 and ((id in (select imgid from tagged_images as a join tags as b on a.tagid = b.id where name like 'HaldCLUT BW'))) order by filename, version limit ?1, ?2";

  int imgid = -1;
  sqlite3_stmt *stmt;
  DT_DEBUG_SQLITE3_PREPARE_V2(dt_database_get(darktable.db), qin, -1, &stmt, NULL);
  DT_DEBUG_SQLITE3_BIND_INT(stmt, 1, 0);
  DT_DEBUG_SQLITE3_BIND_INT(stmt, 2, 1);
  if(sqlite3_step(stmt) == SQLITE_ROW)
  {
    imgid = sqlite3_column_int(stmt, 0);
  }
  sqlite3_finalize(stmt);
  return imgid;
}

/** additional, optional callbacks to capture darkroom center events. */
// void gui_post_expose(dt_iop_module_t *self, cairo_t *cr, int32_t width, int32_t height, int32_t pointerx,
// int32_t pointery);
// int mouse_moved(dt_iop_module_t *self, double x, double y, double pressure, int which);
// int button_pressed(dt_iop_module_t *self, double x, double y, double pressure, int which, int type,
// uint32_t state);
// int button_released(struct dt_iop_module_t *self, double x, double y, int which, uint32_t state);
// int scrolled(dt_iop_module_t *self, double x, double y, int up, uint32_t state);

// modelines: These editor modelines have been set for all relevant files by tools/update_modelines.sh
// vim: shiftwidth=2 expandtab tabstop=2 cindent
// kate: tab-indents: off; indent-width 2; replace-tabs on; indent-mode cstyle; remove-trailing-space on;
