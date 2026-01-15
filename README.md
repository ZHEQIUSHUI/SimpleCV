# SimpleCV

一个用 **stb** 封装的极简 OpenCV 风格库：提供 `Mat`、`imread/imwrite/imencode/imdecode`、`resize`、`cvtColor`。

## 依赖

项目已自带 stb 头文件：

- `src/stb_image.h`
- `src/stb_image_write.h`
- `src/stb_image_resize2.h`

## 构建

```bash
cmake -S . -B build -DSIMPLECV_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## API

- `Mat`：浅拷贝 + 引用计数（`shared_ptr`）
- `imread/imdecode`：支持 `ColorSpace` flag（RGB/BGR/RGBA/BGRA/GRAY/UNCHANGED）
- `cvtColor`：RGB/BGR/RGBA/BGRA/GRAY 任意互转
