#include "Harness.h"
#include <cassert>
#include <fstream>
#include <iostream>

extern unsigned char Oled_GRAM[PAGE_SIZE][WIDTH];

namespace {
bool pixel(int x, int y) {
    return (Oled_GRAM[y / 8][x] & (1u << (y % 8))) != 0;
}
bool letter_a(int x, int y) {
    static const char *rows[] = {
        ".###.", "#...#", "#...#", "#...#", "#####", "#...#", "#...#", "....."
    };
    return x >= 0 && x < 5 && y >= 0 && y < 8 && rows[y][x] == '#';
}
void check_a(int origin_x, int origin_y, int left, int top, int right, int bottom, bool erase) {
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            const bool ink = x >= left && x < right && y >= top && y < bottom &&
                letter_a(x - origin_x, y - origin_y);
            assert(pixel(x, y) == (erase ? !ink : ink));
        }
    }
}
}

void save_oled_frame(const char *name) {
    const char *directory = std::getenv("OLED_PREVIEW_DIR");
    if (!directory) return;
    std::ofstream image(std::string(directory) + "/" + name + ".pbm");
    assert(image);
    image << "P1\n" << WIDTH << ' ' << HEIGHT << '\n';
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) image << (pixel(x, y) ? '1' : '0') << ' ';
        image << '\n';
    }
}

void assert_oled_four_rows() {
    // 每行左右两侧都必须有可见文字，不能只由一个残留像素冒充完整页面。
    for (int page = 0; page < PAGE_SIZE; ++page) {
        unsigned left = 0, right = 0;
        for (int x = 0; x < WIDTH; ++x) {
            if (Oled_GRAM[page][x]) {
                if (x < 32) ++left;
                if (x >= 64) ++right;
            }
        }
        assert(left >= 10 && right >= 5);
    }
}

void oled_checks() {
    oled.xy_set(0, 0, WIDTH, PAGE_SIZE);
    oled.clr();
    oled.str(0, 0, "A", 8, 1, 0);
    check_a(0, 0, 0, 0, WIDTH, HEIGHT, false);

    const int origins[][2] = {{3, 5}, {-3, 1}, {124, 29}, {0, -4}, {0, -8}};
    for (const auto &origin : origins) {
        oled.clr();
        oled.write_char(origin[0], origin[1], 'A', 8, 1);
        check_a(origin[0], origin[1], 0, 0, WIDTH, HEIGHT, false);
        std::memset(Oled_GRAM, 0xff, sizeof(Oled_GRAM));
        oled.write_char(origin[0], origin[1], 'A', 8, 0);
        check_a(origin[0], origin[1], 0, 0, WIDTH, HEIGHT, true);
    }
    oled.xy_set(2, 1, 40, 2);
    oled.clr();
    oled.write_char(0, 5, 'A', 8, 1);
    check_a(0, 5, 2, 8, 40, 24, false);
    oled.xy_set(0, 0, WIDTH, PAGE_SIZE);
    oled.clr();
    oled.str(120, 0, "AA", 8, 1, 1);
    for (int y = 0; y < HEIGHT; ++y)
        for (int x = 0; x < WIDTH; ++x)
            assert(pixel(x, y) == (letter_a(x - 120, y) || letter_a(x, y - 8)));

    oled.clr();
    oled.str(120, 0, "AA", 8, 1, 0);
    for (int y = 0; y < HEIGHT; ++y)
        for (int x = 0; x < WIDTH; ++x)
            assert(pixel(x, y) == (letter_a(x - 120, y) || letter_a(x - 126, y)));

    oled.clr();
    oled.str(10, 8, "A", 16, 1, 0);
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            bool expected = false;
            if (x >= 10 && x < 18 && y >= 8 && y < 24) {
                const int offset = ('A' - ' ') * 16 + ((y - 8) / 8) * 8 + x - 10;
                expected = (F8X16[offset] & (1u << ((y - 8) % 8))) != 0;
            }
            assert(pixel(x, y) == expected);
        }
    }
    oled.clr();
    oled.str(0, 0, "A", 12, 1, 0);
    oled.write_char(0, 0, static_cast<char>(0xff), 8, 1);
    oled.write_char(0, 0, '\n', 8, 1);
    oled.write_char(0, 0, '~', 16, 1);
    for (const auto &page : Oled_GRAM)
        for (unsigned char column : page) assert(column == 0);
    for (int character = 33; character <= 126; ++character) {
        oled.clr();
        oled.write_char(0, 0, static_cast<char>(character), 8, 1);
        bool ink = false;
        for (int x = 0; x < 5; ++x) ink |= Oled_GRAM[0][x] != 0;
        assert(ink && Oled_GRAM[0][5] == 0);
        for (int page = 1; page < PAGE_SIZE; ++page)
            for (unsigned char column : Oled_GRAM[page]) assert(column == 0);
    }
    oled.clr();
    std::cout << "PASS OLED: real 6x8 glyph pixels, clipping, erase, wrap, ASCII range, legacy 8x16\n";
}
