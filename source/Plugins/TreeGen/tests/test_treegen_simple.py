"""
TreeGen Plugin Test - Simple Branch Test
Tests procedural tree generation based on Stava et al. 2014
"""
import os
import sys
from ruzino_graph import RuzinoGraph
import stage_py


def get_binary_dir():
    """Get the binary directory path"""
    test_dir = os.path.dirname(os.path.abspath(__file__))
    binary_dir = os.path.join(test_dir, '..', '..', '..', '..', 'Binaries', 'Debug')
    return os.path.abspath(binary_dir)


def test_simple_branch():
    """Test creating a simple branch and writing to USD"""
    print("\n" + "="*70)
    print("TEST: Simple Tree Branch")
    print("="*70)
    
    binary_dir = get_binary_dir()
    output_file = "test_simple_branch.usdc"
    
    g = RuzinoGraph("SimpleBranchTest")
    
    # Load geometry nodes first for write_usd
    geom_config = os.path.join(binary_dir, "geometry_nodes.json")
    g.loadConfiguration(geom_config)
    print(f"✓ Loaded geometry nodes configuration")
    
    # Load TreeGen nodes
    treegen_config = os.path.join(binary_dir, "Plugins", "TreeGen_geometry_nodes.json")
    g.loadConfiguration(treegen_config)
    print(f"✓ Loaded TreeGen configuration")
    
    # Create nodes
    branch = g.createNode("tree_simple_branch", name="branch")
    print(f"✓ Created node: {branch.ui_name}")
    
    write_node = g.createNode("write_usd", name="writer")
    print(f"✓ Created write node")
    
    # Connect nodes
    g.addEdge(branch, "Branch Curve", write_node, "Geometry")
    print(f"✓ Connected nodes")
    
    # Set parameters
    inputs = {
        (branch, "Length"): 5.0,
        (branch, "Radius"): 0.2,
        (branch, "Subdivisions"): 10
    }
    print(f"✓ Set parameters: Length=5.0, Radius=0.2, Subdivisions=10")
    
    # Create Stage and convert to GeomPayload
    stage = stage_py.Stage(output_file)
    geom_payload = stage_py.create_payload_from_stage(stage, "/tree")
    g.setGlobalParams(geom_payload)
    print(f"✓ Created USD stage")
    
    # Execute
    print(f"\n🚀 Executing graph...")
    g.prepare_and_execute(inputs, required_node=write_node)
    print(f"✓ Executed successfully")
    
    # Save USD
    stage.save()
    print(f"✓ Saved USD to {output_file}")
    
    # Verify file
    assert os.path.exists(output_file), f"USD file not created: {output_file}"
    file_size = os.path.getsize(output_file)
    print(f"✓ File size: {file_size} bytes")
    assert file_size > 100, f"USD file seems empty: {file_size} bytes"
    
    print(f"\n✅ Simple branch test passed!")
    print(f"   Use 'usdcat {output_file}' to inspect the geometry")


if __name__ == "__main__":
    try:
        test_simple_branch()
        
        print("\n" + "="*70)
        print("  🌳 TREEGEN TEST PASSED! 🌳")
        print("="*70)
        
    except Exception as e:
        print(f"\n✗ TEST FAILED: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
