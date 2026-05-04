#include "SimpleCV.hpp"

#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

#define SC_ASSERT(expr) do { \
    if (!(expr)) { \
      std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << "  " #expr << "\n"; \
      return false; \
    } \
  } while(0)

#define SC_ASSERT_NEAR(a, b, eps) do { \
    double _a = (double)(a), _b = (double)(b); \
    if (std::fabs(_a - _b) > (eps)) { \
      std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ \
                << " |" << _a << " - " << _b << "| > " << (eps) << "\n"; \
      return false; \
    } \
  } while(0)

using namespace SimpleCV;

static Mat make_pattern_rgb(int w, int h)
{
    Mat m(h, w, 3);
    for (int y = 0; y < h; ++y)
    {
        unsigned char *row = m.data + y * m.step;
        for (int x = 0; x < w; ++x)
        {
            unsigned char *p = row + x * 3;
            p[0] = (unsigned char)((x * 7) & 0xFF);
            p[1] = (unsigned char)((y * 11) & 0xFF);
            p[2] = (unsigned char)(((x + y) * 13) & 0xFF);
        }
    }
    return m;
}

static Matf make_affine(float a, float b, float tx, float ty)
{
    // forward similarity: [[a,-b,tx],[b,a,ty]]
    Matf M(2, 3);
    M.at(0, 0) = a;  M.at(0, 1) = -b; M.at(0, 2) = tx;
    M.at(1, 0) = b;  M.at(1, 1) = a;  M.at(1, 2) = ty;
    return M;
}

// ===== test 1: identity warp 应该等于 src（在内部区域） =====
static bool test_identity_warp()
{
    Mat src = make_pattern_rgb(64, 48);
    Matf I = make_affine(1.0f, 0.0f, 0.0f, 0.0f);
    Mat dst;
    warpAffine(src, dst, I, Size(64, 48), BorderType::CONSTANT, Scalar(0, 0, 0));

    SC_ASSERT(!dst.empty());
    SC_ASSERT(dst.width == 64 && dst.height == 48 && dst.channels == 3);

    // 内部像素逐字节相等
    for (int y = 1; y < 47; ++y)
    {
        const unsigned char *s = src.data + y * src.step;
        const unsigned char *d = dst.data + y * dst.step;
        for (int x = 1; x < 63; ++x)
        {
            for (int k = 0; k < 3; ++k)
            {
                if (d[x * 3 + k] != s[x * 3 + k])
                {
                    std::cerr << "[FAIL] identity diff at (" << x << "," << y
                              << ") k=" << k << " got=" << (int)d[x*3+k]
                              << " want=" << (int)s[x*3+k] << "\n";
                    return false;
                }
            }
        }
    }
    return true;
}

// ===== test 2: estimateAffinePartial2D 反推已知相似变换 =====
static bool test_estimate_recovers_similarity()
{
    // 真值：缩放 0.7、旋转 30°、平移 (5, -2)
    const float theta = 30.0f * 3.14159265358979323846f / 180.0f;
    const float s = 0.7f;
    const float a = s * std::cos(theta);
    const float b = s * std::sin(theta);
    const float tx = 5.0f, ty = -2.0f;

    std::vector<Point2f> from = {
        {10.0f, 20.0f}, {50.0f, 25.0f}, {30.0f, 60.0f},
        {70.0f, 80.0f}, {15.0f, 90.0f},
    };
    std::vector<Point2f> to;
    to.reserve(from.size());
    for (auto &p : from)
    {
        to.push_back(Point2f(a * p.x - b * p.y + tx,
                             b * p.x + a * p.y + ty));
    }

    Matf M = estimateAffinePartial2D(from, to);
    SC_ASSERT(!M.empty());
    SC_ASSERT(M.height == 2 && M.width == 3);

    SC_ASSERT_NEAR(M.at(0, 0), a, 1e-4);
    SC_ASSERT_NEAR(M.at(0, 1), -b, 1e-4);
    SC_ASSERT_NEAR(M.at(0, 2), tx, 1e-3);
    SC_ASSERT_NEAR(M.at(1, 0), b, 1e-4);
    SC_ASSERT_NEAR(M.at(1, 1), a, 1e-4);
    SC_ASSERT_NEAR(M.at(1, 2), ty, 1e-3);
    return true;
}

// ===== test 3: 端到端 — 估计 M，warp 后看一个亮点是否落在目标位置 =====
static bool test_end_to_end_alignment()
{
    // 在 src (200x200, 3ch) 中心 (100, 80) 画个 5x5 白方块
    Mat src(200, 200, 3);
    std::memset(src.data, 0, (size_t)src.height * src.step);
    for (int y = 78; y <= 82; ++y)
    {
        unsigned char *row = src.data + y * src.step;
        for (int x = 98; x <= 102; ++x)
        {
            unsigned char *p = row + x * 3;
            p[0] = p[1] = p[2] = 255;
        }
    }

    // 想把 (100, 80) 对齐到 dst 的 (56, 56)，dst 大小 112x112
    // 给 2 对点就够定位 + 缩放：用 4 对点更稳
    std::vector<Point2f> from = {
        {100.0f, 80.0f}, {110.0f, 80.0f}, {100.0f, 90.0f}, {90.0f, 80.0f}};
    std::vector<Point2f> to = {
        {56.0f, 56.0f}, {66.0f, 56.0f}, {56.0f, 66.0f}, {46.0f, 56.0f}};

    Matf M = estimateAffinePartial2D(from, to);
    SC_ASSERT(!M.empty());

    Mat aligned;
    warpAffine(src, aligned, M, Size(112, 112), BorderType::CONSTANT, Scalar(0, 0, 0));

    SC_ASSERT(!aligned.empty());
    SC_ASSERT(aligned.width == 112 && aligned.height == 112);

    // dst (56,56) 处应当亮（白方块中心被映射过来）
    const unsigned char *p = aligned.data + 56 * aligned.step + 56 * 3;
    SC_ASSERT(p[0] > 200 && p[1] > 200 && p[2] > 200);

    // 远离目标位置应当为黑
    const unsigned char *q = aligned.data + 10 * aligned.step + 10 * 3;
    SC_ASSERT(q[0] < 10 && q[1] < 10 && q[2] < 10);
    return true;
}

// ===== test 4: 退化输入处理 =====
static bool test_degenerate_inputs()
{
    Mat src = make_pattern_rgb(32, 32);
    Mat dst;

    // 空 M
    Matf M_empty;
    warpAffine(src, dst, M_empty, Size(32, 32));
    SC_ASSERT(dst.empty());

    // 退化 M（行列式 0）
    Matf M_bad = make_affine(0.0f, 0.0f, 0.0f, 0.0f);
    warpAffine(src, dst, M_bad, Size(32, 32));
    SC_ASSERT(dst.empty());

    // 点数不匹配
    std::vector<Point2f> a = {{0, 0}, {1, 1}};
    std::vector<Point2f> b = {{0, 0}};
    SC_ASSERT(estimateAffinePartial2D(a, b).empty());

    // 点数不足
    std::vector<Point2f> one = {{0, 0}};
    SC_ASSERT(estimateAffinePartial2D(one, one).empty());
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

    run("identity_warp", test_identity_warp);
    run("estimate_recovers_similarity", test_estimate_recovers_similarity);
    run("end_to_end_alignment", test_end_to_end_alignment);
    run("degenerate_inputs", test_degenerate_inputs);

    if (failed)
    {
        std::cerr << failed << " test(s) failed\n";
        return 1;
    }
    std::cerr << "all tests passed\n";
    return 0;
}
