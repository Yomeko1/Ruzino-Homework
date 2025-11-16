"""
Test tree generation with OBJ file output (avoiding USD issues)
"""
import os
from ruzino_graph import RuzinoGraph


def test_tree_to_obj():
    """Test tree generation -> mesh -> OBJ file"""
    print("\n" + "="*70)
    print("TEST: Tree Generation -> Mesh -> OBJ")
    print("="*70)
    
    test_dir = os.path.dirname(os.path.abspath(__file__))
    binary_dir = os.path.join(test_dir, '..', '..', '..', '..', 'Binaries', 'Debug')
    binary_dir = os.path.abspath(binary_dir)
    output_file = os.path.join(binary_dir, "tree_mesh.obj")
    
    g = RuzinoGraph("TreeOBJTest")
    
    # Load TreeGen configuration
    g.loadConfiguration(os.path.join(binary_dir, "Plugins", "TreeGen_geometry_nodes.json"))
    print(f"✓ Loaded TreeGen configuration")
    
    # Load geometry nodes for write_obj
    g.loadConfiguration(os.path.join(binary_dir, "geometry_nodes.json"))
    print(f"✓ Loaded geometry nodes configuration")
    
    # Create pipeline: tree_generate -> tree_to_mesh -> write_obj
    tree_gen = g.createNode("tree_generate", name="tree")
    print(f"✓ Created tree_generate node")
    
    to_mesh = g.createNode("tree_to_mesh", name="converter")
    print(f"✓ Created tree_to_mesh node")
    
    # Note: write_obj node name might be "write_obj_std"
    write_obj = g.createNode("write_obj_std", name="writer")  
    print(f"✓ Created write_obj node")
    
    # Connect nodes
    g.addEdge(tree_gen, "Tree Branches", to_mesh, "Tree Branches")
    g.addEdge(to_mesh, "Mesh", write_obj, "Geometry")
    print(f"✓ Connected nodes")
    
    # Set parameters
    inputs = {
        (tree_gen, "Growth Years"): 3,
        (tree_gen, "Internode Length"): 0.5,
        (tree_gen, "Branch Angle"): 30.0,
        (to_mesh, "Radial Segments"): 8,
    }
    print(f"✓ Set parameters: Years=3, Internode=0.5, Angle=30°, Segments=8")
    
    # Set output path
    g.setIOConfig({
        (write_obj, "File Path"): output_file
    })
    print(f"✓ Set output path: {output_file}")
    
    # Execute
    g.prepare_and_execute(inputs, required_node=write_obj)
    print(f"✓ Executed graph")
    
    # Check file
    if os.path.exists(output_file):
        file_size = os.path.getsize(output_file)
        print(f"✓ OBJ file created: {file_size} bytes")
        
        if file_size > 1000:
            print("\n" + "="*70)
            print(f"✅ TEST PASSED: Tree OBJ file generated ({file_size} bytes)!")
            print(f"Output: {output_file}")
            print("="*70)
            assert file_size > 1000
        else:
            print(f"✗ OBJ file too small: {file_size} bytes")
            assert False, f"File too small: {file_size} bytes"
    else:
        print(f"✗ OBJ file not found: {output_file}")
        assert False, f"File not created"
