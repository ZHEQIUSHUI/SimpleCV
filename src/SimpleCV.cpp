#include "SimpleCV.hpp"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#ifndef STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#endif
#ifndef STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#endif

#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_image_resize2.h"

#include <cctype>
#include <algorithm>
#include <filesystem>
#include <string_view>


namespace SimpleCV
{
    static inline ColorSpace infer_space_from_channels(const Mat &m)
    {
        if (m.channels == 1)
            return ColorSpace::GRAY;
        if (m.channels == 3)
            return ColorSpace::RGB; // 默认按 RGB 处理
        if (m.channels == 4)
            return ColorSpace::RGBA; // 默认按 RGBA 处理
        return ColorSpace::UNCHANGED;
    }

    static inline int desired_channels(ColorSpace flag)
    {
        switch (flag)
        {
        case ColorSpace::GRAY:
            return 1;
        case ColorSpace::RGB:
        case ColorSpace::BGR:
            return 3;
        case ColorSpace::RGBA:
        case ColorSpace::BGRA:
            return 4;
        case ColorSpace::UNCHANGED:
        default:
            return 0; // stb: 0 = keep original
        }
    }

    static inline bool is_bgr_family(ColorSpace s)
    {
        return s == ColorSpace::BGR || s == ColorSpace::BGRA;
    }

    static inline bool is_rgb_family(ColorSpace s)
    {
        return s == ColorSpace::RGB || s == ColorSpace::RGBA;
    }

    // 交换 R<->B（适用于 3/4 通道）
    static inline void swap_rb_inplace(Mat &m)
    {
        if (m.empty())
            return;
        if (m.channels != 3 && m.channels != 4)
            return;

        for (int y = 0; y < m.height; ++y)
        {
            unsigned char *row = m.data + y * m.step;
            if (m.channels == 3)
            {
                for (int x = 0; x < m.width; ++x)
                {
                    unsigned char *p = row + x * 3;
                    std::swap(p[0], p[2]);
                }
            }
            else
            {
                for (int x = 0; x < m.width; ++x)
                {
                    unsigned char *p = row + x * 4;
                    std::swap(p[0], p[2]);
                }
            }
        }
    }
}

namespace SimpleCV
{
    Mat imread(const std::string &filename, ColorSpace flag)
    {
        int w = 0, h = 0, c = 0;
        const int req_c = desired_channels(flag);

        unsigned char *p = stbi_load(filename.c_str(), &w, &h, &c, req_c);
        if (!p)
            return Mat();

        int out_c = (req_c != 0) ? req_c : c;

        Mat m;
        m.owner_ = std::shared_ptr<unsigned char>(p, [](unsigned char *ptr)
                                                  { stbi_image_free(ptr); });
        m.height = h;
        m.width = w;
        m.channels = out_c;
        m.step = w * out_c;
        m.data = m.owner_.get();

        // stb 输出 3/4 通道默认 RGB/RGBA，如果用户要 BGR/BGRA，则交换 R/B
        if (flag == ColorSpace::BGR || flag == ColorSpace::BGRA)
        {
            swap_rb_inplace(m);
        }

        return m;
    }

    Mat imdecode(const std::vector<unsigned char> &buf, ColorSpace flag)
    {
        if (buf.empty())
            return Mat();

        int w = 0, h = 0, c = 0;
        const int req_c = desired_channels(flag);

        unsigned char *p = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc *>(buf.data()),
            static_cast<int>(buf.size()),
            &w, &h, &c, req_c);

        if (!p)
            return Mat();

        int out_c = (req_c != 0) ? req_c : c;

        Mat m;
        m.owner_ = std::shared_ptr<unsigned char>(p, [](unsigned char *ptr)
                                                  { stbi_image_free(ptr); });
        m.height = h;
        m.width = w;
        m.channels = out_c;
        m.step = w * out_c;
        m.data = m.owner_.get();

        if (flag == ColorSpace::BGR || flag == ColorSpace::BGRA)
        {
            swap_rb_inplace(m);
        }

        return m;
    }
}

namespace SimpleCV
{
    static inline std::string to_lower(std::string s)
    {
        for (char &ch : s)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return s;
    }

    static inline std::string file_ext_lower(const std::string &filename)
    {
        auto pos = filename.find_last_of('.');
        if (pos == std::string::npos)
            return "";
        return to_lower(filename.substr(pos + 1));
    }

    // stb 写入内存回调
    static void stb_write_to_vector(void *context, void *data, int size)
    {
        auto *v = reinterpret_cast<std::vector<unsigned char> *>(context);
        auto *bytes = reinterpret_cast<unsigned char *>(data);
        v->insert(v->end(), bytes, bytes + size);
    }

    bool imencode(const Mat &mat, std::vector<unsigned char> &buf)
    {
        buf.clear();
        if (mat.empty())
            return false;

        // 默认用 PNG（无损、通用）
        // stride 是每行字节数
        int ok = stbi_write_png_to_func(
            stb_write_to_vector,
            &buf,
            mat.width,
            mat.height,
            mat.channels,
            mat.data,
            mat.step);
        return ok ? true : false;
    }

    bool imwrite(const std::string &filename, const Mat &mat)
    {
        if (mat.empty())
            return false;

        const std::string ext = file_ext_lower(filename);
        int ok = 0;

        if (ext == "png")
        {
            ok = stbi_write_png(filename.c_str(), mat.width, mat.height, mat.channels, mat.data, mat.step);
        }
        else if (ext == "jpg" || ext == "jpeg")
        {
            // jpg 质量默认 95
            // 注意：stb 写 jpg 不支持带 alpha；如果 channels==4 这里会失败/结果异常，通常要先转成 3 通道
            ok = stbi_write_jpg(filename.c_str(), mat.width, mat.height, mat.channels, mat.data, 95);
        }
        else if (ext == "bmp")
        {
            ok = stbi_write_bmp(filename.c_str(), mat.width, mat.height, mat.channels, mat.data);
        }
        else if (ext == "tga")
        {
            ok = stbi_write_tga(filename.c_str(), mat.width, mat.height, mat.channels, mat.data);
        }
        else
        {
            // 未知扩展名：默认写 png
            ok = stbi_write_png(filename.c_str(), mat.width, mat.height, mat.channels, mat.data, mat.step);
        }

        return ok ? true : false;
    }
}

namespace SimpleCV
{
    void resize(const Mat &src, Mat &dst, int dst_width, int dst_height)
    {
        if (src.empty() || dst_width <= 0 || dst_height <= 0)
        {
            dst.release();
            return;
        }
        if (src.step < src.width * src.channels)
        { // 防御：src stride 必须够
            dst.release();
            return;
        }

        // 处理 in-place / 共享内存（最简单：指针相等就当冲突）
        if (dst.data == src.data)
        {
            Mat tmp(dst_height, dst_width, src.channels);
            resize(src, tmp, dst_width, dst_height);
            dst = tmp; // 或 swap
            return;
        }

        bool can_reuse =
            dst.data &&
            dst.width == dst_width &&
            dst.height == dst_height &&
            dst.channels == src.channels &&
            dst.step >= dst_width * dst.channels;

        if (!can_reuse)
            dst.create(dst_height, dst_width, src.channels);

        if (dst.step < dst.width * dst.channels)
        { // 防御：dst stride 必须够
            dst.release();
            return;
        }

        stbir_pixel_layout layout;
        switch (src.channels)
        {
        case 1:
            layout = STBIR_1CHANNEL;
            break;
        case 2:
            layout = STBIR_2CHANNEL;
            break;
        case 3:
            layout = STBIR_RGB;
            break;
        case 4:
            layout = STBIR_RGBA;
            break;
        default:
            dst.release();
            return;
        }

        unsigned char *out = stbir_resize_uint8_linear(
            src.data, src.width, src.height, src.step,
            dst.data, dst.width, dst.height, dst.step,
            layout);

        if (!out)
            dst.release();
    }

}

namespace SimpleCV
{
    static inline unsigned char clamp_u8(int v)
    {
        return (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v));
    }

    static inline unsigned char rgb_to_gray_u8(unsigned char r, unsigned char g, unsigned char b)
    {
        // 0.299R + 0.587G + 0.114B
        int y = (299 * (int)r + 587 * (int)g + 114 * (int)b + 500) / 1000;
        return clamp_u8(y);
    }

    static inline bool dst_buffer_compatible(const Mat &dst, int h, int w, int c)
    {
        if (dst.empty())
            return false;
        if (dst.height != h || dst.width != w || dst.channels != c)
            return false;
        const int min_step = w * c;
        return dst.step >= min_step && dst.data != nullptr;
    }

    void cvtColor(const Mat &src, Mat &dst, ColorSpace dst_space, ColorSpace src_space)
    {
        if (src.empty())
        {
            dst.release();
            return;
        }

        if (src_space == ColorSpace::AUTO)
            src_space = infer_space_from_channels(src);

        // 先把 src_space 归一到 {GRAY, RGB, BGR, RGBA, BGRA}
        if (src_space == ColorSpace::UNCHANGED)
            src_space = infer_space_from_channels(src);

        // dst_space 也不允许 UNCHANGED/AUTO（你也可以允许 AUTO=按 src 直接返回 clone）
        if (dst_space == ColorSpace::AUTO || dst_space == ColorSpace::UNCHANGED)
        {
            // AUTO/UNCHANGED：保持源格式，做浅拷贝（共享底层 buffer）
            dst = src;
            return;
        }

        auto dst_ch = desired_channels(dst_space);
        if (dst_ch == 0)
        {
            dst.release();
            return;
        }

        // 如果用户提供的 dst 尺寸/通道/stride 满足需求，则直接复用；否则重新分配
        if (!dst_buffer_compatible(dst, src.height, src.width, dst_ch))
        {
            dst.create(src.height, src.width, dst_ch);
        }

        // 一些快速路径
        if (src_space == dst_space)
        {
            // channels 必须一致才能直接 copy
            if (src.channels == dst.channels && src.step == dst.step)
            {
                std::memcpy(dst.data, src.data, (size_t)src.height * (size_t)src.step);
                return;
            }
        }

        // 逐像素转换
        for (int y = 0; y < src.height; ++y)
        {
            const unsigned char *sp = src.data + y * src.step;
            unsigned char *dp = dst.data + y * dst.step;

            for (int x = 0; x < src.width; ++x)
            {
                // 先从 src 读出 (r,g,b,a) 的“逻辑值”
                unsigned char r = 0, g = 0, b = 0, a = 255;

                if (src_space == ColorSpace::GRAY)
                {
                    unsigned char v = sp[x];
                    r = g = b = v;
                    a = 255;
                }
                else if (src_space == ColorSpace::RGB)
                {
                    const unsigned char *p = sp + x * 3;
                    r = p[0];
                    g = p[1];
                    b = p[2];
                    a = 255;
                }
                else if (src_space == ColorSpace::BGR)
                {
                    const unsigned char *p = sp + x * 3;
                    b = p[0];
                    g = p[1];
                    r = p[2];
                    a = 255;
                }
                else if (src_space == ColorSpace::RGBA)
                {
                    const unsigned char *p = sp + x * 4;
                    r = p[0];
                    g = p[1];
                    b = p[2];
                    a = p[3];
                }
                else if (src_space == ColorSpace::BGRA)
                {
                    const unsigned char *p = sp + x * 4;
                    b = p[0];
                    g = p[1];
                    r = p[2];
                    a = p[3];
                }
                else
                {
                    // 不支持的 src_space
                    dst.release();
                    return;
                }

                // 写入 dst
                if (dst_space == ColorSpace::GRAY)
                {
                    dp[x] = rgb_to_gray_u8(r, g, b);
                }
                else if (dst_space == ColorSpace::RGB)
                {
                    unsigned char *q = dp + x * 3;
                    q[0] = r;
                    q[1] = g;
                    q[2] = b;
                }
                else if (dst_space == ColorSpace::BGR)
                {
                    unsigned char *q = dp + x * 3;
                    q[0] = b;
                    q[1] = g;
                    q[2] = r;
                }
                else if (dst_space == ColorSpace::RGBA)
                {
                    unsigned char *q = dp + x * 4;
                    q[0] = r;
                    q[1] = g;
                    q[2] = b;
                    q[3] = a;
                }
                else if (dst_space == ColorSpace::BGRA)
                {
                    unsigned char *q = dp + x * 4;
                    q[0] = b;
                    q[1] = g;
                    q[2] = r;
                    q[3] = a;
                }
                else
                {
                    dst.release();
                    return;
                }
            }
        }
    }

    Mat cvtColor(const Mat &src, ColorSpace dst_space, ColorSpace src_space)
    {
        Mat out;
        cvtColor(src, out, dst_space, src_space);
        return out;
    }
}

namespace SimpleCV
{
    static inline unsigned char border_pick_value(
        const std::vector<unsigned char> &v, int k)
    {
        if (v.empty())
            return 0;
        if (v.size() == 1)
            return v[0];
        if (v.size() == 3)
            return (k < 3) ? v[k] : 255;
        if (k < (int)v.size())
            return v[k];
        return v.back();
    }

    static inline int border_map_coord(int p, int len, BorderType bt)
    {
        // 把越界坐标 p 映射到 [0, len-1]
        // len 必须 > 0
        if (len == 1)
            return 0;

        if (bt == BorderType::REPLICATE)
        {
            if (p < 0)
                return 0;
            if (p >= len)
                return len - 1;
            return p;
        }

        if (bt == BorderType::REFLECT || bt == BorderType::REFLECT_101)
        {
            // OpenCV 语义：
            // REFLECT:      fedcba|abcdefgh|hgfedcb
            // REFLECT_101:   gfedcb|abcdefgh|gfedcba  （边界像素不重复）
            const int delta = (bt == BorderType::REFLECT_101) ? 1 : 0;

            while (p < 0 || p >= len)
            {
                if (p < 0)
                    p = -p - 1 + delta;
                else
                    p = (2 * len - 1) - p - delta;
            }
            // 保险
            if (p < 0)
                p = 0;
            if (p >= len)
                p = len - 1;
            return p;
        }

        // CONSTANT 不需要映射（调用方会走填充分支）
        // 为了安全：夹紧
        if (p < 0)
            return 0;
        if (p >= len)
            return len - 1;
        return p;
    }

    void copyMakeBorder(
        const Mat &src,
        Mat &dst,
        int top, int bottom, int left, int right,
        BorderType borderType,
        const std::vector<unsigned char> &value)
    {
        if (src.empty())
        {
            dst.release();
            return;
        }
        if (top < 0)
            top = 0;
        if (bottom < 0)
            bottom = 0;
        if (left < 0)
            left = 0;
        if (right < 0)
            right = 0;

        const int c = src.channels;
        const int out_h = src.height + top + bottom;
        const int out_w = src.width + left + right;

        if (out_h <= 0 || out_w <= 0 || c <= 0)
        {
            dst.release();
            return;
        }

        Mat out(out_h, out_w, c); // step = out_w*c (紧密)

        // ===== 1) CONSTANT：先整张填充，再把 src 贴进去（最快）=====
        if (borderType == BorderType::CONSTANT)
        {
            // fill
            for (int y = 0; y < out.height; ++y)
            {
                unsigned char *row = out.data + y * out.step;
                for (int x = 0; x < out.width; ++x)
                {
                    unsigned char *p = row + x * c;
                    for (int k = 0; k < c; ++k)
                        p[k] = border_pick_value(value, k);
                }
            }

            // paste center
            for (int y = 0; y < src.height; ++y)
            {
                unsigned char *drow = out.data + (y + top) * out.step + left * c;
                const unsigned char *srow = src.data + y * src.step;
                std::memcpy(drow, srow, (size_t)src.width * (size_t)c);
            }

            dst = std::move(out);
            return;
        }

        // ===== 2) 非 CONSTANT：逐像素用映射规则取 src 对应像素（简单通用）=====
        for (int y = 0; y < out.height; ++y)
        {
            const int sy = border_map_coord(y - top, src.height, borderType);
            const unsigned char *srow = src.data + sy * src.step;
            unsigned char *drow = out.data + y * out.step;

            for (int x = 0; x < out.width; ++x)
            {
                const int sx = border_map_coord(x - left, src.width, borderType);
                const unsigned char *sp = srow + sx * c;
                unsigned char *dp = drow + x * c;

                // copy pixel
                for (int k = 0; k < c; ++k)
                    dp[k] = sp[k];
            }
        }

        dst = std::move(out);
    }
}

namespace fs = std::filesystem;

namespace SimpleCV
{

    static inline bool is_sep(char c)
    {
#ifdef _WIN32
        return c == '/' || c == '\\';
#else
        return c == '/';
#endif
    }

    static inline std::string normalize_seps(std::string s)
    {
#ifdef _WIN32
        // filesystem 能处理两种分隔符，但我们内部统一成 '\\' 或 '/'
        // 这里统一成 fs::path::preferred_separator
        for (auto &ch : s)
        {
            if (ch == '/' || ch == '\\')
                ch = fs::path::preferred_separator;
        }
#else
        for (auto &ch : s)
        {
            if (ch == '\\')
                ch = '/';
        }
#endif
        return s;
    }

    // 简单通配符匹配：支持 '*' '?'，不支持字符组 []（你如果需要我也能加）
    static bool match_wildcard(std::string_view str, std::string_view pat)
    {
        size_t s = 0, p = 0;
        size_t star = std::string_view::npos, match = 0;

        while (s < str.size())
        {
            if (p < pat.size() && (pat[p] == '?' || pat[p] == str[s]))
            {
                ++s;
                ++p;
            }
            else if (p < pat.size() && pat[p] == '*')
            {
                star = p++;
                match = s;
            }
            else if (star != std::string_view::npos)
            {
                p = star + 1;
                s = ++match;
            }
            else
            {
                return false;
            }
        }
        while (p < pat.size() && pat[p] == '*')
            ++p;
        return p == pat.size();
    }

    static bool has_wildcards(const std::string &s)
    {
        return s.find('*') != std::string::npos || s.find('?') != std::string::npos;
    }

    // 把 pattern 拆成 path 部分（base dir）+ 余下的段（segments）
    // 例如 "a/b/**/c*.png" -> base="a/b", segments=["**","c*.png"]
    static void split_pattern(const std::string &pattern,
                              fs::path &base_dir,
                              std::vector<std::string> &segments)
    {
        std::string pat = normalize_seps(pattern);

        // 先按分隔符拆段
        std::vector<std::string> parts;
        std::string cur;
        for (char ch : pat)
        {
            if (is_sep(ch))
            {
                if (!cur.empty())
                {
                    parts.push_back(cur);
                    cur.clear();
                }
            }
            else
            {
                cur.push_back(ch);
            }
        }
        if (!cur.empty())
            parts.push_back(cur);

        // base_dir：直到遇到第一个含通配符的段（或 **）
        base_dir.clear();
#ifdef _WIN32
        // Windows: 处理盘符/UNC 前缀由 filesystem 自己解析更可靠
        // 如果 pattern 里有盘符，filesystem 会把 "C:" 当作一个段
#endif

        size_t i = 0;

        // 如果 pattern 是绝对路径，保留根
        fs::path full = fs::path(pat);
        if (full.is_absolute())
        {
            // 这里拿 root_path（比如 "C:\\" 或 "/"），再从相对部分开始拆
            base_dir = full.root_path();
            // 对绝对路径，full.relative_path() 会去掉 root
            // 我们用 full.relative_path() 重新拆段会更复杂，这里简化：
            // 直接用 parts 继续往下拼（盘符已在 root_path 里时，parts 里可能仍含 "C:"）
            // 保险起见：如果 base_dir 有 root_name/root_directory，跳过 parts 开头的 root_name 可能出现的重复
            // （这块不是很完美，但一般 pattern 传入 fs 能正确解析）
        }

        // base_dir 从头拼到第一个通配段之前
        for (; i < parts.size(); ++i)
        {
            const auto &seg = parts[i];
            if (seg == "**" || has_wildcards(seg))
                break;
            base_dir /= seg;
        }

        // 余下都作为 segments
        for (; i < parts.size(); ++i)
            segments.push_back(parts[i]);

        if (base_dir.empty())
            base_dir = ".";
    }

    static void glob_impl(const fs::path &dir,
                          const std::vector<std::string> &segs,
                          size_t idx,
                          bool recursive_double_star,
                          std::vector<std::string> &out)
    {
        if (idx >= segs.size())
        {
            // 到了末尾：dir 本身可能是文件/目录，返回实际存在的路径
            std::error_code ec;
            if (fs::exists(dir, ec))
                out.push_back(dir.string());
            return;
        }

        const std::string &seg = segs[idx];

        if (seg == "**")
        {
            if (!recursive_double_star)
            {
                // 当成普通目录名处理
                fs::path next = dir / seg;
                glob_impl(next, segs, idx + 1, recursive_double_star, out);
                return;
            }

            // "**" 两种展开：
            // 1) 匹配 0 层：直接跳过 "**"
            glob_impl(dir, segs, idx + 1, recursive_double_star, out);

            // 2) 匹配多层：递归遍历所有子目录，把它们当作 dir 继续匹配 "**"（idx 不变）
            std::error_code ec;
            if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec))
                return;

            for (fs::recursive_directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
                 it != end; it.increment(ec))
            {
                if (ec)
                    break;
                if (it->is_directory(ec))
                {
                    glob_impl(it->path(), segs, idx + 1, recursive_double_star, out);
                }
            }
            return;
        }

        std::error_code ec;
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec))
            return;

        // 普通段：遍历当前目录的 direct children，看名字是否匹配
        for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
             it != end; it.increment(ec))
        {
            if (ec)
                break;
            const fs::path p = it->path();
            const std::string name = p.filename().string();

            if (match_wildcard(name, seg))
            {
                glob_impl(p, segs, idx + 1, recursive_double_star, out);
            }
        }
    }

    std::vector<std::string> glob(const std::string &pattern, bool recursive_double_star)
    {
        fs::path base;
        std::vector<std::string> segs;
        split_pattern(pattern, base, segs);

        std::vector<std::string> results;
        glob_impl(base, segs, 0, recursive_double_star, results);

        // 清理：只保留最终匹配到“实际存在的文件或目录”
        // （上面末尾已经 exists 了，这里主要做去重+排序）
        std::sort(results.begin(), results.end());
        results.erase(std::unique(results.begin(), results.end()), results.end());
        return results;
    }

} // namespace SimpleCV
