#include "fileman.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int last_error;
static void capture_error(char *message, int code, ...)
{
    (void)message;
    last_error = code;
}

static void require(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s (error %d)\n", message, last_error);
        exit(1);
    }
}

static void write_archive(const char *path, int invalid)
{
    unsigned char data[34 + 2 * 24 + 2] = {0};
    FILE *out;
    data[32] = 2;
    memcpy(data + 34, "ONE", 4);
    data[34 + 14] = 1;
    data[34 + 18] = M_XMS;
    data[34 + 20] = invalid ? 255 : 82;
    memcpy(data + 58, "TWO", 4);
    data[58 + 14] = 1;
    data[58 + 18] = M_XMS;
    data[58 + 20] = 83;
    data[82] = 'A';
    data[83] = 'B';
    out = fopen(path, "wb");
    require(out != NULL, "create test archive");
    require(fwrite(data, 1, sizeof(data), out) == sizeof(data), "write test archive");
    require(fclose(out) == 0, "close test archive");
}

int main(int argc, char **argv)
{
    void *db;
    unsigned char *asset;
    FILE *out;
    unsigned char header[34], record[24];
    unsigned int count, i;
    require(argc == 3, "test paths supplied");
    require(initfilemanager(1, 0, 8192, capture_error) == 1, "initialize manager");
    db = opendatabase(argv[1]);
    require(db != NULL, "open real game archive");
    asset = loadfile(db, "FONT1.SPR");
    require(asset != NULL, "read real game asset");
    unloadfile(asset);
    out = fopen(argv[1], "rb");
    require(out != NULL && fread(header, 1, sizeof(header), out) == sizeof(header), "read real archive directory");
    count = header[32] | (unsigned int)header[33] << 8;
    for (i = 0; i < count; ++i) {
        char name[15];
        require(fread(record, 1, sizeof(record), out) == sizeof(record), "read real archive entry");
        memcpy(name, record, 14);
        name[14] = 0;
        asset = loadfile(db, name);
        require(asset != NULL, "read every real game asset");
        unloadfile(asset);
    }
    fclose(out);
    closedatabase(db);
    shutfilemanager();

    require(initfilemanager(1, 0, 8192, capture_error) == 1, "reinitialize manager");
    write_archive(argv[2], 0);
    db = opendatabase(argv[2]);
    require(db != NULL, "open synthetic archive");
    asset = loadfile(db, "ONE");
    require(asset && asset[0] == 'A', "read first cached asset");
    unloadfile(asset);
    asset = loadfile(db, "TWO");
    require(asset && asset[0] == 'B', "read asset when cache is full");
    unloadfile(asset);
    closedatabase(db);
    write_archive(argv[2], 1);
    last_error = 0;
    require(opendatabase(argv[2]) == NULL && last_error == EIO, "reject out-of-range entry");
    out = fopen(argv[2], "wb");
    require(out != NULL && fwrite("short", 1, 5, out) == 5 && fclose(out) == 0, "create truncated archive");
    last_error = 0;
    require(opendatabase(argv[2]) == NULL && last_error == EIO, "reject truncated archive");
    remove(argv[2]);
    shutfilemanager();
    return 0;
}
