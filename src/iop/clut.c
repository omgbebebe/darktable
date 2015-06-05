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

typedef struct dt_iop_clut_params_t
{
  // self->params
  int imgid;
} dt_iop_clut_params_t;

typedef struct dt_iop_clut_gui_data_t
{
  // self->gui_data
  GtkTreeView *films;
  GtkListStore *films_list;
  GtkNotebook *film_tabs;
  GtkSizeGroup *sizegroup;
} dt_iop_clut_gui_data_t;

/* typedef struct dt_iop_clut_global_data_t */
/* { */
/*   int clut_size; */
/*   int clut_bpp; */
/*   int clut_level; */
/*   int clut_data_size; */
/*   void *clut_data; */
/*   GList *clut_collection; */
/* } dt_iop_clut_global_data_t; */

typedef enum dt_iop_clut_film_type_t
{
  FILM_TYPE_BW,
  FILM_TYPE_COLOR,
} dt_iop_film_type_t;

typedef struct dt_iop_clut_collection_item_t
{
  uint32_t imgid;
  char *filename;
  dt_iop_film_type_t film_type;
} dt_iop_clut_collection_item_t;

typedef struct dt_iop_clut_data_t
{
  // self->data
  int clut_size;
  int clut_bpp;
  int clut_level;
  int clut_data_size;
  void *clut_data;
  GList *clut_collection;
} dt_iop_clut_data_t;

typedef struct dt_iop_clut_color_t
{
  float red;
  float green;
  float blue;
} dt_iop_clut_color_t;


gboolean is_valid_clut(const dt_image_t*);
void load_cluts(GList*, const char*);
//void load_clut_by_imgid(dt_iop_clut_data_t *d, int imgid);
void load_clut_by_imgid(int imgid, dt_iop_module_t *self);
/* handle clut selection */
static void _dt_iop_clut_row_changed_callback(GtkTreeView *treeview, dt_iop_module_t *self);



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

  const dt_iop_clut_color_t *clut = (dt_iop_clut_color_t *)d->clut_data;
  /* for (int i=0; i < d->clut_data_size; i++) */
  /* { */
  /*   if(clut_data[i] != 0) fprintf(stderr,"%d ", clut_data[i]); */
  /* } */
  /* fprintf(stderr,"\n"); */

  const int ch = piece->colors;
  const int width = roi_out->width;
  const int height = roi_out->height;
//  const int clut_size = d->clut_size;
  const int level = d->clut_level * d->clut_level;

#ifdef _OPENMP
#pragma omp parallel for default(none) schedule(static) shared(i, o, roi_in, roi_out, clut)
#endif
  for(int k = 0; k < height; k++)
  {
    float *in = ((float *)i) + (size_t)k * ch * width;
    float *out = ((float *)o) + (size_t)k * ch * width;
    float MaxRGBDouble = 1.0f;

    for(int j = 0; j < width; j++, in += ch, out += ch)
    {
      uint32_t redaxis, greenaxis, blueaxis, color;
      double sums[9], r, g, b, value;
      long i;
/*
  Calculate the position of each 3D axis pixel level.
*/
      redaxis = (unsigned int) (((double) in[0]/MaxRGBDouble) * (level-1));
      if (redaxis > level - 2)
	redaxis = level - 2;
      greenaxis = (unsigned int) (((double) in[1]/MaxRGBDouble) * (level-1));
      if(greenaxis > level - 2)
	greenaxis = level - 2;
      blueaxis = (unsigned int) (((double) in[2]/MaxRGBDouble) * (level-1));
      if(blueaxis > level - 2)
	blueaxis = level - 2;

      /*
	Convert between the value and the equivalent value position.
      */
      r = ((double) in[0]/MaxRGBDouble) * (level - 1) - redaxis;
      g = ((double) in[1]/MaxRGBDouble) * (level - 1) - greenaxis;
      b = ((double) in[2]/MaxRGBDouble) * (level - 1) - blueaxis;

      color = redaxis + greenaxis * level + blueaxis * level * level;

      i = color;
      sums[0] = ((double) clut[i].red) * (1 - r);
      sums[1] = ((double) clut[i].green) * (1 - r);
      sums[2] = ((double) clut[i].blue) * (1 - r);
      i++;
      sums[0] += ((double) clut[i].red) * r;
      sums[1] += ((double) clut[i].green) * r;
      sums[2] += ((double) clut[i].blue) * r;

      i = (color + level);
      sums[3] = ((double) clut[i].red) * (1 - r);
      sums[4] = ((double) clut[i].green) * (1 - r);
      sums[5] = ((double) clut[i].blue) * (1 - r);
      i++;
      sums[3] += ((double) clut[i].red) * r;
      sums[4] += ((double) clut[i].green) * r;
      sums[5] += ((double) clut[i].blue) * r;

      sums[6] = sums[0] * (1 - g) + sums[3] * g;
      sums[7] = sums[1] * (1 - g) + sums[4] * g;
      sums[8] = sums[2] * (1 - g) + sums[5] * g;

      i = (color + level * level);
      sums[0] = ((double) clut[i].red) * (1 - r);
      sums[1] = ((double) clut[i].green) * (1 - r);
      sums[2] = ((double) clut[i].blue) * (1 - r);
      i++;
      sums[0] += ((double) clut[i].red) * r;
      sums[1] += ((double) clut[i].green) * r;
      sums[2] += ((double) clut[i].blue) * r;

      i = (color + level * level + level);
      sums[3] = ((double) clut[i].red) * (1 - r);
      sums[4] = ((double) clut[i].green) * (1 - r);
      sums[5] = ((double) clut[i].blue) * (1 - r);
      i++;
      sums[3] += ((double) clut[i].red) * r;
      sums[4] += ((double) clut[i].green) * r;
      sums[5] += ((double) clut[i].blue) * r;

      sums[0] = sums[0] * (1 - g) + sums[3] * g;
      sums[1] = sums[1] * (1 - g) + sums[4] * g;
      sums[2] = sums[2] * (1 - g) + sums[5] * g;

      value=(sums[6] * (1 - b) + sums[0] * b);
      out[0] = value;

      value=(sums[7] * (1 - b) + sums[1] * b);
      out[1] = value;

      value=(sums[8] * (1 - b) + sums[2] * b);
      out[2] = value;

      /* out[0] = cl[0]; */
      /* out[1] = cl[1]; */
      /* out[2] = cl[2]; */
    }
  }
  for(int k = 0; k < 3; k++) piece->pipe->processed_maximum[k] = 1.0f;
}


void commit_params(struct dt_iop_module_t *self, dt_iop_params_t *p1, dt_dev_pixelpipe_t *pipe,
                   dt_dev_pixelpipe_iop_t *piece)
{
  fprintf(stderr,"[commit_params]: entry\n");

  dt_iop_clut_params_t *p = (dt_iop_clut_params_t *)p1;
  if(0)
  {
    p = (dt_iop_clut_params_t *)self->default_params;
    fprintf(stderr,"[commit_params]: imgid=%d\n", p->imgid);
  }
  
  dt_iop_clut_data_t *pd = (dt_iop_clut_data_t *)piece->data;
  dt_iop_clut_data_t *d  = (dt_iop_clut_data_t *)self->data;

  /* gd->clut_bpp = 123; */
  /* gd->clut_data = calloc(64,sizeof(uint8_t)); */
  /* memset(gd->clut_data, 12, 64); */

  pd->clut_bpp = d->clut_bpp;
  pd->clut_size = d->clut_size;
  pd->clut_level = d->clut_level;
  pd->clut_data_size = d->clut_data_size;
  pd->clut_data = d->clut_data;
}

void init_pipe(struct dt_iop_module_t *self, dt_dev_pixelpipe_t *pipe, dt_dev_pixelpipe_iop_t *piece)
{
  fprintf(stderr,"[init_pipe]: entry\n");

  piece->data = calloc(1, sizeof(dt_iop_clut_data_t));
  self->commit_params(self, self->default_params, pipe, piece);
}

void cleanup_pipe(struct dt_iop_module_t *self, dt_dev_pixelpipe_t *pipe, dt_dev_pixelpipe_iop_t *piece)
{
  fprintf(stderr,"[cleanup_pipe]: entry\n");

//  dt_iop_clut_data_t *d = (dt_iop_clut_data_t *)piece->data;
  free(piece->data);
  piece->data = NULL;
}

void init_global(dt_iop_module_so_t *module)
{
  fprintf(stderr,"[init_global]: entry\n");

  dt_iop_clut_data_t *d
    = (dt_iop_clut_data_t *)calloc(1, sizeof(dt_iop_clut_data_t));
  module->data = d;
  
  GList *l = g_list_alloc();
  dt_iop_clut_collection_item_t *id = malloc(sizeof(dt_iop_clut_collection_item_t));
  id->filename = "Identity";
  id->imgid = -1;
  l->data = id;
  d->clut_collection = l;
  load_cluts(l, "HaldCLUT BW");

  if(!d->clut_collection) fprintf(stderr,"[init_gloabal]: NULL pointer to collection\n");
  fprintf(stderr,"[init_global]: print images\n");

  GList *elem = NULL;
  for(elem = d->clut_collection; elem != NULL; elem = elem->next)
  {
    dt_iop_clut_collection_item_t *s = (dt_iop_clut_collection_item_t*)elem->data;
    fprintf(stderr,"item: %s\n", s->filename);
  }

}

void reload_defaults(dt_iop_module_t *module)
{
  fprintf(stderr,"[reload_defaults]: entry\n");

  dt_iop_clut_params_t tmp = { -1 };
  if(!module || !module->dev) goto end;

  load_clut_by_imgid(168, module);
  /* dt_iop_clut_data_t *d = (dt_iop_clut_data_t *)module->data; */

  /* d->clut_bpp = 0; */

end:
  memcpy(module->params, &tmp, sizeof(dt_iop_clut_params_t));
  memcpy(module->default_params, &tmp, sizeof(dt_iop_clut_params_t));
  module->default_enabled = 0;
}

void init(dt_iop_module_t *module)
{
  fprintf(stderr,"[init]: entry\n");

  module->params = malloc(sizeof(dt_iop_clut_params_t));
  module->default_params = malloc(sizeof(dt_iop_clut_params_t));
  module->default_enabled = 0;
  module->params_size = sizeof(dt_iop_clut_params_t);
  module->gui_data = NULL;
  module->priority = 901; // module order created by iop_dependencies.py, do not edit!
}

void cleanup(dt_iop_module_t *module)
{
  fprintf(stderr,"[cleanup]: entry\n");

  free(module->gui_data);
  module->gui_data = NULL;
  free(module->params);
  module->params = NULL;
}

void cleanup_global(dt_iop_module_so_t *module)
{
  fprintf(stderr,"[cleanup_global]: entry\n");

  dt_iop_clut_data_t *d = (dt_iop_clut_data_t *)module->data;
  fprintf(stderr, "[cleanup_global]: bpp=%d\n", d->clut_bpp);
  free(module->data);
  module->data = NULL;
}




/** put your local callbacks here, be sure to make them static so they won't be visible outside this file! */

/** gui callbacks, these are needed. */
void gui_update(dt_iop_module_t *self)
{
  fprintf(stderr,"[gui_update]: entry\n");

  // let gui slider match current parameters:
  //dt_iop_useless_gui_data_t *g = (dt_iop_useless_gui_data_t *)self->gui_data;
  //dt_iop_useless_params_t *p = (dt_iop_useless_params_t *)self->params;
  //dt_bauhaus_slider_set(g->scale, p->checker_scale);
}

void gui_init(dt_iop_module_t *self)
{
  fprintf(stderr,"[gui_init]: entry\n");
  // init the slider (more sophisticated layouts are possible with gtk tables and boxes):
  self->gui_data = malloc(sizeof(dt_iop_clut_gui_data_t));
  dt_iop_clut_gui_data_t *g = (dt_iop_clut_gui_data_t *)self->gui_data;
  dt_iop_clut_data_t *d = (dt_iop_clut_data_t *)self->data;

  self->widget = gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_set_size_request(self->widget, -1, DT_PIXEL_APPLY_DPI(208));
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(self->widget), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  g->films = GTK_TREE_VIEW(gtk_tree_view_new());
  gtk_widget_set_size_request(GTK_WIDGET(g->films), DT_PIXEL_APPLY_DPI(50), -1);
  gtk_container_add(GTK_CONTAINER(self->widget), GTK_WIDGET(g->films));

  g->films_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
  GList *elem = NULL;
  GtkTreeIter iter;
  for (elem = d->clut_collection; elem; elem = elem->next)
  {
    /* fprintf(stderr,"[gui_init]: add elem\n"); */
  
    gtk_list_store_append(g->films_list, &iter);
    dt_iop_clut_collection_item_t *data = (dt_iop_clut_collection_item_t *)elem->data;
    gtk_list_store_set (g->films_list, &iter, 0, data->filename, -1);
    gtk_list_store_set (g->films_list, &iter, 1, data->imgid, -1);
//    gtk_list_store_set (g->films_list, &iter, 0, "bebebe", -1);
  }
  gtk_tree_view_set_model(g->films, GTK_TREE_MODEL( g->films_list ));

  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Film",
                                                     renderer,
                                                     "text", 0,
                                                     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (g->films), column);

  g_signal_connect(GTK_WIDGET(g->films), "cursor-changed", G_CALLBACK(_dt_iop_clut_row_changed_callback), self);

  // set cursor to the first row (identity clut)
  gtk_tree_model_get_iter_first (GTK_TREE_MODEL( g->films_list ), &iter);
  GtkTreePath* path = gtk_tree_model_get_path(GTK_TREE_MODEL( g->films_list ), &iter);
  gtk_tree_view_set_cursor(g->films, path, NULL, FALSE);
}

void gui_cleanup(dt_iop_module_t *self)
{
  fprintf(stderr,"[gui_cleanup]: entry\n");
  // nothing else necessary, gtk will clean up the slider.
  free(self->gui_data);
  self->gui_data = NULL;
}

static void _dt_iop_clut_row_changed_callback(GtkTreeView *treeview, dt_iop_module_t *self)
{
//  dt_iop_clut_data_t *d = (dt_iop_clut_data_t *)self->data;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreePath *path;
  char *film_name = NULL;
  int imgid = -1;

  fprintf(stderr, "[row_changed_callback]: entry\n");
  gtk_tree_view_get_cursor(treeview, &path, NULL);

  if(path != NULL)
  {
    model = gtk_tree_view_get_model(treeview);
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_path_free(path);
    gtk_tree_model_get(model, &iter, 0, &film_name, -1);
    gtk_tree_model_get(model, &iter, 1, &imgid, -1);
    fprintf(stderr, "[row_changed_callback]: film_name: %s\n", film_name);
    fprintf(stderr, "[row_changed_callback]: imgid: %d\n", imgid);
    load_clut_by_imgid(imgid, self);
  }

}

void load_clut_by_imgid(int imgid, dt_iop_module_t *self)
{
  fprintf(stderr, "[load_clut_by_imgid]: entry\n");
  dt_iop_clut_data_t *d = (dt_iop_clut_data_t *)self->data;
  dt_iop_clut_params_t *p = (dt_iop_clut_params_t *)self->params;

  if(imgid == -1)
  {
    fprintf(stderr, "[load_clut_by_imgid]: imgid is -1, loading identity clut\n");
    return;
  }
  
  if (d->clut_data)
  {
    free(d->clut_data);
    d->clut_data = NULL;
  }

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
   
  uint32_t data_size = image.width * image.height;
  fprintf(stderr,"[reload_defaults]: data_size: %d\n", data_size);
  fprintf(stderr,"[reload_defaults]: buf_width: %d\n", buf.width);
  fprintf(stderr,"[reload_defaults]: img_width: %d\n", image.width);
  d->clut_size = image.width;
  d->clut_bpp = image.bpp;
  d->clut_level = CUBEROOT(image.width);
  d->clut_data = calloc(data_size, sizeof(dt_iop_clut_color_t));
  int k = 0;
  /* float r, g, b; */
  for (int i = 0; i < data_size; i++)
  {
    dt_iop_clut_color_t *c = (dt_iop_clut_color_t *)d->clut_data;
    float *bb = (float*)buf.buf;
    c[i].red   = bb[k];
    c[i].green = bb[k+1];
    c[i].blue  = bb[k+2];
    k += 4;
    /* fprintf(stderr,"r=%3.2f;g=%3.2f;b=%3.2f\t", c[i].red, c[i].green, c[i].blue); */
  }
  d->clut_data_size = data_size;
  /* memcpy(gd->clut_data, buf.buf, data_size); */

  dt_mipmap_cache_release(darktable.mipmap_cache, &buf);

  p->imgid = imgid;

  dt_dev_reprocess_all(self->dev);
  dt_control_queue_redraw();
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

void load_cluts(GList *images, const char *tag)
{
  fprintf(stderr,"[find_clut]: entry\n");
  gchar *qin = "select distinct id, filename from images where (flags & 256) != 256 and ((id in (select imgid from tagged_images as a join tags as b on a.tagid = b.id where name like 'HaldCLUT BW'))) order by filename, version limit ?1, ?2";

//  GList *images = NULL;
//  images = NULL;

  sqlite3_stmt *stmt;
  DT_DEBUG_SQLITE3_PREPARE_V2(dt_database_get(darktable.db), qin, -1, &stmt, NULL);
  DT_DEBUG_SQLITE3_BIND_INT(stmt, 1, 0);
  DT_DEBUG_SQLITE3_BIND_INT(stmt, 2, 1000);
  while(sqlite3_step(stmt) == SQLITE_ROW)
  {
    dt_iop_clut_collection_item_t *item = malloc(sizeof(dt_iop_clut_collection_item_t));
    const unsigned char *filename = sqlite3_column_text(stmt, 1);
    int imgid = sqlite3_column_int(stmt, 0);
    size_t len = strlen((char*)filename);
    item->filename = calloc(len+1,sizeof(char));
    strncpy(item->filename, (char*)filename, len);
//    int imgid = sqlite3_column_int(stmt, 0);
    item->imgid = imgid;
    images = g_list_append(images, item);
  }
  sqlite3_finalize(stmt);
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
