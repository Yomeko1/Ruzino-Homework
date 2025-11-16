"""
Debug test to check configuration loading
"""
import os
from ruzino_graph import RuzinoGraph


def test_config_paths():
    """Debug configuration paths"""
    print("\n" + "="*70)
    print("DEBUG: Check Configuration Paths")
    print("="*70)
    
    test_dir = os.path.dirname(os.path.abspath(__file__))
    print(f"Test dir: {test_dir}")
    
    binary_dir = os.path.join(test_dir, '..', '..', '..', '..', 'Binaries', 'Debug')
    binary_dir = os.path.abspath(binary_dir)
    print(f"Binary dir: {binary_dir}")
    
    print(f"Current working dir: {os.getcwd()}")
    
    geom_config = os.path.join(binary_dir, "geometry_nodes.json")
    print(f"Geom config path: {geom_config}")
    print(f"Geom config exists: {os.path.exists(geom_config)}")
    
    treegen_config = os.path.join(binary_dir, "Plugins", "TreeGen_geometry_nodes.json")
    print(f"TreeGen config path: {treegen_config}")
    print(f"TreeGen config exists: {os.path.exists(treegen_config)}")
    
    # Try relative path
    rel_geom = "geometry_nodes.json"
    print(f"\nRelative geom config: {rel_geom}")
    print(f"Relative exists: {os.path.exists(rel_geom)}")
    
    # List files in current dir
    print(f"\nFiles in current dir ending with .json:")
    for f in os.listdir('.'):
        if f.endswith('.json'):
            print(f"  {f}")
    
    print("\n" + "="*70)
