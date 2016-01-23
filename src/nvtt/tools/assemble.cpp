// Copyright NVIDIA Corporation 2007 -- Ignacio Castano <icastano@nvidia.com>
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include <cstdio>
#include <cstring>
#include <vector>

#include "cmdline.h"

#include "nvimage/DirectDrawSurface.h"

#include "nvcore/Array.inl"
#include "nvcore/StrLib.h"
#include "nvcore/StdStream.h"


struct ImageData {

	nv::Path               file;
	nv::DirectDrawSurface *dds;

	uint face;

};



bool ProcessCommandLine(
		int                     argc,
		char                  **argv,
		std::vector<nv::Path>  &files,
		nv::Path               &output,
		bool                   &assemble_array,
		bool                   &assemble_cubemap)
{

	assemble_cubemap = false;
	assemble_array   = false;

	output = "nvout.dds";

	bool output_specified = false;

	for (int n=1; n < argc; n++) {

		if (std::strcmp("-cube", argv[n]) == 0)
			assemble_cubemap = true;

		else if (std::strcmp("-array", argv[n]) == 0)
			assemble_array = true;

		else if (std::strcmp("-o", argv[n]) == 0)
			output_specified = true;

		else if (argv[n][0] != '-') {

			if (output_specified) {

				output           = argv[n];
				output_specified = false;

			} else
				files.push_back(argv[n]);

		} else
			printf("Warning: Unrecognized option \"%s\"\n", argv[n]);

	}

	if (files.empty()) {

		printf("\nUsage: nvassemble [-cube] [-array] [-o output] image0.dds image1.dds ... imageN.dds\n\n");
		return false;

	}

	if (nv::strCaseDiff(output.extension(), ".dds") != 0)
		output.append(".dds");

	return true;

}



bool GatherSourceImages(
		std::vector<nv::Path>  &files,
		std::vector<ImageData> &images,
		uint                   &image_width,
		uint                   &image_height,
		uint                   &image_depth,
		uint                   &image_format,
		uint                   &image_mipmaps)
{

	for (int n=0; n < files.size(); n++) {

		images.push_back(ImageData());

		ImageData &image = images.back();

		image.file = files[n];
		image.dds  = new nv::DirectDrawSurface();
		image.face = 0;

		if (!image.dds->load(files[n].str())) {

			printf("Error: Unable to load %s!\n", files[n].str());
			return false;

		}

		const uint faces  = image.dds->arrayCount() * (image.dds->isTextureCube()? 6: 1);
		const uint format = image.dds->header.dx10Format();

		printf("%s: %dx%dx%d %d %#x%s%s (%d)\n",
			files[n].str(),
			image.dds->width(), image.dds->height(), image.dds->depth(),
			image.dds->mipmapCount(),
			format,
			image.dds->isTextureCube()?  " [CUBE]":  "",
			image.dds->isTextureArray()? " [ARRAY]": "",
			faces);

		if (n > 0) {

			if (image.dds->width()       != image_width  ||
				image.dds->height()      != image_height ||
				image.dds->depth()       != image_depth  ||
				format                   != image_format ||
				image.dds->mipmapCount() != image_mipmaps) {

				printf("Error: Image format does not match!\n");
				return false;

			}

		} else {

			image_width   = image.dds->width();
			image_height  = image.dds->height();
			image_depth   = image.dds->depth();
			image_format  = format;
			image_mipmaps = image.dds->mipmapCount();

		}

		for (int m=1; m < faces; m++) {

			images.push_back(ImageData());

			images.back().dds  = image.dds;
			images.back().face = m;

		}

	}

	return true;
}



bool StitchFinalImage(
		const nv::Path               &output,
		const std::vector<ImageData> &images,
		bool                          assemble_array,
		bool                          assemble_cubemap,
		uint                          image_width,
		uint                          image_height,
		uint                          image_depth,
		uint                          image_format,
		uint                          image_mipmaps)
{

	const uint facecount = images.size();
 	const uint expected  = assemble_cubemap? 6: 1;

	if (assemble_array) {

		if (facecount % expected != 0) {

			printf("Error: Expected a multiple of %d images, but %d were specified\n", expected, facecount);
			return false;

		}

	} else {

		if (facecount != expected) {

			printf("Error: Expected %d images, but %d were specified\n", expected, facecount);
			return false;

		}

	}

	if (assemble_cubemap && image_depth > 1) {

		printf("Error: Cannot assemble a cubemap with volume textures\n");
		return false;

	}

	nv::StdOutputStream stream(output.str());

	if (stream.isError()) {

		printf("Error: Failed to open '%s' for writting\n", output.str());
		return 1;

	}

	nv::DDSHeader header;

	header.setTexture2D();
	header.setWidth( image_width);
	header.setHeight(image_height);
	header.setDX10Format( image_format);
	header.setMipmapCount(image_mipmaps);

	if (assemble_cubemap) {

		header.setTextureCube();

	} else if (image_depth > 1) {

		header.setTexture3D();
		header.setDepth(image_depth);

	}

	if (assemble_array)
		header.setArrayCount(facecount / expected);

	stream << header;

	const bool block = header.isBlockFormat();
	const uint step  = block? 4: 1;

	uint mipsize[32];

	for (int m=0; m < image_mipmaps; m++)
		mipsize[m] = images[0].dds->surfaceSize(m);

	unsigned char *pixels = new unsigned char[mipsize[0]];

	for (int f=0; f < facecount; f++)
		for (int m=0; m < image_mipmaps; m++)
			if (!images[f].dds->readSurface(images[f].face, m, pixels, mipsize[m]) ||
				stream.serialize(pixels, mipsize[m]) != mipsize[m]) {

 				printf("Error: Failed to copy mipmap %d of face %d (%s)!\n", m, f + 1, images[f].file.str());
				return false;

			}

	printf("Operation complete.\n");
	return true;

}



void DestroyImageArray(std::vector<ImageData> &images)
{

	while (!images.empty()) {

		ImageData             &image = images.back();
		nv::DirectDrawSurface *dds   = image.dds;

		delete dds;

		while (!images.empty() && images.back().dds == dds)
			images.pop_back();

	}

}



int main(int argc, char *argv[])
{

	MyAssertHandler  assert_handler;
	MyMessageHandler message_handler;

	bool assemble_array;
	bool assemble_cubemap;

	nv::Path output;

	uint image_width, image_height, image_depth;
	uint image_format;
	uint image_mipmaps;

	std::vector<nv::Path>  files;
	std::vector<ImageData> images;

	printf("NVIDIA Texture Tools - Copyright NVIDIA Corporation 2007\n");

	if (!ProcessCommandLine(
			argc,           argv,
			files,          output,
			assemble_array, assemble_cubemap))
		return 1;

	if (!GatherSourceImages(
			files,        images,
			image_width,  image_height, image_depth,
			image_format, image_mipmaps)) {

		DestroyImageArray(images);
		return 2;

	}

	if (!StitchFinalImage(
			output,
			images,
			assemble_array, assemble_cubemap,
			image_width,    image_height, image_depth, image_format, image_mipmaps)) {

		DestroyImageArray(images);
		return 3;

	}

	DestroyImageArray(images);
	return 0;

}

