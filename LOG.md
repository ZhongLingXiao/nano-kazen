# nano-kazen开发日志

`2021.11.9`**目标：最简实现，完成框架，作为后续测试的baseline**

```cpp
// 选择第三方库

数学：Eigen3
图像：oiio，openexr
场景解析：pugixml
多线程：tbb
模型：tinyobj
加速结构：embree3
输出信息：fmt
功能函数：boost
颜色：ocio

```

------



`2021.11.10`**文件目录结构创建**

```cpp
// 目录结构
// .
//  /include
//      /kazen --------------- kazen core部分代码
//  /src
//      /experimental--------- kazen 插件部分，例如自己实现的bsdf等等
//          /accel
//          /bxdf
//          /film
//          ..
//      /kazen
//  CMakeLists.txt
//
```
这样的结构简单清晰，拓展实现放到`experimental`里面，kazen只保留framework

> 需要将rez-env提前设置好，目前tinyobj和Eigen3尚未添加到rez-packages里面，然后通过命令rez-env nanokazen保证findpackage能够找到相应的库文件。例如：

```cmake
find_package(TBB REQUIRED)
find_package(OpenImageIO REQUIRED)
# find_package(embree 3.13.0 REQUIRED) # Here should point a spesific version
find_package(fmt 7.1.3 REQUIRED)
find_package(pugixml 1.11 REQUIRED)

```

------



`2021.11.12`**rez-packages完善**

```c++
// ++ tinyobjloader
// ++ eigen3
// pcg32库中没有cmake文件，不过这个只是一个头文件，可以直接放到我们的源码中

// 明天计划：进行全部的库编译测试，保证库文件可以正确编译链接进kazen
```

------



`2021.11.15`**基础框架代码添加**

```cpp
// 数学类代码添加
// - ray, frame, transform, color ...
// - 添加了wj的pcg32实现
// common.h类的完善

```

