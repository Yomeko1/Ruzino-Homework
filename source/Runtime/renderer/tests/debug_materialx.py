"""
Debug script to test MaterialX shader generation directly
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

from pxr import Usd, UsdShade, Sdf, UsdMtlx

# Load a MaterialX file - use relative path
assets_dir = os.path.abspath(os.path.join(tests_dir, "..", "..", "..", "..", "Assets"))
mtlx_path = os.path.join(assets_dir, "matx_library", "Acryl_Plastic_1k_8b_kylYFM6", "Acryl_Plastic.mtlx")
print(f"\nLoading MaterialX file: {mtlx_path}")

mtlx_stage = Usd.Stage.Open(mtlx_path)
if not mtlx_stage:
    print("ERROR: Failed to open MaterialX file")
    sys.exit(1)

# Find material
material_prim = None
for prim in mtlx_stage.Traverse():
    if prim.IsA(UsdShade.Material):
        material_prim = prim
        print(f"Found material: {prim.GetPath()}")
        break

if not material_prim:
    print("ERROR: No material found in file")
    sys.exit(1)

# Try creating a shader_ball with this material
shader_ball_path = os.path.join(assets_dir, "shader_ball.usdc")
print(f"\nLoading shader ball: {shader_ball_path}")

stage = Usd.Stage.Open(shader_ball_path)
if not stage:
    print("ERROR: Failed to open shader ball")
    sys.exit(1)

# Create a new stage for testing
test_stage = Usd.Stage.CreateNew(os.path.join(binary_dir, "debug_mtlx_test.usdc"))
print(f"\nCreated test stage: debug_mtlx_test.usdc")

# Copy the mesh from shader_ball
default_prim = test_stage.GetDefaultPrim() or test_stage.DefinePrim("/root", "Xform")
test_stage.SetDefaultPrim(default_prim)

# Find meshes in shader_ball
meshes = []
for prim in stage.Traverse():
    if prim.GetTypeName() == "Mesh":
        meshes.append(prim.GetPath())
        print(f"Found mesh: {prim.GetPath()}")

if not meshes:
    print("ERROR: No meshes found in shader_ball")
    sys.exit(1)

# Use first mesh
mesh_path = meshes[0]
print(f"\nCopying mesh: {mesh_path}")

# Copy mesh to test stage
source_prim = stage.GetPrimAtPath(mesh_path)
dest_path = Sdf.Path("/root/test_mesh")
Sdf.CopySpec(stage.GetRootLayer(), mesh_path, test_stage.GetRootLayer(), dest_path)

# Add material reference
material_path_in_test = Sdf.Path("/root/_materials/Test_Material")
material_def = UsdShade.Material.Define(test_stage, material_path_in_test)

# Add reference to MaterialX
ref_path = f"@{mtlx_path}@<{material_prim.GetPath()}>"
print(f"\nAdding material reference: {ref_path}")
material_def.GetPrim().GetReferences().AddReference(mtlx_path, material_prim.GetPath())

# Bind material to mesh
mesh_prim = test_stage.GetPrimAtPath(dest_path)
mesh_shade = UsdShade.MaterialBindingAPI(mesh_prim)
mesh_shade.Bind(material_def)

print(f"Material bound to mesh: {dest_path}")

# Copy camera and lights
for prim in stage.Traverse():
    if prim.GetTypeName() in ["Camera", "DomeLight", "DistantLight", "SphereLight"]:
        print(f"Copying: {prim.GetPath()}")
        Sdf.CopySpec(stage.GetRootLayer(), prim.GetPath(), test_stage.GetRootLayer(), Sdf.Path(f"/root/{prim.GetName()}"))

# Save
test_stage.Save()
print(f"\nSaved test stage")

# Try rendering with verbose output
render_exe = os.path.join(binary_dir, "headless_render.exe")
render_script = os.path.join(assets_dir, "render_nodes_save.json")
output_image = os.path.join(binary_dir, "debug_mtlx_test.png")

print(f"\n{'='*80}")
print(f"Testing render...")
print(f"Command: {render_exe}")
print(f"USD: {test_stage.GetRootLayer().identifier}")
print(f"Output: {output_image}")
print(f"{'='*80}\n")

import subprocess
result = subprocess.run(
    [render_exe, test_stage.GetRootLayer().identifier, render_script, output_image, "1920", "1080", "4"],
    capture_output=True,
    text=True
)

print("STDOUT:")
print(result.stdout)
print("\nSTDERR:")
print(result.stderr)
print(f"\nReturn code: {result.returncode}")

if result.returncode != 0:
    print(f"\n{'='*80}")
    print("RENDER FAILED!")
    print(f"{'='*80}")
    
    # Check if any shader was generated
    import glob
    shaders = glob.glob(os.path.join(binary_dir, "generated_shaders", "*.slang"))
    print(f"\nGenerated shaders: {len(shaders)}")
    for shader in shaders:
        print(f"  - {os.path.basename(shader)}")
else:
    print(f"\n{'='*80}")
    print("RENDER SUCCEEDED!")
    print(f"{'='*80}")
