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

------



`2021.12.14`**env-light的设计思考**

```cpp
// 1. 将shape作为基类抽离出来，创建子类sphere
// 2. sphere作为user geom加入embree：定义intersection方法等<user_geometry>
// 3. bitmap中对texel类型的处理
// 4. 【重难点】Hierarchical Sample Warping

```

------



`2022.1.10`**specular到ior的转换**

```cpp
#include <iostream>
#include <cmath>

float sqr(float a) {
    return a * a;
};

// https://docs.blender.org/manual/en/latest/render/shader_nodes/shader/principled.html
float specular(float ior) {
    return sqr((ior-1.f)/(ior+1.f))/0.08;
};

float ior(float specular) {
    return 2.f/(1.f - std::sqrt(0.08*specular)) - 1.f;
};

// 下面这个一般是标准做法，disney的做法避免了该方法，使用的是F0=0.08*specular的做法
// For a dielectric, R(0) = (eta - 1)^2 / (eta + 1)^2, assuming we're
// coming from air. pbrt-v3
// https://blog.selfshadow.com/publications/s2015-shading-course/burley/s2015_pbs_disney_bsdf_notes.pdf
// 3.1 (7) ==> F0=((eta - 1) / (eta + 1))^2
float SchlickR0FromEta(float eta) { return sqr(eta - 1) / sqr(eta + 1); }

int main() {

    auto specular = 0.5f;
    auto eta = ior(specular);
    auto F0 = sqr((eta-1.f)/(eta+1.f));

    std::cout << F0 << std::endl;

    return 0;
}
```

| specular |  0   | 0.5  |  1   |
| :------- | :--: | :--: | :--: |
| ior      |  1   | 1.5  | 1.8  |
| F0       |  0   | 0.04 | 0.08 |

------



`2022.1.11`**smith ggx**
$$
\Lambda(v)=\frac{-1+\sqrt{1+\frac{1}{a^{2}}}}{2}
其中a=\frac{1}{\alpha tan\theta_{o}}
$$

```cpp
// 市面上的实现没有统一

// 1. disney brdf所谓的G选项，mmp/SORT/tinsel等采用的方法，这里我们的实现以官方为准
// 之所以是这样是因为clearcoat的eval函数是：
// F * D * G / 4.f
//
// 这里约掉了上面的NdotV(cosThetaI & cosThetaO)，但是里面的2为什么没了？
// tinsel中已经改成了F * D * G了
float smithG_GGX(float NdotV, float alphaG)
{
    float a = alphaG*alphaG;
    float b = NdotV*NdotV;
    return 1 / (NdotV + sqrt(a + b - a*b));
}

// 2. G = 1 / 1 + lambda 的标准形式
// GGX法线分布的Λ函数
float SmithG(float NDotV, float alphaG)
{
    float a = alphaG * alphaG;
    float b = NDotV * NDotV;
    return (2.0 * NDotV) / (NDotV + sqrt(a + b - a * b));
}

// 3. Selas的做法，看上去是错的
float SeparableSmithGGXG1(const float3& w, float a)
{
    float a2 = a * a;
    float absDotNV = AbsCosTheta(w);

    return 2.0f / (1.0f + Math::Sqrtf(a2 + (1 - a2) * absDotNV * absDotNV));
}

//-----------------------------------------------------------------------
G1= 1.f / (1+lambda)
其中Trowbridge–Reitz 分布函数是：
lambda=（-1+sqrt(1+alpha^2*tanTheta^2)

然后用代码的形式表示是这样的：
// 2. G = 1 / 1 + lambda 的标准形式
// GGX法线分布的Λ函数
float SmithG(float NDotV, float alphaG)
{
    float a = alphaG * alphaG;
    float b = NDotV * NDotV;
    return (2.0 * NDotV) / (NDotV + sqrt(a + b - a * b));
}

对比一下disney使用的smithGGX：
// disney clearcloat ggx
float disneySmithG(float NdotV, float alphaG)
{
    float a = alphaG*alphaG;
    float b = NdotV*NdotV;
    return 1 / (NdotV + sqrt(a + b - a*b));
}
两者相差2*cosTheta


第一个问题 ：为什么没有cosThetaI*cosThetaO？
这就能解释了函数为什么是F * D * G / 4.f，因为约掉了cosThetaI*cosThetaO这项


第二个问题：specular项就只剩下 Gs*Fs*Ds了？
这是因为，specular中的G1用的是标准SmithG，对比一下disneySmithG，少了一个2。G=G1(l)*G1(v)，就是2x2=4。这样上下约掉了4。就变成了F*D*G
```

------



`2022.1.15`**eval() 函数revisit**

```c++
// eval = f*cosThetaO
// 这里我们把cosThetaO直接乘进来，然后约掉，这样做的好处是
// 在intergrator中的计算可以用eval * Ls得到光源贡献，不需要*cosThetaO。更加简洁
float eval() {
	return F * D * G / (4.f * cosThetaI);    
}

```

------



`2022.1.16`**disney specular brdf**

```c++
/// eval(): f = F * D * G / (4.f * Frame3f::cos_theta(si.wi));


/// ---- Fresnel 部分 

// https://github.com/mmp/pbrt-v3/tree/master/src/materials
// https://seblagarde.wordpress.com/2013/04/29/memo-on-fresnel-equations/
///////////////////////////////////////////////////////////////////////////
// 这里我们还是用F0=0.08*specular的简化方法来避免艺术家直接操作eta，这个参数不是很直观
// 具体实现：https://github.com/wdas/brdf/blob/main/src/brdfs/disney.brdf
// 另外：需要实现DisneyFresnel，即：将fresnelDielectric和schlickFresnel根据metallic参数进行插值，
float3 DisneyFresnel(const SurfaceParameters& surface, const float3& wo, const float3& wm, const float3& wi)
{
	float dotHV = Dot(wm, wo);

	float3 tint = CalculateTint(surface.baseColor);

    // -- See section 3.1 and 3.2 of the 2015 PBR presentation + the Disney BRDF explorer (which does their 2012 remapping
    // -- rather than the SchlickR0FromRelativeIOR seen here but they mentioned the switch in 3.2).
    float3 R0 = Fresnel::SchlickR0FromRelativeIOR(surface.relativeIOR) * Lerp(float3(1.0f), tint, surface.specularTint);
    R0 = Lerp(R0, surface.baseColor, surface.metallic);

    float dielectricFresnel = Fresnel::Dielectric(dotHV, 1.0f, surface.ior);
    float3 metallicFresnel = Fresnel::Schlick(R0, Dot(wi, wm));

    return Lerp(float3(dielectricFresnel), metallicFresnel, surface.metallic);
}

/// Anisotropic specular detail
// https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf
// Addend部分
void calculateAnisotropicParams(float roughness, float anisotropic, float& ax, float& ay)
{
 	float aspect = sqrt(1.0f - 0.9f * anisotropic);
	ax = max(0.001f, sqr(roughness) / aspect);
    ay = max(0.001f, sqr(roughness) * aspect);
}

/// ---- NDF 部分

// https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf
// 附录B.2中的(10-13)
// 
// 上面记录了转化的过程，所以可以有两种表示形式：
// hDotX = sinThetaH * cosThetaPhi
// hDotY = sinThetaH * sinThetaPhi
// hDotN = cosThetaH

// 计算效率更高的版本：an efficient alternate f
float GgxAnisotropicD(const float3& wm, float ax, float ay)
{
    float dotHX2 = Square(wm.x);
    float dotHY2 = Square(wm.y); // Selas的做法是dotHY=wm.z，错误，当然可能是它的渲染器的坐标系不同
    float cos2Theta = Cos2Theta(wm);
    float ax2 = Square(ax);
    float ay2 = Square(ay);

    return 1.0f / (Math::Pi_ * ax * ay * Square(dotHX2 / ax2 + dotHY2 / ay2 + cos2Theta));
}

// PBRT-v3的做法
Float TrowbridgeReitzDistribution::D(const Vector3f &wh) const {
    Float tan2Theta = Tan2Theta(wh);
    if (std::isinf(tan2Theta)) return 0.;
    const Float cos4Theta = Cos2Theta(wh) * Cos2Theta(wh);
    Float e = (Cos2Phi(wh) / (alphax * alphax) + Sin2Phi(wh) / (alphay * alphay)) * tan2Theta;
    
    return 1.f / (Pi * alphax * alphay * cos4Theta * (1 + e) * (1 + e));
}

// 其中sinPhi和cosPhi为：
{
    /** \brief Assuming that the given direction is in the local coordinate 
     * system, return the sine of the phi parameter in spherical coordinates */
    static float sinPhi(const Vector3f &v) {
        float sinTheta = Frame::sinTheta(v);
        if (sinTheta == 0.0f)
            return 1.0f;
        return math::clamp(v.y() / sinTheta, -1.0f, 1.0f);
    }

    /** \brief Assuming that the given direction is in the local coordinate 
     * system, return the cosine of the phi parameter in spherical coordinates */
    static float cosPhi(const Vector3f &v) {
        float sinTheta = Frame::sinTheta(v);
        if (sinTheta == 0.0f)
            return 1.0f;
        return math::clamp(v.x() / sinTheta, -1.0f, 1.0f);
    }
}

// mitsuba2的标准做法
{
	Float alpha_uv = m_alpha_u * m_alpha_v,
			cos_theta         = Frame3f::cos_theta(m),
			cos_theta_2       = sqr(cos_theta),
	// GGX / Trowbridge-Reitz distribution function
	Float result = rcp(Pi * alpha_uv * sqr(sqr(m.x() / m_alpha_u) + sqr(m.y() / m_alpha_v) + sqr(m.z())));
}


/// ---- G 部分

/// Smith's separable shadowing-masking approximation
Float G(const Vector3f &wi, const Vector3f &wo, const Vector3f &m) const {
	return smith_g1(wi, m) * smith_g1(wo, m);
}

Float smith_g1(const Vector3f &v, const Vector3f &m) const {
	Float xy_alpha_2 = sqr(m_alpha_u * v.x()) + sqr(m_alpha_v * v.y()),
			tan_theta_alpha_2 = xy_alpha_2 / sqr(v.z()),
    Float result = 2.f / (1.f + sqrt(1.f + tan_theta_alpha_2));
}

// pdf():  
// p = D*cosTheta/4*hDotL
Float MicrofacetDistribution::Pdf(const Vector3f &wo,
                                  const Vector3f &wh) const {
    if (sampleVisibleArea)
        return D(wh) * G1(wo) * AbsDot(wo, wh) / AbsCosTheta(wo);
    else
        return D(wh) * AbsCosTheta(wh);
}

Float MicrofacetReflection::Pdf(const Vector3f &wo, const Vector3f &wi) const {
    if (!SameHemisphere(wo, wi)) return 0;
    Vector3f wh = Normalize(wo + wi);
    return distribution->Pdf(wo, wh) / (4 * Dot(wo, wh));
}
```

------



`2022.1.17`**VNDF(sample_visible)是怎么回事？**

可以先从这里看起来：https://schuttejoe.github.io/post/ggximportancesamplingpart2

它是来自于Eric Heitz和Eugene d’Eon的文章：

1. [《Importance Sampling Microfacet-Based BSDFs using the Distribution of Visible Normals》](https://hal.inria.fr/hal-00996995v1/document)
2. [《A Simpler and Exact Sampling Routine for the GGX Distribution of Visible Normals》](https://hal.archives-ouvertes.fr/hal-01509746/document)

顺便说一下：mitsuba2里面的mircofacet.h的实现还包括了另外2篇文章：

1. 《Microfacet Models for Refraction through Rough Surfaces》by Bruce Walter, Stephen R. Marschner, Hongsong Li, and Kenneth E. Torrance
2. 《An Improved Visible Normal Sampling Routine for the Beckmann Distribution》 by Wenzel Jakob

Walter07的那篇要好好看一下，后面的refrection也会用。



VNDF主要解决的问题是：只对NDF进行importance sampling会出现2种问题

- firefly：个用来描述单个像素比它周围的像素亮得多的渲染术语
- 浪费采样

这些情况经常性出现在low glancing angle的地方
