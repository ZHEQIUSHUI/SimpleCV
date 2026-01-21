#ifndef SIMPLECV_HPP
#define SIMPLECV_HPP

// ==========================================================
// 跨平台：Windows/Linux 导出宏 + 常见宏污染处理
// ==========================================================
#if defined(_WIN32) || defined(_WIN64)
#ifndef NOMINMAX
// 防止 Windows 头文件把 std::min/std::max 变成宏
#define NOMINMAX
#endif

#if defined(SIMPLECV_BUILD_DLL)
#define SIMPLECV_API __declspec(dllexport)
#elif defined(SIMPLECV_USE_DLL)
#define SIMPLECV_API __declspec(dllimport)
#else
#define SIMPLECV_API
#endif
#else
// GCC/Clang: 可见性控制（不强制，留空也可以）
#if defined(__GNUC__) && __GNUC__ >= 4
#define SIMPLECV_API __attribute__((visibility("default")))
#else
#define SIMPLECV_API
#endif
#endif

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility> // std::move

namespace SimpleCV
{
    // 像素格式/通道排列
    enum class ColorSpace
    {
        AUTO = 0,  // cvtColor 用：自动根据 channels 推断（1->GRAY,3->RGB,4->RGBA）
        UNCHANGED, // imread 用：不改 channels，保持 stb 解码结果（通常是 1/2/3/4）
        GRAY,      // 1 channel
        RGB,       // 3 channel
        BGR,       // 3 channel
        RGBA,      // 4 channel
        BGRA       // 4 channel
    };

    enum class BorderType
    {
        CONSTANT,   // 常量填充
        REPLICATE,  // 边缘复制 (aaaa|abcd|dddd)
        REFLECT,    // 镜像 (dcba|abcd|dcba)
        REFLECT_101 // 镜像101 (cbab|abcd|cbab) 也叫 reflect without repeating edge
    };

    class SIMPLECV_API Mat
    {
    public:
        int height = 0;
        int width = 0;
        int channels = 0;
        unsigned char *data = nullptr;
        int step = 0; // stride in bytes

        // ===== 构造：外部数据（指定 step/stride）=====
        Mat(int h, int w, int c, unsigned char *d, int s, bool is_own_data = false)
        {
            reset(h, w, c, d, s, is_own_data);
        }

        // 允许 size_t stride，避免外部 stride 类型不是 int 时落到 bool 重载
        Mat(int h, int w, int c, unsigned char *d, std::size_t s, bool is_own_data = false)
        {
            int si = (s > static_cast<std::size_t>(std::numeric_limits<int>::max()))
                         ? std::numeric_limits<int>::max()
                         : static_cast<int>(s);
            reset(h, w, c, d, si, is_own_data);
        }

        // ===== 构造：外部数据（紧密存储，step = w*c）=====
        Mat(int h, int w, int c, unsigned char *d)
        {
            reset(h, w, c, d, w * c, false);
        }

        // 显式 ownership（不提供默认值，避免误把 stride 当 bool）
        Mat(int h, int w, int c, unsigned char *d, bool is_own_data)
        {
            reset(h, w, c, d, w * c, is_own_data);
        }

        // ===== 构造：自己分配（step 可指定）=====
        Mat(int h, int w, int c, int s)
        {
            create(h, w, c, s);
        }

        Mat(int h, int w, int c)
        {
            create(h, w, c, w * c);
        }

        Mat() = default;

        Mat(const Mat &) = default;
        Mat &operator=(const Mat &) = default;

        // ===== 移动 =====
        Mat(Mat &&) noexcept = default;
        Mat &operator=(Mat &&) noexcept = default;

        ~Mat() = default;

        bool empty() const { return data == nullptr || height <= 0 || width <= 0 || channels <= 0; }

        Mat clone() const
        {
            if (empty())
                return Mat();
            Mat out(height, width, channels, step);
            for (int y = 0; y < height; ++y)
            {
                std::memcpy(out.data + y * out.step, data + y * step, static_cast<size_t>(width * channels));
            }
            return out;
        }

        void create(int h, int w, int c, int s)
        {
            if (h <= 0 || w <= 0 || c <= 0)
            {
                release();
                return;
            }
            const int min_step = w * c;
            if (s < min_step)
                s = min_step;

            const size_t bytes = static_cast<size_t>(h) * static_cast<size_t>(s);
            unsigned char *p = new unsigned char[bytes];

            std::shared_ptr<unsigned char> sp(p, [](unsigned char *ptr)
                                              { delete[] ptr; });

            owner_ = std::move(sp);
            height = h;
            width = w;
            channels = c;
            step = s;
            data = owner_.get();
        }

        void create(int h, int w, int c)
        {
            create(h, w, c, w * c);
        }

        void release()
        {
            owner_.reset();
            height = width = channels = step = 0;
            data = nullptr;
        }

    private:
        std::shared_ptr<unsigned char> owner_;

        void reset(int h, int w, int c, unsigned char *d, int s, bool is_own_data)
        {
            if (h <= 0 || w <= 0 || c <= 0 || d == nullptr)
            {
                release();
                return;
            }
            const int min_step = w * c;
            if (s < min_step)
                s = min_step;

            if (is_own_data)
            {
                // 认为外部用 new[] 分配
                owner_ = std::shared_ptr<unsigned char>(d, [](unsigned char *ptr)
                                                        { delete[] ptr; });
            }
            else
            {
                // 不拥有：空 deleter
                owner_ = std::shared_ptr<unsigned char>(d, [](unsigned char *) {});
            }

            height = h;
            width = w;
            channels = c;
            step = s;
            data = d;
        }

        friend Mat imread(const std::string &filename, ColorSpace flag);
        friend Mat imdecode(const std::vector<unsigned char> &buf, ColorSpace flag);
    };

    // imgcodec
    SIMPLECV_API Mat imread(const std::string &filename, ColorSpace flag = ColorSpace::UNCHANGED);
    SIMPLECV_API Mat imdecode(const std::vector<unsigned char> &buf, ColorSpace flag = ColorSpace::UNCHANGED);

    SIMPLECV_API bool imwrite(const std::string &filename, const Mat &mat);
    SIMPLECV_API bool imencode(const Mat &mat, std::vector<unsigned char> &buf);

    // imgproc
    SIMPLECV_API void resize(const Mat &src, Mat &dst, int dst_width, int dst_height);

    // 任意互转：RGB/BGR/RGBA/BGRA/GRAY
    SIMPLECV_API void cvtColor(const Mat &src, Mat &dst, ColorSpace dst_space, ColorSpace src_space = ColorSpace::AUTO);
    SIMPLECV_API Mat cvtColor(const Mat &src, ColorSpace dst_space, ColorSpace src_space = ColorSpace::AUTO);

    // value: 支持 1/3/4 通道值；会按 dst.channels 适配
    SIMPLECV_API void copyMakeBorder(
        const Mat &src,
        Mat &dst,
        int top, int bottom, int left, int right,
        BorderType borderType = BorderType::CONSTANT,
        const std::vector<unsigned char> &value = std::vector<unsigned char>{0, 0, 0, 255});

    // 返回匹配到的路径（默认按字典序排序）
    // pattern 示例：
    //   "images/*.png"
    //   "data/??.jpg"
    //   "assets/**/icon-*.png"   (支持 ** 递归)
    //   "C:\\temp\\*.txt"        (Windows)
    SIMPLECV_API std::vector<std::string> glob(const std::string &pattern, bool recursive_double_star = true);
}

#endif // SIMPLECV_HPP
