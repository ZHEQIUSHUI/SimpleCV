#include "SimpleCV.hpp"

#include <cstring>
#include <iostream>
#include <string>

#define SC_ASSERT(expr) do { \
    if (!(expr)) { \
      std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << "  " #expr << "\n"; \
      return false; \
    } \
  } while(0)

using namespace SimpleCV;

static bool test_get_text_size_grows_with_scale()
{
    int bl_a = 0, bl_b = 0;
    Size s1 = getTextSize("Hello", 1.0, 1, &bl_a);
    Size s2 = getTextSize("Hello", 2.0, 1, &bl_b);

    SC_ASSERT(s1.width > 0 && s1.height > 0);
    SC_ASSERT(s2.width > s1.width);
    SC_ASSERT(s2.height > s1.height);
    SC_ASSERT(bl_a >= 0 && bl_b >= bl_a);
    return true;
}

static bool test_get_text_size_multiline()
{
    int bl = 0;
    Size s_one = getTextSize("AB", 1.0, 1, &bl);
    Size s_two = getTextSize("AB\nCD", 1.0, 1, &bl);
    SC_ASSERT(s_two.height > s_one.height);
    return true;
}

// 在固定大小图上画字，验证：
//  - 被改写的像素都落在 (org.x, org.y - text_h) ~ (org.x + text_w, org.y) 包围盒内
//  - 至少有一定数量像素被改写
static bool test_put_text_writes_inside_bbox()
{
    Mat img(120, 320, 3);
    std::memset(img.data, 0, (size_t)img.height * img.step);

    const std::string text = "Hi";
    const double scale = 1.0;
    const int thickness = 1;
    int bl = 0;
    Size sz = getTextSize(text, scale, thickness, &bl);

    Point org(20, 80); // baseline-left
    putText(img, text, org, scale, Scalar(255, 255, 255), thickness);

    // 期望写入的 bbox（OpenCV 语义：org 是 baseline-left，文字在 org 上方）
    int bbox_x0 = org.x;
    int bbox_y1 = org.y;                  // 包围盒下沿
    int bbox_y0 = org.y - sz.height;      // 上沿（允许少量富余）
    int bbox_x1 = org.x + sz.width;

    int written = 0;
    int outside = 0;
    for (int y = 0; y < img.height; ++y)
    {
        const unsigned char *row = img.data + y * img.step;
        for (int x = 0; x < img.width; ++x)
        {
            const unsigned char *p = row + x * 3;
            if (p[0] || p[1] || p[2])
            {
                ++written;
                // 给点边界富余
                const int slack = 4;
                if (x < bbox_x0 - slack || x > bbox_x1 + slack ||
                    y < bbox_y0 - slack || y > bbox_y1 + slack)
                {
                    ++outside;
                }
            }
        }
    }

    SC_ASSERT(written > 50);  // 字形至少写入了一些像素
    SC_ASSERT(outside == 0);  // 全部落在 bbox 内
    return true;
}

static bool test_put_text_unsupported_chars_become_question()
{
    // 不可打印字符要么忽略要么替换为 '?'，都不应崩溃
    Mat img(80, 160, 1);
    std::memset(img.data, 0, (size_t)img.height * img.step);

    putText(img, std::string("\x01\x02"), Point(10, 60), 1.0, Scalar(255), 1);

    int written = 0;
    for (int y = 0; y < img.height; ++y)
        for (int x = 0; x < img.width; ++x)
            if (img.data[y * img.step + x] > 0)
                ++written;

    // 当前实现把 <0x20 的字符替换成 '?'，所以应当画出像素
    SC_ASSERT(written > 0);
    return true;
}

static bool test_put_text_empty_string_noop()
{
    Mat img(40, 80, 1);
    std::memset(img.data, 7, (size_t)img.height * img.step);
    putText(img, "", Point(10, 30), 1.0, Scalar(255), 1);
    // 全图保持原值
    for (int i = 0; i < img.height * img.step; ++i)
        SC_ASSERT(img.data[i] == 7);
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

    run("get_text_size_grows_with_scale", test_get_text_size_grows_with_scale);
    run("get_text_size_multiline", test_get_text_size_multiline);
    run("put_text_writes_inside_bbox", test_put_text_writes_inside_bbox);
    run("put_text_unsupported_chars_become_question", test_put_text_unsupported_chars_become_question);
    run("put_text_empty_string_noop", test_put_text_empty_string_noop);

    if (failed)
    {
        std::cerr << failed << " test(s) failed\n";
        return 1;
    }
    std::cerr << "all tests passed\n";
    return 0;
}
