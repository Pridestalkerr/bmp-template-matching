#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>

#define HEADER_LENGTH 54

typedef unsigned char u8;
typedef unsigned int u32;

typedef struct{
	u8 R;
	u8 G;
	u8 B;
}RGB;

typedef struct{
	u32 x;
	u32 y;
	u32 height;
	u32 width;
	double correlation;
	u8 color;
}match;

typedef struct{
	u32 size;
	match *list;
}matchList;

int min(const int lhs, const int rhs)
{
	if(lhs <= rhs)
		return lhs;
	else
		return rhs;
}

int max(const int lhs, const int rhs)
{
	if(lhs >= rhs)
		return lhs;
	else
		return rhs;
}

int cmpCorr(const void *lhs, const void *rhs)
{
	if(((match*) rhs)->correlation - ((match*) lhs)->correlation < 0)
		return -1;
	else
		return 1;
}

double superposition(int xa1, int xa2, int ya1, int ya2, int xb1, int xb2, int yb1, int yb2)
{
	double intersectionArea = max(0, min(xa2, xb2) - max(xa1, xb1)) * max(0, min(ya2, yb2) - max(ya1, yb1));
	double unionArea = (xa2 - xa1) * (ya2 - ya1) + (xb2 - xb1) * (yb2 - yb1) - intersectionArea;

	return intersectionArea / unionArea;
}

RGB* grayscale(RGB *pixels, u32 height, u32 width)
{
	int itr, jtr;
	RGB *grayscalePixels = (RGB*) malloc(height * width * sizeof(RGB));

	for(itr = 0; itr < height; ++itr)
		for(jtr = 0; jtr < width; ++jtr)
			grayscalePixels[itr * width + jtr].B =
			grayscalePixels[itr * width + jtr].G =
			grayscalePixels[itr * width + jtr].R =
			0.333 * pixels[itr * width + jtr].R + 
			0.333 * pixels[itr * width + jtr].G + 
			0.334 * pixels[itr * width + jtr].B;

	return grayscalePixels;
}

matchList matchSample(RGB *pixels, u32 height, u32 width, RGB *sample, u32 sampleHeight, u32 sampleWidth, double threshold, u8 color)
{
	//this function is horrifying tread carefully (check for cross correlation on wikipedia)
	int itr, jtr, ktr, ltr;
	u32 sampleSize = sampleWidth * sampleHeight;
	double sampleAvg = 0, avg = 0, sampleStdDev = 0, stdDev = 0, correlation = 0;

	//sample average
	for(itr = 0; itr < sampleSize; ++itr)
		sampleAvg += (double) sample[itr].B;
	sampleAvg /= (double) sampleSize;
    
	//sample standard deviation
	for(itr = 0; itr < sampleSize; ++itr)
		sampleStdDev += ((double) sample[itr].B - sampleAvg) * ((double) sample[itr].B - sampleAvg);
	sampleStdDev = sqrt(sampleStdDev / (double) (sampleSize - 1));

	matchList matches;
	matches.list = NULL;
	matches.size = 0;

	for(itr = 0; itr <= height - sampleHeight; ++itr)
		for(jtr = 0; jtr <= width - sampleHeight; ++jtr)
		{
			avg = 0, stdDev = 0, correlation = 0;

			//image average
			for(ktr = itr; ktr < itr + sampleHeight; ++ktr)
				for(ltr = jtr; ltr < jtr + sampleWidth; ++ltr)
					avg += (double) pixels[ktr * width + jtr].B;
			avg /= (double) sampleSize;

			//image standard deviation
			for(ktr = itr; ktr < itr + sampleHeight; ++ktr)
				for(ltr = jtr; ltr < jtr + sampleWidth; ++ltr)
					stdDev += ((double) pixels[ktr * width + ltr].B - avg) * ((double) pixels[ ktr * width + ltr].B - avg);
			stdDev = sqrt(stdDev / (double) sampleSize);

			//correlation
			for(ktr = itr; ktr < itr + sampleHeight; ++ktr)
				for(ltr = jtr; ltr < jtr + sampleWidth; ++ltr)
					correlation += (((double) pixels[ktr * width + ltr].B - avg) * ((double) sample[(ktr - itr) * sampleWidth + ltr - jtr].B - sampleAvg)) / (sampleStdDev * stdDev);
			correlation /= (double) sampleSize;

			if(correlation >= threshold)
			{
				if(matches.list == NULL)
				{
					matches.list = (match*) malloc(sizeof(match));
					matches.size = 1;
				}
				else
					matches.list = (match*) realloc(matches.list, (++matches.size) * sizeof(match));

				matches.list[matches.size - 1].y = itr;
				matches.list[matches.size - 1].x = jtr;
				matches.list[matches.size - 1].height = sampleHeight;
				matches.list[matches.size - 1].width = sampleWidth;
				matches.list[matches.size - 1].correlation = correlation;
				matches.list[matches.size - 1].color = color;
			}
		}

	return matches;
}

void nonMaxSuppression(matchList matches, double threshold)
{
	int itr, jtr;
	for(itr = 0; itr < matches.size - 1; ++itr)
	{
		if(matches.list[itr].correlation != -1)
		for(jtr = itr + 1; jtr < matches.size; ++jtr)
		{
			double sp = superposition(	matches.list[itr].x, matches.list[itr].x + matches.list[itr].width, 
							matches.list[itr].y, matches.list[itr].y + matches.list[itr].height, 
							matches.list[jtr].x, matches.list[jtr].x + matches.list[jtr].width, 
							matches.list[jtr].y, matches.list[jtr].y + matches.list[jtr].height);
			if(sp > threshold)
				matches.list[jtr].correlation = -1;
		}
	}
}

void highlightMatch(RGB *pixels, u32 height, u32 width, match box, RGB color)
{
	int itr;
	for(itr = box.y; itr < box.y + box.height; ++itr)
	{
		pixels[itr * width + box.x] = color;
		pixels[itr * width + box.x + box.width] = color;
	}
	for(itr = box.x; itr < box.x + box.width; ++itr)
	{
		pixels[box.y * width + itr] = color;
		pixels[(box.y + box.height) * width + itr] = color;
	}
}

void readBMPSize(u32 *height, u32 *width, FILE *in)
{
	fseek(in, 18, SEEK_SET);
	fread(width, sizeof(int), 1, in);
	fseek(in, 22, SEEK_SET);
	fread(height, sizeof(int), 1, in);
}

RGB* readPixelsFromBMP(u32 height, u32 width, FILE *in)
{
	int itr, jtr;
	u32 padding = (4 - (width * 3 % 4)) % 4;
	RGB *pixels = (RGB*) malloc(width * height * sizeof(RGB));

	fseek(in, HEADER_LENGTH, SEEK_SET);

	for(itr = height - 1; itr >= 0; --itr)
	{
		for(jtr = 0; jtr < width; ++jtr)
		{
			fread(&pixels[itr * width + jtr].B, 1, 1, in);
			fread(&pixels[itr * width + jtr].G, 1, 1, in);
			fread(&pixels[itr * width + jtr].R, 1, 1, in);
		}
		fseek(in, padding, SEEK_CUR);
	}

	return pixels;
}

void copyHeader(FILE *from, FILE *to)
{
	fseek(from, 0, SEEK_SET);
	fseek(to, 0, SEEK_SET);

	u8 *header = (u8*) malloc(HEADER_LENGTH);

	fread(header, HEADER_LENGTH, 1, from);
	fwrite(header, HEADER_LENGTH, 1, to);

	free(header);
}

void writePixelsToBMP(RGB *pixels, u32 height, u32 width, FILE *out)
{
	int itr, jtr;
	u32 padding = (4 - (width * 3 % 4)) % 4;
	u8 *paddingBytes = (u8*) calloc(padding, 1);

	fseek(out, HEADER_LENGTH, SEEK_SET);

	for(itr = height - 1; itr >= 0; --itr)
	{
		for(jtr = 0; jtr < width; ++jtr)
		{
			fwrite(&pixels[itr * width + jtr].B, 1, 1, out);
			fwrite(&pixels[itr * width + jtr].G, 1, 1, out);
			fwrite(&pixels[itr * width + jtr].R, 1, 1, out);
		}
		fwrite(paddingBytes, 1, padding, out);
	}

	free(paddingBytes);
}

void handle(FILE *in, FILE *out, FILE *list, double matchTh, double overlapTh)
{
	int itr;

	u32 height, width;
	readBMPSize(&height, &width, in);

	RGB *pixels = readPixelsFromBMP(height, width, in);

	RGB *grayscalePixels = grayscale(pixels, height, width);

	RGB colors[10];
	colors[0].R = 255;		colors[0].G = 0;		colors[0].B = 0;		//red
	colors[1].R = 255;		colors[1].G = 255;		colors[1].B = 0;		//yellow
	colors[2].R = 0;		colors[2].G = 255;		colors[2].B = 0;		//green
	colors[3].R = 0;		colors[3].G = 255;		colors[3].B = 255;		//cyan
	colors[4].R = 255;		colors[4].G = 0;		colors[4].B = 255;		//magenta
	colors[5].R = 0;		colors[5].G = 0;		colors[5].B = 255;		//blue
	colors[6].R = 192;		colors[6].G = 192;		colors[6].B = 192;		//silver
	colors[7].R = 255;		colors[7].G = 140;		colors[7].B = 0;		//blue
	colors[8].R = 128;		colors[8].G = 0;		colors[8].B = 128;		//magenta
	colors[9].R = 128;		colors[9].G = 0;		colors[9].B = 0;		//blue

	matchList matches;
	matches.list = NULL;
	matches.size = 0;

	for(itr = 0; itr < 10; ++itr)
	{
		printf("\r%d%%", itr * 10);
		fflush(stdout);

		char sampleFilename[100];
		if(fgets(sampleFilename, 100, list) == NULL)
			break;

		sampleFilename[strlen(sampleFilename) - 1] = '\0';

		FILE *sampleIn = fopen(sampleFilename, "rb");
		if(sampleIn == NULL)
		{
			printf("Couldn't find '%s' ... skipping.\n", sampleFilename);
			continue;
		}

		u32 sampleHeight, sampleWidth;
		readBMPSize(&sampleHeight, &sampleWidth, sampleIn);

		RGB *sample = readPixelsFromBMP(sampleHeight, sampleWidth, sampleIn);

		RGB *sampleGrayscale = grayscale(sample, sampleHeight, sampleWidth);

		matchList sampleMatches = matchSample(grayscalePixels, height, width, sampleGrayscale, sampleHeight, sampleWidth, matchTh, itr);

		if(sampleMatches.size > 0)
		{
			matches.size += sampleMatches.size;

			if(matches.list == NULL)
				matches.list = (match*) malloc(matches.size * sizeof(match));
			else
				matches.list = (match*) realloc(matches.list, matches.size * sizeof(match));

			memcpy(matches.list + matches.size - sampleMatches.size, sampleMatches.list, sampleMatches.size * sizeof(match));
			free(sampleMatches.list);
		}

		free(sample);
		free(sampleGrayscale);
		fclose(sampleIn);
	}

	printf("\n");

	qsort(matches.list, matches.size, sizeof(match), cmpCorr);

	nonMaxSuppression(matches, overlapTh);

	for(itr = 0; itr < matches.size; ++itr)
	{
		if(matches.list[itr].correlation != -1)
			highlightMatch(pixels, height, width, matches.list[itr], colors[matches.list[itr].color]);
	}

	copyHeader(in, out);
	writePixelsToBMP(pixels, height, width, out);

	free(pixels);
	free(grayscalePixels);
	free(matches.list);
}

int main(int argc, char *argv[])
{
	//app source destination list
	if(argc < 6)
	{
		printf("Invalid number of arguments.\n");
		printf("USAGE: [sourcePath] [destinationPath] [listPath] [matchThreshold] [overlapThreshold]\n");
		exit(EXIT_FAILURE);
	}

	FILE *in = fopen(argv[1], "rb");
	if(in == NULL)
	{
		printf("Couldn't open source.\n");
		exit(EXIT_FAILURE);
	}

	FILE *out = fopen(argv[2], "wb");
	if(out == NULL)
	{
		printf("Can't write file in the destination folder.\n");
		exit(EXIT_FAILURE);
	}

	FILE *list = fopen(argv[3], "rb");
	if(list == NULL)
	{
		printf("Can't open list.\n");
		exit(EXIT_FAILURE);
	}

	handle(in, out, list, atof(argv[4]), atof(argv[5]));

	fclose(in);
	fclose(out);
	fclose(list);

	return 0;
}
