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

typedef struct dt_iop_clut_data_t
{
  void *data;
  int size;
  int level;
  int bpp;
} dt_iop_clut_data_t;

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

typedef struct dt_iop_clut_params_t
{
  int imgid;
} dt_iop_clut_params_t;

typedef struct dt_iop_clut_gui_data_t
{
  GtkWidget *scale; // this is needed by gui_update
} dt_iop_clut_gui_data_t;

typedef struct dt_iop_clut_global_data_t
{
  int test;
  dt_iop_clut_data_t *clut;
} dt_iop_clut_global_data_t;

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

//int load_clut(void *data, int size, int bpp, int level)
dt_iop_clut_data_t* load_clut(int imgid)
{
  dt_iop_clut_data_t *clut;
  dt_image_cache_t cache;
  dt_image_cache_init(&cache);
  dt_image_t *img = dt_image_cache_get(&cache, imgid, 'r');
  if (img && is_valid_clut(img))
  {
    clut = malloc(sizeof(dt_iop_clut_data_t));
    clut->size = img->width;
    clut->level = CUBEROOT(clut->size);
    clut->bpp = img->bpp;
//    int data_size = clut->size * clut->size;
//    fprintf(stderr,"data size: %d\n", data_size);
    clut->data = img->cache_entry->data; //malloc(sizeof(uint8_t) * data_size);
//    memcpy(clut->data, img->cache_entry->data, sizeof(uint8_t) * data_size);
  }else{
    fprintf(stderr,"It's not a valid CLUT image!\n");
    fprintf(stderr,"Width: %d\n", img->width);
    fprintf(stderr,"CubeRoot: %d\n", CUBEROOT(img->width));
    dt_image_cache_cleanup(&cache);
    return NULL;
  }
//  dt_image_cache_cleanup(&cache);
  return clut;
}

/** process, all real work is done here. */
void process(struct dt_iop_module_t *self, dt_dev_pixelpipe_iop_t *piece, void *i, void *o,
             const dt_iop_roi_t *roi_in, const dt_iop_roi_t *roi_out)
{
  // this is called for preview and full pipe separately, each with its own pixelpipe piece.
  // get our data struct:
  dt_iop_clut_global_data_t *gd = (dt_iop_clut_global_data_t*)piece->data;
// dt_iop_clut_data_t *cd = gd->clut;
  if(gd)
  {
    fprintf(stderr,"[process]: bpp: %d\n", gd->test);
  }else{
    fprintf(stderr,"[process]: NULL pointer\n");
  }
//  uint8_t *d = gd->clut->data;
  const int ch = piece->colors;
  const int width = roi_out->width;
  const int height = roi_out->height;
// iterate over all output pixels (same coordinates as input)
#ifdef _OPENMP
// optional: parallelize it!
#pragma omp parallel for default(none) schedule(static) shared(i, o, roi_in, roi_out)
#endif
  for(int k = 0; k < height; k++)
  {
    float *in = ((float *)i) + (size_t)k * ch * width;
    float *out = ((float *)o) + (size_t)k * ch * width;
//    float *clut = ((float *)d) + (size_t)k * ch * width;
    
    for(int j = 0; j < width; j++, in += ch, out += ch)
    {
      out[0] = 0.0;
      out[1] = 0.0;
      out[2] = 0.0;
    }
  }
  for(int k = 0; k < 3; k++) piece->pipe->processed_maximum[k] = 1.0f;
}

/** optional: if this exists, it will be called to init new defaults if a new image is loaded from film strip
 * mode. */
void reload_defaults(dt_iop_module_t *module)
{
  // change default_enabled depending on type of image, or set new default_params even.

  // if this callback exists, it has to write default_params and default_enabled.
}


void commit_params(struct dt_iop_module_t *self, dt_iop_params_t *p1, dt_dev_pixelpipe_t *pipe,
                   dt_dev_pixelpipe_iop_t *piece)
{
  fprintf(stderr,"[commit_params]: entry\n");
  if (self->data)
  {
    piece->data = self->data;
  }
  else{
    fprintf(stderr,"[commit_params]: NULL pointer\n");
    return;
  }
}

void cleanup_pipe(struct dt_iop_module_t *self, dt_dev_pixelpipe_t *pipe, dt_dev_pixelpipe_iop_t *piece)
{
//  free(piece->data);
//  piece->data = NULL;
}


void init_pipe(struct dt_iop_module_t *self, dt_dev_pixelpipe_t *pipe, dt_dev_pixelpipe_iop_t *piece)
{
  dt_iop_clut_params_t *default_params = (dt_iop_clut_params_t *)self->default_params;
  dt_iop_clut_global_data_t *gd = (dt_iop_clut_global_data_t*)self->data;

  piece->data = malloc(sizeof(dt_iop_clut_data_t));
  if (!self)
    fprintf(stderr,"[init_pipe]: self is NULL\n");
  else
    fprintf(stderr,"[init_pipe]: imgid: %d\n", default_params->imgid);
  if (gd)
  {
    piece->data = gd;
  }
  else{
    fprintf(stderr,"[init_pipe]: NULL pointer\n");
    return;
  }
//  dt_iop_clut_data_t *d = (dt_iop_clut_data_t*)malloc(sizeof(dt_iop_clut_data_t));
//  d = load_clut(default_params->imgid);
//  piece->data = (void*)d;
//  if(d)
//  {
//    fprintf(stderr,"CLUT BPP inside init: %d\n", d->bpp);
//    fprintf(stderr,"CLUT Size inside init: %d\n", d->size);
//    fprintf(stderr,"CLUT Level inside init: %d\n", d->level);
//  }
}

void init_global(dt_iop_module_t *self)
{
  fprintf(stderr,"[init_global]: entry\n");
  self->data = malloc(sizeof(dt_iop_clut_global_data_t));

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
  fprintf(stderr,"imgid: %d\n", imgid);
//  tmp.imgid = imgid;


  dt_mipmap_buffer_t buf;
  dt_mipmap_cache_get(darktable.mipmap_cache, &buf, imgid, DT_MIPMAP_FULL,DT_MIPMAP_BLOCKING, 'r');
  const dt_image_t *img = dt_image_cache_get(darktable.image_cache, imgid, 'r');
  dt_image_t image = *img;
  dt_image_cache_read_release(darktable.image_cache, img);
  if(!buf.buf)
  {
    dt_control_log(_("failed to get raw buffer from image `%s'"), image.filename);
    dt_mipmap_cache_release(darktable.mipmap_cache, &buf);
    return;
  }
  
//  dt_control_log(_("image width: %d"), image.width);
  dt_iop_clut_global_data_t *gd = (dt_iop_clut_global_data_t*)self->data;
  uint32_t size = image.width * image.height;
//  uint8_t *tbuf = (uint8_t*) buf.buf;
  fprintf(stderr,"size: %d\n", size);
  gd->clut = malloc(sizeof(dt_iop_clut_data_t));
  gd->clut->size = image.width;
  gd->clut->bpp = image.bpp;
  gd->clut->data = malloc(size);
  dt_mipmap_cache_release(darktable.mipmap_cache, &buf);

  if(!self->data)
  {
    fprintf(stderr,"[init_global]: NULL pointer\n");
  }
}

/** init, cleanup, commit to pipeline */
void init(dt_iop_module_t *module)
{
  // we don't need global data:
  //module->data = malloc(sizeof(dt_iop_clut_global_data_t));
  module->params = malloc(sizeof(dt_iop_clut_params_t));
  module->default_params = malloc(sizeof(dt_iop_clut_params_t));
  module->default_enabled = 0;
  // order has to be changed by editing the dependencies in tools/iop_dependencies.py
  module->priority = 901; // module order created by iop_dependencies.py, do not edit!
  module->params_size = sizeof(dt_iop_clut_params_t);
  module->gui_data = NULL;
  // init defaults:
  dt_iop_clut_params_t tmp = (dt_iop_clut_params_t){ -1 };
//  dt_iop_clut_global_data_t gtmp = (dt_iop_clut_global_data_t){ 123, NULL };

  memcpy(module->params, &tmp, sizeof(dt_iop_clut_params_t));
  memcpy(module->default_params, &tmp, sizeof(dt_iop_clut_params_t));
//  memcpy(module->data, &gtmp, sizeof(dt_iop_clut_global_data_t));
  fprintf(stderr,"[init]: complete\n");
}

void cleanup(dt_iop_module_t *module)
{
  fprintf(stderr,"[cleanup]: started\n");
  free(module->gui_data);
  module->gui_data = NULL; // just to be sure
  free(module->params);
  module->params = NULL;
  free(module->data); // just to be sure
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
