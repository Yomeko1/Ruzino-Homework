# TreeGen 树木生成插件文档

## 概述

TreeGen 是一个基于程序化方法的树木生成系统，实现了 Stava et al. 2014 论文 "Inverse Procedural Modelling of Trees" 中描述的树木生长模型。该系统使用发育式生长方法（developmental model），通过模拟芽（bud）的生长和分化来生成逼真的树木结构。

## 理论基础

### 1. 树木生长的生物学原理

树木的生长是一个复杂的生物学过程，主要由以下几个因素控制：

#### 1.1 芽的类型
- **顶芽（Apical Bud）**：位于枝条顶端，向上生长，形成主干或主枝
- **侧芽（Lateral Bud）**：位于枝条侧面，向外生长，形成侧枝

#### 1.2 激素调控（Apical Dominance）
- **生长素（Auxin）**：由顶芽产生，向下运输
- **顶端优势**：生长素抑制侧芽生长，使顶芽优先发育
- **距离衰减**：生长素浓度随距离呈指数衰减

#### 1.3 环境因素
- **向光性（Phototropism）**：枝条向光源方向生长
- **向地性（Gravitropism）**：枝条受重力影响，主干向上，侧枝下垂
- **光照竞争**：上层枝叶遮挡下层，影响芽的萌发

### 2. 程序化生长模型

#### 2.1 生长周期（Growth Cycle）

每个生长周期包含以下步骤：

```
1. 更新芽状态（死亡、休眠）
2. 计算光照强度
3. 计算生长素水平
4. 决定芽的萌发
5. 从萌发的芽生长新枝条
6. 更新枝条半径（管道模型）
7. 应用结构弯曲
8. 修剪枝条
```

#### 2.2 芽的萌发概率

**顶芽萌发概率**：
```
P_apical = I^α
```
其中：
- I：光照强度（0-1）
- α：光照因子（apical_light_factor）

**侧芽萌发概率**：
```
P_lateral = I^β × exp(-A)
```
其中：
- I：光照强度
- β：侧芽光照因子（lateral_light_factor）
- A：生长素浓度

#### 2.3 生长素浓度计算

对于每个侧芽，其生长素浓度由所有位于其上方的芽贡献：

```
A = Σ D_base × exp(-d × D_dist) × D_age^t
```

其中：
- D_base：基础顶端优势（apical_dominance_base）
- d：芽间距离
- D_dist：距离衰减因子（apical_dominance_distance）
- D_age：年龄因子（apical_dominance_age）
- t：树龄

#### 2.4 节间长度计算

节间（internode）长度随树龄递减：

```
L = L_base × L_age^t
```

其中：
- L_base：基础节间长度（internode_base_length）
- L_age：长度衰减因子（internode_length_age_factor）
- t：树龄

#### 2.5 生长速率

每次萌发产生的节间数量受顶端控制：

```
N = G_rate / C^level
```

其中：
- G_rate：基础生长速率（growth_rate）
- C：顶端控制因子（apical_control）
- level：分支层级（0=主干）

#### 2.6 分支角度和叶序

**分支角度（Branching Angle）**：
- 从正态分布采样：N(mean, variance)
- mean：branching_angle_mean（默认45°）
- variance：branching_angle_variance（默认10°）

**旋转角度（Roll Angle / Phyllotaxis）**：
- 使用黄金角（137.5°）实现螺旋排列
- 第 i 个芽的旋转角：137.5° × i
- 这种排列最大化了空间利用和光照接收

#### 2.7 管道模型（Pipe Model）

枝条半径根据子枝条递归计算：

```
R_parent = √(Σ R_child²)
```

这基于管道模型理论：父枝的横截面积等于所有子枝横截面积之和。

### 3. 叶子生成

#### 3.1 叶子位置
- 叶子沿节间均匀分布
- 数量由 `leaves_per_internode` 控制
- 仅在高于最小层级（min_leaf_level）的枝条上生成

#### 3.2 叶子排列
- 使用叶序（phyllotaxis）实现螺旋排列
- 黄金角（137.5°）确保叶子均匀分布，避免重叠

#### 3.3 叶子朝向
- 法向量主要指向外侧
- 添加向上偏置（30%），模拟叶片的向光性
- 切向量垂直于枝条方向

#### 3.4 叶子大小
- 基础大小由 `leaf_size_base` 控制
- 随分支层级衰减（每层级 × 0.8）
- 添加随机变化 `leaf_size_variance`

## 代码结构

### 核心类

#### TreeParameters
定义所有生长参数：
- 几何参数（角度、长度、生长速率）
- 芽命运参数（萌发概率、死亡率）
- 环境参数（光照、重力）
- 叶子参数（数量、大小）

#### TreeStructure
树木的数据结构：
- `TreeBranch`：枝条（节间）
  - 起点、终点、方向、长度、半径
  - 父枝条、子枝条、侧芽、叶子
- `TreeBud`：芽
  - 类型（顶芽/侧芽）、状态（活跃/休眠/死亡）
  - 位置、方向、光照、生长素浓度
- `TreeLeaf`：叶子
  - 位置、法向、切向
  - 大小、旋转

#### TreeGrowth
生长系统的主要逻辑：
- `initialize_tree()`：初始化主干
- `grow_one_cycle()`：执行一个生长周期
- `grow_tree()`：执行多个生长周期

### 几何节点

#### tree_generate
主要的树木生成节点：
- 输入：生长参数（年份、角度、光照等）
- 输出：树枝的曲线几何（Curve Geometry）
- 将树木结构转换为可视化的曲线

#### tree_to_mesh
将曲线转换为网格：
- 输入：树枝曲线、径向分段数
- 输出：圆柱形网格
- 为每个枝条创建带半径变化的圆柱体

#### tree_to_leaves
生成叶子几何：
- 输入：树木结构、叶子尺寸
- 输出：叶子网格
- 创建简单的菱形叶片模板

## 使用方法

### 基本使用

```python
from ruzino_graph import RuzinoGraph

# 创建图
g = RuzinoGraph("TreeTest")

# 加载TreeGen节点
g.loadConfiguration("Plugins/TreeGen_geometry_nodes.json")

# 创建节点
tree_gen = g.createNode("tree_generate", name="tree")
to_mesh = g.createNode("tree_to_mesh", name="mesh")
write_usd = g.createNode("write_usd", name="writer")

# 连接节点
g.addEdge(tree_gen, "Tree Branches", to_mesh, "Tree Branches")
g.addEdge(to_mesh, "Mesh", write_usd, "Geometry")

# 设置参数
inputs = {
    (tree_gen, "Growth Years"): 5,
    (tree_gen, "Branch Angle"): 45.0,
    (tree_gen, "Generate Leaves"): True,
    (to_mesh, "Radial Segments"): 8,
}

# 执行
g.prepare_and_execute(inputs, required_node=write_usd)
```

### 参数调节指南

#### 树形控制

**主干粗壮、分支少**：
- 增大 `Apical Control`（2.0 → 4.0）
- 增大 `Apical Dominance`（1.0 → 2.0）
- 减少 `Lateral Buds`（4 → 2）

**分支繁茂、灌木状**：
- 减小 `Apical Control`（2.0 → 1.0）
- 减小 `Apical Dominance`（1.0 → 0.5）
- 增加 `Lateral Buds`（4 → 6）

#### 分支角度

**直立树形（如松树）**：
- 减小 `Branch Angle`（45° → 25°）
- 增大 `Gravitropism`（0.2 → 0.4）

**开展树形（如橡树）**：
- 增大 `Branch Angle`（45° → 65°）
- 减小 `Gravitropism`（0.2 → 0.1）

#### 树木密度

**稀疏**：
- 减小 `Growth Rate`（3.0 → 2.0）
- 减小 `Internode Length`（0.3 → 0.5）

**密集**：
- 增大 `Growth Rate`（3.0 → 5.0）
- 减小 `Internode Length`（0.3 → 0.15）

#### 叶子调节

**浓密树冠**：
- 增加 `Leaves Per Internode`（3 → 5）
- 增大 `Leaf Size`（0.15 → 0.25）

**稀疏树冠**：
- 减少 `Leaves Per Internode`（3 → 1）
- 减小 `Leaf Size`（0.15 → 0.08）

## 实现细节与论文对照

### 已实现的特性

✅ **芽的生长模型**
- 顶芽和侧芽的区分
- 芽的萌发概率计算（基于光照和生长素）
- 芽的死亡和休眠状态

✅ **顶端优势（Apical Dominance）**
- 生长素的产生和传播
- 距离衰减和年龄因子
- 对侧芽萌发的抑制效果

✅ **光照模型**
- 简化的基于高度的光照计算
- 光照对萌发概率的影响

✅ **向性生长（Tropisms）**
- 向光性（phototropism）
- 向地性（gravitropism）

✅ **几何参数**
- 分支角度的正态分布
- 黄金角叶序（phyllotaxis）
- 节间长度的年龄衰减

✅ **结构模型**
- 管道模型（pipe model）计算半径
- 分支层级系统

✅ **叶子生成**
- 基于叶序的螺旋排列
- 尺寸随层级衰减
- 向外向上的朝向

### 简化的特性

⚠️ **光照计算**
- 论文：完整的阴影投射和遮挡计算
- 当前：基于高度的简化模型

⚠️ **结构弯曲**
- 论文：基于重量的物理弯曲模拟
- 当前：占位符实现（未完全实现）

⚠️ **修剪**
- 论文：基于光照竞争的动态修剪
- 当前：简化的基于高度和光照的标记

### 修正的问题

🔧 **分支创建逻辑**
- 原问题：简化的父节点查找不正确
- 修正：正确查找 shared_ptr 父节点

🔧 **叶序角度**
- 原问题：roll angle 基于索引而非累积
- 修正：使用累积的黄金角（137.5° × i）

🔧 **生长方向归一化**
- 原问题：某些向量叉积未归一化
- 修正：确保所有方向向量归一化

## 扩展建议

### 短期改进

1. **完整的光照计算**
   - 实现射线投射检测遮挡
   - 考虑天空光和直射光
   - 添加自遮挡计算

2. **物理弯曲**
   - 计算枝条重量
   - 模拟重力导致的下垂
   - 添加风力影响

3. **更丰富的叶子**
   - 多种叶片形状（椭圆、掌状、羽状）
   - 叶片簇（compound leaves）
   - 季节变化（颜色、脱落）

4. **纹理坐标**
   - 为树干和树枝生成 UV
   - 支持树皮纹理映射

### 长期扩展

1. **逆向建模**
   - 实现论文中的 MCMC 优化
   - 从输入网格反推参数
   - 参数自动调优

2. **环境交互**
   - 避障生长
   - 多棵树的竞争
   - 地形适应

3. **LOD 系统**
   - 多层次细节生成
   - 距离-based 简化
   - 实例化优化

4. **动画**
   - 生长动画
   - 风力摆动
   - 季节变化

## 参考文献

Stava, O., Pirk, S., Kratt, J., Chen, B., Měch, R., Deussen, O., & Benes, B. (2014). Inverse Procedural Modelling of Trees. *Computer Graphics Forum*, 33(6), 118-131.

## 许可

本插件遵循项目主仓库的许可证。

---

**作者**：TreeGen Plugin Team  
**版本**：1.0  
**最后更新**：2024
