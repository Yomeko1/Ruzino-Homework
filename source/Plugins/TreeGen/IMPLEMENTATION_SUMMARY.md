# TreeGen Implementation Summary

## 实现概述

基于论文 *"Inverse Procedural Modelling of Trees"* (Stava et al., 2014)，我为您实现了一个完整的树木过程式建模系统。这个实现专注于论文中的**前向模型（Forward Model）**，能够根据生物学参数生成多样化的树形结构。

## 已创建的文件

### 头文件（Header Files）

1. **`TreeParameters.h`** - 树生成参数定义
   - 包含论文中的24个参数
   - 分为几何、芽命运、环境三大类
   - 所有参数都有合理的默认值

2. **`TreeStructure.h`** - 树的数据结构
   - `TreeBud`: 芽的结构（顶芽/侧芽）
   - `TreeBranch`: 分支结构（节间）
   - `TreeStructure`: 完整树容器

3. **`TreeGrowth.h`** - 生长算法接口
   - 定义了所有生长相关的方法
   - 包括芽萌发、激素模拟、向性等

### 实现文件（Implementation Files）

4. **`TreeGrowth.cpp`** - 核心生长算法实现
   - ~500行代码
   - 实现了完整的生长周期算法
   - 包括：
     - 芽状态管理
     - 光照计算（简化版）
     - 激素（auxin）传输模拟
     - 芽萌发概率计算（论文公式2-3）
     - 枝条生成
     - 向光性/向地性模拟
     - 结构更新

### 节点文件（Node Files）

5. **`node_tree_simple_branch.cpp`** - 简单枝条生成节点
   - 用于测试和调试
   - 生成单个直立的枝条

6. **`node_tree_generate.cpp`** - 主树生成节点
   - 12个输入参数（精选最重要的参数）
   - 输出树枝曲线集合（Curve Geometry）
   - 集成完整的生长算法

7. **`node_tree_to_mesh.cpp`** - 树转网格节点
   - 将曲线转换为可渲染的网格
   - 支持自定义径向细分

### 配置文件（Configuration Files）

8. **`CMakeLists.txt`** (更新) - TreeGen插件的构建配置
9. **`geometry_nodes/CMakeLists.txt`** (更新) - 几何节点构建配置

### 文档文件（Documentation Files）

10. **`README.md`** - 详细的使用文档
    - 算法原理解释
    - 参数调节指南
    - 使用示例
    - 不同树形的参数推荐

11. **`test_treegen.py`** - Python测试脚本
    - 4个测试用例
    - 验证基本功能和参数变化

## 核心算法实现

### 生长周期（Growth Cycle）

每个生长周期执行以下步骤（对应论文第4.1节）：

```cpp
void TreeGrowth::grow_one_cycle(TreeStructure& tree) {
    1. update_bud_states()           // 更新芽状态（死亡）
    2. calculate_illumination()      // 计算光照
    3. calculate_auxin_levels()      // 计算激素浓度
    4. determine_bud_flushing()      // 判断芽萌发
    5. grow_shoot_from_bud()         // 生长新枝条
    6. update_branch_radii()         // 更新枝条半径
    7. apply_structural_bending()    // 结构弯曲
    8. prune_branches()              // 修剪
}
```

### 关键公式实现

**芽萌发概率**（论文公式2-3）：

```cpp
// 顶芽 (Equation 2)
P(F_apical) = I^α_light

// 侧芽 (Equation 3)
P(F_lateral) = I^α_light * exp(-Σ(auxin * distance_decay))
```

**顶端控制**（论文公式1）：

```cpp
// 高层级分支生长速率降低
growth_rate' = growth_rate / (apical_control)^level
```

**叶序（Phyllotaxis）**：

```cpp
// 使用黄金角137.5°实现螺旋排列
roll_angle = 137.5° * bud_index + random_variance
```

## 节点使用

### 基础树生成

```python
from ruzino_graph import RuzinoGraph

g = RuzinoGraph("TreeDemo")
g.loadConfiguration("TreeGen_geometry_nodes.json")

tree = g.createNode("tree_generate")
inputs = {
    (tree, "Growth Years"): 10,
    (tree, "Lateral Buds"): 4,
    (tree, "Branch Angle"): 45.0,
    (tree, "Apical Control"): 2.0
}

g.prepare_and_execute(inputs, required_node=tree)
result = g.getOutput(tree, "Tree Branches")
```

### 不同树形示例

**松树（高耸直立）**：
```python
inputs = {
    "Apical Control": 3.5,      # 强主干优势
    "Branch Angle": 30.0,        # 小分支角度
    "Growth Rate": 4.0,          # 快速生长
    "Apical Dominance": 2.5      # 强顶端优势
}
```

**橡树（球形树冠）**：
```python
inputs = {
    "Apical Control": 1.2,       # 弱主干优势
    "Branch Angle": 55.0,        # 大分支角度
    "Growth Rate": 2.5,          # 适中生长
    "Apical Dominance": 0.8      # 弱顶端优势
}
```

## 技术特点

### ✅ 已实现的功能

1. **完整的生长模拟**
   - 基于芽的生长机制
   - 激素（auxin）传输模拟
   - 顶端优势和顶端控制

2. **生物学真实性**
   - 叶序（黄金角）
   - 向光性和向地性
   - 随树龄变化的参数

3. **灵活的参数系统**
   - 24个论文参数全部实现
   - 节点接口暴露12个最重要的参数
   - 易于调节和实验

4. **集成到现有系统**
   - 使用CurveComponent和MeshComponent
   - 兼容现有的节点系统
   - 可与curve_to_mesh等节点组合

### 🔄 简化/未实现的部分

1. **光照计算**
   - 当前：基于高度的简化模型
   - 论文：使用光线追踪或体素化

2. **结构弯曲**
   - 当前：简化实现
   - 论文：基于力学的下垂模拟

3. **叶片**
   - 当前：无叶片
   - 论文：使用Livny et al.的方法生成叶片

4. **逆向建模**
   - 当前：仅实现前向模型
   - 论文核心：使用MCMC从输入树反推参数

## 数据流

```
TreeParameters → TreeGrowth → TreeStructure
                     ↓
              [Growth Cycles]
                     ↓
            TreeStructure (Branches)
                     ↓
              CurveComponent
                     ↓
           (可选) MeshComponent
```

## 扩展建议

如果要进一步开发，可以添加：

1. **更好的光照**：体素化空间，真实的遮挡计算
2. **叶片系统**：在末端枝条添加叶片几何
3. **纹理和材质**：树皮纹理，叶片材质
4. **风的影响**：动态摆动
5. **季节变化**：叶片生长/凋落
6. **逆向优化**：实现论文的参数估计（MCMC）

## 编译说明

1. 确保GLM库已安装
2. 在主CMakeLists.txt中已包含TreeGen插件
3. 构建系统会自动扫描并注册所有节点
4. 生成的JSON配置文件：`TreeGen_geometry_nodes.json`

## 测试

运行测试脚本：

```bash
cd tests/treegen
python test_treegen.py
```

测试包括：
- ✅ 简单枝条生成
- ✅ 完整树生成
- ✅ 树到网格转换
- ✅ 参数变化效果

## 总结

这个实现提供了一个坚实的基础，可以生成多样化的树形结构。代码结构清晰，易于理解和扩展。虽然某些部分做了简化（如光照计算），但核心的生物学机制都已实现，可以产生令人信服的结果。

最重要的是，这个系统完全集成到了您现有的节点系统中，可以与其他几何节点（如curve_to_mesh）无缝组合使用。
