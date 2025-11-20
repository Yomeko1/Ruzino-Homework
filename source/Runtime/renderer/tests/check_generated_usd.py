"""
Script to check the generated USD file with MaterialX bindings
"""
import sys
import os

# Setup paths
tests_dir = os.path.dirname(os.path.abspath(__file__))
binary_dir = os.path.abspath(os.path.join(tests_dir, "..", "..", "..", "..", "Binaries", "Debug"))

# Set PXR_USD_WINDOWS_DLL_PATH so USD can find its DLLs
os.environ['PXR_USD_WINDOWS_DLL_PATH'] = binary_dir

# Add to Python path
sys.path.insert(0, binary_dir)

# Change to binary dir so DLLs can be loaded
os.chdir(binary_dir)

from pxr import Usd, UsdGeom, UsdShade

def check_usd(usd_file):
    """Check the structure of a USD file"""
    stage = Usd.Stage.Open(usd_file)
    if not stage:
        print(f"Failed to open {usd_file}")
        return
    
    print(f"=== Checking {usd_file} ===\n")
    
    # Find all materials
    print("Materials:")
    for prim in stage.Traverse():
        if prim.IsA(UsdShade.Material):
            material = UsdShade.Material(prim)
            print(f"  Material: {prim.GetPath()}")
            print(f"    Type: {prim.GetTypeName()}")
            
            # Check surface output
            surface_output = material.GetSurfaceOutput()
            if surface_output:
                print(f"    Surface output: {surface_output.GetAttr().GetPath()}")
                sources = surface_output.GetConnectedSources()
                if sources and len(sources) > 0 and len(sources[0]) > 0:
                    source_info = sources[0][0]
                    print(f"    Connected to: {source_info.source.GetPath()}")
                else:
                    print(f"    Not connected")
            
            # List children
            print(f"    Children:")
            for child in prim.GetChildren():
                print(f"      - {child.GetPath()} ({child.GetTypeName()})")
                if child.IsA(UsdShade.Shader):
                    shader = UsdShade.Shader(child)
                    print(f"        ID: {shader.GetIdAttr().Get()}")
                    for output in shader.GetOutputs():
                        print(f"        Output: {output.GetFullName()}")
            print()
    
    # Check mesh bindings
    print("\nMesh Bindings:")
    for prim in stage.Traverse():
        if prim.IsA(UsdGeom.Mesh):
            binding_api = UsdShade.MaterialBindingAPI(prim)
            bound_material = binding_api.ComputeBoundMaterial()[0]
            if bound_material:
                print(f"  Mesh: {prim.GetPath()}")
                print(f"    Bound to: {bound_material.GetPath()}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: check_generated_usd.py <usd_file>")
        sys.exit(1)
    
    check_usd(sys.argv[1])
