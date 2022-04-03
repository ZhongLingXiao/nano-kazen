# nano-Kazen
> 这是kazen的一小步，更是国人自主光线追踪渲染器的一大步。
>
> Arnold，Renderman直呼：Houston, we get problem!
<br>



## 1. KISS PBR shading model

![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/final_cover.jpg "Figure 1: 3840x2160 | 10000 spp | 3.4 h")
<div align=center><center style="font-size:14px;color:#C0C0C0;text-decoration:underline">Figure 1: 3840x2160 | 10000 spp | 3.4 h</center></div><br>

**Core Feature**

1. Monte Carlo unbiased path tracing 
2. multiple importance sample
3. Material ：kazen initial standard surface ( kiss )
   1. diffuse ：Disney diffuse with Retro-Reflective
   2. specular ：GGX-Smith BRDF with VNDF
   3. clearcoat ：GGX-Smith BRDF with VNDF
   4. sheen ：Disney sheen
4. Textures and simple textures ops ( nested blend, ramp color, scale uv )
5. OIIO texture system
6. Normal mapping
7. Camera : Perspective | Thin Lens
8. Light : mesh light, basic environment ( Blinn/Newell Latitude Mapping )

[See the full kazen-2022-q1 report](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/2022_q1_report.md)
<br>
<br>
## Reference

[1] [Physically Based Shading at Disney](https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf) **Brent Burley**. 2012

[2] [Extending the Disney BRDF to a BSDF with Integrated Subsurface Scattering](https://blog.selfshadow.com/publications/s2015-shading-course/burley/s2015_pbs_disney_bsdf_notes.pdf) **Brent Burley**. 2015

[3] [Simon Kallweit's project report](http://simon-kallweit.me/rendercompo2015/report/)

[4] [Microfacet Models for Refraction through Rough Surfaces](https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf) **Bruce Walter , Stephen R. Marschner**. 2007

[5] [Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs](https://jcgt.org/published/0003/02/03/paper.pdf) **Eric Heitz**. 2014

[6] [Sampling the GGX Distribution of Visible Normals](https://jcgt.org/published/0007/04/01/paper.pdf) **Eric Heitz**. 2018

[7] [ROBUST MONTE CARLO METHODS FOR LIGHT TRANSPORT SIMULATION](https://graphics.stanford.edu/papers/veach_thesis/thesis.pdf) **Eric Veach**. 1997

[8] [Physically Based Rendering: from theory to implementation](https://www.pbrt.org/) **Matt Pharr, Wenzel Jakob and Greg Humphreys**. 2016

[9] [Mitsuba 2: Physically Based Renderer](https://www.mitsuba-renderer.org/) **Wenzel Jakob**

[10] [Embree 3: High Performance Ray Tracing](https://www.embree.org/) **Intel**

[11] [OpenImageIO: a library for reading and writing images, and a bunch of related classes, utilities, and applications](https://sites.google.com/site/openimageio/home) **Larry Gritz**

[12] [Greyscalegorilla: modern surface material collection](https://greyscalegorilla.com/product/modern-surface-material-collection/) **Greyscalegorilla**. 2021
