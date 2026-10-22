#ifndef CANVAS_H
#define CANVAS_H
#include "assets.h"
#include <stdbool.h>
#include <stdint.h>
#define RGB(r,g,b) ((((r)>>3)<<11)|(((g)>>2)<<5)|((b)>>3))
typedef void (*Push)(int,int,int,int,const uint16_t *);
static uint16_t strip[320*8];
static uint32_t hashes[30];
static int band;
static inline void invalidate(void){for(int i=0;i<30;i++)hashes[i]=0;}
static inline void pixel(int x,int y,uint16_t c){if((unsigned)x<320&&(unsigned)(y-band)<8)strip[(y-band)*320+x]=c;}
static inline void rect(int x,int y,int w,int h,uint16_t c){int x0=x<0?0:x,x1=x+w>320?320:x+w,y0=y<band?band:y,y1=y+h>band+8?band+8:y+h;for(int j=y0;j<y1;j++)for(int i=x0;i<x1;i++)strip[(j-band)*320+i]=c;}
static inline void line(int x,int y,int x1,int y1,uint16_t c){int dx=x1>x?x1-x:x-x1,sx=x<x1?1:-1,dy=y1>y?y-y1:y1-y,sy=y<y1?1:-1,e=dx+dy;for(;;){pixel(x,y,c);if(x==x1&&y==y1)break;int v=2*e;if(v>=dy){e+=dy;x+=sx;}if(v<=dx){e+=dx;y+=sy;}}}
static inline void circle(int cx,int cy,int r,uint16_t c){int y0=cy-r<band?band:cy-r,y1=cy+r>band+7?band+7:cy+r;for(int y=y0;y<=y1;y++)for(int x=cx-r;x<=cx+r;x++)if((x-cx)*(x-cx)+(y-cy)*(y-cy)<=r*r)pixel(x,y,c);}
static inline uint16_t blend(uint16_t a,uint16_t b,unsigned n){unsigned r=(((a>>11)*n+(b>>11)*(3-n))+1)/3,g=((((a>>5)&63)*n+((b>>5)&63)*(3-n))+1)/3,v=(((a&31)*n+(b&31)*(3-n))+1)/3;return (uint16_t)((r<<11)|(g<<5)|v);}
static inline const Font* text_font(int size){return &fonts[size<1?0:size>3?2:size-1];}
static inline uint16_t blend_alpha(uint16_t a,uint16_t b,unsigned n){
 unsigned inv=15-n,r=((a>>11)*n+(b>>11)*inv+7)/15;
 unsigned g=(((a>>5)&63)*n+((b>>5)&63)*inv+7)/15,v=((a&31)*n+(b&31)*inv+7)/15;
 return (uint16_t)((r<<11)|(g<<5)|v);
}
static inline unsigned glyph_index(unsigned ch){return ch<32||ch>126?'?'-32:ch-32;}
static inline void glyph(int x,int y,unsigned ch,uint16_t color,int size){
 const Font*f=text_font(size);const Glyph*g=&f->glyphs[glyph_index(ch)];x+=g->xoff;y+=g->yoff;
 int y0=y<band?band:y,y1=y+g->h>band+8?band+8:y+g->h;
 for(int py=y0;py<y1;py++)for(unsigned k=0;k<g->w;k++){
  int px=x+(int)k;if((unsigned)px>=320)continue;
  unsigned i=(py-y)*g->w+k,v=f->pixels[g->offset+i/2],a=(i&1)?v&15:v>>4;
  if(a){uint16_t*p=&strip[(py-band)*320+px];*p=a==15?color:blend_alpha(color,*p,a);}
 }
}
static inline int text_width(const char*s,int size){const Font*f=text_font(size);int w=0;for(;*s;s++)w+=f->glyphs[glyph_index((unsigned char)*s)].advance;return w;}
static inline void text(int x,int y,const char*s,uint16_t c,int size){const Font*f=text_font(size);for(;*s;s++){unsigned i=glyph_index((unsigned char)*s);glyph(x,y,(unsigned char)*s,c,size);x+=f->glyphs[i].advance;}}
static inline void center(int x,int y,const char*s,uint16_t c,int scale){text(x-text_width(s,scale)/2,y,s,c,scale);}
static inline void number(int x,int y,uint32_t n,uint16_t c,int scale,bool centered){char s[11],rev[10];int count=0;do{rev[count++]=(char)('0'+n%10);n/=10;}while(n);for(int i=0;i<count;i++)s[i]=rev[count-1-i];s[count]=0;if(centered)center(x,y,s,c,scale);else text(x,y,s,c,scale);}
static inline uint16_t image_color(const Image *im,int x,int y){unsigned i=y*im->w+x,v=im->pixels[i/2];return im->palette[(i&1)?v&15:v>>4];}
static inline void image(const Image*im,int x,int y,int width,int height){int y0=y<band?band:y,y1=y+height>band+8?band+8:y+height;for(int py=y0;py<y1;py++)for(int px=x<0?0:x;px<x+width&&px<320;px++){unsigned sx=(unsigned)((px-x)*im->w/width),sy=(unsigned)((py-y)*im->h/height),i=sy*im->w+sx,v=im->pixels[i/2],n=(i&1)?v&15:v>>4;if(n)pixel(px,py,im->palette[n]);}}
static inline void outline(int x,int y,int w,int h,uint16_t c){rect(x,y,w,1,c);rect(x,y+h-1,w,1,c);rect(x,y,1,h,c);rect(x+w-1,y,1,h,c);}
static inline void present(Push push){uint32_t h=2166136261u;for(unsigned i=0;i<320*8;i++)h=(h^strip[i])*16777619u;if(h!=hashes[band/8]){hashes[band/8]=h;push(0,band,320,8,strip);}}
#endif
