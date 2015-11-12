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


// General C Routines common to bpq32 and linbpq.mainly moved from BPQ32.c

#pragma data_seg("_BPQDATA")

#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T

#pragma data_seg("_BPQDATA")

#include "CHeaders.h"

char * GetTemplateFromFile(int Version, char * FN)
{
	int FileSize;
	char * MsgBytes;
	char MsgFile[265];
	FILE * hFile;
	int ReadLen;
	BOOL Special = FALSE;
	struct stat STAT;

	sprintf(MsgFile, "%s/HTML/%s", BPQDirectory, FN);

	if (stat(MsgFile, &STAT) == -1)
	{
		MsgBytes = _strdup("File is missing");
		return MsgBytes;
	}

	hFile = fopen(MsgFile, "rb");
	
	if (hFile == 0)
	{
		MsgBytes = _strdup("File is missing");
		return MsgBytes;
	}

	FileSize = STAT.st_size;
	MsgBytes = malloc(FileSize + 1);
	ReadLen = fread(MsgBytes, 1, FileSize, hFile); 
	MsgBytes[FileSize] = 0;
	fclose(hFile);

	// Check Version

	if (Version)
	{
		int PageVersion = 0;

		if (memcmp(MsgBytes, "<!-- Version", 12) == 0)
			PageVersion = atoi(&MsgBytes[13]);

		if (Version != PageVersion)
		{
			free(MsgBytes);
			MsgBytes = malloc(256);
			sprintf(MsgBytes, "Wrong Version of HTML Page %s - is %d should be %d. Please update", FN, PageVersion, Version);
		}
	}
	
	return MsgBytes;
}
