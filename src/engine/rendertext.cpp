#include "engine.h"

#include <freetype2/ft2build.h>
#include FT_FREETYPE_H

static hashnameset<font> fonts;
// TODO: better font setup
font *curfont = NULL;

FT_Library ft;
bool init_fonts()
{
    if(FT_Init_FreeType(&ft))
    {
        return false;
    }
    return true;
}

// This was instrumental in writing:
// https://learnopengl.com/In-Practice/Text-Rendering
void newfont(char *name, char *fp, int *defaultw, int *defaulth)
{
    font *f = &fonts[name];
    if (!f->name) f->name = newstring(name);

    f->scale = *defaulth;

    FT_Face face;
    if (FT_New_Face(ft, fp, 0, &face))
    {
        conoutf(CON_ERROR, "freetype could not load font %s", fp);
    }

    FT_Set_Pixel_Sizes(face, *(unsigned int*)defaultw, *(unsigned int*)defaulth);

    f->lineheight = *defaulth + 1; // TODO: find the line height with FreeType

    f->chars.shrink(0);
    f->charoffset = 0;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction

    unsigned char c;
    for (c = 0; c < 128; c++) // hehe c++
    {
        // conoutf(CON_DEBUG, "%c", c);
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            conoutf(CON_WARN, "Font is missing glpyh %c.", c);
            continue;
        }

        // TODO: glue to the textures system
        unsigned int texid;
        glGenTextures(1, &texid);
        glBindTexture(GL_TEXTURE_2D, texid);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );

        // Swizzle to alpha
        GLint swizzlemask[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzlemask);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        font::charinfo &cinfo = f->chars.add();
        cinfo.tex = texid;
        cinfo.x = face->glyph->bitmap_left;
        cinfo.y = face->glyph->bitmap_top;
        cinfo.w = face->glyph->bitmap.width;
        cinfo.h = face->glyph->bitmap.rows;
        cinfo.advance = face->glyph->advance.x;
    }

    FT_Done_Face(face);
}

COMMANDN(font, newfont, "ssii");

void fontalias(const char *dst, const char *src)
{
}

COMMAND(fontalias, "ss");

bool setfont(const char *name)
{
    font *f = fonts.access(name);
    if(!f) return false;
    curfont = f;
    return true;
}

static vector<font *> fontstack;

void pushfont()
{
}

bool popfont()
{
    return true;
}

void gettextres(int &w, int &h)
{
}

float text_widthf(const char *str) 
{
    return 16.0;
}
    
void draw_textf(const char *fstr, int left, int top, ...)
{
}

const matrix4x3 *textmatrix = NULL;

static float draw_char(char c, float x, float y, float scale)
{
    if (!curfont->chars.inrange(c-curfont->charoffset)) return 0.0;
    font::charinfo cinfo = curfont->chars[c-curfont->charoffset];

    float x1 = x + cinfo.x;
    float y1 = y - (cinfo.h - cinfo.y);

    float x2 = x1 + cinfo.w;
    float y2 = y1 + cinfo.h;

    gle::begin(GL_QUADS);
        glBindTexture(GL_TEXTURE_2D, cinfo.tex);

        gle::attribf(x1, y1);  gle::attribf(0,0);
        gle::attribf(x2, y1);  gle::attribf(1,0);
        gle::attribf(x2, y2);  gle::attribf(1,1);
        gle::attribf(x1, y2);  gle::attribf(0,1);

    gle::end();

    return cinfo.advance;
}

//stack[sp] is current color index
static void text_color(char c, char *stack, int size, int &sp, bvec color, int a) 
{
    if(c=='s') // save color
    {   
        c = stack[sp];
        if(sp<size-1) stack[++sp] = c;
    }
    else
    {
        xtraverts += gle::end();
        if(c=='r') { if(sp > 0) --sp; c = stack[sp]; } // restore color
        else stack[sp] = c;
        switch(c)
        {
            case '0': color = bvec( 64, 255, 128); break;   // green: player talk
            case '1': color = bvec( 96, 160, 255); break;   // blue: "echo" command
            case '2': color = bvec(255, 192,  64); break;   // yellow: gameplay messages 
            case '3': color = bvec(255,  64,  64); break;   // red: important errors
            case '4': color = bvec(128, 128, 128); break;   // gray
            case '5': color = bvec(192,  64, 192); break;   // magenta
            case '6': color = bvec(255, 128,   0); break;   // orange
            case '7': color = bvec(255, 255, 255); break;   // white
            case '8': color = bvec( 96, 240, 255); break;   // cyan
            // provided color: everything else
        }
        gle::color(color, a);
    } 
}

int text_visible(const char *str, float hitx, float hity, int maxwidth)
{
    return 0;
}

//inverse of text_visible
void text_posf(const char *str, int cursor, float &cx, float &cy, int maxwidth) 
{
}

void text_boundsf(const char *str, float &width, float &height, int maxwidth)
{
}

void draw_text(const char *str, int left, int top, int r, int g, int b, int a, int cursor, int maxwidth) 
{
    bvec color(r,g,b);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    gle::color(color, a);
    gle::defvertex(2);

    gle::deftexcoord0();

    float x = left;
    float y = top;

    int i;
    for(i = 0; str[i]; i++)
    {

        char c = uchar(str[i]);
        
        // Test for special characters, like newlines
        if (c == *(unsigned char *)"\n")
        {
            x = left;
            y += curfont->lineheight;
            continue;
        }

        // For some reason the value here isn't in pixels.
        // WHY? Anyway, we need to bitshift it.
        x += ((int)draw_char(c, x, y, 1.0) >> 6);
    }
}

void reloadfonts()
{
}