/*
BSD 3-Clause License

Copyright (c) 2021-Present, ZhongLingXiao && Joey Chen
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
// version 0.1.1 : Noise reduction related works
//                 * New samplers.
//                      * stratified.
//                      * correlated.
//                      * pmj02-bn.
//                 * Geometric shadow terminator : remove the shadow-line artifact cause by geometry.
//                 * Fire fly reduction: increase roughness per bounce.
//                 * Configurable ray bias for reduce ray intersection computations error (floating point error).
//                 * Light primary visibility : toggle light visibility
// version 0.1.0 : K.I.S.S shading model
//                 * Monte Carlo unbiased path tracing.
//                 * Multiple importance sample.
//                 * Material ：kazen initial standard surface (K.I.S.S).
//                      * diffuse ：Disney diffuse with Retro-Reflective.
//                      * specular ：GGX-Smith BRDF with VNDF.
//                      * clearcoat ：GGX-Smith BRDF with VNDF.
//                      * sheen ：Disney sheen.
//                 * Textures and simple textures ops (nested blend, ramp color, scale uv).
//                 * OIIO texture system.
//                 * Normal mapping.
//                 * Camera : Perspective | Thin Lens.
//                 * Light : mesh light, basic environment (Blinn/Newell Latitude Mapping).
#pragma once

#define KAZEN_VERSION_MAJOR 0
#define KAZEN_VERSION_MINOR 1
#define KAZEN_VERSION_PATCH 1

#define KAZEN_AUTHORS "ZhongLingXiao && Joey Chen"
#define KAZEN_YEAR 2022

/// defines
#if !defined(NAMESPACE_BEGIN)
#  define NAMESPACE_BEGIN(name) namespace name {
#endif
#if !defined(NAMESPACE_END)
#  define NAMESPACE_END(name) }
#endif

#define KAZEN_INLINE __attribute__((always_inline)) inline
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)