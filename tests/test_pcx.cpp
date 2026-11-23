#include "fileman.h"
#include "pcx.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <span>
#include <vector>

static void fail(const char *message) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
static void archive_error(char *, int, ...) {}
static void test_small_pcx() {
    std::vector<std::uint8_t> data(128, 0);
    data[0] = 10; data[2] = 1; data[3] = 8;
    data[8] = 2; data[10] = 1; // 3 x 2
    data[65] = 1; data[66] = 4; // one padding byte per row
    const std::uint8_t encoded[] = {1, 0xc2, 2, 9, 0xc1, 0xc0, 4, 5, 9};
    data.insert(data.end(), std::begin(encoded), std::end(encoded));
    data.push_back(12);
    data.resize(data.size() + 768, 0);
    const auto palette_start = data.size() - 768;
    data[palette_start + 2 * 3] = 12;
    data[palette_start + 2 * 3 + 1] = 34;
    data[palette_start + 2 * 3 + 2] = 56;
    auto image = lastbeer::decode_pcx(data);
    if (image.width != 3 || image.height != 2 ||
        image.pixels != std::vector<std::uint8_t>({1, 2, 2, 0xc0, 4, 5}) ||
        image.palette[2].r != 12 || image.palette[2].g != 34 || image.palette[2].b != 56)
        fail("PCX pixels, row padding or palette decoded incorrectly");
    data[128] = 0xc5; // A run cannot extend beyond the encoded row.
    bool rejected = false;
    try { lastbeer::decode_pcx(data); } catch (const std::exception &) { rejected = true; }
    if (!rejected) fail("oversized PCX run accepted");
}
int main(int argc, char **argv) {
    test_small_pcx();
    if (argc != 2 || initfilemanager(1, 0, 8192, archive_error) != 1) fail("init failed");
    void *db = opendatabase(argv[1]);
    if (!db) fail("archive open failed");
    const std::array names = {"MENU.PCX", "STORE.PCX", "MENUCARD.PCX", "LANDSCAP.PCX", "SELL.PCX",
                              "LONGTIME.PCX", "CREDIT.PCX", "BACK.PCX", "TITLE.PCX", "SCORE.PCX", "SKY.PCX"};
    for (auto name : names) {
        size_t size = 0;
        if (!database_file_size(db, name, &size)) fail("asset size missing");
        auto data = loadfile(db, const_cast<char *>(name));
        if (!data) fail("asset missing");
        try {
            auto image = lastbeer::decode_pcx(std::span<const std::uint8_t>(data, size));
            if (image.width != 320 || image.height < 200 || image.pixels.size() != std::size_t(image.width * image.height))
                fail("incorrect PCX dimensions");
            bool rejected = false;
            try { lastbeer::decode_pcx(std::span<const std::uint8_t>(data, 128)); }
            catch (const std::exception &) { rejected = true; }
            if (!rejected) fail("truncated PCX accepted");
        } catch (const std::exception &e) { fail(e.what()); }
        unloadfile(data);
    }
    size_t missing = 0;
    if (database_file_size(db, "NOFILE", &missing)) fail("unknown asset reported present");
    closedatabase(db);
    shutfilemanager();
}
