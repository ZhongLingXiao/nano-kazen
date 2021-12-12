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

------



`2021.11.21`**完成第一张图片渲染-milestone-0.0.1**

```cpp
// 创建最基本的integrator
// 绘制第一张png和exr文件
// 

```

------



`2021.11.25 - 2021.11.26`**调研osl和embree中的简单渲染器是如何集成这2个库的**

```cpp
// osl中的渲染器需要继承RendererServices这个类
//
// ShaderGlobals的解释: src/include/OSL/shaderglobals.h
/// The ShaderGlobals structure represents the state describing a particular
/// point to be shaded. It serves two primary purposes: (1) it holds the
/// values of the "global" variables accessible from a shader (such as P, N,
/// Ci, etc.); (2) it serves as a means of passing (via opaque pointers)
/// additional state between the renderer when it invokes the shader, and
/// the RendererServices that fields requests from OSL back to the renderer.
///
/// Except where noted, it is expected that all values are filled in by the
/// renderer before passing it to ShadingSystem::execute() to actually run
/// the shader. Not all fields will be valid in all contexts. In particular,
/// a few are only needed for lights and volumes.
///
/// All points, vectors and normals are given in "common" space.

// ShaderGlobals里面的值是通过globals_from_hit方法设置的。
// scene.intersect()方法得到RayHit信息，然后通过uv==>重心坐标==>Ns(Shading normal)
// Ng可以通过RTCRayHit.ray中的信息拿到，其它的以此类推得到

// 1. scene.interscet
// 2. rayhit -->postInterset(这个步骤是计算uv，shaderId等)
// 3. 将postIntersect信息 --> ShaderGlobals(过程类似globals_from_hit)。似乎sg包括differential_geometry信息
// 4. shadingsys->execute (*ctx, *m_shaders[shaderID], sg); 这样的方式执行shader【ShaderGroupRef】
//    在这类渲染器中都不使用指针来管理对象，而是通各种id来进行管理：shaderId，geomId，primId(mesh的face)，instId。
// 5. 这些id信息来自于初始化，类似于管理vector<mesh>, vector<shader>
class MeshNode: Node {
	uint32 geomId;
    uint32 shaderId
}
// 当遇到了mesh列表中不存在的mesh(例如通过filename来判断)，就添加进vector <== 前面步骤有待商榷，然后把id设置给meshnode的方法，
// 总体管理geom的原数据。通过id来索引获取数据信息。所以这种node应该只包括比如transform信息等等？
```

------



`2021.11.27 ~ 11.28`**osl和embree中sceneGraph**

```cpp
// 思考：<--- 进行中，不是最终答案
// Node类型是Data类型的owner，它并不存储数据。所以有2中类型：xxxNode和xxxData
// 
// Data存储数据实体，Node提供数据处理方法
//
// 下面的三个信息基本一致
// embree: 	DifferentialGeometry <-- 来自于RTCRayHitX的构建
// osl: 	ShaderGlobals
// nori: 	Intersection
//
// embree中renderPixel的流程是这样的:
// 1. get camera ray
// 2. RTCIntersectContext + rtcIntersect1 获取RTCRayHit_值
// 3. 如果没有相交：计算灯光贡献 + background贡献
// 4. 如果相交：计算dg(DifferentialGeometry)
// 5. 然后利用dg来计算BSDF和生成BSDF和light sample。这里主要是进行direct light的MIS
// 6. 生成下一条光线，然后进行loop(终止条件maxBounce或者max_path_length这种)，而不是递归。注意这里没有rr，所以我们需要自己加。

```

------



`2021.11.29`**warp实现**

```cpp
// 基本参考了mitsuba的实现完成


```

------



`2021.11.30`**embree3 集成**

```cpp
// embree3 intersection集成
// fix warp bug: 主要是eigen取数据的方式是x()而不是x
// 1. TODO: 渲染结果有错误：是一张全黑的图片，需要debug问题。
// 原因分析：需要添加这2行代码
//    rayhit.ray.mask = -1;
//    rayhit.ray.flags = 0;
//
// 2. 渲染结果不对，阴影错误。原因是错误使用rtcOccluded方法，这个要调研正确使用方法。
/* NOTICE: rtcOccluded1 should not use in this way */
/* trace shadow ray */
// if (shadowRay) {
//     rtcOccluded1(m_scene, &context, &rayhit.ray);
//     if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
//         return true;
//     } else {
//         return false;
//     }
// }


```

------



`2021.12.3`**到元旦之前的计划，还有4周**

```cpp
// light：area和env
// sampler: zsampler和pmj02bn
// bsdf: disney brdf
// texture: oiio textureSys
// scene: 准备一个cornellbox和麦克风模型


```

------



`2021.12.5`**Whitted-style integrator练习**

```cpp
// 错误总结：
// 1. 计算LightQueryRecord中的wi不要忘记normalize
// 2. 设置shadowRay的时候同时要把tMax和tMin设置，不然错到离谱
// 3. 【遗留2】渲染方程里是定义在立体角上，需要直接转换成立体角上的pdf，也就是将distance2/cosTheta放到pdf中
// 4. 对于rfilter: mitchell会出现光源周围出现黑边(估计是默认参数设置太小)，暂时使用gaussian
// 
// 遗留问题
// 1. 出现Integrator: computed an invalid radiance value: [-nan, -nan, -nan]
//    目前猜测是斜掠角导致的除0的问题
// 2. 【解决】area light 的pdf和sample函数还有疑问，distance2到底应该放到哪里？ 
// 3. 【解决】mis过程重新梳理 (之前一直错误的原因是pdf被重复计算，现在pdf函数被隔离出来)


```

------



`2021.12.7`**MIS + NEE完成**

```cpp
// 进度展望
// 目前MIS+NEE流程完成，余下3部分：
// 1. texture：oiio里面有大坑，比如textureSys创建单一一个，还是tls多个？
//    那内存如何分配？等等，目前简单材质一个ts肯定够用，后面tex量上来了，就不好说了
// 2. 当tex完成之后就可以进行env light的实现
// 3. brdf材质扩展：https://github.com/boksajak/brdf/blob/master/brdf.h
// 4. sampler：pmj02bn暂时就这一个，然后和independent对比一下效果。使用方法还要再确认


```

------



`2021.12.8`**texture相关问题的思考及设计**

```cpp
// 1. mesh还是bsdf中应该有texture对象？感觉应该是bsdf合理，材质关联texture，mesh如果需要query tex数据可通过类似getBSDF().getTexture()
// 来获取数据。似乎这样设计更合理
//
// 2. 记住个名词ROI：ROI is a small helper struct describing a rectangular region of interest in an image.
// 3. 由于oiio对颜色完成了很好的处理(集成ocio2.0)，所以简单调研了下颜色：https://zhuanlan.zhihu.com/p/24214731
// 4. texture基本形式应该是这样的，但是如何对应参数？
/*
	<mesh type="obj">
		<string name="filename" value="ajax.obj"/>

		<bsdf type="microfacet">
			<texture name="intIOR" type="image">
				<string name="filename" value="baseColor.png"/>
			</texture>
		</bsdf>
	</mesh>
*/

```
------



`2021.12.12`**一些简单实现**

```cpp
// 1. texture中使用bitmap存储管理数据(目前只支持init from exr)
// 2. 【TODO】squareToDisk: https://www.pbr-book.org/3ed-2018/Monte_Carlo_Integration/2D_Sampling_with_Multidimensional_Transformations#SamplingaUnitDisk
// 3. 【TODO】normal map实现
// 4. disney bsdf

```

