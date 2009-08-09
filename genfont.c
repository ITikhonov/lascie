#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>

#define MAX(x,y) (((x)>(y))?(x):(y))

int main() {
	FT_Library lib;
	FT_Init_FreeType(&lib);
	FT_Face face;
	FT_Matrix mt;

	mt.xx=mt.yy=0x10000/2;
	mt.xy=0;
	mt.yx=0;

	FT_New_Face(lib,"/usr/share/fonts/truetype/ms/times.ttf",0,&face);
	FT_Set_Char_Size(face,12*64,0,72*2,72*2);

	printf("struct chare { int w,h,l,t,a; unsigned char *p; } chars[] = {");
	int i=0;

	FT_Vector zr = {0,0};
	int mh=0;
	for(;i<=0x7f;i++) {
		FT_Set_Transform(face,&mt,&zr);
		FT_Load_Char(face,i,FT_LOAD_RENDER);
		int h=face->glyph->bitmap.rows;
		int w=face->glyph->bitmap.width;

		if(i>0x20 && i!='\\') { printf("// %c\n",i); }
		printf("{%d,%d,%d,%d,%d,\n",w,h,face->glyph->bitmap_left,face->glyph->bitmap_top,(int)(face->glyph->advance.x/64));
		if(h>0) printf("(unsigned char *)\n");
		printf("// %d\n", face->glyph->bitmap.pixel_mode);
		printf("// %d\n", face->glyph->bitmap.num_grays);
		printf("// %d\n", face->glyph->bitmap.pitch);
		mh=MAX(h,mh);

		int x=0,y=0;
		unsigned char *i=face->glyph->bitmap.buffer;
		for(y=0;y<h;y++) {
			printf("	\"");
			for(x=0;x<w;x++) {
				printf("\\x%02x",*i++);
			}
			i+=w-face->glyph->bitmap.pitch;
			printf("\"\n");
		}
		printf("},\n");
	}
	printf("};\n");
	printf("int charsh=%d;\n",mh);
	return 0;
}

