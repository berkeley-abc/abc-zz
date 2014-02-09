//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Png.hh
//| Author(s)   : Niklas Een
//| Module      : Graphics
//| Description : Read and write PNG files.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Png.hh"

#include "png.h"


namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// PNG reader/writer:


// Returns FALSE if file does not exist or is not a PNG file (or some internal error occurred).
bool readPng(String filename, Bitmap& bm)
{
    assert(bm.data == NULL);

    FILE* in = fopen(filename.c_str(), "rb");
    if (in == NULL)
        return false;       // No such file

    png_byte header[8];
    size_t dummy ___unused = fread(header, 1, 8, in);
    if (png_sig_cmp(header, 0, 8))
        return false;       // Not a PNG file

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL)
        return false;       // Failed...

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL)
        return false;       // Failed...

    if (setjmp(png_jmpbuf(png_ptr)))
        return false;       // Failed...

    png_init_io(png_ptr, in);
    png_set_sig_bytes(png_ptr, 8);

    png_read_info(png_ptr, info_ptr);


    // Create bitmap:
    uint width  = png_get_image_width (png_ptr, info_ptr);
    uint height = png_get_image_height(png_ptr, info_ptr);
    bm.init(width, height, xmalloc<Color>(width * height), true);

    // Normalize image:
    png_set_expand(png_ptr);
    png_set_strip_16(png_ptr);      // (don't care about this now)
    //**/png_set_gamma(png_ptr, 3.0, 1.0);

    //int number_of_passes = png_set_interlace_handling(png_ptr);   // ???
    png_read_update_info(png_ptr, info_ptr);


    // Read file:
    if (setjmp(png_jmpbuf(png_ptr))){
        bm.clear();
        return false; }

    png_bytep* row_pointers = xmalloc<png_bytep>(bm.height);
    for (uint y = 0; y < bm.height; y++){
        row_pointers[y] = xmalloc<png_byte>(png_get_rowbytes(png_ptr, info_ptr)); }
    png_read_image(png_ptr, row_pointers);

    fclose(in);


    // Convert to bitmap:
#if 0
    printf("width = %lu\n", info_ptr->width);
    printf("height = %lu\n", info_ptr->height);

    printf("valid = %lu\n", info_ptr->valid);
    printf("rowbytes = %lu\n", info_ptr->rowbytes);
    printf("palette = %p\n", info_ptr->palette);

    printf("num_palette = %d\n", info_ptr->num_palette);
    printf("num_trans = %d\n", info_ptr->num_trans);

    printf("bit_depth = %d\n", info_ptr->bit_depth);
    printf("color_type = %d\n", info_ptr->color_type);

    printf("compression_type = %d\n", info_ptr->compression_type);
    printf("filter_type = %d\n", info_ptr->filter_type);
    printf("interlace_type = %d\n", info_ptr->interlace_type);

    printf("channels = %d\n", info_ptr->channels);
    printf("pixel_depth = %d\n", info_ptr->pixel_depth);
    printf("spare_byte = %d\n", info_ptr->spare_byte);

    //exit(0);
#endif

    bool ret = true;
    uchar color_type = png_get_color_type(png_ptr, info_ptr);
    if (color_type == PNG_COLOR_TYPE_RGB){
        for (uint y = 0; y < bm.height; y++){
            png_byte* row = row_pointers[y];
            for (uint x = 0; x < bm.width; x++){
                png_byte* ptr = &(row[x*3]);
                bm(x, y) = Color(ptr[0], ptr[1], ptr[2], 255);
            }
        }

    }else if (color_type == PNG_COLOR_TYPE_RGBA){
        for (uint y = 0; y < bm.height; y++){
            png_byte* row = row_pointers[y];
            for (uint x = 0; x < bm.width; x++){
                png_byte* ptr = &(row[x*4]);
                bm(x, y) = Color(ptr[0], ptr[1], ptr[2], ptr[3]);
            }
        }

    }else if (color_type == PNG_COLOR_TYPE_GRAY){
        for (uint y = 0; y < bm.height; y++){
            png_byte* row = row_pointers[y];
            for (uint x = 0; x < bm.width; x++){
                png_byte* ptr = &(row[x]);
                bm(x, y) = Color(255-ptr[0], 255-ptr[0], 255-ptr[0], 255);
            }
        }

    }else if (color_type == PNG_COLOR_TYPE_GA){
        for (uint y = 0; y < bm.height; y++){
            png_byte* row = row_pointers[y];
            for (uint x = 0; x < bm.width; x++){
                png_byte* ptr = &(row[x*2]);
                bm(x, y) = Color(255-ptr[0], 255-ptr[0], 255-ptr[0], ptr[1]);
            }
        }

    }else{
        bm.clear();
        ret = false;
    }

    // Dispose:
    for (uint y = 0; y < bm.height; y++)
        xfree(row_pointers[y]);
    xfree(row_pointers);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    return ret;
}


bool writePng(String filename, const Bitmap& bm)
{
    FILE* out = fopen(filename.c_str(), "wb");
    if (out == NULL)
        return false;       // Could not create file

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL)
        return false;       // Failed...

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL)
        return false;       // Failed...

    if (setjmp(png_jmpbuf(png_ptr)))
        return false;       // Failed...

    png_init_io(png_ptr, out);

    if (setjmp(png_jmpbuf(png_ptr)))        // (why again?)
        return false;       // Failed...

    png_set_IHDR(png_ptr, info_ptr, bm.width, bm.height,
             /*bitdepth*/8, /*colortype*/PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
             PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);

    if (setjmp(png_jmpbuf(png_ptr)))        // (why again??)
        return false;       // Failed...

    png_bytep* row_pointers = xmalloc<png_bytep>(bm.height);
    for (uint y = 0; y < bm.height; y++){
        row_pointers[y] = xmalloc<png_byte>(bm.width * 4);
        for (uint x = 0; x < bm.width; x++){
            Color c = bm(x, y);
            row_pointers[y][4*x  ] = c.r;
            row_pointers[y][4*x+1] = c.g;
            row_pointers[y][4*x+2] = c.b;
            row_pointers[y][4*x+3] = c.a;
        }
    }
    png_write_image(png_ptr, row_pointers);

    for (uint y = 0; y < bm.height; y++)
        xfree(row_pointers[y]);
    xfree(row_pointers);

    if (setjmp(png_jmpbuf(png_ptr)))        // (why again??)
        return false;       // Failed...

    png_write_end(png_ptr, NULL);

    fclose(out);
    return true;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
