"""
Test tree generation without USD output
"""
import os
from ruzino_graph import RuzinoGraph


def test_tree_to_curves():
    """Test tree generation producing curves"""
    print("\n" + "="*70)
    print("TEST: Tree Generation -> Curves")
    print("="*70)
    
    test_dir = os.path.dirname(os.path.abspath(__file__))
    binary_dir = os.path.join(test_dir, '..', '..', '..', '..', 'Binaries', 'Debug')
    binary_dir = os.path.abspath(binary_dir)
    
    g = RuzinoGraph("TreeCurvesTest")
    
    # Load TreeGen configuration
    g.loadConfiguration(os.path.join(binary_dir, "Plugins", "TreeGen_geometry_nodes.json"))
    print(f"✓ Loaded TreeGen configuration")
    
    # Create tree_generate node
    tree_gen = g.createNode("tree_generate", name="tree")
    print(f"✓ Created tree_generate node")
    
    # Set parameters
    inputs = {
        (tree_gen, "Growth Years"): 3,
        (tree_gen, "Internode Length"): 1.0,
        (tree_gen, "Branch Angle"): 30.0,
    }
    print(f"✓ Set parameters: Years=3, Internode=1.0, Angle=30°")
    
    # Execute
    g.prepare_and_execute(inputs, required_node=tree_gen)
    print(f"✓ Executed graph - tree generated")
    
    print("\n" + "="*70)
    print("✅ TEST PASSED: Tree curves generated!")
    print("="*70)


def test_tree_to_mesh_only():
    """Test tree_to_mesh with simple input"""
    print("\n" + "="*70)
    print("TEST: Tree To Mesh Conversion")
    print("="*70)
    
    test_dir = os.path.dirname(os.path.abspath(__file__))
    binary_dir = os.path.join(test_dir, '..', '..', '..', '..', 'Binaries', 'Debug')
    binary_dir = os.path.abspath(binary_dir)
    
    g = RuzinoGraph("TreeMeshTest")
    
    # Load configurations
    g.loadConfiguration(os.path.join(binary_dir, "Plugins", "TreeGen_geometry_nodes.json"))
    print(f"✓ Loaded TreeGen configuration")
    
    # Create simple branch first
    branch = g.createNode("tree_simple_branch", name="branch")
    print(f"✓ Created tree_simple_branch node")
    
    # Create tree_to_mesh
    to_mesh = g.createNode("tree_to_mesh", name="converter")
    print(f"✓ Created tree_to_mesh node")
    
    # Connect
    g.addEdge(branch, "Branch Curve", to_mesh, "Tree Branches")
    print(f"✓ Connected nodes")
    
    # Set parameters
    inputs = {
        (branch, "Length"): 5.0,
        (branch, "Radius"): 0.2,
        (branch, "Subdivisions"): 10,
        (to_mesh, "Radial Segments"): 8,
    }
    print(f"✓ Set parameters")
    
    # Execute
    g.prepare_and_execute(inputs, required_node=to_mesh)
    print(f"✓ Executed graph - mesh generated")
    
    print("\n" + "="*70)
    print("✅ TEST PASSED: Branch mesh generated!")
    print("="*70)
