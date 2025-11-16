"""
Test TreeGen simple branch node without USD output
"""
import sys
import os

from ruzino_graph import RuzinoGraph


def get_binary_dir():
    """Get the binary directory path"""
    test_dir = os.path.dirname(os.path.abspath(__file__))
    binary_dir = os.path.join(test_dir, '..', '..', '..', '..', 'Binaries', 'Debug')
    return os.path.abspath(binary_dir)


def test_simple_branch():
    """Test creating a simple upward branch"""
    print("\n" + "="*70)
    print("TEST: Simple Tree Branch (No USD)")
    print("="*70)
    
    binary_dir = get_binary_dir()
    
    g = RuzinoGraph("SimpleBranchTest")
    
    # Load TreeGen nodes
    treegen_config = os.path.join(binary_dir, "Plugins", "TreeGen_geometry_nodes.json")
    g.loadConfiguration(treegen_config)
    print(f"✓ Loaded TreeGen configuration")
    
    # Create node
    branch = g.createNode("tree_simple_branch", name="branch")
    print(f"✓ Created node: {branch.ui_name}")
    
    # Set parameters
    inputs = {
        (branch, "Length"): 5.0,
        (branch, "Radius"): 0.2,
        (branch, "Subdivisions"): 10
    }
    print(f"✓ Set parameters: Length=5.0, Radius=0.2, Subdivisions=10")
    
    # Execute
    g.prepare_and_execute(inputs, required_node=branch)
    print(f"✓ Executed graph")
    
    print("\n" + "="*70)
    print("✅ TEST PASSED: Simple branch created successfully!")
    print("="*70)
