/* Allegro port Copyright (C) 2014 Gavin Smith. */
#ifndef LASTBEER_FILEMAN_H
#define LASTBEER_FILEMAN_H

#include <stdio.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define M_NONE		0x00		// No buffering at all.
#define M_XMS		0x01		// Buffer file in XMS if possible.
#define M_NOFREEUP 	0x02            // Don't throw away any file in
					//   buffer if there's no room
					//   to buffer it.

int initfilemanager(int handles, int minsize, int maxsize,
		    void (*err)(char *text, int code, ...));
void shutfilemanager(void);
void *opendatabase(char *file);
void closedatabase(void *database);
unsigned char *loadfile(void *database, char *file);
void *loadfiledirect(char *file, int flags);
void unloadfile(void *ptr);
FILE *openfile(void *database, char *file);
FILE *openfiledirect(char *file);
void closefile(FILE *filvar);

// Available only if DEBUG is defined.
void printbuffer(void);


/* Returns 1 and sets size when the archive contains the named asset. */
int database_file_size(void *database, const char *file, size_t *size);

#ifdef __cplusplus
}
#endif
#endif
