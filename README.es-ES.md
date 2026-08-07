

<!-- Allow this file to not have a first line heading -->
<!-- markdownlint-disable-file MD041 -->

<!-- inline html -->
<!-- markdownlint-disable-file MD033 -->

``` 
  __  ___      ___      ________   _______ .__   __.   
 |  |/  /     /   \    |       /  |   ____||  \ |  | 
 |  '  /     /  ^  \   `---/  /   |  |__   |   \|  | 
 |    <     /  /_\  \     /  /    |   __|  |  . `  | 
 |  .  \   /  _____  \   /  /----.|  |____ |  |\   | 
 |__|\__\ /__/     \__\ /________||_______||__| \__| 
====================================================================
 nano-kazen: Physically Based Renderer                               
 Version Alpha 0.1.0                                             
 Copyright (C) 2022 ZhongLingXiao && Joey Chen. All rights reserved.                          
====================================================================
```

<div align="center">

# 💡 nano-kazen

**Path tracer experimental basado en CPU**
</div>

[![License](https://img.shields.io/badge/license-BSD%203--Clause-blue.svg?style=flat-square)](https://github.com/ZhongLingXiao/nano-kazen/blob/main/LICENSE)


## kazen-con 2 : katastrophe

![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q2/img/kc_v02.png "Figura 1: 1920x1080 | 16384 spp | 1.1 h")
<div align=center><center style="font-size:14px;color:#C0C0C0;text-decoration:underline">Figura 1: 1920x1080 | 16384 spp | 1.1 h</center></div><br>

**Características principales**

1. Nuevos muestreadores
   - estratificado
   - correlacionado
   - pmj02-bn
2. Terminador de sombras geométrico: elimina el artefacto de línea de sombra causado por la geometría
3. Reducción de luciérnagas: aumenta la rugosidad por rebote
4. Sesgo de rayos configurable para reducir errores en los cálculos de intersección de rayos
5. Visibilidad primaria de la luz: alternar visibilidad de la luz

[Ver el informe completo de kazen-2022-q2](https://kazen-renderer.github.io/2022/07/kazen-con-v002-report.html)

**Referencias**

[1] [Stochastic Sampling in Computer Graphics](http://www.cs.cmu.edu/afs/cs/academic/class/15462-s15/www/lec_slides/p51-cook.pdf) **ROBERT L. COOK**. 1986

[2] [Correlated Multi-Jittered Sampling](https://graphics.pixar.com/library/MultiJitteredSampling/paper.pdf) **Andrew Kensler**. 2013

[3] [Andrew Kensler's permute()](https://andrew-helmer.github.io/permute/) **Andrew Helmer**. 2021

[4] [Progressive Multi-Jittered Sample Sequences](https://graphics.pixar.com/library/ProgressiveMultiJitteredSampling/paper.pdf) **Per Christensen, Andrew Kensler and Charlie Kilpatrick**. 2015

[5] [Physically Based Rendering v4](https://www.pbrt.org/) **Matt Pharr, Wenzel Jakob and Greg Humphreys**. 2023

[6] [Hacking the Shadow Terminator](https://jo.dreggn.org/home/2021_terminator.pdf) **Johannes Hanika**. 2021

[7] [Accumulate a roughness bias based on roughness of hitting surface](https://twitter.com/YuriyODonnell/status/1199253959086612480) **Yuriy O'Donnell**. 2019

[8] [Ray Tracing Denoisinge](https://alain.xyz/blog/ray-tracing-denoising#statistical-analysis) **Alain Galvan**. 2020


<br>

## kazen-con 1 : kiss pbr shading model

![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/final_cover.jpg "Figura 2: 3840x2160 | 10000 spp | 3.4 h")
<div align=center><center style="font-size:14px;color:#C0C0C0;text-decoration:underline">Figura 2: 3840x2160 | 10000 spp | 3.4 h</center></div><br>

**Características principales**

1. Path tracing no sesgado de Monte Carlo 
2. Muestreo de importancia múltiple
3. Material: superficie estándar inicial de kazen (kiss )
   - difuso: difuso de Disney con retro-reflexión
   - especular: BRDF GGX-Smith con VNDF
   - clearcoat: BRDF GGX-Smith con VNDF
   - sheen: sheen de Disney
4. Texturas y operaciones simples de texturas ( mezcla anidada, rampa de color, escala UV )
5. Sistema de texturas OIIO
6. Mapeo de normales
7. Cámara: Perspectiva | Lente delgada
8. Luz: luz de malla, entorno básico ( mapeo de latitud de Blinn/Newell )

[Ver el informe completo de kazen-2022-q1](https://kazen-renderer.github.io/2022/04/kazen-con-v001-report.html)


**Referencias**

[1] [Physically Based Shading at Disney](https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf) **Brent Burley**. 2012

[2] [Extending the Disney BRDF to a BSDF with Integrated Subsurface Scattering](https://blog.selfshadow.com/publications/s2015-shading-course/burley/s2015_pbs_disney_bsdf_notes.pdf) **Brent Burley**. 2015

[3] [Simon Kallweit's project report](http://simon-kallweit.me/rendercompo2015/report/) **Simon Kallweit**. 2015

[4] [Microfacet Models for Refraction through Rough Surfaces](https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf) **Bruce Walter**. 2007

[5] [Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs](https://jcgt.org/published/0003/02/03/paper.pdf) **Eric Heitz**. 2014

[6] [Sampling the GGX Distribution of Visible Normals](https://jcgt.org/published/0007/04/01/paper.pdf) **Eric Heitz**. 2018

[7] [ROBUST MONTE CARLO METHODS FOR LIGHT TRANSPORT SIMULATION](https://graphics.stanford.edu/papers/veach_thesis/thesis.pdf) **Eric Veach**. 1997

[8] [Physically Based Rendering: from theory to implementation](https://www.pbrt.org/) **Matt Pharr, Wenzel Jakob and Greg Humphreys**. 2016

[9] [Mitsuba 2: Physically Based Renderer](https://www.mitsuba-renderer.org/) **Wenzel Jakob**

[10] [Embree 3: High Performance Ray Tracing](https://www.embree.org/) **Intel**

[11] [OpenImageIO](https://sites.google.com/site/openimageio/home) **Larry Gritz**

[12] [Greyscalegorilla: modern surface material collection](https://greyscalegorilla.com/product/modern-surface-material-collection/) **Greyscalegorilla**. 2021
