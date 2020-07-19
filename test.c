//Copyright (c) 2011-2020 <>< Charles Lohr - Under the MIT/x11 or NewBSD License you choose.
// NO WARRANTY! NO GUARANTEE OF SUPPORT! USE AT YOUR OWN RISK

#include <arpa/inet.h>
#include <math.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
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

static int upper_cx;
static int upper_cy;
static int lower_cx;
static int lower_cy;

static ASensorManager *sm;
static const ASensor *as;
static ASensorEventQueue *aeq;
static ALooper *l;

void SetupIMU()
{
   sm = ASensorManager_getInstance();
   as = ASensorManager_getDefaultSensor(sm, ASENSOR_TYPE_GYROSCOPE);
   l = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
   aeq = ASensorManager_createEventQueue(sm, (ALooper *) & l, 0, 0, 0); //XXX??!?! This looks wrong.
   ASensorEventQueue_enableSensor(aeq, as);
   printf("setEvent Rate: %d\n", ASensorEventQueue_setEventRate(aeq, as, 10000));
}

void AccCheck()
{
   ASensorEvent evt = { };
   do
   {
      ssize_t s = ASensorEventQueue_getEvents(aeq, &evt, 1);
      if(s <= 0)
         break;
#if 0
      evt.vector.v[0];
      evt.vector.v[1];
      evt.vector.v[2];
#endif
   }
   while(1);
}

void AndroidDisplayKeyboard(int pShow);

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

void HandleButton(int x, int y, int button, int down)
{
   if(!down_state && down)
   {
      arrow_x = x - lower_cx + upper_cx;
      arrow_y = y - lower_cy + upper_cy;
      arrow_on = 1;
   }
   else if(down_state && !down)
   {
      arrow_on = 0;
   }

   down_state = down;

   debug("%s %d %d %d %d %d", __FUNCTION__, x, y, button, down, down_state);
}

void HandleMotion(int x, int y, int mask)
{
   debug("%s %d %d 0x%08x", __FUNCTION__, x, y, mask);

   debug("%s arrow %d %d %d", __FUNCTION__, arrow_on, arrow_x, arrow_y);
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

      rings[ring_elems].threshold = SQ(outer_radius);
      rings[ring_elems].colour = colours[score].outer;
      rings[ring_elems].score = score;
      ++ring_elems;

      rings[ring_elems].threshold = SQ(inner_radius);
      rings[ring_elems].colour = colours[score].inner;
      rings[ring_elems].score = score;
      ++ring_elems;

      if(ring_elems >= NELEM_RINGS)
      {
         break;
      }
   }
}

static int plot_colour(int cx, int cy, int x, int y, uint32_t *colour)
{
   int dx = x - cx;
   int dy = y - cy;
   int square_sum = SQ(dx) + SQ(dy);

   *colour = 0;

   if(square_sum <= rings[0].threshold)
   {
      for(int index = ring_elems - 1; index >= 0; --index)
      {
         if(square_sum <= rings[index].threshold)
         {
            *colour = rings[index].colour;
            return 1;
         }
      }
   }
   return 0;
}

int radius;

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
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

         if(plot_colour(upper_cx, upper_cy, x, y, &colour))
         {
            ret[x + y * screenx] = colour;
         }
         else if(plot_colour(lower_cx, lower_cy, x, y, &colour))
         {
            ret[x + y * screenx] = colour;
         }
      }
   }
   return ret;
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
         const int selector[GREEN_TRIANGLES][3] = {
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
         const int selector[BLACK_TRIANGLES][3] = {
            {0, 1, 9, },
            {0, 3, 1, },
            {0, 4, 3, },
            {0, 6, 4, },
            {0, 7, 6, },
            {0, 9, 7, },
         };

         for(int point_index = 0; point_index < 3; ++point_index)
         {
            struct point *p = amp_arrow_triangle->black_triangles[black_triangle_index] + point_index;

            *p = amp_arrow_points[selector[black_triangle_index][point_index]];
         }
      }
   }
}

void plot_arrow_flawed(int x, int y, float scale, int amp_index)
{
   struct arrow_triangles *amp_arrow_triangle = amp_arrow_triangles + amp_index;

   CNFGColor(0x00ff00);

   for(int green_triangle_index = 0; green_triangle_index < GREEN_TRIANGLES; ++green_triangle_index)
   {
      RDPoint pp[3] = { };

      for(int point_index = 0; point_index < 3; ++point_index)
      {
         pp[point_index].x = x + amp_arrow_triangles->green_triangles[green_triangle_index][point_index].x * scale;
         pp[point_index].y = y + amp_arrow_triangles->green_triangles[green_triangle_index][point_index].y * scale;
      }
      CNFGTackPoly(pp, 3);
   }

   CNFGColor(0x000000);

   for(int black_triangle_index = 0; black_triangle_index < BLACK_TRIANGLES; ++black_triangle_index)
   {
      RDPoint pp[3] = { };

      for(int point_index = 0; point_index < 3; ++point_index)
      {
         pp[point_index].x = x + amp_arrow_triangles->black_triangles[black_triangle_index][point_index].x * scale;
         pp[point_index].y = y + amp_arrow_triangles->black_triangles[black_triangle_index][point_index].y * scale;
      }
      CNFGTackPoly(pp, 3);
   }
}

void plot_arrow(int x, int y, float scale, int amp_index)
{
   plot_arrow_flawed(x, y, scale, amp_index); /* Oh poops. See this: https://blog.jayway.com/2009/12/04/opengl-es-tutorial-for-android-part-ii-building-a-polygon/ */
}

int main()
{
   double ThisTime;
   double LastFrameTime = OGGetAbsoluteTime();
   double SecToWait;
   short screenx, screeny;

   log("starting");

   CNFGBGColor = 0x800000;
   CNFGDialogColor = 0x444444;
   CNFGSetup("Test Bench", 270, 480);

   CNFGGetDimensions(&screenx, &screeny);

   init_amp_arrow_triangles(40.0);
   backdrop = make_backdrop(screenx, screeny);

   debug("hello");

   while(1)
   {
      CNFGHandleInput();

      CNFGClearFrame();
      CNFGColor(0xFFFFFF);

      CNFGUpdateScreenWithBitmap(backdrop, screenx, screeny);

      if(arrow_on)
      {
         plot_arrow(arrow_x, arrow_y, 1.0 * radius / 40 / 40, 5);
//       CNFGDrawBox(arrow_x - 3, arrow_y - 3, arrow_x + 3, arrow_y + 3);
      }

      CNFGSwapBuffers();

      ThisTime = OGGetAbsoluteTime();

      SecToWait = .016 - (ThisTime - LastFrameTime);
      LastFrameTime += .016;
      if(SecToWait > 0)
         OGUSleep((int)(SecToWait * 1000000));
   }

   return (0);
}
