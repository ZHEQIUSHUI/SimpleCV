#ifndef SIMPLECV_HPP
#define SIMPLECV_HPP

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <cstring>
#include <algorithm>

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

    class Mat
    {
    public:
        int height = 0;
        int width = 0;
        int channels = 0;
        unsigned char *data = nullptr;
        int step = 0; // stride in bytes

        Mat(int h, int w, int c, unsigned char *d, int s, bool is_own_data)
        {
            reset(h, w, c, d, s, is_own_data);
        }

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
    Mat imread(const std::string &filename, ColorSpace flag = ColorSpace::UNCHANGED);
    Mat imdecode(const std::vector<unsigned char> &buf, ColorSpace flag = ColorSpace::UNCHANGED);

    bool imwrite(const std::string &filename, const Mat &mat);
    bool imencode(const Mat &mat, std::vector<unsigned char> &buf);

    // imgproc
    void resize(const Mat &src, Mat &dst, int dst_height, int dst_width);

    // 任意互转：RGB/BGR/RGBA/BGRA/GRAY
    Mat cvtColor(const Mat &src, ColorSpace dst_space, ColorSpace src_space = ColorSpace::AUTO);
}

#endif // SIMPLECV_HPP
