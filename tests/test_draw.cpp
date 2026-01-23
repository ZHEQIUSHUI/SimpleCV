#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <iostream>

// 换成你的头文件
#include "SimpleCV.hpp"

using namespace SimpleCV;

static int g_fail = 0;

static void CHECK(bool cond, const char* msg) {
    if (!cond) {
        ++g_fail;
        std::cerr << "[FAIL] " << msg << "\n";
    }
}

static void CHECK_EQ_U8(unsigned char a, unsigned char b, const char* msg) {
    if (a != b) {
        ++g_fail;
        std::cerr << "[FAIL] " << msg << " expected=" << (int)b << " got=" << (int)a << "\n";
    }
}

// 读一个像素到 buf[4]（不足4的通道填0）
static void get_pixel(const Mat& img, int x, int y, unsigned char out[4]) {
    out[0] = out[1] = out[2] = out[3] = 0;
    if (img.empty()) return;
    if ((unsigned)x >= (unsigned)img.width || (unsigned)y >= (unsigned)img.height) return;
    const unsigned char* p = img.data + (size_t)y * (size_t)img.step + (size_t)x * (size_t)img.channels;
    for (int c = 0; c < img.channels && c < 4; ++c) out[c] = p[c];
}

static void CHECK_PIXEL_EQ(const Mat& img, int x, int y, const Scalar& color, const char* msg) {
    unsigned char pix[4];
    get_pixel(img, x, y, pix);

    CHECK_EQ_U8(pix[0], color.v[0], msg);
    if (img.channels >= 2) CHECK_EQ_U8(pix[1], color.v[1], msg);
    if (img.channels >= 3) CHECK_EQ_U8(pix[2], color.v[2], msg);
    if (img.channels >= 4) CHECK_EQ_U8(pix[3], color.v[3], msg);
}

static void CHECK_PIXEL_ZERO(const Mat& img, int x, int y, const char* msg) {
    unsigned char pix[4];
    get_pixel(img, x, y, pix);
    CHECK_EQ_U8(pix[0], 0, msg);
    if (img.channels >= 2) CHECK_EQ_U8(pix[1], 0, msg);
    if (img.channels >= 3) CHECK_EQ_U8(pix[2], 0, msg);
    if (img.channels >= 4) CHECK_EQ_U8(pix[3], 0, msg);
}

static bool save_image(const Mat& img, const std::string& path) {
    if (img.empty() || !img.data) return false;

    SimpleCV::imwrite(path, img);
    return true;
}

static void fill_zero(Mat& img) {
    if (img.empty() || !img.data) return;
    for (int y = 0; y < img.height; ++y) {
        std::memset(img.data + (size_t)y * (size_t)img.step, 0, (size_t)img.step);
    }
}

static void test_line_basic() {
    Mat img;
    img.create(64, 64, 3);
    fill_zero(img);

    Scalar col(10, 20, 30, 0);

    line(img, Point(0, 0), Point(63, 63), col, 1);

    CHECK_PIXEL_EQ(img, 0, 0, col, "line diag start");
    CHECK_PIXEL_EQ(img, 63, 63, col, "line diag end");
    CHECK_PIXEL_EQ(img, 10, 10, col, "line diag mid");

    CHECK_PIXEL_ZERO(img, 0, 63, "line should not color corner");
    save_image(img, "out_line_basic.png");
}

static void test_line_thick() {
    Mat img;
    img.create(64, 64, 3);
    fill_zero(img);

    Scalar col(100, 50, 25, 0);

    line(img, Point(5, 32), Point(58, 32), col, 7);

    // 中心线附近应该被涂到
    CHECK_PIXEL_EQ(img, 10, 32, col, "thick line center");
    CHECK_PIXEL_EQ(img, 10, 30, col, "thick line near center");
    CHECK_PIXEL_EQ(img, 10, 34, col, "thick line near center");

    // 离得太远应保持 0（粗略检查）
    CHECK_PIXEL_ZERO(img, 10, 20, "thick line far should be zero");

    save_image(img, "out_line_thick.png");
}

static void test_rectangle_border_and_fill() {
    Mat img;
    img.create(80, 80, 3);
    fill_zero(img);

    Scalar col(0, 200, 0, 0);

    // border thickness=3
    rectangle(img, Point(10, 10), Point(60, 60), col, 3);

    CHECK_PIXEL_EQ(img, 10, 10, col, "rect border corner");
    CHECK_PIXEL_EQ(img, 12, 10, col, "rect border top band");
    CHECK_PIXEL_EQ(img, 10, 12, col, "rect border left band");

    // inside should remain 0 (border only)
    CHECK_PIXEL_ZERO(img, 20, 20, "rect inside should be zero when border");
    save_image(img, "out_rect_border.png");

    // filled rectangle thickness<0
    fill_zero(img);
    rectangle(img, Point(10, 10), Point(60, 60), col, -1);

    CHECK_PIXEL_EQ(img, 20, 20, col, "rect filled inside");
    CHECK_PIXEL_EQ(img, 10, 10, col, "rect filled corner");
    CHECK_PIXEL_ZERO(img, 5, 5, "rect filled outside");
    save_image(img, "out_rect_filled.png");
}

static void test_rectangle_rect_overload_and_clip() {
    Mat img;
    img.create(50, 50, 3);
    fill_zero(img);

    Scalar col(200, 0, 0, 0);

    // 故意越界：应该裁剪，不崩溃
    rectangle(img, Rect(-10, -10, 30, 30), col, -1);

    CHECK_PIXEL_EQ(img, 0, 0, col, "clip filled rect should color (0,0)");
    CHECK_PIXEL_EQ(img, 10, 10, col, "clip filled rect inside");
    CHECK_PIXEL_ZERO(img, 40, 40, "clip filled rect outside");
    save_image(img, "out_rect_clip.png");
}

static void test_circle_border_and_fill() {
    Mat img;
    img.create(100, 100, 3);
    fill_zero(img);

    Scalar col(0, 0, 255, 0);

    // circle outline
    circle(img, Point(50, 50), 20, col, 1);

    // 圆周上几个点
    CHECK_PIXEL_EQ(img, 70, 50, col, "circle outline right");
    CHECK_PIXEL_EQ(img, 30, 50, col, "circle outline left");
    CHECK_PIXEL_EQ(img, 50, 70, col, "circle outline bottom");
    CHECK_PIXEL_EQ(img, 50, 30, col, "circle outline top");

    // 圆内中心应为 0（outline）
    CHECK_PIXEL_ZERO(img, 50, 50, "circle outline center should be zero");
    save_image(img, "out_circle_outline.png");

    // filled circle
    fill_zero(img);
    circle(img, Point(50, 50), 20, col, -1);
    CHECK_PIXEL_EQ(img, 50, 50, col, "circle filled center");
    CHECK_PIXEL_EQ(img, 60, 50, col, "circle filled inside");
    CHECK_PIXEL_ZERO(img, 10, 10, "circle filled outside");
    save_image(img, "out_circle_filled.png");
}

int main() {
    test_line_basic();
    test_line_thick();
    test_rectangle_border_and_fill();
    test_rectangle_rect_overload_and_clip();
    test_circle_border_and_fill();

    if (g_fail == 0) {
        std::cout << "[OK] all tests passed\n";
        return 0;
    } else {
        std::cout << "[FAIL] total failed checks: " << g_fail << "\n";
        return 1;
    }
}
