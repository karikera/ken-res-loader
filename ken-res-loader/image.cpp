#include "include/image.h"
#include "kpng.h"
#include "jpeg.h"
#include "tga.h"
#include "util.h"

#include <string.h>
#include <assert.h>
#include <stdlib.h>

using namespace kr;

#pragma pack(push, 1)
struct BMP_HEADER {
	uint16_t    bfType;
	uint32_t    bfSize;
	uint16_t    bfReserved1;
	uint16_t    bfReserved2;
	uint32_t    bfOffBits;
};

struct BITMAP_FILE
{
	uint32_t      biSize;
	int32_t       biWidth;
	int32_t       biHeight;
	uint16_t      biPlanes;
	uint16_t      biBitCount;
	uint32_t      biCompression;
	uint32_t      biSizeImage;
	int32_t       biXPelsPerMeter;
	int32_t       biYPelsPerMeter;
	uint32_t      biClrUsed;
	uint32_t      biClrImportant;

	void* getBits() const noexcept
	{
		return (uint8_t*)this + biSize;
	}
	uint32_t getLineBytes() const noexcept
	{
		return (biWidth * biBitCount + 31) >> 5 << 2;
	}
	const KrbImagePalette* getPalette() const noexcept
	{
		return (KrbImagePalette*)((uint8_t*)this + sizeof(BITMAP_FILE));
	}
};

#pragma pack(pop)



bool KEN_EXTERNAL kr::krb_load_image(KrbExtension extension, KrbImageCallback* callback, KrbFile* file)
{
	switch (extension)
	{
	case KrbExtension::ImagePng:
		return kr::backend::Png::load(callback, file);
	case KrbExtension::ImageJpeg:
	case KrbExtension::ImageJpg:
		return kr::backend::Jpeg::load(callback, file);
	case KrbExtension::ImageTga:
		return kr::backend::Tga::load(callback, file);
	case KrbExtension::ImageBmp:
	{
		BMP_HEADER bfh;
		file->read(&bfh, sizeof(bfh));
		if (bfh.bfType != "BM"_sig) return false;

		size_t tempBufferSize = bfh.bfOffBits - sizeof(bfh);
		uint8_t * tempBuffer = (uint8_t*)malloc(tempBufferSize);
		BITMAP_FILE * bi = (BITMAP_FILE*)tempBuffer;
		file->read(tempBuffer, tempBufferSize);

		size_t widthBytes = (bi->biWidth * bi->biBitCount + 7) / 8;
		widthBytes = (widthBytes + 3) & ~3;
		size_t totalBytes = widthBytes * bi->biHeight;
		if (bi->biSizeImage == 0)
			bi->biSizeImage = (int)totalBytes;

		uint8_t * imageBuffer = (uint8_t*)malloc(bi->biSizeImage);
		file->read(imageBuffer, bi->biSizeImage);

		KrbImageInfo info;
		info.width = bi->biWidth;
		info.height = bi->biHeight;
		info.pitchBytes = (uint32_t)widthBytes;

		switch (bi->biBitCount)
		{
		case 8:
			info.pixelformat = PixelFormatIndex;
			assert(callback->palette);
			memcpy(callback->palette->color, bi->getPalette(), sizeof(uint32_t) * 256);
			for (uint32_t& v : callback->palette->color)
			{
				((uint8_t*)& v)[3] = 0xff;
			}
			break;
		case 16:
			info.pixelformat = PixelFormatX1RGB5;
			break;
		case 24:
			info.pixelformat = PixelFormatRGB8;
			break;
		case 32:
			info.pixelformat = PixelFormatARGB8;
			break;
		default:
			assert(!"Not Supported Yet");
			free(imageBuffer);
			free(tempBuffer);
			return false;
		}

		uint8_t* dest = (uint8_t*)callback->start(callback, &info);
		if (!dest)
		{
			free(imageBuffer);
			free(tempBuffer);
			return false;
		}

		size_t srcWidth = info.width * bi->biBitCount / 8;
		uint8_t* src = imageBuffer + totalBytes * info.height - widthBytes;
		uint8_t* dest_end = dest + totalBytes;
		while (dest != dest_end)
		{
			memcpy(dest, src, srcWidth);
			dest += info.pitchBytes;
			src -= widthBytes;
		}

		free(imageBuffer);
		free(tempBuffer);
		return true;
	}
	default:
		return false;
	}
	return false;
}
bool KEN_EXTERNAL kr::krb_save_image(KrbExtension extension, const KrbImageSaveInfo* info, KrbFile* file)
{
	switch (extension)
	{
	case KrbExtension::ImagePng:
		return kr::backend::Png::save(info, file);
	case KrbExtension::ImageJpg:
	case KrbExtension::ImageJpeg:
		return kr::backend::Jpeg::save(info, file);
	case KrbExtension::ImageTga:
		return kr::backend::Tga::save(info, file);
	case KrbExtension::ImageBmp:
		assert(!"Not implemented yet");
		//BMP_HEADER bfh;
		//bfh.
		//krb_fwrite(file, &bfh, sizeof(bfh));
		//if (bfh.bfType != "BM"_sig) return false;

		//size_t tempBufferSize = bfh.bfOffBits - sizeof(bfh);
		//uint8_t * tempBuffer = (uint8_t*)malloc(tempBufferSize);
		//BITMAP_FILE * bi = (BITMAP_FILE*)tempBuffer;
		//krb_fread(file, tempBuffer, tempBufferSize);

		//size_t widthBytes = (bi->biWidth * bi->biBitCount + 7) / 8;
		//widthBytes = (widthBytes + 3) & ~3;
		//size_t totalBytes = widthBytes * bi->biHeight;
		//if (bi->biSizeImage == 0)
		//	bi->biSizeImage = (int)totalBytes;

		//uint8_t * imageBuffer = (uint8_t*)malloc(bi->biSizeImage);
		//krb_fread(file, imageBuffer, bi->biSizeImage);

		//KrbImageInfo info;
		//info.width = bi->biWidth;
		//info.height = bi->biHeight;
		//info.pitchBytes = widthBytes;

		//switch (bi->biBitCount)
		//{
		//case 8:
		//	info.pixelformat = PixelFormatIndex;
		//	assert(info->palette);
		//	memcpy(info->palette, bi->getPalette(), sizeof(uint32_t) * 256);
		//	for (uint32_t& v : callback->palette->color)
		//	{
		//		((uint8_t*)& v)[3] = 0xff;
		//	}
		//	break;
		//case 16:
		//	info.pixelformat = PixelFormatX1RGB5;
		//	break;
		//case 24:
		//	info.pixelformat = PixelFormatRGB8;
		//	break;
		//case 32:
		//	info.pixelformat = PixelFormatARGB8;
		//	break;
		//default:
		//	assert(!"Not Supported Yet");
		//	free(imageBuffer);
		//	free(tempBuffer);
		//	return false;
		//}

		//if (!callback->start(callback, &info))
		//{
		//	free(imageBuffer);
		//	free(tempBuffer);
		//	return false;
		//}
		//uint8_t* dest = (uint8_t*)info.data;

		//size_t srcWidth = info.width * bi->biBitCount / 8;
		//uint8_t* src = imageBuffer + totalBytes * info.height - widthBytes;
		//uint8_t* dest_end = dest + totalBytes;
		//while (dest != dest_end)
		//{
		//	memcpy(dest, src, srcWidth);
		//	dest += info.pitchBytes;
		//	src -= widthBytes;
		//}

		//free(imageBuffer);
		//free(tempBuffer);
		//return true;
		return false;
	default:
		return false;
	}
	assert(!"Not supported yet");
	return false;
}
