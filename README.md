# nano-Kazen
> 这是kazen的一小步，更是国人自主光线追踪渲染器的一大步。
>
> Arnold，Renderman直呼：Houston, we get problem!

------



## 1. KISS PBR shading model

![](.\doc\2022_q1\img\final_cover.jpg)

<center style="font-size:14px;color:#C0C0C0;text-decoration:underline">Figure 1: 3840x2160 | 10000 spp | 3.4 h</center>

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

[See the full kazen-2022-q1 report](.\doc\2022_q1\2022_q1_report.md)
