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

uint32_t backdrop[256 * 256];

uint32_t *make_backdrop(int screenx, int screeny)
{
   return 0;
}

int main()
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

   for(int y = 0; y < 256; y++)
   {
      for(int x = 0; x < 256; x++)
      {
         int dx = x - screenx / 2;
         int dy = y - screeny / 4;
         
         if(dx * dx + dy * dy < 1000)
         {
            backdrop[x + y * 256] = rand();
         }
      }
   }

   while(1)
   {
      iframeno++;

      CNFGHandleInput();

      CNFGClearFrame();
      CNFGColor(0xFFFFFF);
//    CNFGGetDimensions(&screenx, &screeny);

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

      CNFGUpdateScreenWithBitmap(backdrop, 256, 256);

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
