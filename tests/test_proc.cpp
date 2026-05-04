#include "SimpleCV.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#define SC_ASSERT(expr) do { \
    if (!(expr)) { \
      std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << "  " #expr << "\n"; \
      return false; \
    } \
  } while(0)

using namespace SimpleCV;

// ===== resize =====

static bool test_resize_downscale_3ch()
{
    Mat src(8, 8, 3);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
        {
            unsigned char *p = src.data + y * src.step + x * 3;
            p[0] = (unsigned char)(x * 16);
            p[1] = (unsigned char)(y * 16);
            p[2] = 100;
        }

    Mat dst;
    resize(src, dst, 4, 4);
    SC_ASSERT(!dst.empty());
    SC_ASSERT(dst.width == 4 && dst.height == 4 && dst.channels == 3);
    SC_ASSERT(dst.depth == Depth::U8);
    return true;
}

static bool test_resize_upscale_1ch()
{
    Mat src(2, 2, 1);
    src.data[0] = 0;   src.data[1] = 255;
    src.data[2 + 0] = 100; src.data[2 + 1] = 50;
    // step is 2 (tightly packed), so row 1 starts at offset 2

    Mat dst;
    resize(src, dst, 8, 8);
    SC_ASSERT(!dst.empty());
    SC_ASSERT(dst.width == 8 && dst.height == 8 && dst.channels == 1);
    return true;
}

static bool test_resize_4ch_keeps_alpha()
{
    Mat src(4, 4, 4);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
        {
            unsigned char *p = src.data + y * src.step + x * 4;
            p[0] = 200; p[1] = 100; p[2] = 50; p[3] = 128;
        }
    Mat dst;
    resize(src, dst, 2, 2);
    SC_ASSERT(!dst.empty());
    SC_ASSERT(dst.channels == 4);
    // 全图常量 alpha 应保留
    for (int y = 0; y < dst.height; ++y)
        for (int x = 0; x < dst.width; ++x)
            SC_ASSERT(dst.data[y * dst.step + x * 4 + 3] == 128);
    return true;
}

static bool test_resize_reuses_dst()
{
    Mat src(6, 6, 3);
    std::memset(src.data, 200, (size_t)src.height * src.step);

    Mat dst(3, 3, 3);
    unsigned char *kept = dst.data;
    resize(src, dst, 3, 3);
    SC_ASSERT(dst.data == kept);  // 复用已有 buffer
    SC_ASSERT(dst.width == 3 && dst.height == 3);
    return true;
}

static bool test_resize_invalid_inputs()
{
    Mat empty;
    Mat dst;
    resize(empty, dst, 4, 4);
    SC_ASSERT(dst.empty());

    Mat src(4, 4, 3);
    resize(src, dst, 0, 4);
    SC_ASSERT(dst.empty());
    resize(src, dst, 4, -1);
    SC_ASSERT(dst.empty());
    return true;
}

// ===== copyMakeBorder =====

static bool test_border_constant_3ch()
{
    Mat src(2, 2, 3);
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x)
        {
            unsigned char *p = src.data + y * src.step + x * 3;
            p[0] = 10; p[1] = 20; p[2] = 30;
        }

    Mat dst;
    std::vector<unsigned char> v = {77, 88, 99};
    copyMakeBorder(src, dst, 1, 1, 1, 1, BorderType::CONSTANT, v);

    SC_ASSERT(dst.width == 4 && dst.height == 4 && dst.channels == 3);
    // 角落都是 (77,88,99)
    const unsigned char *tl = dst.data + 0 * dst.step + 0 * 3;
    SC_ASSERT(tl[0] == 77 && tl[1] == 88 && tl[2] == 99);
    const unsigned char *br = dst.data + 3 * dst.step + 3 * 3;
    SC_ASSERT(br[0] == 77 && br[1] == 88 && br[2] == 99);
    // 中心 (1,1) 来自 src(0,0)
    const unsigned char *c = dst.data + 1 * dst.step + 1 * 3;
    SC_ASSERT(c[0] == 10 && c[1] == 20 && c[2] == 30);
    return true;
}

static bool test_border_replicate()
{
    // 5x1 灰度： [1, 2, 3, 4, 5]
    Mat src(1, 5, 1);
    for (int x = 0; x < 5; ++x) src.data[x] = (unsigned char)(x + 1);

    Mat dst;
    copyMakeBorder(src, dst, 0, 0, 2, 2, BorderType::REPLICATE);
    SC_ASSERT(dst.width == 9 && dst.height == 1 && dst.channels == 1);

    // 期望：[1,1,1,2,3,4,5,5,5]
    unsigned char expected[9] = {1, 1, 1, 2, 3, 4, 5, 5, 5};
    for (int x = 0; x < 9; ++x)
        SC_ASSERT(dst.data[x] == expected[x]);
    return true;
}

static bool test_border_reflect_vs_reflect_101()
{
    // src: [1, 2, 3, 4, 5]
    Mat src(1, 5, 1);
    for (int x = 0; x < 5; ++x) src.data[x] = (unsigned char)(x + 1);

    // REFLECT, pad=2 each side: [2,1,1,2,3,4,5,5,4]
    {
        Mat dst;
        copyMakeBorder(src, dst, 0, 0, 2, 2, BorderType::REFLECT);
        SC_ASSERT(dst.width == 9);
        unsigned char want[9] = {2, 1, 1, 2, 3, 4, 5, 5, 4};
        for (int x = 0; x < 9; ++x)
            SC_ASSERT(dst.data[x] == want[x]);
    }

    // REFLECT_101, pad=2 each side: [3,2,1,2,3,4,5,4,3]
    {
        Mat dst;
        copyMakeBorder(src, dst, 0, 0, 2, 2, BorderType::REFLECT_101);
        SC_ASSERT(dst.width == 9);
        unsigned char want[9] = {3, 2, 1, 2, 3, 4, 5, 4, 3};
        for (int x = 0; x < 9; ++x)
            SC_ASSERT(dst.data[x] == want[x]);
    }
    return true;
}

static bool test_border_zero_padding_clones()
{
    Mat src(3, 3, 1);
    for (int i = 0; i < 9; ++i) src.data[i] = (unsigned char)i;

    Mat dst;
    copyMakeBorder(src, dst, 0, 0, 0, 0, BorderType::CONSTANT);
    SC_ASSERT(dst.width == 3 && dst.height == 3);
    for (int i = 0; i < 9; ++i)
        SC_ASSERT(dst.data[i] == src.data[i]);
    return true;
}

// ===== cvtColor on padded buffer =====

static bool test_cvtcolor_with_padded_step()
{
    // 3x2 RGB，每行 padding 到 step=12（实际 width*3=6）
    Mat src;
    src.create(2, 3, 3, Depth::U8, 12);
    SC_ASSERT(src.step == 12);
    SC_ASSERT(!src.isContinuous());

    // 行 0: (R=10, G=20, B=30) × 3
    // 行 1: (R=40, G=50, B=60) × 3
    for (int y = 0; y < 2; ++y)
    {
        unsigned char base_r = (y == 0) ? 10 : 40;
        unsigned char *row = src.data + y * src.step;
        for (int x = 0; x < 3; ++x)
        {
            row[x * 3 + 0] = base_r;
            row[x * 3 + 1] = (unsigned char)(base_r + 10);
            row[x * 3 + 2] = (unsigned char)(base_r + 20);
        }
        // padding 区写脏数据，验证 cvtColor 不读
        for (int p = 9; p < 12; ++p) row[p] = 0xCD;
    }

    Mat dst;
    cvtColor(src, dst, ColorSpace::BGR, ColorSpace::RGB);
    SC_ASSERT(!dst.empty());
    SC_ASSERT(dst.height == 2 && dst.width == 3 && dst.channels == 3);

    // 验证逐像素 R↔B 交换
    for (int y = 0; y < 2; ++y)
    {
        unsigned char base_r = (y == 0) ? 10 : 40;
        const unsigned char *row = dst.data + y * dst.step;
        for (int x = 0; x < 3; ++x)
        {
            SC_ASSERT(row[x * 3 + 0] == (unsigned char)(base_r + 20));
            SC_ASSERT(row[x * 3 + 1] == (unsigned char)(base_r + 10));
            SC_ASSERT(row[x * 3 + 2] == base_r);
        }
    }
    return true;
}

int main()
{
    int failed = 0;
    auto run = [&](const char *name, bool (*fn)()) {
        std::cerr << "[ RUN  ] " << name << "\n";
        if (!fn()) { ++failed; std::cerr << "[ FAIL ] " << name << "\n"; }
        else std::cerr << "[  OK  ] " << name << "\n";
    };

    run("resize_downscale_3ch", test_resize_downscale_3ch);
    run("resize_upscale_1ch", test_resize_upscale_1ch);
    run("resize_4ch_keeps_alpha", test_resize_4ch_keeps_alpha);
    run("resize_reuses_dst", test_resize_reuses_dst);
    run("resize_invalid_inputs", test_resize_invalid_inputs);
    run("border_constant_3ch", test_border_constant_3ch);
    run("border_replicate", test_border_replicate);
    run("border_reflect_vs_reflect_101", test_border_reflect_vs_reflect_101);
    run("border_zero_padding_clones", test_border_zero_padding_clones);
    run("cvtcolor_with_padded_step", test_cvtcolor_with_padded_step);

    if (failed)
    {
        std::cerr << failed << " test(s) failed\n";
        return 1;
    }
    std::cerr << "all tests passed\n";
    return 0;
}
