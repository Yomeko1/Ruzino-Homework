"""
Check the USD file's material reference
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

from pxr import Usd, UsdShade, Sdf

# Calculate path relative to script location
usd_file = os.path.join(binary_dir, "material_tests", "shader_ball_Aluminum.usdc")
stage = Usd.Stage.Open(usd_file)

material_prim = stage.GetPrimAtPath("/root/_materials/Aluminum")
print(f"Material prim: {material_prim}")
print(f"Material type: {material_prim.GetTypeName()}")

# Check references using stack
prim_stack = material_prim.GetPrimStack()
print(f"Prim stack:")
for spec in prim_stack:
    print(f"  Layer: {spec.layer.identifier}")
    if spec.referenceList.prependedItems:
        print(f"    Refs: {spec.referenceList.prependedItems}")

# Try to find what's actually in the mtlx file
assets_dir = os.path.abspath(os.path.join(tests_dir, "..", "..", "..", "..", "Assets"))
mtlx_file = os.path.join(assets_dir, "matx_library", "Aluminum_1k_8b_tAdaTTp", "Aluminum.mtlx")
print(f"\nOpening MaterialX file directly...")
mtlx_stage = Usd.Stage.Open(mtlx_file)
print(f"Root prims in mtlx:")
for prim in mtlx_stage.GetPseudoRoot().GetChildren():
    print(f"  {prim.GetPath()} ({prim.GetTypeName()})")
