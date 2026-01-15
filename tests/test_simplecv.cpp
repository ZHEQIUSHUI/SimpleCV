#include "SimpleCV.hpp"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

// ----------------- minimal test helpers -----------------
#define SC_ASSERT(expr) do { \
    if (!(expr)) { \
      std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << "  " #expr << "\n"; \
      return false; \
    } \
  } while(0)

static bool bytes_equal(const unsigned char* a, const unsigned char* b, size_t n)
{
  return std::memcmp(a, b, n) == 0;
}

static void fill_pattern_rgb(SimpleCV::Mat& m)
{
  // RGB pattern: R = x, G = y, B = x+y
  for (int y = 0; y < m.height; ++y)
  {
    unsigned char* row = m.data + y * m.step;
    for (int x = 0; x < m.width; ++x)
    {
      unsigned char* p = row + x * 3;
      p[0] = static_cast<unsigned char>(x);
      p[1] = static_cast<unsigned char>(y);
      p[2] = static_cast<unsigned char>(x + y);
    }
  }
}

static bool test_mat_copy_and_clone()
{
  SimpleCV::Mat a(2, 3, 3);
  fill_pattern_rgb(a);

  // shallow copy shares data
  SimpleCV::Mat b = a;
  SC_ASSERT(a.data == b.data);
  b.data[0] = 123;
  SC_ASSERT(a.data[0] == 123);

  // clone deep copies
  SimpleCV::Mat c = a.clone();
  SC_ASSERT(c.data != a.data);
  SC_ASSERT(bytes_equal(c.data, a.data, static_cast<size_t>(a.height) * a.step));
  c.data[0] = 7;
  SC_ASSERT(a.data[0] == 123);
  SC_ASSERT(c.data[0] == 7);
  return true;
}

static bool test_cvt_rgb_bgr()
{
  SimpleCV::Mat rgb(1, 2, 3);
  // pixel0: (10,20,30), pixel1: (1,2,3)
  rgb.data[0] = 10; rgb.data[1] = 20; rgb.data[2] = 30;
  rgb.data[3] = 1;  rgb.data[4] = 2;  rgb.data[5] = 3;

  auto bgr = SimpleCV::cvtColor(rgb, SimpleCV::ColorSpace::BGR, SimpleCV::ColorSpace::RGB);
  SC_ASSERT(bgr.channels == 3);
  SC_ASSERT(bgr.data[0] == 30 && bgr.data[1] == 20 && bgr.data[2] == 10);
  SC_ASSERT(bgr.data[3] == 3  && bgr.data[4] == 2  && bgr.data[5] == 1);
  return true;
}

static bool test_cvt_rgb_gray()
{
  SimpleCV::Mat rgb(1, 1, 3);
  rgb.data[0] = 100; // R
  rgb.data[1] = 150; // G
  rgb.data[2] = 200; // B

  auto g = SimpleCV::cvtColor(rgb, SimpleCV::ColorSpace::GRAY, SimpleCV::ColorSpace::RGB);
  SC_ASSERT(g.channels == 1);

  // expected = round((299R + 587G + 114B)/1000)
  int expected = (299*100 + 587*150 + 114*200 + 500) / 1000;
  SC_ASSERT(g.data[0] == static_cast<unsigned char>(expected));
  return true;
}

static bool test_cvt_rgba_bgra_and_back()
{
  SimpleCV::Mat rgba(1, 1, 4);
  rgba.data[0] = 11; // R
  rgba.data[1] = 22; // G
  rgba.data[2] = 33; // B
  rgba.data[3] = 44; // A

  auto bgra = SimpleCV::cvtColor(rgba, SimpleCV::ColorSpace::BGRA, SimpleCV::ColorSpace::RGBA);
  SC_ASSERT(bgra.channels == 4);
  SC_ASSERT(bgra.data[0] == 33 && bgra.data[1] == 22 && bgra.data[2] == 11 && bgra.data[3] == 44);

  auto rgba2 = SimpleCV::cvtColor(bgra, SimpleCV::ColorSpace::RGBA, SimpleCV::ColorSpace::BGRA);
  SC_ASSERT(rgba2.channels == 4);
  SC_ASSERT(rgba2.data[0] == 11 && rgba2.data[1] == 22 && rgba2.data[2] == 33 && rgba2.data[3] == 44);
  return true;
}

static bool test_cvt_gray_to_rgba()
{
  SimpleCV::Mat g(1, 2, 1);
  g.data[0] = 9;
  g.data[1] = 250;

  auto rgba = SimpleCV::cvtColor(g, SimpleCV::ColorSpace::RGBA, SimpleCV::ColorSpace::GRAY);
  SC_ASSERT(rgba.channels == 4);
  // (v,v,v,255)
  SC_ASSERT(rgba.data[0] == 9 && rgba.data[1] == 9 && rgba.data[2] == 9 && rgba.data[3] == 255);
  SC_ASSERT(rgba.data[4] == 250 && rgba.data[5] == 250 && rgba.data[6] == 250 && rgba.data[7] == 255);
  return true;
}

static bool test_imencode_imdecode_png_roundtrip()
{
  SimpleCV::Mat rgb(4, 5, 3);
  fill_pattern_rgb(rgb);

  std::vector<unsigned char> buf;
  SC_ASSERT(SimpleCV::imencode(rgb, buf) == 1);
  SC_ASSERT(!buf.empty());

  auto dec = SimpleCV::imdecode(buf, SimpleCV::ColorSpace::RGB);
  SC_ASSERT(dec.height == rgb.height && dec.width == rgb.width && dec.channels == 3);
  SC_ASSERT(bytes_equal(dec.data, rgb.data, static_cast<size_t>(rgb.height) * rgb.step));
  return true;
}

static bool test_imwrite_imread_flags()
{
  SimpleCV::Mat rgb(3, 4, 3);
  fill_pattern_rgb(rgb);

  fs::path out = fs::current_path() / "simplecv_test_out.png";
  SC_ASSERT(SimpleCV::imwrite(out.string(), rgb) == 1);

  auto r = SimpleCV::imread(out.string(), SimpleCV::ColorSpace::RGB);
  SC_ASSERT(r.height == rgb.height && r.width == rgb.width && r.channels == 3);
  SC_ASSERT(bytes_equal(r.data, rgb.data, static_cast<size_t>(rgb.height) * rgb.step));

  auto b = SimpleCV::imread(out.string(), SimpleCV::ColorSpace::BGR);
  SC_ASSERT(b.height == rgb.height && b.width == rgb.width && b.channels == 3);
  // check swap: pixel(0,0) in rgb is (0,0,0). choose a non-zero pixel
  int x = 2, y = 1;
  const unsigned char* rp = r.data + y * r.step + x * 3;
  const unsigned char* bp = b.data + y * b.step + x * 3;
  SC_ASSERT(bp[0] == rp[2] && bp[1] == rp[1] && bp[2] == rp[0]);

  std::error_code ec;
  fs::remove(out, ec);
  return true;
}

int main()
{
  struct Case { const char* name; bool (*fn)(); };
  Case cases[] = {
    {"mat_copy_and_clone", test_mat_copy_and_clone},
    {"cvt_rgb_bgr", test_cvt_rgb_bgr},
    {"cvt_rgb_gray", test_cvt_rgb_gray},
    {"cvt_rgba_bgra_and_back", test_cvt_rgba_bgra_and_back},
    {"cvt_gray_to_rgba", test_cvt_gray_to_rgba},
    {"imencode_imdecode_png_roundtrip", test_imencode_imdecode_png_roundtrip},
    {"imwrite_imread_flags", test_imwrite_imread_flags},
  };

  int passed = 0;
  for (auto& t : cases)
  {
    bool ok = false;
    try { ok = t.fn(); }
    catch (const std::exception& e)
    {
      std::cerr << "[EXCEPTION] " << t.name << ": " << e.what() << "\n";
      ok = false;
    }

    if (ok)
    {
      ++passed;
      std::cout << "[PASS] " << t.name << "\n";
    }
    else
    {
      std::cout << "[FAIL] " << t.name << "\n";
    }
  }

  std::cout << "\n" << passed << "/" << (int)(sizeof(cases)/sizeof(cases[0])) << " tests passed.\n";
  return (passed == (int)(sizeof(cases)/sizeof(cases[0]))) ? 0 : 1;
}
