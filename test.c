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

void udp_block(char *addr, uint32_t port, void *data, uint32_t len)
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

int debug(char *addr, uint32_t port, const char *fmt, ...)
{
   int size = 0;
   va_list ap;
   char buf[0x80] = { };

   va_start(ap, fmt);
   size = vsnprintf(buf, (int)sizeof buf - 1, fmt, ap);
   va_end(ap);

   log(buf);

   udp_block(addr, port, buf, size);

   return size;
}

float mountainoffsetx;
float mountainoffsety;

ASensorManager *sm;
const ASensor *as;
ASensorEventQueue *aeq;
ALooper *l;

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
   ASensorEvent evt;
   do
   {
      ssize_t s = ASensorEventQueue_getEvents(aeq, &evt, 1);
      if(s <= 0)
         break;
      evt.vector.v[0];
      evt.vector.v[1];
      evt.vector.v[2];
   }
   while(1);
}

void AndroidDisplayKeyboard(int pShow);

int lastbuttonx = 0;
int lastbuttony = 0;
int lastmotionx = 0;
int lastmotiony = 0;
int lastbid = 0;
int lastmask = 0;
int lastkey, lastkeydown;

static int keyboard_up;

void HandleKey(int keycode, int bDown)
{
   lastkey = keycode;
   lastkeydown = bDown;
   if(0 && keycode == 10 && !bDown)
   {
      keyboard_up = 0;
      AndroidDisplayKeyboard(keyboard_up);
   }

   if(keycode == 4)
   {
      AndroidSendToBack(1);
   }                            //Handle Physical Back Button.
}

void HandleButton(int x, int y, int button, int bDown)
{
   lastbid = button;
   lastbuttonx = x;
   lastbuttony = y;

   if(0 && bDown)
   {
      keyboard_up = !keyboard_up;
      AndroidDisplayKeyboard(keyboard_up);
   }
}

void HandleMotion(int x, int y, int mask)
{
   lastmask = mask;
   lastmotionx = x;
   lastmotiony = y;
}

#define HMX 162
#define HMY 162
short screenx, screeny;
float Heightmap[HMX * HMY];

extern struct android_app *gapp;

void HandleDestroy()
{
   printf("Destroying\n");
   exit(10);
}

volatile int suspended;

void HandleSuspend()
{
   suspended = 1;
}

void HandleResume()
{
   suspended = 0;
}

uint32_t *backdrop;

#define SQ(X) ((X) * (X))
int ring_elems;

enum
{
   NELEM_RINGS = 0x40,
};

struct target
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

void init_target(int radius)
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

int plot_colour(int cx, int cy, int x, int y, uint32_t *colour)
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

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
uint32_t *make_backdrop(int screenx, int screeny)
{
   uint32_t size = screenx * screeny * sizeof(uint32_t);
   uint32_t *ret = memset(calloc(1, size), 0xff, size);
   int radius = MIN(screenx / 2, screeny / 4) * 3 / 4;

   init_target(radius);

   for(int y = 0; y < screeny; y++)
   {
      for(int x = 0; x < screenx; x++)
      {
         uint32_t colour = 0;

         if(plot_colour(screenx / 2, radius * 4 / 3, x, y, &colour))
         {
            ret[x + y * screenx] = colour;
         }
         else if(plot_colour(screenx / 2, screeny - radius * 5 / 3, x, y, &colour))
         {
            ret[x + y * screenx] = colour;
         }
      }
   }

   return ret;
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

   backdrop = make_backdrop(screenx, screeny);

   debug("192.168.1.104", 23456, "hello\n");

   while(1)
   {
      CNFGHandleInput();

      CNFGClearFrame();
      CNFGColor(0xFFFFFF);

      CNFGUpdateScreenWithBitmap(backdrop, screenx, screeny);

      CNFGDrawBox(100, 100, 105, 105);

      CNFGSwapBuffers();

      ThisTime = OGGetAbsoluteTime();

      SecToWait = .016 - (ThisTime - LastFrameTime);
      LastFrameTime += .016;
      if(SecToWait > 0)
         OGUSleep((int)(SecToWait * 1000000));
   }

   return (0);
}
