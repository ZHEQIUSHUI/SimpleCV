#include "SimpleCV.hpp"
#include <iostream>

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <image_path> <output_path>" << std::endl;
        return -1;
    }
    SimpleCV::Mat img = SimpleCV::imread(argv[1]);
    if (img.empty())
    {
        std::cerr << "Failed to read image" << std::endl;
        return -1;
    }
    // 写入
    bool ret = SimpleCV::imwrite(argv[2], img);
    if (!ret)
    {
        std::cerr << "Failed to write image" << std::endl;
        return -1;
    }
    return 0;
}