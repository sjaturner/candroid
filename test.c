//Copyright (c) 2011-2020 <>< Charles Lohr - Under the MIT/x11 or NewBSD License you choose.
// NO WARRANTY! NO GUARANTEE OF SUPPORT! USE AT YOUR OWN RISK

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


#include "os_generic.h"
#include <GLES3/gl3.h>
#include <asset_manager.h>
#include <asset_manager_jni.h>
#include <android_native_app_glue.h>
#include <android/sensor.h>
#include "CNFGAndroid.h"
#include <android/log.h>

#define CNFG_IMPLEMENTATION
#define CNFG3D
#include "CNFG.h"

#define log(...) __android_log_print(ANDROID_LOG_VERBOSE, "spood", __VA_ARGS__)

#define TARGET_IP "192.168.1.104"
#define TARGET_PORT 23456
#define debug(...) udp_debug(TARGET_IP, TARGET_PORT, __VA_ARGS__)

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))
#define NELEM(A) ((size_t)(sizeof(A)/sizeof(A[0])))

static void udp_block(char *addr, uint32_t port, void *data, uint32_t len)
{
   int sock = -1;
   struct sockaddr_in server = {
      .sin_family = AF_INET,
      .sin_port = htons(port),
      .sin_addr.s_addr = inet_addr(addr),
   };

   if((sock = socket(AF_INET, SOCK_DGRAM, 0)) >= 0)
   {
      /* Send the message in buf to the server */
      if(sendto(sock, data, len, 0, (struct sockaddr *)&server, sizeof(server)) < 0)
      {
         log("sendto failed");
      }

      close(sock);
   }
   else
   {
      log("socket open failed");
   }
}

static int udp_debug(char *addr, uint32_t port, const char *fmt, ...)
{
   int size = 0;
   va_list ap;
   char buf[0x80] = { };

   va_start(ap, fmt);
   size = vsnprintf(buf, (int)sizeof buf - 1, fmt, ap);
   va_end(ap);

   udp_block(addr, port, buf, size);

   return size;
}

short screenx;
short screeny;
static int radius;
static int upper_cx;
static int upper_cy;
static int lower_cx;
static int lower_cy;

static uint32_t *backdrop;

#define SQ(X) ((X) * (X))
static int ring_elems;

enum
{
   NELEM_RINGS = 0x40,
};

static struct target
{
   int threshold;
   uint32_t colour;
   int score;
   int line;
}
rings[NELEM_RINGS];

enum
{
   BK = 0x000000,
   WT = 0xffffff,
   BE = 0xff0000,
   RD = 0x0000ff,
   YO = 0x00ffff,
};

static void init_target(int radius)
{
   struct
   {
      uint32_t outer;
      uint32_t inner;
   }
   colours[] = {
      {0, 0,}, /* Unused. */
      {BK, WT,},
      {BK, WT,},
      {BK, BK,},
      {WT, BK,},
      {BK, BE,},
      {BK, BE,},
      {BK, RD,},
      {BK, RD,},
      {BK, YO,},
      {BK, YO,},
   };

   for(int score = 1; score <= 10; ++score)
   {
      float outer_radius = radius - radius * (score - 1) / 10;
      float inner_radius = outer_radius - radius * 0.01;

      rings[ring_elems].threshold = outer_radius;
      rings[ring_elems].colour = colours[score].outer;
      rings[ring_elems].score = score;
      rings[ring_elems].line = 1;
      ++ring_elems;

      rings[ring_elems].threshold = inner_radius;
      rings[ring_elems].colour = colours[score].inner;
      rings[ring_elems].score = score;
      ++ring_elems;

      if(ring_elems >= NELEM_RINGS)
      {
         break;
      }
   }

   for(int index = 0; index < ring_elems; ++index)
   {
      debug("0x%08x 0x%08x 0x%08x 0x%08x", index, rings[index].threshold, rings[index].score, rings[index].line);
   }
}


uint32_t scale_colour(uint32_t colour, int colour_modifier)
{
   if(colour_modifier)
   {
      uint32_t modified_colour = 0;

      for(uint32_t byte = 0; byte < 3; ++byte)
      {
         uint32_t scaled = ((colour >> 8 * byte) & 0xff) * colour_modifier / 0x100;

         modified_colour |= (scaled & 0xff) << 8 * byte;
      }

      return modified_colour;
   }
   else
   {
      return colour;
   }
}

static int plot_colour(int cx, int cy, int x, int y, uint32_t *colour, uint32_t *colour_modifier)
{
   int dx = x - cx;
   int dy = y - cy;
   int square_sum = SQ(dx) + SQ(dy);
   int abs_dx = dx < 0 ? -dx : dx;
   int abs_dy = dy < 0 ? -dy : dy;

   *colour = 0;

   for(int index = 0; !*colour_modifier && index < ring_elems - 1; ++index)
   {
      if(rings[index].line && abs_dx < rings[index].threshold && abs_dx > rings[index + 1].threshold)
      {
         *colour_modifier = 0xc0;
      }
      else if(rings[index].line && abs_dy < rings[index].threshold && abs_dy > rings[index + 1].threshold)
      {
         *colour_modifier = 0xc0;
      }
   }

   if(square_sum <= SQ(rings[0].threshold))
   {
      for(int index = ring_elems - 1; index >= 0; --index)
      {
         if(square_sum <= SQ(rings[index].threshold))
         {
            *colour = rings[index].colour;
            return 1;
         }
      }
   }

   return 0;
}

static int score(int x, int y)
{
   int square_sum = SQ(x) + SQ(y);
   int last_score = 0;

   if(square_sum <= SQ(rings[0].threshold))
   {
      for(int index = 0; index < ring_elems; ++index)
      {
         if(square_sum > SQ(rings[index].threshold))
         {
            debug("last_score:%d\n", last_score);
            break;
         }
         last_score = rings[index].score;
      }
   }

   return last_score;
}

static uint32_t *make_backdrop(int screenx, int screeny)
{
   uint32_t size = screenx * screeny * sizeof(uint32_t);
   uint32_t *ret = memset(calloc(1, size), 0xff, size);
   radius = MIN(screenx / 2, screeny / 4) * 3 / 4;

   upper_cx = screenx / 2;
   upper_cy = radius * 4 / 3;
   lower_cx = screenx / 2;
   lower_cy = screeny - radius * 5 / 3;

   init_target(radius);

   for(int y = 0; y < screeny; y++)
   {
      for(int x = 0; x < screenx; x++)
      {
         uint32_t colour = 0;
         uint32_t colour_modifier = 0;

         if(0)
         {
         }
         else if(plot_colour(upper_cx, upper_cy, x, y, &colour, &colour_modifier))
         {
            ret[x + y * screenx] = scale_colour(colour, colour_modifier);
         }
         else if(plot_colour(lower_cx, lower_cy, x, y, &colour, &colour_modifier))
         {
            ret[x + y * screenx] = scale_colour(colour, colour_modifier);
         }
         else
         {
            ret[x + y * screenx] = scale_colour(WT, colour_modifier);
         }
      }
   }
   return ret;
}

struct bin
{
   int a;
   int b;
};

enum
{
   BINS = 20,
};

struct hist
{
   int elems;
   struct bin bins[BINS];
};

struct hist hist_x;
struct hist hist_y;

void init_bins(struct hist *hist, int c, int end)
{
   int last_line = 0;
   int down = -1;

   hist->elems = 0;

   for(int p = 0; p < end; p++)
   {
      int d = p - c;
      int abs_d = d < 0 ? -d : d;
      int line = 0;

      for(int index = 0; index < ring_elems - 1; ++index)
      {
         if(abs_d < radius * 0.01 || (rings[index].line && abs_d < rings[index].threshold && abs_d > rings[index + 1].threshold))
         {
            line = 1;
         }
      }

      if(line && !last_line && hist->elems < BINS)
      {
         if(down >= 0)
         {
            struct bin *bin = hist->bins + hist->elems++;

            bin->a = down;
            bin->b = p;
         }
      }
      else if(!line && last_line)
      {
         down = p;
      }
      last_line = line;
   }
}

enum
{
   NORTH,
   EAST,
   SOUTH,
   WEST,
};

void plot_hist(struct hist *hist, int direction, int max, float *norm, int elems)
{
   int base_y = 0;

   for(int index = 0; index < hist->elems && index < elems; ++index)
   {
      struct bin *bin = hist->bins + index;
      int a_x = bin->a;
      int a_y = base_y;
      int b_x = bin->b;
      int b_y = base_y + max * norm[index];

      int tl_x = MIN(a_x, b_x);
      int tl_y = MIN(a_y, b_y);
      int tr_x = MAX(a_x, b_x);
      int tr_y = MIN(a_y, b_y);
      int br_x = MAX(a_x, b_x);
      int br_y = MAX(a_y, b_y);
      int bl_x = MIN(a_x, b_x);
      int bl_y = MAX(a_y, b_y);

      {
         enum
         {
            POINTS = 6,
         };
         RDPoint pp[POINTS] = {
            {tl_x, tl_y},
            {tr_x, tr_y},
            {br_x, br_y},

            {tl_x, tl_y},
            {br_x, br_y},
            {bl_x, bl_y},
         };

         CNFGColor(0x808080);
         CNFGTackPoly(pp, POINTS);
      }
   }
}

enum
{
   ARROW_POINTS = 10,
};

struct
{
   float x;
   float y;
}
arrow_points[ARROW_POINTS];

void init_arrow_points(void)
{
   enum
   {
      STEPS = ARROW_POINTS - 1,
   };
   int arrow_steps[STEPS] = {
      1, 2, 3, 5, 6, 7, 9, 10, 11
   };

   for(int index = 0; index < STEPS; ++index)
   {
      float theta = M_PI * arrow_steps[index] / 6;

      arrow_points[index + 1].x = cos(theta);
      arrow_points[index + 1].y = sin(theta);
   }
}

enum
{
   GREEN_TRIANGLES = 3,
   BLACK_TRIANGLES = 6,
};

struct point
{
   int x;
   int y;
};

struct arrow_triangles
{
   struct point green_triangles[GREEN_TRIANGLES][3];
   struct point black_triangles[BLACK_TRIANGLES][3];
};

enum
{
   AMP_ARROW_TRIANGLES = 10,
};

struct arrow_triangles amp_arrow_triangles[AMP_ARROW_TRIANGLES];

void init_amp_arrow_triangles(float scale)
{
   init_arrow_points();

   for(int amp_index = 0; amp_index < AMP_ARROW_TRIANGLES; ++amp_index)
   {
      struct point amp_arrow_points[ARROW_POINTS] = { };
      struct arrow_triangles *amp_arrow_triangle = amp_arrow_triangles + amp_index;

      for(int point_index = 0; point_index < ARROW_POINTS; ++point_index)
      {
         float factor = 1;

         switch (point_index)
         {
            case 2:
            case 5:
            case 8:
               factor = (1 + AMP_ARROW_TRIANGLES - amp_index) * 1.0;
         }

         amp_arrow_points[point_index].x = arrow_points[point_index].x * scale * factor;
         amp_arrow_points[point_index].y = arrow_points[point_index].y * scale * factor;
      }

      for(int green_triangle_index = 0; green_triangle_index < GREEN_TRIANGLES; ++green_triangle_index)
      {
         const int selector[GREEN_TRIANGLES][3] = { /* Annoying trianges only in cockwise order, FFS. */
            {1, 3, 2,},
            {4, 6, 5,},
            {7, 8, 9,},
         };

         for(int point_index = 0; point_index < 3; ++point_index)
         {
            struct point *p = amp_arrow_triangle->green_triangles[green_triangle_index] + point_index;

            *p = amp_arrow_points[selector[green_triangle_index][point_index]];
         }
      }

      for(int black_triangle_index = 0; black_triangle_index < BLACK_TRIANGLES; ++black_triangle_index)
      {
         const int selector[BLACK_TRIANGLES][3] = { /* Annoying trianges only in cockwise order, FFS. */
            {0, 1, 9,},
            {0, 3, 1,},
            {0, 4, 3,},
            {0, 6, 4,},
            {0, 7, 6,},
            {0, 9, 7,},
         };

         for(int point_index = 0; point_index < 3; ++point_index)
         {
            struct point *p = amp_arrow_triangle->black_triangles[black_triangle_index] + point_index;

            *p = amp_arrow_points[selector[black_triangle_index][point_index]];
         }
      }
   }
}

void plot_arrow(int x, int y, float scale, int amp_index)
{
   struct arrow_triangles *amp_arrow_triangle = amp_arrow_triangles + amp_index;

   CNFGColor(0x00ff00);

   for(int green_triangle_index = 0; green_triangle_index < GREEN_TRIANGLES; ++green_triangle_index)
   {
      RDPoint pp[3] = { };

      for(int point_index = 0; point_index < 3; ++point_index)
      {
         pp[point_index].x = x + amp_arrow_triangle->green_triangles[green_triangle_index][point_index].x * scale;
         pp[point_index].y = y + amp_arrow_triangle->green_triangles[green_triangle_index][point_index].y * scale;
      }
      CNFGTackPoly(pp, 3);
   }

   CNFGColor(0x000000);

   for(int black_triangle_index = 0; black_triangle_index < BLACK_TRIANGLES; ++black_triangle_index)
   {
      RDPoint pp[3] = { };

      for(int point_index = 0; point_index < 3; ++point_index)
      {
         pp[point_index].x = x + amp_arrow_triangle->black_triangles[black_triangle_index][point_index].x * scale;
         pp[point_index].y = y + amp_arrow_triangle->black_triangles[black_triangle_index][point_index].y * scale;
      }
      CNFGTackPoly(pp, 3);
   }
}

void HandleKey(int keycode, int down)
{
   if(keycode == 4)
   {
      AndroidSendToBack(1);
   }                            //Handle Physical Back Button.
   debug("%s %d %d", __FUNCTION__, keycode, down);
}

int down_state;

int arrow_on;
int arrow_x;
int arrow_y;

struct shot
{
   time_t epoch;
   float x;
   float y;
   int score;
};

enum
{
   SHOTS = 1000,
};
struct shot shots[SHOTS];
int shot_count;

#define CONTROL_SCREEN_FRACTION 8.0

int in_top_left(int x, int y)
{
   float dim = MIN(screenx, screeny) / CONTROL_SCREEN_FRACTION;

   if(!dim)
   {
      return 0;
   }

   debug("%s %d %d %f %d", __FUNCTION__, x, y, dim, x + y < dim);
   return x + y < dim;
}

char *files_directory;

void save_shots(void)
{
   debug("%s:%d\n", __FILE__, __LINE__);

   if(!files_directory || shot_count <= 0)
   {
      debug("%s:%d\n", __FILE__, __LINE__);
      return;
   }
   else
   {
      char formatted_time[0x80] = { };
      struct tm tm = *localtime(&shots[0].epoch);
      char filename[0x100] = { };

      strftime(formatted_time, sizeof(formatted_time), "%Y-%m-%d-%H-%M-%S.csv", &tm);
      if(snprintf(filename, sizeof filename, "%s/%s", files_directory, formatted_time) >= (int)sizeof filename - 1)
      {
         debug("%s:%d\n", __FILE__, __LINE__);
         return;
      }
      else
      {
         remove(filename);

         {
            int file = open(filename, O_WRONLY | O_CREAT, 0666);

            if(file < 0)
            {
               debug("%s:%d %s %s\n", __FILE__, __LINE__, filename, strerror(errno));
               return;
            }
            else
            {
               debug("%s:%d %s\n", __FILE__, __LINE__, filename);
               for(int index = 0; index < shot_count; ++index)
               {
                  struct shot *shot = shots + index;
                  char row[0x100] = { };
                  char formatted_time[0x80] = { };
                  struct tm tm = *localtime(&shot->epoch);
                  int row_length = 0;

                  strftime(formatted_time, sizeof(formatted_time), "%Y-%m-%d-%H-%M-%S", &tm);

                  row_length = snprintf(row, sizeof row, "\"%s\",\"%u\",\"%f\",\"%f\"\n", formatted_time, (unsigned)shot->epoch, shot->x, shot->y);
                  if(write(file, row, row_length) < 0)
                  {
                     debug("%s:%d\n", __FILE__, __LINE__);
                  }
               }
            }
            close(file);
            shot_count = 0;
         }
      }
   }
}

int in_top_right(int x, int y)
{
   return in_top_left(screenx - x, y);
}

void HandleButton(int x, int y, int button, int down)
{
   int coord_x = x - lower_cx + upper_cx;
   int coord_y = y - lower_cy + upper_cy;

   if(!down_state && down)
   {
      arrow_x = coord_x;
      arrow_y = coord_y;
      arrow_on = 1;
   }
   else if(down_state && !down)
   {
      if(in_top_left(coord_x, coord_y))
      {
         shot_count = shot_count > 0 ? shot_count - 1 : 0;
      }
      else if(in_top_right(coord_x, coord_y))
      {
         save_shots();
      }
      else if(shot_count < SHOTS)
      {
         struct shot *shot = shots + shot_count++;
         int dx = x - lower_cx;
         int dy = y - lower_cy;

         shot->epoch = time(0);
         shot->x = 1.0 * dx / radius;
         shot->y = 1.0 * dy / radius;
         shot->score = score(dx, dy);
      }

      arrow_on = 0;
   }

   down_state = down;

// debug("%s %d %d %d %d %d", __FUNCTION__, x, y, button, down, down_state);
}

void HandleMotion(int x, int y, int mask)
{
// debug("%s %d %d 0x%08x", __FUNCTION__, x, y, mask);

// debug("%s arrow %d %d %d", __FUNCTION__, arrow_on, arrow_x, arrow_y);
   if(arrow_on)
   {
      arrow_x = x - lower_cx + upper_cx;
      arrow_y = y - lower_cy + upper_cy;
   }
}

extern struct android_app *gapp;

void HandleDestroy()
{
   printf("Destroying\n");
   exit(10);
}

volatile int suspended;

void HandleSuspend()
{
   debug("%s", __FUNCTION__);
   suspended = 1;
}

void HandleResume()
{
   debug("%s", __FUNCTION__);
   suspended = 0;
}

int main(int argc, char *argv[])
{
   double ThisTime;
   double LastFrameTime = OGGetAbsoluteTime();
   double SecToWait;

   log("starting:%s", argv[1]);

   if(0)
   {
      files_directory = argv[1];
   }
   else
   {
      files_directory = "/sdcard/Android/data/org.yourorg.spoods/files";
   }

   CNFGBGColor = 0x800000;
   CNFGDialogColor = 0x444444;
   CNFGSetup("Test Bench", 270, 480);

   CNFGGetDimensions(&screenx, &screeny);

   init_amp_arrow_triangles(40.0);
   backdrop = make_backdrop(screenx, screeny);
   init_bins(&hist_x, lower_cx, screenx);
   init_bins(&hist_y, lower_cy, screeny);

   debug("hello");
   debug("%s", files_directory);

   while(1)
   {
      CNFGHandleInput();

      CNFGClearFrame();
      CNFGColor(0xFFFFFF);

      CNFGUpdateScreenWithBitmap(backdrop, screenx, screeny);

      {
         float dim = MIN(screenx, screeny) / CONTROL_SCREEN_FRACTION;
         RDPoint pp[3] = {
            {0.0 * dim, 0.0 * dim},
            {1.0 * dim, 0.0 * dim},
            {0.0 * dim, 1.0 * dim},
         };

         CNFGColor(0x808080);
         CNFGTackPoly(pp, 3);
         CNFGPenX = 20;
         CNFGPenY = 20;

         CNFGColor(0x000000);
         CNFGDrawText("U", 10);
      }

      {
         float dim = MIN(screenx, screeny) / CONTROL_SCREEN_FRACTION;
         RDPoint pp[3] = {
            {screenx - 1.0 * dim, 0.0 * dim},
            {screenx - 0.0 * dim, 0.0 * dim},
            {screenx - 0.0 * dim, 1.0 * dim},
         };

         CNFGColor(0x808080);
         CNFGTackPoly(pp, 3);
         CNFGPenX = screenx - 40;
         CNFGPenY = 20;

         CNFGColor(0xff0000);
         CNFGDrawText("S", 10);
      }

      for(int index = 0; index < shot_count; ++index)
      {
         struct shot *shot = shots + index;
         int amp = shot_count - index;

         amp = MIN(amp, AMP_ARROW_TRIANGLES - 1);

         plot_arrow(shot->x * radius + upper_cx, shot->y * radius + upper_cy, 1.0 * radius / 40 / 40, amp);
      }

      if(arrow_on)
      {
         plot_arrow(arrow_x, arrow_y, 1.0 * radius / 40 / 40, 0);
      }

      if(0)
      {
         for(int index = 0; index < hist_x.elems; ++index)
         {
            struct bin *bin = hist_x.bins + index;
            RDPoint pp[3] = {
               {bin->b, 0},
               {bin->a, 0},
               {(bin->a + bin->b) / 2, 20},
            };

            CNFGColor(0x808080);
            CNFGTackPoly(pp, 3);
         }

         for(int index = 0; index < hist_y.elems; ++index) {
            struct bin *bin = hist_y.bins + index;
            RDPoint pp[3] = {
               {0, bin->a},
               {20, (bin->a + bin->b) / 2},
               {0, bin->b},
            };

            CNFGColor(0x808080);
            CNFGTackPoly(pp, 3);
         }
      }

      {
         float norm[] = {0.1, 0.2, 0.0, 0.4, 0.5};

         plot_hist(&hist_x, NORTH, screeny / 16.0, norm, NELEM(norm));
      };

      CNFGSwapBuffers();

      ThisTime = OGGetAbsoluteTime();

      SecToWait = .016 - (ThisTime - LastFrameTime);
      LastFrameTime += .016;
      if(SecToWait > 0)
         OGUSleep((int)(SecToWait * 1000000));
   }

   return (0);
}
