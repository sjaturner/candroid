#include "font_coord.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <ctype.h>

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

void code_tile(int encoding)
{
   struct font_coord codes[Y_ELEMS * X_ELEMS][2] = { };
   int encoded = 0;
   int empty = 1;

   do
   {
      int max = -INT_MAX;
      struct font_coord
      {
         int row;
         int col;
      }
      best[2] = { };

      empty = 1;

      for(int col = 0; col < Y_ELEMS; ++col)
      {
         for(int row = 0; row < X_ELEMS; ++row)
         {
            int pixel = tiles[encoding][col][row] == '#';

            if(pixel)
            {
               empty = 0;
               int extend = 0;

               for(extend = 0; extend + row + 1 < X_ELEMS && tiles[encoding][col][row + extend + 1] == '#'; ++extend)
               {
               }

               if(extend > max)
               {
                  max = extend;
                  best[0].row = row;
                  best[0].col = col;
                  best[1].row = row + extend;
                  best[1].col = col;
               }

               for(extend = 0; extend + col + 1 < Y_ELEMS && tiles[encoding][col + extend + 1][row] == '#'; ++extend)
               {
               }

               if(extend > max)
               {
                  max = extend;
                  best[0].row = row;
                  best[0].col = col;
                  best[1].row = row;
                  best[1].col = col + extend;
               }
            }
         }
      }

      if(max >= 0)
      {
         codes[encoded][0].row = best[0].row;
         codes[encoded][0].col = best[0].col;
         codes[encoded][1].row = best[1].row;
         codes[encoded][1].col = best[1].col;
         ++encoded;

         for(int row = best[0].row; row <= best[1].row; ++row)
         {
            for(int col = best[0].col; col <= best[1].col; ++col)
            {
               tiles[encoding][col][row] = '@';
            }
         }
      }
   }
   while(!empty);

   printf("static struct font_coord _0x%02x_font_coords[] = {\n", encoding);
   if(isprint(encoding))
   {
      printf(" /* \"%c\" */", encoding);
   }
   printf("\n");
   for(int index = 0; index < encoded; ++index)
   {
      printf("   {%2d,%2d,}, {%2d,%2d,},\n", codes[index][0].row, codes[index][0].col, codes[index][1].row, codes[index][1].col);
   }
   printf("};\n");
   printf("static struct font_strokes _0x%02x_font_strokes = { .length = %d, .font_coords = _0x%02x_font_coords, };\n", encoding, encoded, encoding);
}

int main(int argc, char *argv[])
{
   char buf[0x100] = { };
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
         char junk[0x100] = { };

         if(2 != sscanf(buf, "%s %d", junk, &encoding))
         {
            assert(0);
         }
      }
      else if(strstr(buf, "BITMAP"))
      {
         y = 0;
         bits = 1;
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
            for(int bit = 8; bit > 0; --bit, ++col)
            {
               int row = 13 - y;

               assert(encoding < 0x100);
               assert(row >= 0 && row < Y_ELEMS);
               assert(col >= 0 && col < X_ELEMS);

               tiles[encoding][row][col] = pattern & 1 << bit ? '#' : ' ';
            }
         }

         ++y;
      }

      memset(buf, 0, sizeof buf);
   }

   printf("#include \"font_coord.h\"\n");
   for(int encoding = 0; encoding < 0x100; ++encoding)
   {
      if(isprint(encoding))
      {
         code_tile(encoding);
      }
//    dump_tile(encoding);
   }

   if(argc > 1)
   {
      printf("struct font_strokes *font_strokes_%s[0x100] = {", argv[1]);
   }
   else
   {
      printf("struct font_strokes *font_strokes[0x100] = {");
   }

   for(int encoding = 0; encoding < 0x100; ++encoding)
   {
      if(isprint(encoding))
      {
         printf("   [0x%02x] = &_0x%02x_font_strokes,", encoding, encoding);
         printf(" /* \"%c\" */", encoding);
         printf("\n");
      }
   }
   printf("};\n");
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
