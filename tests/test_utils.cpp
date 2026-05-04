#include "SimpleCV.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#define SC_ASSERT(expr) do { \
    if (!(expr)) { \
      std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << "  " #expr << "\n"; \
      return false; \
    } \
  } while(0)

namespace fs = std::filesystem;

namespace
{
struct TempTree
{
    fs::path root;

    TempTree()
    {
        const auto seed = std::chrono::system_clock::now().time_since_epoch().count();
        std::mt19937_64 rng((uint64_t)seed);
        std::uniform_int_distribution<uint64_t> dist;
        root = fs::temp_directory_path() / ("scv_glob_" + std::to_string(dist(rng)));
        fs::create_directories(root);
    }
    ~TempTree()
    {
        std::error_code ec;
        fs::remove_all(root, ec);
    }

    void touch(const std::string &rel)
    {
        const fs::path p = root / rel;
        fs::create_directories(p.parent_path());
        std::ofstream(p.string()) << "x";
    }

    std::string at(const std::string &rel) const
    {
        return (root / rel).lexically_normal().string();
    }
};
}

static bool contains(const std::vector<std::string> &v, const std::string &needle)
{
    return std::find(v.begin(), v.end(), needle) != v.end();
}

static bool test_glob_star_in_one_dir()
{
    TempTree t;
    t.touch("a.png");
    t.touch("b.png");
    t.touch("c.txt");

    auto pat = (t.root / "*.png").string();
    auto r = SimpleCV::glob(pat);

    SC_ASSERT(r.size() == 2);
    SC_ASSERT(contains(r, t.at("a.png")));
    SC_ASSERT(contains(r, t.at("b.png")));
    return true;
}

static bool test_glob_question_mark()
{
    TempTree t;
    t.touch("a.png");
    t.touch("ab.png");
    t.touch("abc.png");

    // 单字符通配
    auto pat = (t.root / "?.png").string();
    auto r = SimpleCV::glob(pat);

    SC_ASSERT(r.size() == 1);
    SC_ASSERT(contains(r, t.at("a.png")));
    return true;
}

static bool test_glob_double_star_recursive()
{
    TempTree t;
    t.touch("a.png");
    t.touch("sub/b.png");
    t.touch("sub/c.txt");
    t.touch("sub/deeper/d.png");

    auto pat = (t.root / "**" / "*.png").string();
    auto r = SimpleCV::glob(pat);

    // ** 含 0 层与多层：根目录 + sub + sub/deeper
    SC_ASSERT(r.size() == 3);
    SC_ASSERT(contains(r, t.at("a.png")));
    SC_ASSERT(contains(r, t.at("sub/b.png")));
    SC_ASSERT(contains(r, t.at("sub/deeper/d.png")));
    return true;
}

static bool test_glob_double_star_disabled()
{
    TempTree t;
    t.touch("sub/x.png");

    // recursive_double_star=false：当作字面目录名 "**"，不会递归
    auto pat = (t.root / "**" / "*.png").string();
    auto r = SimpleCV::glob(pat, /*recursive_double_star=*/false);
    SC_ASSERT(r.empty());
    return true;
}

static bool test_glob_no_match()
{
    TempTree t;
    t.touch("a.txt");

    auto pat = (t.root / "*.png").string();
    auto r = SimpleCV::glob(pat);
    SC_ASSERT(r.empty());
    return true;
}

static bool test_glob_results_sorted_and_unique()
{
    TempTree t;
    t.touch("c.png");
    t.touch("a.png");
    t.touch("b.png");

    auto pat = (t.root / "*.png").string();
    auto r = SimpleCV::glob(pat);

    SC_ASSERT(r.size() == 3);
    SC_ASSERT(std::is_sorted(r.begin(), r.end()));
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

    run("glob_star_in_one_dir", test_glob_star_in_one_dir);
    run("glob_question_mark", test_glob_question_mark);
    run("glob_double_star_recursive", test_glob_double_star_recursive);
    run("glob_double_star_disabled", test_glob_double_star_disabled);
    run("glob_no_match", test_glob_no_match);
    run("glob_results_sorted_and_unique", test_glob_results_sorted_and_unique);

    if (failed)
    {
        std::cerr << failed << " test(s) failed\n";
        return 1;
    }
    std::cerr << "all tests passed\n";
    return 0;
}
