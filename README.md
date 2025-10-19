# VulkanVoxel

A modern voxel game engine built with C++23 and Vulkan 1.4.

## Features

- Modern C++23
- Vulkan 1.4 rendering
- Cross-platform (Windows, Linux, macOS)
- Advanced voxel rendering techniques

## Dependencies

- **Vulkan 1.4** - Modern graphics API
- **GLFW 3.4** - Window and input management
- **GLM 1.0.2** - Mathematics library
- **VulkanMemoryAllocator 3.3.0** - GPU memory management
- **ImGui 1.92.3** - Debug UI
- **FastNoise2** - Procedural terrain generation
- **libspng 0.7.4** - PNG texture loading
- **simdjson 3.13.0** - Fast JSON parsing
- **fmt 12.0.0** - String formatting
- **spdlog 1.15.3** - Logging
- **Tracy 0.12.2** - Profiler

## Building

The project uses CMake and automatically fetches all dependencies.

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Planning

### Core Systems Architecture

#### 1. Foundation Layer

**Window & Input System** (GLFW)
- Window management, resize handling
- Input processing (keyboard, mouse, gamepad)
- Event system for decoupling input from game logic

**Rendering Engine** (Vulkan 1.4)
- Core Vulkan abstraction layer
- Device/queue management
- Swapchain handling
- Command buffer management
- Descriptor set management
- Pipeline state objects (PSOs)

**Memory Management**
- VMA integration for efficient GPU memory allocation
- Staging buffer pool for uploads
- Ring buffer for dynamic per-frame data

#### 2. Voxel-Specific Systems

**Chunk System**
- Chunk data structure (recommended: 32x32x32 or 16x256x16)
- Chunk loading/unloading based on player position
- LOD (Level of Detail) system for distant chunks
- Chunk meshing (greedy meshing or culled meshing)
- Dirty chunk tracking for remeshing

**World Generation**
- FastNoise2 integration for terrain generation
- Biome system
- Cave/structure generation
- Chunk generation pipeline (async)

**Voxel Data**
- Block registry system
- Block state/metadata
- Palette-based compression for chunk storage
- Block models and textures

#### 3. Rendering Pipeline

**Modern Vulkan 1.4 Features**
- Mesh shading (`.msh`/`.tsh`) for efficient chunk rendering
- Indirect drawing for batched chunk rendering
- Compute shaders for chunk meshing on GPU
- Dynamic rendering (no render passes)
- Buffer device address

**Rendering Passes**
- Opaque geometry (chunks, terrain)
- Transparent blocks (water, glass) with OIT or sorted
- Sky/atmosphere rendering
- Post-processing (FXAA, tone mapping, bloom)

**Texture System**
- Texture atlas/array for block textures
- Bindless textures using descriptor indexing
- libspng integration for texture loading
- Mipmapping for distant chunks

#### 4. Game Systems

**Player/Camera**
- First-person camera controller
- Physics (collision detection with voxel world)
- Player state management

**Block Interaction**
- Raycast for block selection
- Block placement/destruction
- Block update system (falling sand, water flow, etc.)

**Save/Load System**
- Chunk serialization (consider format: Anvil-like or custom)
- World metadata storage
- Async I/O for background saving

#### 5. Performance & Debug

**Profiling**
- Tracy integration for GPU/CPU profiling
- GPU profiling with timestamp queries
- Frame time analysis

**Debug UI**
- ImGui integration for runtime debugging
- Performance metrics display
- Chunk boundaries visualization
- Debug overlays (wireframe, chunk borders)

**Frustum Culling**
- View frustum calculation
- Chunk-level culling
- Occlusion culling (optional: Hi-Z or compute-based)

### Recommended Project Structure

```
src/
├── core/
│   ├── Application.cpp/h       # Main app loop
│   ├── Window.cpp/h            # GLFW wrapper
│   └── Input.cpp/h             # Input handling
├── rendering/
│   ├── VulkanContext.cpp/h     # Core Vulkan setup
│   ├── Device.cpp/h            # Logical device
│   ├── Swapchain.cpp/h         # Swapchain management
│   ├── Pipeline.cpp/h          # Pipeline abstractions
│   ├── Buffer.cpp/h            # Buffer wrapper
│   ├── Image.cpp/h             # Image/texture wrapper
│   ├── CommandBuffer.cpp/h     # Command buffer wrapper
│   └── DescriptorSet.cpp/h     # Descriptor management
├── voxel/
│   ├── Chunk.cpp/h             # Chunk data structure
│   ├── ChunkManager.cpp/h      # Chunk lifecycle
│   ├── ChunkMesher.cpp/h       # Meshing algorithms
│   ├── Block.cpp/h             # Block definitions
│   ├── BlockRegistry.cpp/h     # Block type registry
│   └── World.cpp/h             # World state
├── world/
│   ├── WorldGenerator.cpp/h    # Terrain generation
│   ├── BiomeSystem.cpp/h       # Biome logic
│   └── NoiseGenerator.cpp/h    # FastNoise2 wrapper
├── player/
│   ├── Camera.cpp/h            # Camera controller
│   ├── Player.cpp/h            # Player state
│   └── Physics.cpp/h           # Collision/movement
├── utils/
│   ├── Logger.cpp/h            # spdlog wrapper
│   ├── Math.cpp/h              # GLM helpers
│   └── ThreadPool.cpp/h        # Async work
└── main.cpp
```

### Key Technical Decisions

1. **Chunk Size**: Start with 32x32x32 - good balance between draw calls and data size
2. **Meshing Strategy**: Greedy meshing on CPU initially, move to GPU compute later
3. **Texture Management**: Use texture arrays for block faces
4. **Memory Strategy**: Use VMA for all allocations, prefer device-local memory
5. **Threading**: Separate threads for world generation, chunk loading, and meshing
6. **Coordinate System**: Right-handed Y-up (standard for Vulkan)

### Development Phases

**Phase 1: Core Engine**
- Vulkan initialization
- Basic window/input
- Simple rendering pipeline
- Camera controller

**Phase 2: Voxel Foundation**
- Chunk data structure
- Simple chunk meshing (naive)
- Single chunk rendering
- Block placement/removal

**Phase 3: World System**
- Chunk manager with loading/unloading
- Basic world generation
- Multiple chunk rendering
- Frustum culling

**Phase 4: Optimization**
- Greedy meshing
- Mesh shading pipeline
- GPU-based meshing
- LOD system

**Phase 5: Features**
- Save/load system
- Advanced world generation
- Block physics
- Polish & UI

## License

TBD