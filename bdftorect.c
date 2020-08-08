#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

enum
{
   Y_ELEMS = 0x20,
   X_ELEMS = 0x20,
};
char tiles[0x100][Y_ELEMS][X_ELEMS];

void dump_tile(int encoding)
{
   for(int col = 0; col < Y_ELEMS; ++col)
   {
      for(int row = 0; row < X_ELEMS; ++row)
      {
         int pixel = tiles[encoding][col][row];

         if(pixel)
         {
            printf("%c", pixel);
         }
      }
      printf("\n");
   }
   printf("-");
   printf("\n");
}

int main()
{
   char buf[0x100] = {};
   int encoding = 0;
   int y = 0;
   int bits = 0;

   while(fgets(buf, sizeof buf, stdin))
   {
      char *nl = strpbrk(buf, "\r\n");

      
      assert(nl);

      *nl = 0;

      if(strstr(buf, "CHARSET_ENCODING"))
      {
      }
      else if(strstr(buf, "ENCODING"))
      {
         char junk[0x100] = {};

         if(2 != sscanf(buf, "%s %d", junk, &encoding))
         {
            assert(0);
         }

         printf("ENCODING %d\n", encoding);
      }
      else if(strstr(buf, "BITMAP"))
      {
         y = 0;
         bits = 1;
//       printf("\n");
      }
      else if(strstr(buf, "ENDCHAR"))
      {
         y = 0;
         bits = 0;
      }

      if(bits)
      {
         unsigned long pattern = strtoul(buf, 0, 16);

         if(y >= 1 && y <= 13)
         {
            int col = 0;
//          printf("%02d ", y);
            for(int bit = 8; bit > 0; --bit, ++col)
            {
               int row = 13 - y;

//             printf("%c", pattern & 1 << bit ? '#' : ' ');
               assert(encoding < 0x100);
               assert(row >= 0 && row < Y_ELEMS);
               assert(col >= 0 && col < X_ELEMS);

               tiles[encoding][row][col] = pattern & 1 << bit ? '#' : ' ';
            }
//          printf("\n");
         }

         ++y;
      }

      memset(buf, 0, sizeof buf);
   }

   for(int encoding = 0; encoding < 0x100; ++encoding)
   {
      dump_tile(encoding);
   }
}

/*
   STARTCHAR ydieresis
#  ENCODING 255
   SWIDTH 480 0
   DWIDTH 6 0
   BBX 6 13 0 -2
#  BITMAP
#  ENDCHAR
*/

