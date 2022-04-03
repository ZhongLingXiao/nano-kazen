# 2022 Q1 总结与展望



## 1. 特性

1. 基于蒙特卡洛的无偏路径追踪
2. 多重重要性采样：multiple importance sample
3. 材质：kazen independent standard surface (kiss)
   1. diffuse ：Disney diffuse with RR
   2. specular ：GGX-Smith BSDF with VNDF
   3. clearcoat ：GGX-Smith BSDF with VNDF
   4. sheen ：Disney sheen
4. 纹理，image，嵌套贴图（blend），rampColor 支持 scale uv 操作
5. Normal mapping
6. Thin Lens Camera
7. OIIO texture system
8. 光源 mesh light， 基本环境光（Blinn/Newell Latitude Mapping）



## 2. 总结 & 反思

- 总结

  - 代码量  / 引用文章 / 社区贡献 / 学习笔记 

  - 通过 demo 查缺补漏，效果非常好

  - 数学及原理性知识需要加强，完成了很多代码，但依然讲不清楚原理

    

- 反思

  - 与blender对比和遇到小坑
    - 1/4 的采样就比我们的好，采样器不同
    - 【保持一致】颜色不一致，blender 更 smooth 一些（ 后来发现是cycles里面有 world 环境光，以及遇到自发光停止 trace ）
    - 【保持一致】blender 遇到自发光停止 trace 
    - 【保持一致】blender 有 max depth，不然出现 firefly
  - 正常面光源（不可见），我们现在的做法相当于自发光
  - 没有 blender 的插件，很难进行迭代，无法所见即所得
  - 没有 environment 很多光影表现受到限制
  - 批量渲染输出参数对比很好用 （`batch.py` 文件）
  - 棘手的 NaN 问题
  - ref 文件



## 3. Q2 & 展望

- 技术
  - 光源完善 ：hdr ，点，面
  - 更好的 sampler ：mmp 最喜欢的 `sampler 0-2 pmj bn`
  - bsdf 完善 : brdf ==> bsdf。
  - 微分部分 : 法线微分（bump_map），贴图微分(dtdx, dsdx, dtdy, dsdy)
  - uv mapping / 多个 uv sets
  - 加分项：
    - realistic camera
    - vol_path : 体积渲染及 sss
    - 颜色 ocio

> 目标：更快的性能；更好的表现力；更舒适的工作流。可量化



- 品牌

  - 2 - 3 篇高质量知乎文章

  - 网站

  - 第一个视频，迈出第一步 ：rez 或者 wsl2 开发环境
  - kazen 用于展示的标志性 3D 模型，图标等

