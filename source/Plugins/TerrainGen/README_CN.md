# TerrainGen - Procedural Terrain Generation Plugin

## 概述

TerrainGen 是一个过程式地形生成插件，支持多尺度噪声、侵蚀模拟和基于生物群系的地形生成。

## 主要特性

### 1. 多种噪声类型
- **Perlin 噪声**: 经典的平滑噪声，适合生成自然起伏的地形
- **Simplex 噪声**: 更高效的梯度噪声
- **Worley/Voronoi 噪声**: 单元格噪声，适合生成裂纹、晶体状地形
- **Ridged 噪声**: 山脉噪声，用于生成尖锐的山峰
- **Billow 噪声**: 云状噪声，用于生成圆润的丘陵

### 2. 多尺度特征
- **山脉层**: 大尺度、尖锐的山峰
- **山谷层**: 中等尺度的负地形
- **丘陵层**: 中等尺度的起伏
- **细节层**: 小尺度的表面细节

### 3. 侵蚀模拟
- **水力侵蚀**: 模拟水滴流动、搬运和沉积过程
  - 可配置侵蚀强度、沉积强度
  - 水的惯性、蒸发率
  - 侵蚀笔刷半径
- **热力侵蚀**: 基于坡度的物质滑落
  - 休止角（talus angle）控制
  - 多次迭代逐步稳定地形

### 4. 生物群系系统
- 温度和湿度图生成
- 基于温度、湿度、海拔的生物群系分类
- 支持的生物群系:
  - 海洋、海滩
  - 沙漠、草原、稀树草原
  - 森林、热带雨林、针叶林
  - 苔原、高山、雪地

### 5. 高级特性
- **域扭曲 (Domain Warping)**: 扭曲噪声空间，创造更自然的地形
- **梯田效果**: 创建阶梯状高原
- **岛屿模式**: 边缘高度衰减，形成岛屿
- **高度曲线**: 幂函数调整高度分布
- **后处理平滑**: 高斯模糊平滑地形

## 参数说明

### 网格参数
```cpp
int grid_resolution = 256;     // 网格分辨率 (顶点数)
float grid_size = 100.0f;      // 网格大小 (世界单位)
float min_height = 0.0f;       // 最小高度
float max_height = 50.0f;      // 最大高度
```

### 噪声参数
```cpp
int octaves = 6;               // 噪声层数 (越多越详细)
float frequency = 1.0f;        // 频率 (越高越密集)
float persistence = 0.5f;      // 持久度 (控制粗糙程度)
float lacunarity = 2.0f;       // 间隙度 (层间频率倍数)
```

### 侵蚀参数
```cpp
int erosion_iterations = 50000;        // 侵蚀迭代次数
float erosion_strength = 0.3f;         // 侵蚀强度
float deposition_strength = 0.3f;      // 沉积强度
float evaporation_rate = 0.01f;        // 蒸发率
float water_inertia = 0.3f;            // 水的惯性
```

## 使用示例

```cpp
#include "TerrainGen/TerrainGeneration.h"

using namespace TerrainGen;

// 创建地形参数
TerrainParameters params;
params.grid_resolution = 512;
params.grid_size = 200.0f;
params.max_height = 80.0f;

// 配置噪声
params.noise_type = TerrainParameters::NoiseType::Perlin;
params.octaves = 8;
params.frequency = 1.5f;
params.persistence = 0.6f;

// 启用多尺度特征
params.enable_multi_scale = true;
params.mountain_amplitude = 40.0f;
params.valley_depth = 15.0f;

// 启用侵蚀
params.enable_erosion = true;
params.erosion_iterations = 100000;
params.erosion_strength = 0.4f;

// 生成地形
TerrainGenerator generator;
auto terrain = generator.generate(params);

// 访问高度场
auto& height_field = *terrain->height_field;
for (int y = 0; y < height_field.height; ++y) {
    for (int x = 0; x < height_field.width; ++x) {
        float height = height_field.at(x, y);
        glm::vec3 normal = height_field.get_normal(x, y);
        // 使用高度和法线数据...
    }
}
```

## 生成流程

1. **基础高度图生成**: 使用 FBM (Fractional Brownian Motion) 生成基础噪声
2. **多尺度特征叠加**: 添加山脉、山谷、丘陵、细节层
3. **水力侵蚀**: 模拟水滴流动，形成自然的河流和山谷
4. **热力侵蚀**: 物质滑落，稳定陡峭的坡面
5. **气候图生成**: 生成温度和湿度分布 (如果启用生物群系)
6. **生物群系应用**: 根据生物群系修改地形特征
7. **后处理**: 应用高度曲线、梯田、平滑等效果

## 性能优化建议

- 对于快速预览，使用较低的 `grid_resolution` (128-256)
- 对于最终渲染，使用较高的 `grid_resolution` (512-1024)
- 侵蚀计算密集，减少 `erosion_iterations` 可提高速度
- 减少 `erosion_brush_radius` 可提高侵蚀性能
- 禁用不需要的特性 (生物群系、域扭曲等)

## 算法参考

- **FBM (Fractional Brownian Motion)**: 多层噪声叠加
- **水力侵蚀**: 基于粒子的侵蚀模拟
- **热力侵蚀**: 基于休止角的坡度稳定
- **生物群系分类**: Whittaker 图表方法

## 与 TreeGen 的关系

TerrainGen 参考了 TreeGen 的架构设计:
- 类似的参数结构 (Parameters)
- 类似的数据结构 (Structure)
- 类似的生成器模式 (Generator)
- 支持 Python 绑定用于测试和原型设计
