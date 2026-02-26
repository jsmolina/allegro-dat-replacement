// src/dat_loader_font.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dat_loader_font.h"
#include "dat_loader_bmp.h"

static void set_bit(u8 *byte, int bit){ *byte |= (u8)(1u << (7-bit)); }

static int build_font_from_bitmap(const char *filename, int height, int threshold, DatFont **out)
{
    *out = NULL; DatBitmap *bmp=NULL; if(!load_bmp_to_dat_bitmap(filename,&bmp)) return 0;
    if(bmp->width != 95*8 || bmp->height != (u16)height){ free_dat_bitmap(bmp); return 0; }
    int bppB = bmp->bits_per_pixel/8; if(!(bppB==1 || bppB==3 || bppB==4)){ free_dat_bitmap(bmp); return 0; }
    const u8 *img = bmp->image; int W=bmp->width, H=bmp->height;
    if(height==8){
        DatFont *f = (DatFont*)calloc(1,sizeof(DatFont)); if(!f){ free_dat_bitmap(bmp); return 0; }
        f->font_size=8; f->u.f8=(DatFont8*)calloc(1,sizeof(DatFont8)); if(!f->u.f8){ free(f); free_dat_bitmap(bmp); return 0; }
        for(int c=0;c<95;c++){
            for(int y=0;y<8;y++){
                u8 row=0; for(int x=0;x<8;x++){
                    const u8 *px = img + (y*W + c*8 + x)*bppB;
                    int v = (bppB==1) ? px[0]*4 : ( (int)px[2] + (int)px[1] + (int)px[0] )/3; // approx luminance, BGR order
                    if(v>=threshold) set_bit(&row,x);
                }
                f->u.f8->chars[c][y]=row;
            }
        }
        free_dat_bitmap(bmp); *out=f; return 1;
    } else if(height==16){
        DatFont *f = (DatFont*)calloc(1,sizeof(DatFont)); if(!f){ free_dat_bitmap(bmp); return 0; }
        f->font_size=16; f->u.f16=(DatFont16*)calloc(1,sizeof(DatFont16)); if(!f->u.f16){ free(f); free_dat_bitmap(bmp); return 0; }
        for(int c=0;c<95;c++){
            for(int y=0;y<16;y++){
                u8 row=0; for(int x=0;x<8;x++){
                    const u8 *px = img + (y*W + c*8 + x)*bppB;
                    int v = (bppB==1) ? px[0]*4 : ( (int)px[2] + (int)px[1] + (int)px[0] )/3;
                    if(v>=threshold) set_bit(&row,x);
                }
                f->u.f16->chars[c][y]=row;
            }
        }
        free_dat_bitmap(bmp); *out=f; return 1;
    }
    free_dat_bitmap(bmp); return 0;
}

int build_font8_from_bmp(const char *filename, int threshold, DatFont **out){ return build_font_from_bitmap(filename,8,threshold,out);} 
int build_font16_from_bmp(const char *filename, int threshold, DatFont **out){ return build_font_from_bitmap(filename,16,threshold,out);} 
