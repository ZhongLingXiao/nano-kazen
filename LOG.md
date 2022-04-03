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
// 1. 【解决】出现Integrator: computed an invalid radiance value: [-nan, -nan, -nan]
//    目前猜测是斜掠角导致的除0的问题
// 2. 【解决】area light 的pdf和sample函数还有疑问，distance2到底应该放到哪里？ 
// 3. 【解决】mis过程重新梳理 (之前一直错误的原因是pdf被重复计算，现在pdf函数被隔离出来)


```

**NaN错误原因 ：rr的时候用小于号判断p和random的关系：[`445e255`NAN issue fix and path_mats integrator](https://github.com/ZhongLingXiao/nano-kazen/commit/445e255b54bf93fb10c51d36a898b2092f9e7006)**

```
if (depth >= 3) {
    // continuation probability
    auto probability = std::min(throughput.maxCoeff()*eta*eta, 0.95f);
    if (probability <= sampler->next1D())
        break;

    throughput /= probability;
}
```

这里` probability <= sampler->next1D()` 一定要小于或等于` <= `因为此时可能出现 `probability == 0 && sampler->next1D() == 0`
这时候如果只是` < ` 的话if 判断不成立执行`throughput /= probability;`
然后就会出现0/0 = NaN这种情况了**结论：rr的时候一定要用**` **<=** ` **来进行判断**



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
	Float result = rcp(Pi * alpha_uv * sqr(sqr(m.x() / m_alpha_u) + 
                                           sqr(m.y() / m_alpha_v) + sqr(m.z())));
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

> 这个是对**分布函数**进行采样，例如`TrowbridgeReitzDistribution`或者`BeckmannDistribution`



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

```c++
// sample VNDF的5个步骤
vec3 sampleGGXVNDF(vec3 V_, float alpha_x, float alpha_y, float U1, float U2)
{
    // 1. stretch view
    vec3 V = normalize(vec3(alpha_x * V_.x, alpha_y * V_.y, V_.z));
    
    // 2. orthonormal basis
    vec3 T1 = (V.z < 0.9999) ? normalize(cross(V, vec3(0,0,1))) : vec3(1,0,0);
    vec3 T2 = cross(T1, V);
    
    // 3. sample point with polar coordinates (r, phi)
    float a = 1.0 / (1.0 + V.z);
    float r = sqrt(U1);
    float phi = (U2<a) ? U2/a * M_PI : M_PI + (U2-a)/(1.0-a) * M_PI;
    float P1 = r*cos(phi);
    float P2 = r*sin(phi)*((U2<a) ? 1.0 : V.z);
    
    // 4. compute normal
    vec3 N = P1*T1 + P2*T2 + sqrt(max(0.0, 1.0 - P1*P1 - P2*P2))*V;
    
    // 5. unstretch
    N = normalize(vec3(alpha_x*N.x, alpha_y*N.y, max(0.0, N.z)));
	
    return N;
}
```



------

`2022.1.18`**【未解决】mis-path的一些问题**

path integrator中的`light sample`过程中会判断：shadowRay是否与场景相交。如果是的话，则认为有物体遮挡，不计算这种direct light的贡献。

**但是，对于材质如果是玻璃这种，似乎不成立也不合理**。

- 如果`light sample`贡献为0，那么是不是应该提高`bsdf sample`的权重，而放弃`powerHeuristic`？
- 之前`roughDielectric`结果比较暗，如果提高`bsdf sample`的权重会怎么样？



------



`2022.1.19`**bsdf system设计的思考**

通过观察：pbrt和mitsuba2中的做法是，设计一个microfacet的文件。这里文件中包含的是分布函数类，例如

```cpp
// pbrt-v3的做法是派生了2个子类，mitsuba2的做法是只有一个类，然后判断通过MicrofacetType来进行区分计算
// enum class MicrofacetType : uint32_t {
//    /// Beckmann distribution derived from Gaussian random surfaces
//    Beckmann = 0,
//    /// GGX: Long-tailed distribution for very rough surfaces (Trowbridge-Reitz distr.)
//    GGX = 1
//};
//
// 这里的sampleVisibleArea或者sample_visible就是之前看过的VNDF的计算
class MicrofacetDistribution {
  public:
    // MicrofacetDistribution Public Methods
    virtual ~MicrofacetDistribution();
    virtual Float D(const Vector3f &wh) const = 0;
    virtual Float Lambda(const Vector3f &w) const = 0;
    Float G1(const Vector3f &w) const {
        //    if (Dot(w, wh) * CosTheta(w) < 0.) return 0.;
        return 1 / (1 + Lambda(w));
    }
    virtual Float G(const Vector3f &wo, const Vector3f &wi) const {
        return 1 / (1 + Lambda(wo) + Lambda(wi));
    }
    virtual Vector3f Sample_wh(const Vector3f &wo, const Point2f &u) const = 0;
    Float Pdf(const Vector3f &wo, const Vector3f &wh) const;
    virtual std::string ToString() const = 0;

  protected:
    // MicrofacetDistribution Protected Methods
    MicrofacetDistribution(bool sampleVisibleArea)
        : sampleVisibleArea(sampleVisibleArea) {}

    // MicrofacetDistribution Protected Data
    const bool sampleVisibleArea;
};
```

------



`2022.1.22`**review RichieSams的integrator实现方式**

```c++
// 更加清晰的积分器实现
void Li(Ray& ray, Sampler* sampler) {
    // 
    float3 color = 0.f;
    float3 throughput = 1.f;
    Intersection intersection;
    
    // 场景中开始发射光线，这个版本是有最大弹射次数(maxBounces)限制的
    const uint maxBounces = 15;
    for (uint bounces=0; bounces<maxBounces; ++bounces) {
        
        m_scene->intersect(ray);
        
        // 如果ray与场景没有相交，那么返回backgroundColor
        if (ray.GeomID == INVAILD_GEOMETRY_ID) {
            color += throughput * m_scene.getBackgroundColor();
            break;
        }

        // 获取当前材质
        Material* material = m_scene->getMaterial(ray.GeomID);
        // geom对象有可能是light，这里我们尝试获取对应的light
        Light* light = m_scene->getLight(ray>GeomID);
        
        // 击中光源，计算Le贡献
        if (light != nullptr) {
            color += throughput * light->Le();
        }
        
        // 设置intersection数据
        // p,n分别为位置和法线信息
        // wi指的是入射方向，在渲染器中通常可以理解为eye相对于hitpoint的方向，
        // 所以wi与射线(ray.d)的方向相反。另外就是为了方便计算cos等数据，
        // 即dot(n, wi) > 0
        // 不然的话dot(n, ray.d) < 0，使用起来很不直观
        intersection.p = ray.o * ray.t * ray.d;
        intersection.n = normalize(m_scene->interpolateNormal(ray.GeomID, ray.primID, ray.uv));
        intersection.wi = normalize(-ray.d);
        
        // 计算直接光照
        color += throughput * sampleOneLight(sampler, interaction, material->bsdf, light);
        
        // [ISSUE]: 这里的方向是新采样的，但是NEE过程中已经有sample了,
        // 所以为什么不复用？比如，直接用interaction.wo进行eval
        // 但这个做法是pbrt的做法
        //
        // 下一步就是确定新的射线方向
        // 我们需要根据bsdf sample获取wo的方向
        //
        // 这里需要注意的是，先要采样，得到intersection.wo方向，
        // 才能计算pdf。所以下面的两个函数是有先后顺序的
        material->getBSDF()->sample(intersection, sampler); 
        float pdf = material->getBSDF->pdf(intersection);
        
        // 计算throughput，throughput可以翻译为通量，
        // 其实就是指当前路径片段(w1 -> w2)的贡献值
        //
        // 一条完整的路径(w0->w1->w2...->wn)，可以理解为：w0是眼睛，wn是最终的光源。
        // 由于pbr原则：光路可逆，所以对于wn来说w(n-1)就相当于eye的方向
        // 他们组成的“光源-着色点-眼睛的直接光照模型”中w(n-1)的能量来自于wn；
        // w(n-2)的能量来自于w(n-1) ... w0的能量来自与w1。
        // 所以通量可以理解为：这种通过路径逐层返回的能量。
        // 那么它的计算方式就是：t(当前) = t(上一层) * 当前材质的衰减;
        //
        // 一般的设计中：sample会有一个return值color，color=eval/pdf。
        // 但这里的设计就是sample返回void，然后在外面计算贡献。哪种更清晰一点呢？
        throughput = throughput * material->getBSDF()->eval(intersection)/pdf; 
        	
        // 通过intersection的p和wo更新ray的信息
        ray.o = intersection.p;
        ray.d = intersection.wo;
        ray.min = 0.001f;
        ray.max = infinity;
        
        // 最后，当bounce数目大于3的时候，进行Russian Roulette。太早进行rr效果不好
        if (bounces > 3) {
            float p = throughput.maxCoeff();
            if (p < sampler->next1D()) {
                break;
            }
            
            throughput /= p;            
        }
    
    } // end: for loop
} // end: Li()


```



**光源采样**

```c++
// 这里认为光源的pdf = 1 / numLights;
// 则场景光源贡献是：estimateDirect / pdf == estimateDirect * numLights
float3 sampleOneLight(Sampler *sampler, Interaction interaction, BSDF *bsdf, Light *hitLight) const {
    std::size_t numLights = m_scene->NumLights();

    // Return black if there are no lights
    // And don't let a light contribute light to itself
    // Aka, if we hit a light
    // This is the special case where there is only 1 light
    if (numLights == 0 || numLights == 1 && hitLight != nullptr) {
        return float3(0.f);
    }

    // Don't let a light contribute light to itself
    // Choose another one
    Light *light;
    do {
        light = m_scene->randomOneLight(sampler);
    } while (light == hitLight);

    return numLights * estimateDirect(light, sampler, interaction, bsdf);
}
```



**NEE with mis**

```c++
// 这里感觉estimateDirect并不合适，因为这里使用了mis对bsdf进行eval
// 通常我们应该称这个函数为：NEE (Next event estimate)，不过pbrt就是这么叫的...
float3 estimateDirect(Light *light, Sampler *sampler, Interaction &interaction, BSDF *bsdf) const {
    float3 directLighting = float3(0.0f);
    float3 f; // brdf
    float lightPdf, bsdfPdf;


    // Sample lighting with multiple importance sampling
    // Only sample if the BRDF is non-specular 
    if ((bsdf->SupportedLobes & ~BSDFLobe::Specular) != 0) {
        float3 Li = light->sampleLi(sampler, m_scene, interaction, &lightPdf);

        // Make sure the pdf isn't zero and the radiance isn't black
        if (lightPdf != 0.0f && !all(Li)) {
            // Calculate the brdf value
            f = bsdf->eval(interaction);
            bsdfPdf = bsdf->pdf(interaction);

            if (bsdfPdf != 0.0f && !all(f)) {
                float weight = powerHeuristic(lightPdf, bsdfPdf);
                // 这里更直观的写法是：(f*Li/lightPdf) * weight
                // [ISSUE]: mitsuba中的做法是sampleLight =  f * Li * weight
                // 所以这里还要除lightPdf到底对不对？
                // 不过，pbrt-v3的做法就是这样的。
                directLighting += f * Li * weight / lightPdf;
            }
        }
    }


    // Sample brdf with multiple importance sampling
    bsdf->Sample(interaction, sampler); // 这里的intersection.wo可以直接作为newRay吧？
    f = bsdf->eval(interaction);
    bsdfPdf = bsdf->pdf(interaction);
    if (bsdfPdf != 0.0f && !all(f)) {
        lightPdf = light->pdfLi(m_scene, interaction);
        if (lightPdf == 0.0f) {
            // We didn't hit anything, so ignore the brdf sample
            return directLighting;
        }

        float weight = powerHeuristic(bsdfPdf, lightPdf);
        float3 Li = light->Le();
        directLighting += f * Li * weight / bsdfPdf;
    }

    return directLighting;
}
```

NEE过程解析

> 1. **First, we sample the light**
>
>    - This updates `interaction.wo`（光源方向）
>    - Gives us the `Li` for the light
>    - And the pdf of choosing that point on the light
>
> 2. Check that the pdf is valid and the radiance is non-zero
>
> 3. Evaluate the BSDF using the sampled `interaction.wo`
>
> 4. Calculate the pdf for the BSDF given the sampled `interaction.wo`
>
>    - Essentially, how likely is this sample, if we were to sample using the BSDF, instead of the light
>
> 5. Calculate the weight, using the light pdf and the BSDF pdf
>
>    - Veach and Guibas define a couple different ways to calculate the weight. Experimentally, they found the 
>
>      power heuristic with a power of 2 to work the best for most cases. I refer you to the paper for more details. 
>
>      The implementation is below
>
> 6. Multiply the weight with the direct lighting calculation and divide by the light pdf. (For Monte Carlo) And add to the direct light accumulation.
>
> 7. **Then, we sample the BRDF**
>
>    - This updates `interaction.wo`
>
> 8. Evaluate the BRDF
>
> 9. Get the pdf for choosing this direction based on the BRDF
>
> 10. Calculate the light pdf, given the sampled `interaction.wo`
>
>     - This is the mirror of before. How likely is this direction, if we were to sample the light
>
> 11. `If lightPdf == 0.f`, then the ray missed the light, so just return the direct lighting from the light sample.
>
> 12. Otherwise, calculate the weight, and add the BSDF direct lighting to the accumulation
>
> 13. Finally, return the accumulated direct lighting



**powerHeuristic**

```
inline float powerHeuristic(float fPdf, float gPdf) {
    float f = fPdf;
    float g = gPdf;

    return (f * f) / (f * f + g * g);
}
```

------



`2022.2.9`**代码清理，开始完成Q1计划**

- [x] `volpath`:  medium材质中的Beer-Lambert
  - [x] `medium` class
  - [x] 根据**bsdf lobe type**来判断是否设置当前介质状态
  - [x] 根据是否有介质来进行transmission计算：衰减距离*衰减系数
- [ ] Disney BSDF
  - [ ] `mircofacet` class设计与实现
  - [x] **bsdf lobe type**
  - [ ] 参考：https://kazenworkspace.slack.com/archives/C02KNMTJNP5/p1642645043001800
  - [ ] 格外注意：**Total Internal Reflection**
- [x] texture：
  - [x] filter | type | 等属性
  - [x] 还是说使用oiio？
- [ ] normalMap：accel中的post intersection部分计算shadingFrame
- [ ] 场景搭建：blender导出脚本



------

 

`2022.2.13`**Q1内容设计【持续迭代】**

```c++
// 对于media transmission来说(glossy | ideal) 类型对于计算并不重要，我们只需要知道当前是不是折射即可
// 所以最后将类型简化为反射和折射2种类型即可，具体的type可以放到BSDFType来进行管理
// BSDFFlag Definition
enum BSDFFlag {
    Unset = 0,
    Reflection = 1 << 0,
    Transmission = 1 << 1,
    All = Reflection | Transmission
};

// BSDFType Definition
// 这里没用specular的原因是specular=Glossy|Ideal，即高光=光泽|理想镜面
// 
enum BSDFType {
    Unset = 0,
    Reflection = 1 << 0,
    Transmission = 1 << 1,
    Diffuse = 1 << 2,
    Glossy = 1 << 3,
    Ideal = 1 << 4,
    // Composite BSDFType definitions
    DiffuseReflection = Diffuse | Reflection,
    DiffuseTransmission = Diffuse | Transmission,
    GlossyReflection = Glossy | Reflection,
    GlossyTransmission = Glossy | Transmission,
    IdealReflection = Ideal | Reflection,
    IdealTransmission = Ideal | Transmission,
    All = Diffuse | Glossy | Ideal | Reflection | Transmission

};
```

- [ ] **BSDF类型思考：如何理解这几种类型？**
  - [ ] diffuse：漫反射
  - [ ] specular：相对于diffuse的概念，高光
  - [ ] glossy | rougness：参数光泽度、粗糙度
  - [ ] mirror：镜面反射，或者表示为ideal/delta



------



`2022.3.1`**优先级重排**

- [ ] normal 法线贴图，最出效果，方便 demo
- [ ] disney brdf
- [ ] rough dielectric
- [ ] Mircofacet | Fresnel 类重构
- [ ] Texture 类重构，oiio texturesys 设计更加合理
- [ ] uv mapping，可以 rotate | scale uv 
- [ ] volpath 积分器



------



`2022.3.11`**normal map 的思考与问题**

1. **计算切线空间**

```c++
// 这里 dpdu 可以理解为 tangent, dpdv 可以理解为binormal
// 

Vector3f dP1=p1-p0, dP2=p2-p0;
Point2f dUV1=uv1-uv0, dUV2=uv2-uv0;
Normal3f n = dP1.cross(dP2).normalized();

float determinant = dUV1.x()*dUV2.y() - dUV1.y()*dUV2.x();
if (determinant == 0) {
	coordinateSystem(n.normalized(), its.dpdu, its.dpdv);
}
else {
    float invDet = 1.0f / determinant;
    its.dpdu = ( dUV2.y() * dP1 - dUV1.y() * dP2) * invDet;
    its.dpdv = (-dUV2.x() * dP1 + dUV1.x() * dP2) * invDet;    
}
```

2. **normal map 扰动法线计算**

```c++
Frame getFrame(const Intersection &its, Vector3f wi) const {
	Color3f rgb = m_normalMap->eval(its.uv, false);
	Vector3f localNormal(2 * rgb.r() - 1, 2 * rgb.g() - 1, 2 * rgb.b() - 1);

	Frame result;
    result.n = its.shFrame.toWorld(localNormal).normalized();
    result.s = (its.dpdu - result.n * result.n.dot(its.dpdu)).normalized();
    result.t = result.n.cross(result.s).normalized();       

    return result;
}
```

> **问题**
>
> 法线扰动可能使得法线方向 n 与如何光线方向 wi 的夹角大于90°。然后导致整体变黑，目前没有一致性的解决方案，cycles 和 Arnold 渲染器有不同的方法。
>
> cycles 效果稍好



1.  [高度图，视差贴图(Bump-maps)，置换贴图(displacement)，法线贴图的本质](https://zhuanlan.zhihu.com/p/266434175)
2.  [在使用光线追踪类算法时应该如何处理normal mapping等技术造成的artifacts？](https://www.zhihu.com/question/316029127)
3.  [通过基于微表面阴影函数的方案解决Bump Map阴影失真问题](https://zhuanlan.zhihu.com/p/63419999)
4. [Math for Game Developers - Normal Maps](https://www.youtube.com/watch?v=SZBkSYelJcg)



[Microfacet-based Normal Mapping for Robust Monte Carlo Path Tracing](https://jo.dreggn.org/home/2017_normalmap.pdf ) 

1. **NormalMircofacetDefault 方法没跑通**
2. **(normal flip  及 normal switch 这两种工业界常用方法)，有颜色断层，需要进一步验证**



> 最后采用的方法就是最 naive 的实现，即：上述 `getFrame()` 方法。



------



`2022.3.13`**normal map 类型整理 **

1. 采用最 naive 的normal map 实现
2. 实现了 `NormalMap BSDF` 类型，嵌套 `BSDF` 对象



------



`2022.3.15`**嵌套 texture 实现**

1. ramp color ：将颜色值重映射到某个范围，这个主要用作调整某个 texture 比如 roughness 的范围
2. blend ：混合 texture ，目前提供 mix 和 multiply 两种方式

> 注意：这部分代码主要为 Q1 demo 使用。最后还是要被 osl 替代掉。



------



`2022.3.26`**light path**

blender 中有这种现象，材质只有 diffuse 部分能被照亮，specular 部分无法被某些光源照亮。

做法是通过一种叫做 light path 的方式把光源过滤出来。

这种光源的好处是：可以 filter 掉高光，类似偏振光的感觉

如果想达到同样的效果，设置 ray 的类型？



------



`2022.3.27`**有关 firefly 的一些思考**

**1. Integrator 中的 maxDepth 可以有效去除掉一些 firefly** 

这种 `maxDepth` 参数非常好用，可以有效避免边角处的反复弹射，出现那种过分的色溢效果。

但这种强制截断光路，而不是通过 russian roulette 的方式会导致结果有偏差。这个还需要后面看其它渲染器的做法。

其中对于 maxDepth ：

- 支持
  - pbrt
  - cycles
- 不支持
  - mitsuba2
  - nori

**2. 我们的 `block.put()` 函数其实已经帮我们 filter 掉了一些负值和 NaN 的情况**



------



`2022.4.3`**顺利完成 kazen con 2022 Q1**

https://github.com/ZhongLingXiao/nano-kazen

https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/2022_q1_report.md



开始思考下一阶段开发计划

- [ ] 光源完善 ：hdr ，点，面
- [ ] 更好的 sampler ：mmp 最喜欢的 `sampler 0-2 pmj bn`
- [ ] bsdf 完善 : brdf ==> bsdf。
- [ ] 微分部分 : 法线微分（bump_map），贴图微分(dtdx, dsdx, dtdy, dsdy)
- [ ] uv mapping / 多个 uv sets
- [ ] ref 文件
- [ ] blender 插件方便快速迭代
- [ ] 加分项：
  - [ ] realistic camera
  - [ ] vol_path : 体积渲染及 sss
  - [ ] 颜色 ocio

> 目标：更快的性能；更好的表现力；更舒适的工作流。可量化
>





