"""
Script to read and print the structure of shader_ball.usdc
"""
import sys
import os

# Setup paths
tests_dir = os.path.dirname(os.path.abspath(__file__))
binary_dir = os.path.abspath(os.path.join(tests_dir, "..", "..", "..", "..", "Binaries", "Debug"))

# Set PXR_USD_WINDOWS_DLL_PATH so USD can find its DLLs
os.environ['PXR_USD_WINDOWS_DLL_PATH'] = binary_dir
print(f"Set PXR_USD_WINDOWS_DLL_PATH={binary_dir}")

# Add to Python path
sys.path.insert(0, binary_dir)

# Change to binary dir so DLLs can be loaded
os.chdir(binary_dir)
print(f"Changed working directory to: {os.getcwd()}")

from pxr import Usd, UsdGeom, UsdShade

def print_prim_info(prim, indent=0):
    """Print information about a prim and its children"""
    prefix = "  " * indent
    print(f"{prefix}{prim.GetPath()} ({prim.GetTypeName()})")
    
    # Print attributes
    for attr in prim.GetAttributes():
        if attr.Get() is not None:
            value = attr.Get()
            # Only print non-empty, simple values
            if isinstance(value, (int, float, str, bool)):
                print(f"{prefix}  - {attr.GetName()}: {value}")
    
    # Recursively print children
    for child in prim.GetChildren():
        print_prim_info(child, indent + 1)

def main():
    # Path to shader_ball.usdc - use absolute path
    usd_file = r"c:\Users\Pengfei\WorkSpace\Ruzino\Assets\shader_ball.usdc"
    
    print(f"Reading USD file: {usd_file}\n")
    
    # Open the stage
    stage = Usd.Stage.Open(usd_file)
    if not stage:
        print(f"Failed to open {usd_file}")
        return 1
    
    print("=" * 80)
    print("Stage Structure:")
    print("=" * 80)
    
    # Print the entire stage hierarchy
    print_prim_info(stage.GetPseudoRoot())
    
    print("\n" + "=" * 80)
    print("Mesh Information:")
    print("=" * 80)
    
    # Find all mesh prims
    meshes = []
    for prim in stage.Traverse():
        if prim.IsA(UsdGeom.Mesh):
            meshes.append(prim)
            mesh = UsdGeom.Mesh(prim)
            print(f"\nMesh: {prim.GetPath()}")
            print(f"  Type: {prim.GetTypeName()}")
            
            # Get face count
            face_count = mesh.GetFaceVertexCountsAttr().Get()
            if face_count:
                print(f"  Faces: {len(face_count)}")
            
            # Get point count
            points = mesh.GetPointsAttr().Get()
            if points:
                print(f"  Vertices: {len(points)}")
            
            # Check for material binding
            material_binding = UsdShade.MaterialBindingAPI(prim)
            bound_material = material_binding.ComputeBoundMaterial()[0]
            if bound_material:
                print(f"  Material: {bound_material.GetPath()}")
            else:
                print(f"  Material: None")
    
    print("\n" + "=" * 80)
    print(f"Summary: Found {len(meshes)} mesh(es)")
    print("=" * 80)
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
