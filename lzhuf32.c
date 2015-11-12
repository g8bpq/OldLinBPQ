/*
Copyright 2001-2015 John Wiseman G8BPQ

This file is part of LinBPQ/BPQ32.

LinBPQ/BPQ32 is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

LinBPQ/BPQ32 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with LinBPQ/BPQ32.  If not, see http://www.gnu.org/licenses
*/	

// Mail and Chat Server for BPQ32 Packet Switch
//
// lzhuf Routines

#include "BPQMail.h"


/**************************************************************
        lzhuf.c
    written by Haruyasu Yoshizaki 1988/11/20
    some minor changes 1989/04/06
    comments translated by Haruhiko Okumura 1989/04/07
    getbit and getbyte modified 1990/03/23 by Paul Edwards
      so that they would work on machines where integers are
      not necessarily 16 bits (although ANSI guarantees a
      minimum of 16).  This program has compiled and run with
      no errors under Turbo C 2.0, Power C, and SAS/C 4.5
      (running on an IBM mainframe under MVS/XA 2.2).  Could
      people please use YYYY/MM/DD date format so that everyone
      in the world can know what format the date is in?
    external storage of filesize changed 1990/04/18 by Paul Edwards to
      Intel's "little endian" rather than a machine-dependant style so
      that files produced on one machine with lzhuf can be decoded on
      any other.  "little endian" style was chosen since lzhuf
      originated on PC's, and therefore they should dictate the
      standard.
    initialization of something predicting spaces changed 1990/04/22 by
      Paul Edwards so that when the compressed file is taken somewhere
      else, it will decode properly, without changing ascii spaces to
      ebcdic spaces.  This was done by changing the ' ' (space literal)
      to 0x20 (which is the far most likely character to occur, if you
      don't know what environment it will be running on.
**************************************************************/
	   

#define int __int16

/* crctab calculated by Mark G. Mendel, Network Systems Corporation */

UCHAR  *infile, *outfile, * endinfile;

short Get()
{
	if (infile == endinfile)
		return 	-1;
	else
		return *(infile++);
}

static unsigned short crctab[256] = {
    0x0000,  0x1021,  0x2042,  0x3063,  0x4084,  0x50a5,  0x60c6,  0x70e7,
    0x8108,  0x9129,  0xa14a,  0xb16b,  0xc18c,  0xd1ad,  0xe1ce,  0xf1ef,
    0x1231,  0x0210,  0x3273,  0x2252,  0x52b5,  0x4294,  0x72f7,  0x62d6,
    0x9339,  0x8318,  0xb37b,  0xa35a,  0xd3bd,  0xc39c,  0xf3ff,  0xe3de,
    0x2462,  0x3443,  0x0420,  0x1401,  0x64e6,  0x74c7,  0x44a4,  0x5485,
    0xa56a,  0xb54b,  0x8528,  0x9509,  0xe5ee,  0xf5cf,  0xc5ac,  0xd58d,
    0x3653,  0x2672,  0x1611,  0x0630,  0x76d7,  0x66f6,  0x5695,  0x46b4,
    0xb75b,  0xa77a,  0x9719,  0x8738,  0xf7df,  0xe7fe,  0xd79d,  0xc7bc,
    0x48c4,  0x58e5,  0x6886,  0x78a7,  0x0840,  0x1861,  0x2802,  0x3823,
    0xc9cc,  0xd9ed,  0xe98e,  0xf9af,  0x8948,  0x9969,  0xa90a,  0xb92b,
    0x5af5,  0x4ad4,  0x7ab7,  0x6a96,  0x1a71,  0x0a50,  0x3a33,  0x2a12,
    0xdbfd,  0xcbdc,  0xfbbf,  0xeb9e,  0x9b79,  0x8b58,  0xbb3b,  0xab1a,
    0x6ca6,  0x7c87,  0x4ce4,  0x5cc5,  0x2c22,  0x3c03,  0x0c60,  0x1c41,
    0xedae,  0xfd8f,  0xcdec,  0xddcd,  0xad2a,  0xbd0b,  0x8d68,  0x9d49,
    0x7e97,  0x6eb6,  0x5ed5,  0x4ef4,  0x3e13,  0x2e32,  0x1e51,  0x0e70,
    0xff9f,  0xefbe,  0xdfdd,  0xcffc,  0xbf1b,  0xaf3a,  0x9f59,  0x8f78,
    0x9188,  0x81a9,  0xb1ca,  0xa1eb,  0xd10c,  0xc12d,  0xf14e,  0xe16f,
    0x1080,  0x00a1,  0x30c2,  0x20e3,  0x5004,  0x4025,  0x7046,  0x6067,
    0x83b9,  0x9398,  0xa3fb,  0xb3da,  0xc33d,  0xd31c,  0xe37f,  0xf35e,
    0x02b1,  0x1290,  0x22f3,  0x32d2,  0x4235,  0x5214,  0x6277,  0x7256,
    0xb5ea,  0xa5cb,  0x95a8,  0x8589,  0xf56e,  0xe54f,  0xd52c,  0xc50d,
	0x34e2,  0x24c3,  0x14a0,  0x0481,  0x7466,  0x6447,  0x5424,  0x4405,
	0xa7db,  0xb7fa,  0x8799,  0x97b8,  0xe75f,  0xf77e,  0xc71d,  0xd73c,
	0x26d3,  0x36f2,  0x0691,  0x16b0,  0x6657,  0x7676,  0x4615,  0x5634,
	0xd94c,  0xc96d,  0xf90e,  0xe92f,  0x99c8,  0x89e9,  0xb98a,  0xa9ab,
	0x5844,  0x4865,  0x7806,  0x6827,  0x18c0,  0x08e1,  0x3882,  0x28a3,
	0xcb7d,  0xdb5c,  0xeb3f,  0xfb1e,  0x8bf9,  0x9bd8,  0xabbb,  0xbb9a,
	0x4a75,  0x5a54,  0x6a37,  0x7a16,  0x0af1,  0x1ad0,  0x2ab3,  0x3a92,
	0xfd2e,  0xed0f,  0xdd6c,  0xcd4d,  0xbdaa,  0xad8b,  0x9de8,  0x8dc9,
	0x7c26,  0x6c07,  0x5c64,  0x4c45,  0x3ca2,  0x2c83,  0x1ce0,  0x0cc1,
	0xef1f,  0xff3e,  0xcf5d,  0xdf7c,  0xaf9b,  0xbfba,  0x8fd9,  0x9ff8,
	0x6e17,  0x7e36,  0x4e55,  0x5e74,  0x2e93,  0x3eb2,  0x0ed1,  0x1ef0
};


#define updcrc(cp, crc) ((crc << 8) ^ crctab[(cp & 0xff) ^ (crc >> 8)])

unsigned	long 	textsize = 0, codesize = 0;
unsigned	short	crc;
int					version_1;
char wterr[] = "Can't write.";

void Error(char *message)
{
        printf("\n%s\n", message);
        exit(EXIT_FAILURE);
}

/********** LZSS compression **********/

#define N               2048    /* buffer size */
#define F               60      /* lookahead buffer size */
#define THRESHOLD       2
#define NIL             N       /* leaf of tree */

unsigned char
                text_buf[N + F - 1];
static int     match_position, match_length,
                lson[N + 1], rson[N + 257], dad[N + 1];

static int crc_fputc(unsigned short c)
{
	crc = updcrc(c, crc);
	*(outfile++) = (unsigned char)c;
	return 0;
}

static short crc_fgetc()
{
	short retour = *(infile++);

	return(retour);
}

void InitTree(void)  /* initialize trees */
{
		int  i;

        for (i = N + 1; i <= N + 256; i++)
                rson[i] = NIL;                  /* root */
        for (i = 0; i < N; i++)
                dad[i] = NIL;                   /* node */
}

static void InsertNode(int r)  /* insert to tree */
{
    int  i, p, cmp;
        unsigned char  *key;
        unsigned c;

        cmp = 1;
        key = &text_buf[r];
        p = N + 1 + key[0];
        rson[r] = lson[r] = NIL;
        match_length = 0;
        for ( ; ; ) {
                if (cmp >= 0) {
                        if (rson[p] != NIL)
                                p = rson[p];
                        else {
                                rson[p] = r;
                                dad[r] = p;
                                return;
                        }
                } else {
                        if (lson[p] != NIL)
                                p = lson[p];
                        else {
                                lson[p] = r;
                                dad[r] = p;
                                return;
                        }
                }
                for (i = 1; i < F; i++)
                        if ((cmp = key[i] - text_buf[p + i]) != 0)
                                break;
                if (i > THRESHOLD) {
                        if (i > match_length) {
                                match_position = ((r - p) & (N - 1)) - 1;
                                if ((match_length = i) >= F)
                                        break;
                        }
                        if (i == match_length) {
                if ((c = ((r - p) & (N-1)) - 1) < (unsigned)match_position) {
                                        match_position = c;
                                }
                        }
                }
        }
        dad[r] = dad[p];
        lson[r] = lson[p];
        rson[r] = rson[p];
        dad[lson[p]] = r;
        dad[rson[p]] = r;
        if (rson[dad[p]] == p)
                rson[dad[p]] = r;
        else
                lson[dad[p]] = r;
        dad[p] = NIL;  /* remove p */
}

static void DeleteNode(int p)  /* remove from tree */
{
    int  q;

        if (dad[p] == NIL)
                return;                 /* not registered */
        if (rson[p] == NIL)
                q = lson[p];
        else
        if (lson[p] == NIL)
                q = rson[p];
        else {
                q = lson[p];
                if (rson[q] != NIL) {
                        do {
                                q = rson[q];
                        } while (rson[q] != NIL);
                        rson[dad[q]] = lson[q];
                        dad[lson[q]] = dad[q];
                        lson[q] = lson[p];
                        dad[lson[p]] = q;
                }
                rson[q] = rson[p];
                dad[rson[p]] = q;
        }
        dad[q] = dad[p];
        if (rson[dad[p]] == p)
                rson[dad[p]] = q;
        else
                lson[dad[p]] = q;
        dad[p] = NIL;
}

/* Huffman coding */

#define N_CHAR          (256 - THRESHOLD + F)
                                /* kinds of characters (character code = 0..N_CHAR-1) */
#define T               (N_CHAR * 2 - 1)        /* size of table */
#define R               (T - 1)                 /* position of root */
#define MAX_FREQ        0x8000          /* updates tree when the */
                                        /* root frequency comes to this value. */
typedef unsigned char uchar;


/* table for encoding and decoding the upper 6 bits of position */

/* for encoding */
uchar p_len[64] = {
        0x03, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05,
        0x05, 0x05, 0x05, 0x05, 0x06, 0x06, 0x06, 0x06,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
        0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08
};

uchar p_code[64] = {
        0x00, 0x20, 0x30, 0x40, 0x50, 0x58, 0x60, 0x68,
        0x70, 0x78, 0x80, 0x88, 0x90, 0x94, 0x98, 0x9C,
        0xA0, 0xA4, 0xA8, 0xAC, 0xB0, 0xB4, 0xB8, 0xBC,
        0xC0, 0xC2, 0xC4, 0xC6, 0xC8, 0xCA, 0xCC, 0xCE,
        0xD0, 0xD2, 0xD4, 0xD6, 0xD8, 0xDA, 0xDC, 0xDE,
        0xE0, 0xE2, 0xE4, 0xE6, 0xE8, 0xEA, 0xEC, 0xEE,
        0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
        0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/* for decoding */
uchar d_code[256] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
        0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
        0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A,
        0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B,
        0x0C, 0x0C, 0x0C, 0x0C, 0x0D, 0x0D, 0x0D, 0x0D,
        0x0E, 0x0E, 0x0E, 0x0E, 0x0F, 0x0F, 0x0F, 0x0F,
        0x10, 0x10, 0x10, 0x10, 0x11, 0x11, 0x11, 0x11,
        0x12, 0x12, 0x12, 0x12, 0x13, 0x13, 0x13, 0x13,
        0x14, 0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x15,
        0x16, 0x16, 0x16, 0x16, 0x17, 0x17, 0x17, 0x17,
        0x18, 0x18, 0x19, 0x19, 0x1A, 0x1A, 0x1B, 0x1B,
        0x1C, 0x1C, 0x1D, 0x1D, 0x1E, 0x1E, 0x1F, 0x1F,
        0x20, 0x20, 0x21, 0x21, 0x22, 0x22, 0x23, 0x23,
        0x24, 0x24, 0x25, 0x25, 0x26, 0x26, 0x27, 0x27,
        0x28, 0x28, 0x29, 0x29, 0x2A, 0x2A, 0x2B, 0x2B,
        0x2C, 0x2C, 0x2D, 0x2D, 0x2E, 0x2E, 0x2F, 0x2F,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
};

uchar d_len[256] = {
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
        0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
};

unsigned int freq[T + 1]; /* frequency table */

int prnt[T + N_CHAR]; /* pointers to parent nodes, except for the */
                        /* elements [T..T + N_CHAR - 1] which are used to get */
                        /* the positions of leaves corresponding to the codes. */

int son[T];   /* pointers to child nodes (son[], son[] + 1) */

unsigned int getbuf = 0;
uchar getlen = 0;

static int GetBit(void)    /* get one bit */
{
    unsigned int i;

        while (getlen <= 8) {
				if ((i = crc_fgetc()) < 0) i = 0;
                getbuf |= i << (8 - getlen);
                getlen += 8;
        }
        i = getbuf;
        getbuf <<= 1;
        getlen--;
    return (int)((i & 0x8000) >> 15);
}

static int GetByte(void)   /* get one byte */
{
    unsigned int i;

        while (getlen <= 8) {
				if ((i = crc_fgetc()) == 0xffff) i = 0;
                getbuf |= i << (8 - getlen);
                getlen += 8;
        }
        i = getbuf;
        getbuf <<= 8;
        getlen -= 8;
    return (int)((i & 0xff00) >> 8);
}

unsigned int putbuf = 0;
uchar putlen = 0;

static void Putcode(int l, unsigned c)     /* output c bits of code */
{
        putbuf |= c >> putlen;
        if ((putlen += l) >= 8) {
				if (crc_fputc(putbuf >> 8) == EOF) {
                        Error(wterr);
                }
                if ((putlen -= 8) >= 8) {
						if (crc_fputc(putbuf) == EOF) {
                                Error(wterr);
                        }
                        codesize += 2;
                        putlen -= 8;
                        putbuf = c << (l - putlen);
                } else {
                        putbuf <<= 8;
                        codesize++;
                }
        }
}


/* initialization of tree */

static void StartHuff(void)
{
    int i, j;

        for (i = 0; i < N_CHAR; i++) {
                freq[i] = 1;
                son[i] = i + T;
                prnt[i + T] = i;
        }
        i = 0; j = N_CHAR;
        while (j <= R) {
                freq[j] = freq[i] + freq[i + 1];
                son[j] = i;
                prnt[i] = prnt[i + 1] = j;
                i += 2; j++;
        }
        freq[T] = 0xffff;
        prnt[R] = 0;
}


/* reconstruction of tree */

static void reconst(void)
{
    int i, j, k;
    unsigned int f, l;

        /* collect leaf nodes in the first half of the table */
        /* and replace the freq by (freq + 1) / 2. */
        j = 0;
        for (i = 0; i < T; i++) {
                if (son[i] >= T) {
                        freq[j] = (freq[i] + 1) / 2;
                        son[j] = son[i];
                        j++;
                }
        }
        /* begin constructing tree by connecting sons */
        for (i = 0, j = N_CHAR; j < T; i += 2, j++) {
                k = i + 1;
                f = freq[j] = freq[i] + freq[k];
                for (k = j - 1; f < freq[k]; k--);
                k++;
                l = (j - k) * 2;
                memmove(&freq[k + 1], &freq[k], l);
                freq[k] = f;
                memmove(&son[k + 1], &son[k], l);
                son[k] = i;
        }
        /* connect prnt */
        for (i = 0; i < T; i++) {
                if ((k = son[i]) >= T) {
                        prnt[k] = i;
                } else {
                        prnt[k] = prnt[k + 1] = i;
                }
        }
}


/* increment frequency of given code by one, and update tree */

static void update(int c)
{
    int i, j, l;
	unsigned int k;

        if (freq[R] == MAX_FREQ) {
                reconst();
        }
        c = prnt[c + T];
        do {
                k = ++freq[c];

                /* if the order is disturbed, exchange nodes */

        l = c + 1;
		
		if ((unsigned)k > freq[l]) 
		{
            while ((unsigned)k > freq[++l]);
                        l--;
                        freq[c] = freq[l];
                        freq[l] = k;

                        i = son[c];
                        prnt[i] = l;
                        if (i < T) prnt[i + 1] = l;

                        j = son[l];
                        son[l] = i;

                        prnt[j] = c;
                        if (j < T) prnt[j + 1] = c;
                        son[c] = j;

                        c = l;
                }
        } while ((c = prnt[c]) != 0);   /* repeat up to root */
}

unsigned code, len;

static void EncodeChar(unsigned int c)
{
    unsigned int i;
    int j, k;

        i = 0;
        j = 0;
        k = prnt[c + T];

        /* travel from leaf to root */
        do {
                i >>= 1;

                /* if node's address is odd-numbered, choose bigger brother node */
                if (k & 1) i += 0x8000;

                j++;
        } while ((k = prnt[k]) != R);
        Putcode(j, i);
        code = i;
        len = j;
        update(c);
}

static void EncodePosition(unsigned int c)
{
    unsigned int i;

        /* output upper 6 bits by table lookup */
        i = c >> 6;
        Putcode(p_len[i], (unsigned)p_code[i] << 8);

        /* output lower 6 bits verbatim */
        Putcode(6, (c & 0x3f) << 10);
}

static void EncodeEnd(void)
{
        if (putlen) {
				if (crc_fputc(putbuf >> 8) == EOF) {
                        Error(wterr);
                }
                codesize++;
        }
}

static int DecodeChar(void)
{
    unsigned int c;

        c = son[R];

        /* travel from root to leaf, */
        /* choosing the smaller child node (son[]) if the read bit is 0, */
        /* the bigger (son[]+1} if 1 */
        while (c < T) {
                c += GetBit();
                c = son[c];
        }
        c -= T;
        update(c);
    return (int)c;
}

static int DecodePosition(void)
{
    unsigned int i, j, c;

		/* recover upper 6 bits from table */
		i = GetByte();
		c = (unsigned)d_code[i] << 6;
		j = d_len[i];

		/* read lower 6 bits verbatim */
		j -= 2;
		while (j--) {
				i = (i << 1) + GetBit();
		}
    return (int)(c | (i & 0x3f));
}

/* compression */

long Encode(char * in, char * out, long inlen, BOOL B1Protocol)
{
		int  i, c, len, r, s, last_match_length;
		unsigned char *ptr;

		putbuf = 0;
		putlen = 0;
		textsize = 0;
		codesize = 0;

		crc = 0;
		outfile = out;

		if (B1Protocol)
		{
			outfile+=2;				// Space for CRC
		}

//		infile = &conn->MailBuffer[2];

		textsize = inlen;

		ptr = (char *)&textsize;

#ifdef __BIG_ENDIAN__
		
		crc_fputc(*(ptr+3));
		crc_fputc(*(ptr+2));
		crc_fputc(*(ptr+1));
		crc_fputc(*(ptr));
#else
		crc_fputc(*(ptr++));
		crc_fputc(*(ptr++));
		crc_fputc(*(ptr++));
		crc_fputc(*(ptr++));	
#endif

		if (textsize == 0)
			return 0;

		infile = in;
		endinfile = infile + inlen;
		textsize = 0;                   /* rewind and re-read */
		StartHuff();
		InitTree();
		s = 0;
		r = N - F;
		for (i = s; i < r; i++)
				text_buf[i] = 0x20;
		for (len = 0; len < F && (c = Get()) != EOF; len++)
				text_buf[r + len] = (unsigned char)c;
		textsize = len;
		for (i = 1; i <= F; i++)
				InsertNode(r - i);
		InsertNode(r);
		do {
				if (match_length > len)
						match_length = len;
				if (match_length <= THRESHOLD) {
						match_length = 1;
						EncodeChar(text_buf[r]);
				} else {
						EncodeChar(255 - THRESHOLD + match_length);
						EncodePosition(match_position);
				}
				last_match_length = match_length;
				for (i = 0; i < last_match_length &&
								(c = Get()) != EOF; i++) {
						DeleteNode(s);
            text_buf[s] = (unsigned char)c;
						if (s < F - 1)
                text_buf[s + N] = (unsigned char)c;
						s = (s + 1) & (N - 1);
						r = (r + 1) & (N - 1);
						InsertNode(r);
				}

				while (i++ < last_match_length) {
						DeleteNode(s);
						s = (s + 1) & (N - 1);
						r = (r + 1) & (N - 1);
						if (--len) InsertNode(r);
				}
		} while (len > 0);
		EncodeEnd();

		if (B1Protocol)
		{
			out[0] = crc & 0xff;
			out[1]= crc >> 8;
			codesize+=2;
		}

		 codesize += 4;

		Logprintf(LOG_BBS, NULL, '|', "Compressed Message Comp Len %d Msg Len %d CRC %x", 
				codesize, inlen, crc);

		return codesize;
}

BOOL CheckifPacket(char * Via)
{
	char * ptr1, * ptr2;
	
	// Message addressed to a non-winlink address
	// Need to see if real smtp, or a packet address

	// No . in address assume Packet - g8bpq@g8bpq

	ptr1 = strchr(Via, '.');

	if (ptr1 == NULL)
		return TRUE;			// Packet

	// Find Last Element

	ptr2 = strchr(++ptr1, '.');

	while (ptr2)
	{
		ptr1 = ptr2;
		ptr2 = strchr(++ptr1, '.');
	}

	// ptr1 is last element. If a valid continent, it is a packet message
	
	if (FindContinent(ptr1))
		return TRUE;			// Packet

	if ((_stricmp(ptr1, "MARS") == 0) || (_stricmp(ptr1, "USA") == 0))		// MARS used both
		return TRUE;			// Packet

	return FALSE;
}

void Decode(CIRCUIT * conn)  
{
	unsigned char *ptr;
	char * StartofMsg;
	short  i, j, k, r;
	short c;
	unsigned long count;
	unsigned short  crc_read;
	int Index = 0;
	struct FBBHeaderLine * FBBHeader= &conn->FBBHeaders[0];	// The Headers from an FFB forward block
	BOOL NTS = FALSE;

	getbuf = 0;
	getlen = 0;
	textsize = 0;
	codesize = 0;

	infile = &conn->MailBuffer[0];

	crc = 0;

	if (conn->BBSFlags & FBBB1Mode)
	{
		short val;
		long n;
			
		crc_read = infile[0];
		crc_read |= infile[1] << 8;

		for (n = 2; n < conn->TempMsg->length; n++)
		{
			val = infile[n];
			crc = updcrc(val, crc);
		}
		if (crc != crc_read)
		{
			nodeprintf(conn, "*** Message CRC Error File %x Calc %x\r", crc_read, crc);
			free(conn->MailBuffer);
			conn->MailBufferSize=0;
			conn->MailBuffer=0;
			conn->CloseAfterFlush = 20;			// 2 Secs

			return;
		}

		infile+=2;
	}

	textsize = 0;
	ptr = (char *)&textsize;

#ifdef __BIG_ENDIAN__

	ptr[3] = (unsigned char)crc_fgetc();
	ptr[2] = (unsigned char)crc_fgetc();
	ptr[1] = (unsigned char)crc_fgetc();
	ptr[0] = (unsigned char)crc_fgetc();

#else

	for (i = 0 ; i < sizeof(textsize) ; i++)
		ptr[i] = (unsigned char)crc_fgetc();

#endif

	// Temp fix for duff MACBPQ (Message Length send big-endian)

	if (textsize > 5000000)
	{
		char x[4];
		char y[4];

		memcpy(x, &textsize, 4);
		y[0] = x[3];
		y[1] = x[2];
		y[2] = x[1];
		y[3] = x[0];

		memcpy(&textsize, y, 4);

		if (textsize > 5000000)
		{
			nodeprintf(conn, "*** Message Size Invalid %d\r", textsize);
			Debugprintf("*** Message Size Invalid %d\r", textsize);
			free(conn->MailBuffer);
			conn->MailBufferSize=0;
			conn->MailBuffer=0;
			conn->CloseAfterFlush = 20;			// 2 Secs
			return;
		}
	}

	Logprintf(LOG_BBS, conn, '|', "Uncompressing Message Comp Len %d Msg Len %d CRC %x", 
			conn->TempMsg->length, textsize, crc);
	
	outfile = zalloc(textsize + 10000);		// Lots of space for B2 header manipulations

	if (textsize == 0)
		return;

	StartHuff();
	
	for (i = 0; i < N - F; i++)
		text_buf[i] = 0x20;

	r = N - F;

	for (count = 0; count < textsize; )
	{
		c = DecodeChar();
		if (c < 256)
		{
			*(outfile++) = (unsigned char)c;
			text_buf[r++] = (unsigned char)c;
			r &= (N - 1);
			count++;
		} 
		else
		{
			i = (r - DecodePosition() - 1) & (N - 1);
			j = c - 255 + THRESHOLD;
			for (k = 0; k < j; k++)
			{
				c = text_buf[(i + k) & (N - 1)];
				*(outfile++) = (unsigned char)c;
                text_buf[r++] = (unsigned char)c;
				r &= (N - 1);
				count++;
			}
		}
	}

	outfile -=count;

	free(conn->MailBuffer);
	conn->MailBuffer = outfile;
		
	conn->TempMsg->length = count;

	if (FBBHeader->MsgType == 'P')
		Index = PMSG;
	else if (FBBHeader->MsgType == 'B')
		Index = BMSG;
	else if (FBBHeader->MsgType == 'T')
		Index = TMSG;

	conn->UserPointer->Total.MsgsReceived[Index]++;
	conn->UserPointer->Total.BytesForwardedIn[Index] += count;

	if (FBBHeader->B2Message)
	{
		// Parse the Message for B2 From and To info
/*
MID: A3EDD4P00P55
Date: 2009/07/25 10:08
Type: Private
From: SMTP:john.wiseman@ntlworld.com
To: G8BPQ
Subject: RE: RMS Test Messaage
Mbo: SMTP
Body: 214
File: 3556 NOLA.XLS
File: 5566 NEWBOAT.HOMEPORT.JPG

*/
		UCHAR * ptr1, * ptr2, * ptr3;
		__int32 linelen, MsgLen = 0;
		struct MsgInfo * Msg = conn->TempMsg;
		time_t Date;
		char FullTo[100];
		char FullFrom[100];
		char ** RecpTo = NULL;				// May be several Recipients
		char ** HddrTo = NULL;				// May be several Recipients
		char ** Via = NULL;					// May be several Recipients
		__int32 LocalMsg[1000];				// Set if Recipient is a local wl2k address
		char Type[1000];					// Message Type for each dest
		__int32 B2To;						// Offset to To: fields in B2 header
		__int32 Recipients = 0;
		__int32 RMSMsgs = 0, BBSMsgs = 0;
#ifndef LINBPQ
		struct _EXCEPTION_POINTERS exinfo;

		__try {
#endif
		Msg->B2Flags |= B2Msg;

		// Display the whole header for debugging

/*
		ptr1 = strstr(outfile, "\r\n\r\n");

		if (ptr1)
		{
			*ptr1 = 0;
			Debugprintf("B2 Header = %s", outfile);
			*ptr1 = '\r';
		}
*/
		if (_stricmp(conn->Callsign, "RMS") == 0)
			Msg->B2Flags |= FromRMS;

		ptr1 = outfile;
	Loop:
		ptr2 = strchr(ptr1, '\r');

		linelen = ptr2 - ptr1;

		if (_memicmp(ptr1, "From:", 5) == 0)
		{
			memcpy(FullFrom, ptr1, linelen);
			FullFrom[linelen] = 0;

			if (conn->Paclink)
			{
				// Messages just have the call - need to add @winlink.org

				strcpy(Msg->emailfrom, "@winlink.org");

			}
			if (_memicmp(&ptr1[6], "smtp:", 5) == 0)
			{
				if (_stricmp(conn->Callsign, "RMS") == 0)
				{
					// Swap smtp: to rms: and save originator so we can reply via RMS

					strcpy(Msg->from, "RMS:");
					memcpy(Msg->emailfrom, &ptr1[11], linelen - 11);
				}
				else
				{
					strcpy(Msg->from, "SMTP:");
					memcpy(Msg->emailfrom, &ptr1[11], linelen - 11);
				}
			}
			else
			{
				char SaveFrom[100];
				char * FromHA;

				// B2 From may now contain an @BBS 

				strcpy(SaveFrom, FullFrom);
				
				FromHA = strlop(SaveFrom, '@');

				if (strlen(SaveFrom) > 12) SaveFrom[12] = 0;
				strcpy(Msg->from, &SaveFrom[6]);

				if (FromHA)
				{
					if (strlen(FromHA) > 39) FromHA[39] = 0;
					Msg->emailfrom[0] = '@';
					strcpy(&Msg->emailfrom[1], _strupr(FromHA));
				}

				// Remove any SSID

				ptr3 = strchr(Msg->from, '-');
				if (ptr3) *ptr3 = 0;
			}

			// If from RMS, and no @ in message, append @winlink.org to the B2 Header.
			// so messages passed via B2 know it is from Winlink

			if ((Msg->B2Flags & FromRMS) && strchr(FullFrom, '@') == NULL)
			{
				// Move Message down buffer - ptr2 is the insertion point

				memmove(ptr2+12, ptr2, count);
				memcpy(ptr2, "@winlink.org", 12);
				count += 12;
				conn->TempMsg->length += 12;

				// Also set Emailfrom, in case read on BBS (eg by outpost)
				
				strcpy(Msg->emailfrom, "@winlink.org");
			}

		}
		else if (_memicmp(ptr1, "To:", 3) == 0 || _memicmp(ptr1, "cc:", 3) == 0)
		{
			int toLen;
			
			HddrTo=realloc(HddrTo, (Recipients+1)*4);
			HddrTo[Recipients] = zalloc(100);

			memset(FullTo, 0, 99);
			memcpy(FullTo, &ptr1[4], linelen-4);
			memcpy(HddrTo[Recipients], ptr1, linelen+2);
			LocalMsg[Recipients] = FALSE;
			Type[Recipients] = Msg->type;	// Default to Type from Header

			Logprintf(LOG_BBS, conn, '?', "B2 Msg To: %s", FullTo);

			conn->TempMsg->length -= strlen(HddrTo[Recipients]);

			B2To = ptr1 - outfile;

			// if ending in AMPR.ORG send via ISP if we have enabled forwarding AMPR

			toLen = strlen(FullTo);

			if (_memicmp(&FullTo[toLen - 8], "ampr.org", 8) == 0)
			{
				// if our domain keep here.
				
				// if not, and SendAMPRDirect set, set as ISP,
				// else set as RMS			
				
				memcpy(Msg->via, FullTo, toLen);

				ptr3 = strchr(FullTo, '@');

				if (ptr3)
				{
					ptr3++;

					if (_stricmp(ptr3, AMPRDomain) == 0)
					{
						// Our Message

						strcpy(Msg->via, ptr3);
						strlop(FullTo,'@');
						BBSMsgs ++;
						goto BBSMsg;
					}
				}

				if (SendAMPRDirect)
				{
					strcpy(Msg->via, ptr3);
					strlop(FullTo,'@');
					BBSMsgs ++;
					goto BBSMsg;
				}
				
				strcpy(FullTo,"RMS");
				RMSMsgs ++;
			}

			if (conn->BPQBBS && !CheckifPacket(FullTo))  // May be an message for RMS being passed to an intermediate BBS
			{
				// Internet address - send via RMS

				strcpy(Msg->via, FullTo);
				strcpy(FullTo,"RMS");
				RMSMsgs ++;
			}
			else
			{
				ptr3 = strchr(FullTo, '@');

				if (ptr3)
				{
					*ptr3++ = 0;
					strcpy(Msg->via, ptr3);
				}
				else
					Msg->via[0] = 0;
			}
		
			if (conn->Paclink)
			{
				Msg->B2Flags |= FromPaclink;

				// Message from paclink

				// Messages to WL2K just have call.
				// Messages to email or BBS addresses have smtp:
			

				if (_memicmp(&ptr1[4], "SMTP:", 5) == 0)
				{
					// See if Packet or SMTP
					
					if (CheckifPacket(Msg->via))	//  If no RMS, don't check for routing to it)
					{
						// Packet Message

						memmove(FullTo, &FullTo[5], strlen(FullTo) - 4);
						_strupr(FullTo);
						_strupr(Msg->via);
						
						// Update the saved to: line (remove the smtp:)

						strcpy(&HddrTo[Recipients][4], &HddrTo[Recipients][9]);
						BBSMsgs++;

					}
					else
					{
						// Internet address - do we send via RMS??


						// ??? Need to see if RMS is available

						memcpy(Msg->via, &ptr1[9], linelen);
						Msg->via[linelen-9] = 0;
						strcpy(FullTo,"RMS");
						RMSMsgs ++;
					}

				}
				else
				{
					if ((conn->UserPointer->flags & F_NOWINLINK) == 0)	// treat as Packet Address?
					{
						strcpy(Msg->via, "winlink.org");		// Message for WL2K - add via
						RMSMsgs ++;
						LocalMsg[Recipients] = CheckifLocalRMSUser(FullTo);
					}
					else
					{
						BBSMsgs++;
					}
				}

			}
//			else if (conn->RMSExpress && FindRMS())			//  If no RMS, don't check for routing to it
			else if (conn->RMSExpress)	
			{
				Msg->B2Flags |= FromRMSExpress;

				// Message from RMS Express
				// Messages to WL2K just have call.
				// Messages to email or BBS addresses don't have smtp:
			

				if (Msg->via[0])
				{
					// Has an @ - See if Packet or SMTP. If to our AMPR address, treat as packet
					
					if (CheckifPacket(Msg->via) || _stricmp(Msg->via, AMPRDomain) == 0)
					{
						// Packet Message

						_strupr(FullTo);
						_strupr(Msg->via);
						BBSMsgs++;
					}
					else
					{
						// Internet address - do we send via RMS??

						memcpy(Msg->via, &ptr1[4], linelen);
						Msg->via[linelen-4] = 0;
						strcpy(FullTo,"RMS");
						RMSMsgs ++;
					}
				}
				else
				{
					if ((conn->UserPointer->flags & F_NOWINLINK) == 0)	//  Dont default to winlink.org
					{
						strcpy(Msg->via, "winlink.org");		// Message for WL2K - add via
						RMSMsgs ++;
						LocalMsg[Recipients] = CheckifLocalRMSUser(FullTo);
					}
					else
						goto BBSMsg;
				}
			}

			else			// Not Paclink or RMS Express (or no RMS)
			{
				if (_memicmp(&ptr1[4], "SMTP:", 5) == 0)
				{
					// Airmail Sends MARS messages as SMTP
					
					if (CheckifPacket(Msg->via))
					{
						// Packet Message

						memmove(FullTo, &FullTo[5], strlen(FullTo) - 4);
						_strupr(FullTo);
						_strupr(Msg->via);
						
						// Update the saved to: line (remove the smtp:)

						strcpy(&HddrTo[Recipients][4], &HddrTo[Recipients][9]);
						BBSMsgs++;
						goto BBSMsg;
					}

					// If a winlink.org address we need to convert to call

					if (_stricmp(Msg->via, "winlink.org") == 0)
					{
						memmove(FullTo, &FullTo[5], strlen(FullTo) - 4);
						_strupr(FullTo);
						LocalMsg[Recipients] = CheckifLocalRMSUser(FullTo);
					}
					else
					{
						memcpy(Msg->via, &ptr1[9], linelen);
						Msg->via[linelen - 9] = 0;
						strcpy(FullTo,"RMS");
					}
//					FullTo[0] = 0;
				}
				else
				{
		BBSMsg:		
					_strupr(FullTo);
					_strupr(Msg->via);
				}
			}

			if (memcmp(FullTo, "RMS:", 4) == 0)
			{
				// remove RMS and add @winlink.org

				memmove(FullTo, &FullTo[4], strlen(FullTo) - 3);
				strcpy(Msg->via, "winlink.org");
				sprintf(HddrTo[Recipients], "To: %s\r\n", FullTo);
			}

			else if (memcmp(FullTo, "NTS:", 4) == 0)
			{
				// remove NTS and set type 'T'

				memmove(FullTo, &FullTo[4], strlen(FullTo) - 3);		
				Type[Recipients] = 'T';		// NTS
				memmove(HddrTo[Recipients] + 4, HddrTo[Recipients] + 8, 91);

				// Replace Type: Private with Type: Traffic

			}
			else if ((_memicmp(FullTo, "bull/", 5) == 0) || (_memicmp(FullTo, "bull:", 5) == 0))
			{
				// remove bull/ and set type 'B'

				memmove(FullTo, &FullTo[5], strlen(FullTo) - 4);		
				Type[Recipients] = 'B';		// Bulletin
				memmove(HddrTo[Recipients] + 4, HddrTo[Recipients] + 9, 90);

				// Replace Type: Private with Type: Bulletin
				// Have to move rest of header down to make space

			}

			if (strcmp(Msg->via, "RMS") == 0)
			{
				// replace RMS with @winlink.org

				strcpy(Msg->via, "winlink.org");
				sprintf(HddrTo[Recipients], "To: %s@winlink.org\r\n", FullTo);
			}

			if (strlen(FullTo) > 6)
				FullTo[6] = 0;

			strlop(FullTo, '-');

			strcpy(Msg->to, FullTo);

			if (SendBBStoSYSOPCall)
				if (_stricmp(FullTo, BBSName) == 0)
					strcpy(Msg->to, SYSOPCall);

			if ((Msg->via[0] == 0 || strcmp(Msg->via, "BPQ") == 0 || strcmp(Msg->via, "BBS") == 0)
				&& (conn->Paclink || conn->RMSExpress))
			{
				// No routing - check @BBS and WP

				struct UserInfo * ToUser = LookupCall(FullTo);

				Msg->via[0] = 0;				// In case BPQ and not found

				if (ToUser)
				{
					// Local User. If Home BBS is specified, use it

					if (ToUser->HomeBBS[0])
					{
						strcpy(Msg->via, ToUser->HomeBBS); 
					}
				}
				else
				{
					WPRecP WP = LookupWP(FullTo);

					if (WP)
					{
						strcpy(Msg->via, WP->first_homebbs);
			
					}
				}

				// Fix To: address in B2 Header

				if (Msg->via[0])
					sprintf(HddrTo[Recipients], "To: %s@%s\r\n", FullTo, Msg->via);
				else
					sprintf(HddrTo[Recipients], "To: %s\r\n", FullTo);

			}

			RecpTo=realloc(RecpTo, (Recipients+1)*4);
			RecpTo[Recipients] = zalloc(10);

			Via=realloc(Via, (Recipients+1)*4);
			Via[Recipients] = zalloc(50);

			strcpy(Via[Recipients], Msg->via);
			strcpy(RecpTo[Recipients++], FullTo);

			// Remove the To: Line from the buffer

			memmove(ptr1, ptr2+2, count);
			goto Loop;
			
		}
		else if (_memicmp(ptr1, "Type:", 4) == 0)
		{
			if (ptr1[6] == 'N')
				Msg->type = 'T';				// NTS
			else
				Msg->type = ptr1[6];
		}
		else if (_memicmp(ptr1, "Body:", 4) == 0)
		{
			MsgLen = atoi(&ptr1[5]);
			StartofMsg = ptr1;
		}
		else if (_memicmp(ptr1, "File:", 5) == 0)
		{
			Msg->B2Flags |= Attachments;
		}
		else if (_memicmp(ptr1, "Date:", 5) == 0)
		{
			struct tm rtime;
			char seps[] = " ,\t\r";

			memset(&rtime, 0, sizeof(struct tm));

			// Date: 2009/07/25 10:08
	
			sscanf(&ptr1[5], "%04d/%02d/%02d %02d:%02d",
					&rtime.tm_year, &rtime.tm_mon, &rtime.tm_mday, &rtime.tm_hour, &rtime.tm_min);

			rtime.tm_year -= 1900;

			Date = mktime(&rtime) - (time_t)_MYTIMEZONE;
	
			if (Date == (time_t)-1)
				Date = time(NULL);

		}

		if (linelen)			// Not Null line
		{
			ptr1 = ptr2 + 2;		// Skip crlf
			goto Loop;
		}

		// Processed all headers

		// If multiple recipents, create one copy for each BBS address, and one for all others (via RMS)
	
		if (Recipients == 0 || HddrTo == NULL)
		{
			Debugprintf("B2 Message with no recipients from %s", conn->Callsign);	
			Logprintf(LOG_BBS, conn, '!', "B2 Message with no recipients from %s", conn->Callsign);
			SetupNextFBBMessage(conn);
			return;
		}
		else
		{
			__int32 i;
			struct MsgInfo * SaveMsg;
			char * SaveBody;
			__int32 SaveMsgLen = count;
			BOOL SentToRMS = FALSE;
			__int32 ToLen;
			char * ToString = malloc(Recipients * 100);

			SaveMsg = Msg;
			SaveBody = conn->MailBuffer;

			// If from WL2K, create one message for each to: or cc: that is a local user

			if (Msg->B2Flags & FromRMS)
			{
				struct UserInfo * user;
				char Call[10];
				char * ptr;

				for (i = 0; i < Recipients; i++)
				{
					memcpy(Call, &HddrTo[i][4], 9);
					ptr = strchr(Call, 13);
					if (ptr)
						*ptr = 0;

					strlop(Call, '-');
					
					user = LookupCall(Call);

					if (user == 0)
						continue;

					if (strcmp(Call, BBSName) != 0)	// always accept to bbs call
						if ((user->flags & F_POLLRMS) == 0)
							continue;
					
					conn->TempMsg = Msg = malloc(sizeof(struct MsgInfo));
					memcpy(Msg, SaveMsg, sizeof(struct MsgInfo));
	
					conn->MailBuffer = malloc(SaveMsgLen + 1000);
					memcpy(conn->MailBuffer, SaveBody, SaveMsgLen);

					// Add our To: 
		
					ToLen = strlen(HddrTo[i]);

					if (_memicmp(HddrTo[i], "CC", 2) == 0)	// Replace CC: with TO:
						memcpy(HddrTo[i], "To", 2);

					memmove(&conn->MailBuffer[B2To + ToLen], &conn->MailBuffer[B2To], count);
					memcpy(&conn->MailBuffer[B2To], HddrTo[i], ToLen); 

					conn->TempMsg->length += ToLen;

					strcpy(Msg->to, RecpTo[i]);
					strcpy(Msg->via, Via[i]);
					Msg->type = Type[i];
				
					if (Recipients > 1)
						Msg->bid[0] = 0;

					CreateMessageFromBuffer(conn);
				}
			}
			else
			{
				// From a client - Create one copy with all RMS recipients, and another for each packet recipient	

			// Merge all RMS To: lines 

			ToLen = 0;
			ToString[0] = 0;

			for (i = 0; i < Recipients; i++)
			{
				if (LocalMsg[i])
					continue;						// For a local RMS user
				
				if (_stricmp(Via[i], "WINLINK.ORG") == 0 || _memicmp (&HddrTo[i][4], "SMTP:", 5) == 0 ||
					_stricmp(RecpTo[i], "RMS") == 0)
				{
					if (ToLen == 0)			// First Addr
						memcpy(HddrTo[i], "To", 2);			// In Case CC

					ToLen += strlen(HddrTo[i]);
					strcat(ToString, HddrTo[i]);
				}
			}

			if (ToLen)
			{
				conn->TempMsg = Msg = malloc(sizeof(struct MsgInfo));
				memcpy(Msg, SaveMsg, sizeof(struct MsgInfo));
	
				conn->MailBuffer = malloc(SaveMsgLen + 1000);
				memcpy(conn->MailBuffer, SaveBody, SaveMsgLen);

				// Add all the To: lines back to message

				memmove(&conn->MailBuffer[B2To + ToLen], &conn->MailBuffer[B2To], count);
				memcpy(&conn->MailBuffer[B2To], ToString, ToLen); 

				conn->TempMsg->length += ToLen;


				if (Recipients > 1)
				{
					strcpy(Msg->via, "winlink.org");
					strcpy(Msg->to, "RMS");
					Msg->bid[0] = 0;		// Must Change the BID
				}
				
				// Don't change type, as we don't change the B2 Header for messages to RMS
				//Msg->type = Type[0];
	
				CreateMessageFromBuffer(conn);
			}

			free(ToString);

			for (i = 0; i < Recipients; i++)
			{
				// Only Process Non - RMS Dests or local RMS Users

				if (LocalMsg[i] == 0)
					if (_stricmp (Via[i], "WINLINK.ORG") == 0 ||
						_memicmp (&HddrTo[i][4], "SMTP:", 5) == 0 ||
						_stricmp(RecpTo[i], "RMS") == 0)		
					continue;

				conn->TempMsg = Msg = malloc(sizeof(struct MsgInfo));
				memcpy(Msg, SaveMsg, sizeof(struct MsgInfo));
	
				conn->MailBuffer = malloc(SaveMsgLen + 1000);
				memcpy(conn->MailBuffer, SaveBody, SaveMsgLen);

				// Add our To: 

				ptr = HddrTo[i];

				// We removed any nts: or bull: earlier on, 
				// and saved type. We need to set type here, as
				// may be sending to more than one type
				// If message contains Type: Private and not 'P',
				// need to changes to Traffic or Bulletin

				ToLen = strlen(ptr);

	//			if (_memicmp(HddrTo[i], "CC", 2) == 0)	// Replace CC: with TO:
					memcpy(HddrTo[i], "To", 2);

				memmove(&conn->MailBuffer[B2To + ToLen], &conn->MailBuffer[B2To], count);
				memcpy(&conn->MailBuffer[B2To], HddrTo[i], ToLen); 

				conn->TempMsg->length += ToLen;

				Msg->type = Type[i];

				ptr = strstr(conn->MailBuffer, "Type: ");

				if (ptr)
				{
					ptr += 6;
					if (_memicmp(ptr, "Private", 7) == 0 && Msg->type != 'P')
					{
						if (Msg->type == 'T')
							memcpy(ptr, "Traffic", 7);
						else
						if (Msg->type == 'B')
						{
							// have to make space

							memmove(ptr + 1, ptr, count);
							conn->TempMsg->length++;
							memcpy(ptr, "Bulletin", 8);
						}

						// remove //wl2k from subject

						ptr = strstr(conn->MailBuffer, "Subject: ");

						if (ptr && _memicmp(ptr + 9, "//WL2K ", 7) == 0)
						{
							memmove(ptr + 9, ptr + 16, count);
							conn->TempMsg->length -= 7;
							memmove(conn->TempMsg->title, &conn->TempMsg->title[7], strlen(conn->TempMsg->title) - 6);
						}
					}
				}

				strcpy(Msg->to, RecpTo[i]);
				strcpy(Msg->via, Via[i]);
				
				if (i > 0 && Msg->type != 'B')			// Must Change the BID
					Msg->bid[0] = 0;

				CreateMessageFromBuffer(conn);
			}
			}	// End not from RMS

			free(SaveMsg);
			free(SaveBody);
			conn->MailBuffer = NULL;
			conn->MailBufferSize=0;

			SetupNextFBBMessage(conn);
			return;
		}
/*
		else
		{
			// Single Destination -  Need to put to: line back in message

			char * ptr = HddrTo[0];
			__int32 ToLen;
			char toCopy[80];
			
			
			ptr = HddrTo[0];

			if (_memicmp(&ptr[4], "nts:", 4) == 0)
				memmove(ptr + 4, ptr + 8, strlen(ptr + 7));

			ToLen = strlen(ptr);

			memmove(&conn->MailBuffer[B2To + ToLen], &conn->MailBuffer[B2To], count);
			memcpy(&conn->MailBuffer[B2To], HddrTo[0], ToLen); 
			conn->TempMsg->length += ToLen;
			Msg->type = Type[i];
	
			CreateMessageFromBuffer(conn);
			SetupNextFBBMessage(conn);
			return;
		}
*/
#ifndef LINBPQ
		}
			#define EXCEPTMSG "Error Decoding B2 Message"
			#include "StdExcept.c"

		BBSputs(conn, "*** Program Error Decoding B2 Message\r");
		Flush(conn);
		conn->CloseAfterFlush = 20;			// 2 Secs
		
		return;
		}
#endif	
	} // end if B2Msg

	// Look for 

	CreateMessageFromBuffer(conn);
	SetupNextFBBMessage(conn);
}