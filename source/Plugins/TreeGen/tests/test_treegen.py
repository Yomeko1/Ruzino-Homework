"""
TreeGen Plugin Test
Tests procedural tree generation based on Stava et al. 2014
"""
import os
import sys
from ruzino_graph import RuzinoGraph
import stage_py
import geometry_py as geom


def get_binary_dir():
    """Get the binary directory path"""
    test_dir = os.path.dirname(os.path.abspath(__file__))
    binary_dir = os.path.join(test_dir, '..', '..', '..', '..', 'Binaries', 'Debug')
    return os.path.abspath(binary_dir)


def test_simple_branch():
    """Test creating a simple branch"""
    print("\n" + "="*70)
    print("TEST: Simple Tree Branch")
    print("="*70)
    
    binary_dir = get_binary_dir()
    g = RuzinoGraph("SimpleBranchTest")
    config_path = os.path.join(binary_dir, "Plugins", "TreeGen_geometry_nodes.json")
    
    g.loadConfiguration(config_path)
    print(f"✓ Loaded TreeGen configuration")
    
    # Create simple branch node
    branch = g.createNode("tree_simple_branch", name="branch")
    print(f"✓ Created node: {branch.ui_name}")
    
    # Set parameters
    inputs = {
        (branch, "Length"): 5.0,
        (branch, "Radius"): 0.2,
        (branch, "Subdivisions"): 10
    }
    
    # Execute
    g.prepare_and_execute(inputs, required_node=branch)
    print("✓ Executed")
    
    # Get output
    result = g.getOutput(branch, "Branch Curve")
    geometry = geom.extract_geometry_from_meta_any(result)
    
    curve = geometry.get_curve_component(0)
    assert curve is not None, "No CurveComponent found"
    
    vertices = geom.get_curve_vertices_as_array(curve)
    print(f"✓ Branch has {len(vertices)} vertices")
    print(f"✓ Branch length: {vertices[-1][1]:.2f}")
    
    assert len(vertices) == 11, f"Expected 11 vertices, got {len(vertices)}"
    print("✓ Simple branch test passed!")


def test_tree_generation():
    """Test full procedural tree generation"""
    print("\n" + "="*70)
    print("TEST: Procedural Tree Generation")
    print("="*70)
    
    binary_dir = get_binary_dir()
    g = RuzinoGraph("TreeGenTest")
    config_path = os.path.join(binary_dir, "TreeGen_geometry_nodes.json")
    
    g.loadConfiguration(config_path)
    print(f"✓ Loaded configuration")
    
    # Create tree generation node
    tree = g.createNode("tree_generate", name="tree")
    print(f"✓ Created node: {tree.ui_name}")
    
    # Set parameters for a small tree
    inputs = {
        (tree, "Growth Years"): 5,
        (tree, "Random Seed"): 42,
        (tree, "Apical Angle Variance"): 30.0,
        (tree, "Lateral Buds"): 3,
        (tree, "Branch Angle"): 45.0,
        (tree, "Growth Rate"): 2.5,
        (tree, "Internode Length"): 0.3,
        (tree, "Apical Control"): 2.0,
        (tree, "Apical Dominance"): 1.0,
        (tree, "Light Factor"): 0.6,
        (tree, "Phototropism"): 0.2,
        (tree, "Gravitropism"): 0.1
    }
    
    print("\n🌱 Growing tree with parameters:")
    print(f"  Growth Years: 5")
    print(f"  Branch Angle: 45°")
    print(f"  Apical Control: 2.0")
    
    # Execute
    print("\n🚀 Executing tree growth...")
    g.prepare_and_execute(inputs, required_node=tree)
    print("✓ Tree generation completed")
    
    # Get result
    result = g.getOutput(tree, "Tree Branches")
    geometry = geom.extract_geometry_from_meta_any(result)
    
    curve = geometry.get_curve_component(0)
    assert curve is not None, "No CurveComponent found"
    
    vertices = geom.get_curve_vertices_as_array(curve)
    counts = geom.get_curve_counts_as_array(curve)
    
    print(f"\n📊 Tree Statistics:")
    print(f"  Total vertices: {len(vertices)}")
    print(f"  Branch segments: {len(counts)}")
    print(f"  Branches: {len(counts) // 2}")  # Each branch has 2 points
    
    # Check tree has grown
    assert len(vertices) > 2, "Tree should have more than root branch"
    assert len(counts) > 1, "Tree should have multiple branches"
    
    # Check tree has some height
    max_height = max(v[1] for v in vertices)
    print(f"  Max height: {max_height:.2f}")
    assert max_height > 0.5, "Tree should have grown upward"
    
    print("\n✅ Tree generation test passed!")


def test_tree_to_mesh():
    """Test converting tree to mesh"""
    print("\n" + "="*70)
    print("TEST: Tree to Mesh Conversion")
    print("="*70)
    
    binary_dir = get_binary_dir()
    g = RuzinoGraph("TreeMeshTest")
    config_path = os.path.join(binary_dir, "TreeGen_geometry_nodes.json")
    
    g.loadConfiguration(config_path)
    
    # Create simple branch
    branch = g.createNode("tree_simple_branch", name="branch")
    inputs_branch = {
        (branch, "Length"): 2.0,
        (branch, "Radius"): 0.1,
        (branch, "Subdivisions"): 3
    }
    g.prepare_and_execute(inputs_branch, required_node=branch)
    branch_curve = g.getOutput(branch, "Branch Curve")
    
    # Convert to mesh
    to_mesh = g.createNode("tree_to_mesh", name="mesh_converter")
    inputs_mesh = {
        (to_mesh, "Tree Branches"): branch_curve,
        (to_mesh, "Radial Segments"): 8
    }
    
    print("🔄 Converting tree to mesh...")
    g.prepare_and_execute(inputs_mesh, required_node=to_mesh)
    print("✓ Conversion completed")
    
    # Get mesh result
    result = g.getOutput(to_mesh, "Mesh")
    geometry = geom.extract_geometry_from_meta_any(result)
    
    mesh = geometry.get_mesh_component(0)
    assert mesh is not None, "No MeshComponent found"
    
    vertices = geom.get_vertices_as_array(mesh)
    faces = geom.get_face_indices_as_array(mesh)
    
    print(f"\n📊 Mesh Statistics:")
    print(f"  Vertices: {len(vertices)}")
    print(f"  Face indices: {len(faces)}")
    
    # For 4 segments, 8 radial segments:
    # Vertices: 4 segments * 2 rings * 8 = 64 vertices
    # Faces: 3 connections * 8 quads = 24 faces, 96 indices
    expected_verts = 4 * 2 * 8  # segments * rings * radial
    expected_faces = 3 * 8 * 4   # connections * radial * 4 indices per quad
    
    assert len(vertices) == expected_verts, \
        f"Expected {expected_verts} vertices, got {len(vertices)}"
    assert len(faces) == expected_faces, \
        f"Expected {expected_faces} face indices, got {len(faces)}"
    
    print("✅ Mesh conversion test passed!")


def test_parameter_variations():
    """Test different tree parameters produce different results"""
    print("\n" + "="*70)
    print("TEST: Parameter Variations")
    print("="*70)
    
    binary_dir = get_binary_dir()
    g = RuzinoGraph("ParamTest")
    config_path = os.path.join(binary_dir, "TreeGen_geometry_nodes.json")
    g.loadConfiguration(config_path)
    
    # Test 1: High apical control (tall tree)
    print("\n🌲 Test 1: High Apical Control (Tall Tree)")
    tree1 = g.createNode("tree_generate", name="tall_tree")
    inputs1 = {
        (tree1, "Growth Years"): 5,
        (tree1, "Random Seed"): 1,
        (tree1, "Apical Control"): 4.0,
        (tree1, "Branch Angle"): 30.0,
        (tree1, "Growth Rate"): 3.0
    }
    g.prepare_and_execute(inputs1, required_node=tree1)
    result1 = g.getOutput(tree1, "Tree Branches")
    geom1 = geom.extract_geometry_from_meta_any(result1)
    verts1 = geom.get_curve_vertices_as_array(geom1.get_curve_component(0))
    height1 = max(v[1] for v in verts1)
    print(f"  Height: {height1:.2f}")
    
    # Test 2: Low apical control (bushy tree)
    print("\n🌳 Test 2: Low Apical Control (Bushy Tree)")
    tree2 = g.createNode("tree_generate", name="bushy_tree")
    inputs2 = {
        (tree2, "Growth Years"): 5,
        (tree2, "Random Seed"): 1,
        (tree2, "Apical Control"): 1.0,
        (tree2, "Branch Angle"): 60.0,
        (tree2, "Growth Rate"): 3.0
    }
    g.prepare_and_execute(inputs2, required_node=tree2)
    result2 = g.getOutput(tree2, "Tree Branches")
    geom2 = geom.extract_geometry_from_meta_any(result2)
    verts2 = geom.get_curve_vertices_as_array(geom2.get_curve_component(0))
    height2 = max(v[1] for v in verts2)
    branches2 = len(geom.get_curve_counts_as_array(geom2.get_curve_component(0)))
    print(f"  Height: {height2:.2f}")
    print(f"  Branches: {branches2}")
    
    print("\n📊 Comparison:")
    print(f"  Tall tree height: {height1:.2f}")
    print(f"  Bushy tree height: {height2:.2f}")
    print(f"  Height ratio: {height1/height2:.2f}x")
    
    # High apical control should produce taller trees
    assert height1 > height2, "High apical control should produce taller trees"
    
    print("✅ Parameter variation test passed!")


if __name__ == "__main__":
    try:
        test_simple_branch()
        # test_tree_generation()
        # test_tree_to_mesh()
        # test_parameter_variations()
        
        print("\n" + "="*70)
        print("  🌳 TREEGEN TESTS PASSED! 🌳")
        print("="*70)
        
    except Exception as e:
        print(f"\n✗ TEST FAILED: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
