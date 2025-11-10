// AudioManager.cpp - miniaudio implementation
// This file exists solely to compile the miniaudio implementation ONCE
// All other files should include AudioManager.hpp WITHOUT defining MINIAUDIO_IMPLEMENTATION

// Prevent Windows.h from defining min/max macros
#ifndef NOMINMAX
#define NOMINMAX
#endif

// Step 1: Include stb_vorbis HEADER before miniaudio
#define STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"

// Step 2: Define miniaudio implementation (ONLY in this .cpp file)
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

// Step 3: Include stb_vorbis IMPLEMENTATION after miniaudio
#undef STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"