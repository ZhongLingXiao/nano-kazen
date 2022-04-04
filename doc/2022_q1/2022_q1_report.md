



# kazen 2022 Q1 report

![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/final_cover.jpg)



## 1. Core Feature

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
<br>


## 2. KISS PBR shading model

<div align=center><img src=".\img\kiss_sm.jpg" width="420" /></div>  

<div align=center><center style="font-size:14px;color:#C0C0C0;text-decoration:underline">Figure 1: Structure of the KISS PBR Shading Model</center></div><br>



**KISS** (kazen initial standard surface) , is a linear blend of a metallic BSDF and a dielectric BSDF, see **Figure 1**. Currently we only support opaque dielectric BSDF and it can be used as materials like plastics, wood, or stone.
<br>


**KISS** blend metallic and dielectric BSDFs based on parameters **metallic**.

<div align=center><img src=".\img\kiss_algo.jpg" width="420"" /></div>  

<div align=center><center style="font-size:14px;color:#C0C0C0;text-decoration:underline">Figure 2: The KISS BRDF is a blend of metallic and dielectric BRDF models based on a metallic shader parameter</center></div><br>




For sampling the BRDF, I first use Russian-roulette to decide between sampling the diffuse lobe or the specular lobes with the following ratio:

```c++
float ratioDiffuse = (1.0f - metallic) / 2;
```
<br>


For non-metallic materials, 1/2 the samples are sampled with cosine weighted hemisphere sampling, the 1/2 with specular sampling described below. For metallic materials, all samples are sampled using specular sampling.



The specular samples are divided into sampling the GGX (specular lobe) and GGX (clearcoat lobe) distribution with the following ratio:

```c++
float ratioGGX = 1.0f / (1.0f + clearcoat);
```
<br>


For materials with no clearcoat, samples are only sampled using the GGX distribution, for materials with 100% clearcoat, 1/2 the samples are directed to either of the distributions.
	
<br>


These following images shows material parameters support by **KISS**:

### 2.1 `roughness`

|             0.0              |              0.5               |
| :--------------------------: | :----------------------------: |
| ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/m0.0_r0.png) | ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/m0.0_r0.5.png) |
|           **0.0**            |            **1.0**             |
| ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/m0.0_r0.png) |  ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/m0.0_r1.png)  |
<br>


### 2.2 `metallic`

|            0.0             |             0.5              |
| :------------------------: | :--------------------------: |
| ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/m1_r0.png) | ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/m1_r0.5.png) |
|          **0.0**           |           **1.0**            |
| ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/m1_r0.png) |  ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/m1_r1.png)  |
<br>


### 2.3 `specular`

|               0.0                |                0.5                 |
| :------------------------------: | :--------------------------------: |
| ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/m0_r0_spec0.png) | ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/m0_r0_spec0.5.png) |
|             **0.0**              |              **1.0**               |
| ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/m0_r0_spec0.png) |  ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/m0_r0_spec1.png)  |
<br>


### 2.4 `specularTint`

|               0.0                |                  0.5                   |
| :------------------------------: | :------------------------------------: |
| ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/m0_r0_spec1.png) | ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/m0_r0_spec1_st0.5.png) |
|             **0.0**              |                **1.0**                 |
| ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/m0_r0_spec1.png) |  ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/m0_r0_spec1_st1.png)  |
<br>


### 2.5 `clearcoat`

|             0.0              |              0.5               |
| :--------------------------: | :----------------------------: |
| ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/r0.5_c0.png) | ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/r0.5_c0.5.png) |
|           **0.0**            |            **1.0**             |
| ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/r0.5_c0.png) |  ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/r0.5_c1.png)  |
<br>


### 2.6 `clearcoatRoughness`

|             0.0              |                0.5                 |
| :--------------------------: | :--------------------------------: |
| ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/r0.5_c1.png) | ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/r0.5_c1_cr0.5.png) |
|           **0.0**            |              **1.0**               |
| ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/r0.5_c1.png) |  ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/r0.5_c1_cr1.png)  |
<br>


### 2.7 `sheen`

|            0.0             |             0.5              |
| :------------------------: | :--------------------------: |
| ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/r0_s0.png) | ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/r0_s0.5.png) |
|          **0.0**           |           **1.0**            |
| ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/r0_s0.png) |  ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/r0_s1.png)  |
<br>


### 2.8 `sheenTint`

|            0.0             |               0.5                |
| :------------------------: | :------------------------------: |
| ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/r0_s1.png) | ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/r0_s1_st0.5.png) |
|          **0.0**           |             **1.0**              |
| ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/r0_s1.png) |  ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/r0_s1_st1.png)  |
<br>

## 3. Debug error

![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/m0_r0_spec0.5_error.png)

<div align=center><center style="font-size:14px;color:#C0C0C0;text-decoration:underline">Figure 3: Firefly error and incorrect color bleeding in shadow | 4096 spp</center></div><br>

This error occurred when we set integrator **`maxDepth`** to a very high value, so the light path will be terminated only by russian roulette. The scenario is the light will bounce between floor and sphere forever, and make the shadow part look aweful. 

Althrought, kill light path by **`maxDepth`** without compensation will cause statistics bias.

|        maxDepth = infinity         |            maxDepth = 5            |
| :--------------------------------: | :--------------------------------: |
| ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/m0_r0_spec0.5_error.png) | ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/param/m0_r0_spec0.5.png) |
<br>


> `maxDepth` parameter is found when I check cycles render setting : Light path, by default it sets `maxBounces` to 4.<br>
> **So the take-away is : Reverse thinking code logic by ground truth (cycles) results, not just debug through code.**
<br>

## 4. Look-dev

This shows the **cycles** shading graph.

![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/cycles_shader_graph.jpg)

<div align=center><center style="font-size:14px;color:#C0C0C0;text-decoration:underline">Figure 4: The shading graph for creating the look</center></div><br>

The corresponding **nano-Kazen** material scene graph:

```xml
<bsdf type="normalmap">
	<texture type="imagetexture">
		<string name="filename" value="textures/MSMC_Brass_Hammered_Normal.jpg"/>
		<float name="scale" value="1.0"/>
		<string name="colorspace" value="linear"/>
	</texture>
	<bsdf type="kazenstandard">
		<texture type="blend" id="baseColor">
			<string name="blendmode" value="mix"/>
			<texture type="constanttexture" id="input1">
				<color name="color" value="0.887923 0.351533 0.002125"/>
			</texture>
			<texture type="constanttexture" id="input2">
				<color name="color" value="0.982251 0.822786 0.62396"/>
			</texture>
			<texture type="imagetexture" id="mask">
				<string name="filename" value="textures/MSMC_Circle_Outline_Grid.jpg"/>
				<float name="scale" value="1.0"/>
				<string name="colorspace" value="linear"/>
			</texture>
		</texture>
		<texture type="constanttexture" id="roughness">
			<color name="color" value="0.166233 0.166233 0.166233"/>
		</texture>
		<texture type="imagetexture" id="metallic">
			<string name="filename" value="textures/MSMC_Circle_Outline_Grid.jpg"/>
			<float name="scale" value="1.0"/>
			<string name="colorspace" value="srgb"/>
		</texture>
		<float name="anisotropy" value="0.0"/>
		<float name="specular" value="0.5"/>
		<float name="specularTint" value="0.0"/>
		<float name="clearcoat" value="0.0"/>
		<float name="clearcoatRoughness" value="0.0"/>
		<float name="sheen" value="0.0"/>
		<float name="sheenTint" value="0.0"/>
	</bsdf>
</bsdf>
```

![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/look1.png)

<div align=center><center style="font-size:14px;color:#C0C0C0;text-decoration:underline">Figure 5: 1920x1080 | 4096 spp | 11.7 min</center></div><br>


## 5. Final result

![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/final_cover.jpg)

<div align=center><center style="font-size:14px;color:#C0C0C0;text-decoration:underline">Figure 6: 3840x2160 | 10000 spp | 3.4 h</center></div><br>



Compare with Blender render result.

|          blender-cycles           |                 nano-Kazen                  |
| :-------------------------------: | :-----------------------------------------: |
|    ![](https://github.com/ZhongLingXiao/nano-kazen/blob/main/doc/2022_q1/img/blender_ref.png)     | <div align=center><img src=".\img\final_3.4h_4k_10000spp.png" width="1920" /></div>  |
| 1920x1080 \| 2500 spp \| 4.47 min |      3840x2160  \| 10000 spp \| 3.4 h       |
<br>


## Reference

[1] [Physically Based Shading at Disney](https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf) **Brent Burley**. 2012

[2] [Extending the Disney BRDF to a BSDF with Integrated Subsurface Scattering](https://blog.selfshadow.com/publications/s2015-shading-course/burley/s2015_pbs_disney_bsdf_notes.pdf) **Brent Burley**. 2015

[3] [Simon Kallweit's project report](http://simon-kallweit.me/rendercompo2015/report/)

[4] [Microfacet Models for Refraction through Rough Surfaces](https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf) **Bruce Walter , Stephen R. Marschner , Hongsong Li , Kenneth E. Torrance**. 2007

[5] [Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs](https://jcgt.org/published/0003/02/03/paper.pdf) **Eric Heitz**. 2014

[6] [Sampling the GGX Distribution of Visible Normals](https://jcgt.org/published/0007/04/01/paper.pdf) **Eric Heitz**. 2018

[7] [ROBUST MONTE CARLO METHODS FOR LIGHT TRANSPORT SIMULATION](https://graphics.stanford.edu/papers/veach_thesis/thesis.pdf) **Eric Veach**. 1997

[8] [Physically Based Rendering: from theory to implementation](https://www.pbrt.org/) **Matt Pharr, Wenzel Jakob and Greg Humphreys**. 2016

[9] [Mitsuba 2: Physically Based Renderer](https://www.mitsuba-renderer.org/) **Wenzel Jakob**

[10] [Embree 3: High Performance Ray Tracing](https://www.embree.org/) **Intel**

[11] [OpenImageIO: a library for reading and writing images, and a bunch of related classes, utilities, and applications](https://sites.google.com/site/openimageio/home) **Larry Gritz**

[12] [Greyscalegorilla: modern surface material collection](https://greyscalegorilla.com/product/modern-surface-material-collection/) **Greyscalegorilla**. 2021
