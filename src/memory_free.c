// src/memory_free.c
#include <stdlib.h>
#include <string.h>
#include "allegro_dat_structs.h"

void free_property(Property *p){ if(!p) return; if(p->body) free(p->body); }

void free_dat_bitmap(DatBitmap *b){ if(!b) return; if(b->image) free(b->image); free(b); }

void free_dat_rle(DatRleSprite *r){ if(!r) return; if(r->image) free(r->image); free(r); }

void free_dat_font(DatFont *f){ if(!f) return; if(f->font_size==8 && f->u.f8){ free(f->u.f8); } else if(f->font_size==16 && f->u.f16){ free(f->u.f16);} free(f);} 

void free_dat_object(DatObject *o){ if(!o) return; for(int i=0;i<o->num_properties;i++) free_property(&o->properties[i]); if(o->properties) free(o->properties); 
    if(!memcmp(o->type,"BMP ",4)) free_dat_bitmap(o->body.bmp);
    else if(!memcmp(o->type,"PAL ",4)) { if(o->body.pal) free(o->body.pal);} 
    else if(!memcmp(o->type,"RLE ",4)) free_dat_rle(o->body.rle);
    else if(!memcmp(o->type,"FONT",4)) free_dat_font(o->body.font);
}

void free_allegro_dat(AllegroDat *d){ if(!d) return; if(d->objects){ for(u32 i=0;i<d->num_objects;i++) free_dat_object(&d->objects[i]); free(d->objects);} free(d);} 
