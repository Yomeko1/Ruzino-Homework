# TerrainGen - Procedural Terrain Generation Plugin

## Overview

TerrainGen is a procedural terrain generation plugin that supports multi-scale noise, erosion simulation, and biome-based terrain generation.

## Key Features

### 1. Multiple Noise Types
- **Perlin Noise**: Classic smooth noise for natural terrain undulation
- **Simplex Noise**: More efficient gradient noise
- **Worley/Voronoi Noise**: Cellular noise for cracked, crystalline terrain
- **Ridged Noise**: Mountain noise for sharp peaks
- **Billow Noise**: Cloud-like noise for rounded hills

### 2. Multi-Scale Features
- **Mountain Layer**: Large-scale, sharp peaks
- **Valley Layer**: Medium-scale negative terrain
- **Hill Layer**: Medium-scale undulation
- **Detail Layer**: Small-scale surface detail

### 3. Erosion Simulation
- **Hydraulic Erosion**: Simulates water droplet flow, transport, and deposition
  - Configurable erosion/deposition strength
  - Water inertia and evaporation rate
  - Erosion brush radius
- **Thermal Erosion**: Slope-based material sliding
  - Talus angle control
  - Multiple iterations for gradual stabilization

### 4. Biome System
- Temperature and moisture map generation
- Biome classification based on temperature, moisture, and altitude
- Supported biomes:
  - Ocean, Beach
  - Desert, Grassland, Savanna
  - Forest, Rainforest, Taiga
  - Tundra, Mountain, Snow

### 5. Advanced Features
- **Domain Warping**: Distort noise space for more natural terrain
- **Terracing**: Create stepped plateaus
- **Island Mode**: Height falloff towards edges
- **Height Curve**: Power function for height distribution
- **Post-processing Smoothing**: Gaussian blur for terrain smoothing

## Parameters

### Grid Parameters
```cpp
int grid_resolution = 256;     // Grid resolution (vertices per side)
float grid_size = 100.0f;      // Grid size (world units)
float min_height = 0.0f;       // Minimum height
float max_height = 50.0f;      // Maximum height
```

### Noise Parameters
```cpp
int octaves = 6;               // Noise layers (more = more detail)
float frequency = 1.0f;        // Frequency (higher = more compressed)
float persistence = 0.5f;      // Persistence (controls roughness)
float lacunarity = 2.0f;       // Lacunarity (frequency multiplier)
```

### Erosion Parameters
```cpp
int erosion_iterations = 50000;        // Erosion iteration count
float erosion_strength = 0.3f;         // Erosion strength
float deposition_strength = 0.3f;      // Deposition strength
float evaporation_rate = 0.01f;        // Evaporation rate
float water_inertia = 0.3f;            // Water inertia
```

## Usage Example

```cpp
#include "TerrainGen/TerrainGeneration.h"

using namespace TerrainGen;

// Create terrain parameters
TerrainParameters params;
params.grid_resolution = 512;
params.grid_size = 200.0f;
params.max_height = 80.0f;

// Configure noise
params.noise_type = TerrainParameters::NoiseType::Perlin;
params.octaves = 8;
params.frequency = 1.5f;
params.persistence = 0.6f;

// Enable multi-scale features
params.enable_multi_scale = true;
params.mountain_amplitude = 40.0f;
params.valley_depth = 15.0f;

// Enable erosion
params.enable_erosion = true;
params.erosion_iterations = 100000;
params.erosion_strength = 0.4f;

// Generate terrain
TerrainGenerator generator;
auto terrain = generator.generate(params);

// Access height field
auto& height_field = *terrain->height_field;
for (int y = 0; y < height_field.height; ++y) {
    for (int x = 0; x < height_field.width; ++x) {
        float height = height_field.at(x, y);
        glm::vec3 normal = height_field.get_normal(x, y);
        // Use height and normal data...
    }
}
```

## Generation Pipeline

1. **Base Heightmap Generation**: Generate base noise using FBM (Fractional Brownian Motion)
2. **Multi-Scale Feature Overlay**: Add mountain, valley, hill, and detail layers
3. **Hydraulic Erosion**: Simulate water droplet flow to form natural rivers and valleys
4. **Thermal Erosion**: Material sliding to stabilize steep slopes
5. **Climate Map Generation**: Generate temperature and moisture distribution (if biomes enabled)
6. **Biome Application**: Modify terrain features based on biomes
7. **Post-Processing**: Apply height curves, terracing, smoothing, etc.

## Performance Tips

- For quick preview, use lower `grid_resolution` (128-256)
- For final rendering, use higher `grid_resolution` (512-1024)
- Erosion is computationally intensive; reduce `erosion_iterations` for speed
- Reduce `erosion_brush_radius` for better erosion performance
- Disable unnecessary features (biomes, domain warping, etc.)

## Algorithm References

- **FBM (Fractional Brownian Motion)**: Multi-layer noise composition
- **Hydraulic Erosion**: Particle-based erosion simulation
- **Thermal Erosion**: Talus angle-based slope stabilization
- **Biome Classification**: Whittaker diagram approach

## Relationship with TreeGen

TerrainGen follows TreeGen's architectural design:
- Similar parameter structure (Parameters)
- Similar data structures (Structure)
- Similar generator pattern (Generator)
- Support for Python bindings for testing and prototyping
