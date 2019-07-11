#ifndef SHADER_TYPES
#define SHADER_TYPES

// Definitions for each shader type.
#define RASTER 0
#define RASTER_PLUS_SHADOWS 1
#define REFLECTIONS 2
#define PERFECT_MIRROR 3
#define PATH_TRACING 4
#define TORUS 5
#define SEIFERT_WEBER_DODECAHEDRON 6
#define MIRRORED_DODECAHEDRON 7
#define POINCARE_DODECAHEDRON 8

// Optimization. Disable code for specific cases to simplify shader.
#define PATH_TRACING_ENABLED
#define TORUS_ENABLED
#define DODECAHEDRON_ENABLED
#define POINCARE_ENABLED

#endif
