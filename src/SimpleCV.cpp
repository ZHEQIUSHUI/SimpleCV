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
    void resize(const Mat &src, Mat &dst, int dst_height, int dst_width)
    {
        if (src.empty() || dst_height <= 0 || dst_width <= 0)
        {
            dst.release();
            return;
        }

        dst.create(dst_height, dst_width, src.channels, dst_width * src.channels);

        // stbir_pixel_layout：这里用 "channels" 的数值直接 cast（1..4）
        // 1->STBIR_1CHANNEL, 2->STBIR_2CHANNEL, 3->STBIR_RGB, 4->STBIR_RGBA
        auto layout = (stbir_pixel_layout)src.channels;

        unsigned char *out = stbir_resize_uint8_linear(
            src.data, src.width, src.height, src.step,
            dst.data, dst.width, dst.height, dst.step,
            layout);

        if (!out)
            dst.release(); // 失败返回 NULL（并不会写 dst）
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

    Mat cvtColor(const Mat &src, ColorSpace dst_space, ColorSpace src_space)
    {
        if (src.empty())
            return Mat();

        if (src_space == ColorSpace::AUTO)
            src_space = infer_space_from_channels(src);

        // 先把 src_space 归一到 {GRAY, RGB, BGR, RGBA, BGRA}
        if (src_space == ColorSpace::UNCHANGED)
            src_space = infer_space_from_channels(src);

        // dst_space 也不允许 UNCHANGED/AUTO（你也可以允许 AUTO=按 src 直接返回 clone）
        if (dst_space == ColorSpace::AUTO || dst_space == ColorSpace::UNCHANGED)
        {
            // 这里选择：AUTO/UNCHANGED -> 返回浅拷贝（或 clone，看你习惯）
            return src; // 浅拷贝，共享内存
        }

        auto dst_ch = desired_channels(dst_space);
        if (dst_ch == 0)
            return Mat();

        Mat dst(src.height, src.width, dst_ch);

        // 一些快速路径
        if (src_space == dst_space)
        {
            // channels 必须一致才能直接 copy
            if (src.channels == dst.channels && src.step == dst.step)
            {
                std::memcpy(dst.data, src.data, (size_t)src.height * (size_t)src.step);
                return dst;
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
                    return Mat();
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
                    return Mat();
                }
            }
        }

        return dst;
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
