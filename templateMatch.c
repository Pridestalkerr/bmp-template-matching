#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>

#define HEADER_LENGTH 54

typedef unsigned char byte;

typedef struct{
    byte R;
    byte G;
    byte B;
}RGB;

typedef struct{
    unsigned int size;
    unsigned int width;
    unsigned int height;
}bmpHeader;

typedef struct{
    int x;
    int y;
    double correlation;
    int color;
}coordinate;

typedef struct{
    int total;
    coordinate *pair;
}matchArray;

int cmpCorr(const void * a, const void * b)
{
    coordinate *v1=(coordinate*)a;
    coordinate *v2=(coordinate*)b;
    double ret=v2->correlation-v1->correlation;
    if(ret==0)
        return 0;
    if(ret<0)
        return -1;
    else
        return 1;
}

void prompt(char *in, char *out)
{
    printf("Source filename (maximum of 50 characters, no spaces): ");
    scanf("%s", in);
    printf("\n\nDestination filename (maximum of 50 characters, no spaces): ");
    scanf("%s", out);
}

void getHeaderData(bmpHeader *header, FILE *in)
{
    fseek(in, 2, SEEK_SET);
    fread(&header->size, 4, 1, in);
    fseek(in, 18, SEEK_SET);
    fread(&header->width, 4, 1, in);
    fread(&header->height, 4, 1,in);
}

void makePixelArray(RGB **pixelArray, bmpHeader header, FILE *in)
{
    fseek(in, HEADER_LENGTH, SEEK_SET);
    int i,j;
    int padding=(4-(header.width*3%4))%4;
    for(i=header.height-1;i>=0;--i)
    {
        for(j=0;j<header.width;++j)
        {
            fread(&(*pixelArray)[i*header.width+j].B, 1, 1, in);
            fread(&(*pixelArray)[i*header.width+j].G, 1, 1, in);
            fread(&(*pixelArray)[i*header.width+j].R, 1, 1, in);
        }
        fseek(in, padding, SEEK_CUR);
    }
}

void grayscale(RGB **pixelArray, bmpHeader header)
{
    int i, j;
    for(i=0;i<header.height;++i)
        for(j=0;j<header.width;++j)
            (*pixelArray)[i*header.width+j].B=(*pixelArray)[i*header.width+j].G=(*pixelArray)[i*header.width+j].R=0.299*(*pixelArray)[i*header.width+j].R + 0.587*(*pixelArray)[i*header.width+j].G + 0.114*(*pixelArray)[i*header.width+j].B;
}

matchArray matchTemplate(RGB **pixelArray, bmpHeader header, RGB **samplePixelArray, bmpHeader sampleHeader, double threshold, int color)
{
    int i,j,k,l,h=0;
    double sb=0,fb=0,ds=0,df=0,correlation=0;
    int winHeight=sampleHeader.height;
    int winWidth=sampleHeader.width;
    int n=winWidth*winHeight;
    for(i=0;i<n;++i)
        sb+=(double)(*samplePixelArray)[i].B;
    sb/=(double)n;
    for(i=0;i<n;++i)
    {
        ds+=((double)(*samplePixelArray)[i].B-sb)*((double)(*samplePixelArray)[i].B-sb);
    }
    ds=sqrt(ds/(double)(n-1));
    matchArray matches;
    matches.pair=NULL;
	matches.total=0;
    for(i=0;i<=header.height-winHeight;++i)
    {
        for(j=0;j<=header.width-winHeight;++j)
        {
            //sum
            fb=0, df=0, correlation=0;
            for(k=i;k<i+winHeight;++k)
                for(l=j;l<j+winWidth;++l)
                    fb+=(double)(*pixelArray)[k*(header.width)+j].B;
            fb/=(double)n;
            for(k=i;k<i+winHeight;++k)
                for(l=j;l<j+winWidth;++l)
                {
                    df+=((double)(*pixelArray)[k*(header.width)+l].B - fb) * ((double)(*pixelArray)[k*(header.width)+l].B - fb);
                }
            df=sqrt(df/(double)n);
            for(k=i;k<i+winHeight;++k)
                for(l=j;l<j+winWidth;++l)
                    correlation+=(((double)(*pixelArray)[k*(header.width)+l].B - fb) * ((double)(*samplePixelArray)[(k-i)*winWidth+(l-j)].B - sb))/(ds*df);
            correlation/=(double)n;
            if(correlation>=threshold)
            {
                if(matches.pair==NULL)
                {
                    matches.pair=malloc(sizeof(coordinate));
                    matches.total=1;
                }
                else
                {
                    matches.pair=realloc(matches.pair, (matches.total+1)*sizeof(coordinate));
                    matches.total++;
                }
                matches.pair[matches.total-1].x=i;
                matches.pair[matches.total-1].y=j;
                matches.pair[matches.total-1].correlation=correlation;
                matches.pair[matches.total-1].color=color;
            }
        }
    }
    return matches;
}

void highlightMatch(RGB **pixelArray, coordinate box, RGB color, bmpHeader sampleHeader, bmpHeader header)
{
    int i,j,k;
    for(i=box.x;i<box.x+sampleHeader.height;++i)
    {
        (*pixelArray)[i*header.width+box.y]=color;
        (*pixelArray)[i*header.width+box.y+sampleHeader.width]=color;
    }
    for(i=box.y;i<box.y+sampleHeader.width;++i)
    {
        (*pixelArray)[box.x*header.width+i]=color;
        (*pixelArray)[(box.x+sampleHeader.height)*header.width+i]=color;
    }

}

int min(int a,int b)
{
    if(a<=b)
        return a;
    else
        return b;
}

int max(int a,int b)
{
    if(a>=b)
        return a;
    else
        return b;
}

double superposition(int xa1, int xa2, int ya1, int ya2, int xb1, int xb2, int yb1, int yb2)
{
    int a;
    a=max(0, min(xa2, xb2) - max(xa1, xb1)) * max(0, min(ya2, yb2) - max(ya1, yb1));

    int b;
    b= (xa2-xa1)*(ya2-ya1) + (xb2-xb1)*(yb2-yb1) -a;

    return (double)a / (double)b;
}

void nonMaxSuppresion(matchArray *SAD)
{
    int i,j;
    for(i=0;i<SAD->total-1;++i)
    {
        if(SAD->pair[i].correlation!=-1)
            for(j=i+1;j<SAD->total;++j)
            {
                double sp=superposition(SAD->pair[i].x, SAD->pair[i].x+15, SAD->pair[i].y, SAD->pair[i].y+11, SAD->pair[j].x, SAD->pair[j].x+15, SAD->pair[j].y, SAD->pair[j].y+11);
                if(sp>0.2)
                    SAD->pair[j].correlation=-1;
            }
    }
}

void copyHeader(FILE *in, FILE *out)
{
    int i;
    byte headerByte;
    fseek(in, 0, SEEK_SET);
    fseek(out, 0, SEEK_SET);
    for(i=0;i<HEADER_LENGTH;++i)
    {
        fread(&headerByte, 1, 1, in);
        fwrite(&headerByte, 1, 1, out);
    }
}

void writePixelArray(RGB **pixelArray, bmpHeader header, FILE *out)
{
    fseek(out, HEADER_LENGTH, SEEK_SET);
    int i,j;
    int padding=(4-(header.width*3%4))%4;
    for(i=header.height-1;i>=0;--i)
    {
        for(j=0;j<header.width;++j)
        {
            fwrite(&(*pixelArray)[i*header.width+j].B, 1, 1, out);
            fwrite(&(*pixelArray)[i*header.width+j].G, 1, 1, out);
            fwrite(&(*pixelArray)[i*header.width+j].R, 1, 1, out);
        }
        for(j=0;j<padding;++j)
            fwrite((byte[]){0}, 1, 1, out);
    }
}

void findDigits(char *source, char *destination)
{
    //open reading file and create output file
    FILE *in=fopen(source, "rb");
    FILE *out=fopen(destination, "wb");

    //save needed data from header (size of file, width, height)
    bmpHeader header;
    getHeaderData(&header, in);

    //save pixels/image into an array
    RGB *pixelArray=malloc(header.height*header.width*sizeof(RGB));
    makePixelArray(&pixelArray, header, in);

    //grayscale the pixels/image
    grayscale(&pixelArray, header);

    //make an output image array (not grayscaled)
    RGB *pixelArrayFinal=malloc(header.height*header.width*sizeof(RGB));
    makePixelArray(&pixelArrayFinal, header, in);

    //make a color array
    RGB color[10];
    color[0].R=255; color[0].G=0; color[0].B=0;
    color[1].R=255; color[1].G=255; color[1].B=0;
    color[2].R=0; color[2].G=255; color[2].B=0;
    color[3].R=0; color[3].G=255; color[3].B=255;
    color[4].R=255; color[4].G=0; color[4].B=255;
    color[5].R=0; color[5].G=0; color[5].B=255;
    color[6].R=192; color[6].G=192; color[6].B=192;
    color[7].R=255; color[7].G=140; color[7].B=0;
    color[8].R=128; color[8].G=0; color[8].B=128;
    color[9].R=128; color[9].G=0; color[9].B=0;

    //create a total correlation array (to be sorted)
    matchArray SAD;
    SAD.pair=NULL;
    SAD.total=0;

    //template match all the samples and save each correlation into SAD
    int i;
    for(i=0;i<10;++i)
    {
        //load sample image
        char sampleName[6];
        sampleName[0]=i+48; sampleName[1]='.'; sampleName[2]='b'; sampleName[3]='m'; sampleName[4]='p'; sampleName[5]='\0';
        FILE *samepleIn=fopen(sampleName, "rb");

        //save sample header data
        bmpHeader sampleHeader;
        getHeaderData(&sampleHeader, samepleIn);

        //save sample pixels into an array
        RGB *samplePixelArray=malloc(sampleHeader.height*sampleHeader.width*sizeof(RGB));
        makePixelArray(&samplePixelArray, sampleHeader, samepleIn);

        //template match the sample to the main image and save correlations
        matchArray matches;
        matches=matchTemplate(&pixelArray, header, &samplePixelArray, sampleHeader, 0.5, i);

        //save them into the big array thats to be sortedor
        if(matches.total>0)
        {
            SAD.total+=matches.total;
            if(SAD.pair==NULL)
                SAD.pair=malloc(SAD.total*sizeof(coordinate));
            else
                SAD.pair=realloc(SAD.pair, SAD.total*sizeof(coordinate));
            memcpy(SAD.pair+SAD.total-matches.total, matches.pair, matches.total*sizeof(coordinate));
	    free(matches.pair);
        }

        free(samplePixelArray);
    }

    qsort(SAD.pair, SAD.total, sizeof(coordinate), cmpCorr);

    nonMaxSuppresion(&SAD);
    /*printf("uwu\n\n\n");
    for(i=0;i<SAD.total;++i)
        printf("%f ", SAD.pair[i].correlation);*/

    bmpHeader sampleHeader;
    sampleHeader.height=15;
    sampleHeader.width=11;
    sampleHeader.size=0;
    for(i=0;i<SAD.total;++i)
    {
        if(SAD.pair[i].correlation!=-1)
            highlightMatch(&pixelArrayFinal, SAD.pair[i], color[SAD.pair[i].color], sampleHeader, header);
    }

    //copy the header into output file
    copyHeader(in, out);

    //write the pixels/image into the output file
    writePixelArray(&pixelArrayFinal, header, out);
    free(pixelArray);
    free(pixelArrayFinal);
}

int main()
{
    char source[50], destination[50];
    prompt(source, destination);
    findDigits(source, destination);
    return 0;
}
