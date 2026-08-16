#include "EmbeddedResources.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <png/png.h>
#else
#include <png.h>
#endif

namespace {

struct PngMemoryReader {
    const unsigned char* data;
    unsigned long size;
    unsigned long position;
};

static void PNGAPI readPngFromMemory(png_structp pngPtr,
                                     png_bytep output,
                                     png_size_t bytesToRead) {
    PngMemoryReader* reader = (PngMemoryReader*)png_get_io_ptr(pngPtr);
    unsigned long count = (unsigned long)bytesToRead;

    if (reader == NULL || output == NULL ||
        reader->position > reader->size ||
        count > reader->size - reader->position) {
        png_error(pngPtr, "Embedded PNG read past end of resource");
        return;
    }

    memcpy(output, reader->data + reader->position, count);
    reader->position += count;
}

} /* namespace */

const EmbeddedResourceEntry* EmbeddedResources::find(const char* name) {
    unsigned int i;

    if (name == NULL || name[0] == '\0') {
        return NULL;
    }

    for (i = 0; i < gEmbeddedResourceCount; ++i) {
        if (strcmp(gEmbeddedResources[i].name, name) == 0) {
            return &gEmbeddedResources[i];
        }
    }

    return NULL;
}

const unsigned char* EmbeddedResources::getData(const char* name,
                                                 unsigned long* size,
                                                 EmbeddedResourceType* type) {
    const EmbeddedResourceEntry* entry = find(name);

    if (size != NULL) {
        *size = 0;
    }
    if (type != NULL) {
        *type = EMBEDDED_RESOURCE_BINARY;
    }

    if (entry == NULL) {
        return NULL;
    }

    if (entry->offset > gEmbeddedResourceDataSize ||
        entry->size > gEmbeddedResourceDataSize - entry->offset) {
        fprintf(stderr, "JOndra: invalid embedded resource range: %s\n", name);
        return NULL;
    }

    if (size != NULL) {
        *size = entry->size;
    }
    if (type != NULL) {
        *type = entry->type;
    }

    return gEmbeddedResourceData + entry->offset;
}

bool EmbeddedResources::copyData(const char* name,
                                 void* destination,
                                 unsigned long expectedSize) {
    unsigned long size = 0;
    EmbeddedResourceType type = EMBEDDED_RESOURCE_BINARY;
    const unsigned char* data;

    if (destination == NULL) {
        return false;
    }

    data = getData(name, &size, &type);
    if (data == NULL || type != EMBEDDED_RESOURCE_BINARY || size != expectedSize) {
        return false;
    }

    if (size > 0) {
        memcpy(destination, data, size);
    }
    return true;
}

Fl_RGB_Image* EmbeddedResources::loadPng(const char* name) {
    unsigned long resourceSize = 0;
    EmbeddedResourceType type = EMBEDDED_RESOURCE_BINARY;
    const unsigned char* resourceData = getData(name, &resourceSize, &type);
    png_structp pngPtr = NULL;
    png_infop infoPtr = NULL;
    PngMemoryReader reader;
    png_uint_32 width = 0;
    png_uint_32 height = 0;
    int bitDepth = 0;
    int colorType = 0;
    int interlaceType = 0;
    int compressionType = 0;
    int filterMethod = 0;
    png_size_t rowBytes = 0;
    unsigned char* volatile pixels = NULL;
    png_bytep* volatile rows = NULL;
    Fl_RGB_Image* result = NULL;
    png_uint_32 y;
    int hasTransparency = 0;

    if (resourceData == NULL || type != EMBEDDED_RESOURCE_PNG ||
        resourceSize < 8 || png_sig_cmp((png_bytep)resourceData, 0, 8) != 0) {
        fprintf(stderr, "JOndra: embedded PNG not found or invalid: %s\n",
                name != NULL ? name : "(null)");
        return NULL;
    }

    pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (pngPtr == NULL) {
        return NULL;
    }

    infoPtr = png_create_info_struct(pngPtr);
    if (infoPtr == NULL) {
        png_destroy_read_struct(&pngPtr, NULL, NULL);
        return NULL;
    }

    if (setjmp(png_jmpbuf(pngPtr))) {
        if (rows != NULL) {
            delete[] (png_bytep*)rows;
        }
        if (pixels != NULL) {
            delete[] (unsigned char*)pixels;
        }
        png_destroy_read_struct(&pngPtr, &infoPtr, NULL);
        fprintf(stderr, "JOndra: cannot decode embedded PNG: %s\n",
                name != NULL ? name : "(null)");
        return NULL;
    }

    reader.data = resourceData;
    reader.size = resourceSize;
    reader.position = 0;
    png_set_read_fn(pngPtr, &reader, readPngFromMemory);

    png_read_info(pngPtr, infoPtr);
    png_get_IHDR(pngPtr, infoPtr, &width, &height, &bitDepth, &colorType,
                 &interlaceType, &compressionType, &filterMethod);

    if (width == 0 || height == 0 || width > 32767UL || height > 32767UL) {
        png_error(pngPtr, "Invalid embedded PNG dimensions");
    }

    if (bitDepth == 16) {
        png_set_strip_16(pngPtr);
    }
    if (colorType == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(pngPtr);
    }
    if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8) {
#if PNG_LIBPNG_VER >= 10400
        png_set_expand_gray_1_2_4_to_8(pngPtr);
#else
        png_set_gray_1_2_4_to_8(pngPtr);
#endif
    }

    hasTransparency = png_get_valid(pngPtr, infoPtr, PNG_INFO_tRNS) != 0;
    if (hasTransparency) {
        png_set_tRNS_to_alpha(pngPtr);
    }

    if (colorType == PNG_COLOR_TYPE_GRAY ||
        colorType == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(pngPtr);
    }

    if ((colorType & PNG_COLOR_MASK_ALPHA) == 0 && !hasTransparency) {
        png_set_filler(pngPtr, 0xff, PNG_FILLER_AFTER);
    }

    png_set_interlace_handling(pngPtr);
    png_read_update_info(pngPtr, infoPtr);

    rowBytes = png_get_rowbytes(pngPtr, infoPtr);
    if (rowBytes < width * 4UL ||
        rowBytes > 0x7fffffffUL ||
        height > 0x7fffffffUL / rowBytes) {
        png_error(pngPtr, "Invalid embedded PNG row size");
    }

    pixels = new unsigned char[(unsigned long)rowBytes * (unsigned long)height];
    rows = new png_bytep[(unsigned long)height];
    if (pixels == NULL || rows == NULL) {
        png_error(pngPtr, "Out of memory decoding embedded PNG");
    }

    for (y = 0; y < height; ++y) {
        ((png_bytep*)rows)[y] = (png_bytep)((unsigned char*)pixels +
                                (unsigned long)y * (unsigned long)rowBytes);
    }

    png_read_image(pngPtr, (png_bytepp)rows);
    png_read_end(pngPtr, infoPtr);
    png_destroy_read_struct(&pngPtr, &infoPtr, NULL);

    delete[] (png_bytep*)rows;
    rows = NULL;

    result = new Fl_RGB_Image((const unsigned char*)pixels,
                                            (int)width,
                                            (int)height,
                                            4,
                                            (int)rowBytes);
    if (result == NULL) {
        delete[] (unsigned char*)pixels;
        return NULL;
    }

    /* Fl_RGB_Image does not copy its input array. Let the image own it. */
    result->alloc_array = 1;
    pixels = NULL;
    return result;
}
