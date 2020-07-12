//Copyright (c) 2011 <>< Charles Lohr - Under the MIT/x11 or NewBSD License you choose.

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "os_generic.h"

#define CNFG3D
#define CNFG_IMPLEMENTATION
#include "CNFG.h"

void HandleKey(int keycode, int bDown)
{
   if(keycode == 65307)
      exit(0);
   printf("Key: %d -> %d\n", keycode, bDown);
}

void HandleButton(int x, int y, int button, int bDown)
{
   printf("Button: %d,%d (%d) -> %d\n", x, y, button, bDown);
}

void HandleMotion(int x, int y, int mask)
{
   printf("Motion: %d,%d (%d)\n", x, y, mask);
}

void HandleDestroy()
{
   printf("Destroying\n");
   exit(10);
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
   BE = 0x0000ff,
   RD = 0xff0000,
   YO = 0xffff00,
};

void init_target(int radius)
{
   struct
   {
      uint32_t outer;
      uint32_t inner;
   }
   colours[] = 
   {
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

int dross()
{
   double ThisTime;
   double LastFPSTime = OGGetAbsoluteTime();
   double LastFrameTime = OGGetAbsoluteTime();
   double SecToWait;
   short screenx, screeny;
   unsigned frames = 0;
   unsigned long iframeno = 0;

   CNFGBGColor = 0x800000;
   CNFGDialogColor = 0x444444;
   CNFGSetup("Test Bench", 270, 480);

   CNFGGetDimensions(&screenx, &screeny);

   backdrop = make_backdrop(screenx, screeny);

   while(1)
   {
      iframeno++;

      CNFGHandleInput();

      CNFGClearFrame();
      CNFGColor(0xFFFFFF);

      if(0)
      CNFGDrawBox(0, 0, 260, 260);

      CNFGPenX = 10;
      CNFGPenY = 10;

      if(0)
      {
         // Text
         CNFGColor(0xffffff);
         for(int i = 0; i < 1; i++)
         {
            int c;
            char tw[2] = { 0, 0 };
            for(c = 0; c < 256; c++)
            {
               tw[0] = c;

               CNFGPenX = (c % 16) * 16 + 5;
               CNFGPenY = (c / 16) * 16 + 5;
               CNFGDrawText(tw, 2);
            }
         }
      }

      if(0)
      {
         // Green triangles
         CNFGPenX = 0;
         CNFGPenY = 0;

         for(int i = 0; i < 400; i++)
         {
            RDPoint pp[3];
            CNFGColor(0x00FF00);
            pp[0].x = (short)(50 * sin((float)(i + iframeno) * .01) + (i % 20) * 30);
            pp[0].y = (short)(50 * cos((float)(i + iframeno) * .01) + (i / 20) * 20);
            pp[1].x = (short)(20 * sin((float)(i + iframeno) * .01) + (i % 20) * 30);
            pp[1].y = (short)(50 * cos((float)(i + iframeno) * .01) + (i / 20) * 20);
            pp[2].x = (short)(10 * sin((float)(i + iframeno) * .01) + (i % 20) * 30);
            pp[2].y = (short)(30 * cos((float)(i + iframeno) * .01) + (i / 20) * 20);
            CNFGTackPoly(pp, 3);
         }
      }

      CNFGUpdateScreenWithBitmap(backdrop, screenx, screeny);

      frames++;
      CNFGSwapBuffers();

      ThisTime = OGGetAbsoluteTime();
      if(ThisTime > LastFPSTime + 1)
      {
         printf("FPS: %d\n", frames);
         frames = 0;
         LastFPSTime += 1;
      }

      SecToWait = .016 - (ThisTime - LastFrameTime);
      LastFrameTime += .016;
      if(SecToWait > 0)
         OGUSleep((int)(SecToWait * 1000000));

   }

   return (0);
}

int main()
{
   double ThisTime;
   double LastFrameTime = OGGetAbsoluteTime();
   double SecToWait;
   short screenx, screeny;

   CNFGBGColor = 0x800000;
   CNFGDialogColor = 0x444444;
   CNFGSetup("Test Bench", 270, 480);

   CNFGGetDimensions(&screenx, &screeny);

   backdrop = make_backdrop(screenx, screeny);

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
