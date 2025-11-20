"""
Inspect the generated USD stage
"""
import sys
import os

# Setup paths
tests_dir = os.path.dirname(os.path.abspath(__file__))
binary_dir = os.path.abspath(os.path.join(tests_dir, "..", "..", "..", "..", "Binaries", "Debug"))

# Set PXR_USD_WINDOWS_DLL_PATH so USD can find its DLLs
os.environ['PXR_USD_WINDOWS_DLL_PATH'] = binary_dir
sys.path.insert(0, binary_dir)
os.chdir(binary_dir)

from pxr import Usd, UsdShade

stage = Usd.Stage.Open('debug_mtlx_test.usdc')

print("\n=== Stage Contents ===")
for prim in stage.Traverse():
    print(f"{prim.GetPath()}: {prim.GetTypeName()}")

print("\n=== Material Info ===")
mat = UsdShade.Material.Get(stage, '/root/_materials/Test_Material')
print(f"Material Valid: {bool(mat)}")

if mat:
    mat_prim = mat.GetPrim()
    print(f"Material Prim: {mat_prim.GetPath()}")
    print(f"Material Type: {mat_prim.GetTypeName()}")
    
    # Check for references
    if mat_prim.HasAuthoredReferences():
        print("Material has references")
    
    # Get child prims
    print("\nChild prims:")
    for child in mat_prim.GetChildren():
        print(f"  {child.GetPath()}: {child.GetTypeName()}")
    
    # Get network
    surface_output = mat.GetSurfaceOutput()
    print(f"\nSurface Output: {surface_output}")
    if surface_output:
        connections = surface_output.GetConnectedSources()
        print(f"Surface Connections: {connections}")

print("\n=== Mesh Material Binding ===")
mesh = stage.GetPrimAtPath('/root/test_mesh')
if mesh:
    binding_api = UsdShade.MaterialBindingAPI(mesh)
    bound_mat = binding_api.ComputeBoundMaterial()[0]
    print(f"Bound Material: {bound_mat.GetPath() if bound_mat else 'None'}")
