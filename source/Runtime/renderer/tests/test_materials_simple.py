"""
Simple test to isolate the material binding issue
"""
import sys
import os

# Setup paths
tests_dir = os.path.dirname(os.path.abspath(__file__))
binary_dir = os.path.abspath(os.path.join(tests_dir, "..", "..", "..", "..", "Binaries", "Debug"))

os.environ['PXR_USD_WINDOWS_DLL_PATH'] = binary_dir
sys.path.insert(0, binary_dir)
os.chdir(binary_dir)

print(f"Working directory: {os.getcwd()}")

from pxr import Usd, UsdShade

# Test opening the MaterialX file
material_path = r"c:\Users\Pengfei\WorkSpace\Ruzino\Assets\matx_library\Acryl_Plastic_1k_8b_kylYFM6\Acryl_Plastic.mtlx"
print(f"\nOpening MaterialX file: {material_path}")

try:
    mtlx_stage = Usd.Stage.Open(material_path)
    if not mtlx_stage:
        print("ERROR: Failed to open MaterialX file")
        sys.exit(1)
    
    print(f"SUCCESS: Opened MaterialX stage")
    print(f"Root layer: {mtlx_stage.GetRootLayer().identifier}")
    
    # Find materials
    print("\nSearching for materials...")
    material_found = None
    prim_count = 0
    for prim in mtlx_stage.Traverse():
        prim_count += 1
        print(f"  Prim {prim_count}: {prim.GetPath()} - Type: {prim.GetTypeName()}")
        if prim.IsA(UsdShade.Material):
            material_found = prim
            print(f"  >>> Found Material!")
            break
        if prim_count > 20:  # Limit output
            print("  ... (more prims)")
            break
    
    if not material_found:
        print("ERROR: No material found in file")
        # Continue traversing to find it
        for prim in mtlx_stage.Traverse():
            if prim.IsA(UsdShade.Material):
                material_found = prim
                print(f"  Found material: {prim.GetPath()}")
                break
    
    if material_found:
        print(f"\nMaterial found at: {material_found.GetPath()}")
        material = UsdShade.Material(material_found)
        surface_output = material.GetSurfaceOutput()
        print(f"Surface output: {surface_output}")
        if surface_output:
            print(f"Getting connected sources...")
            try:
                connected = surface_output.GetConnectedSources()
                print(f"Connected sources: {connected}")
                print(f"Connected sources length: {len(connected)}")
            except Exception as e:
                print(f"ERROR getting connected sources: {e}")
        else:
            print("No surface output found")
    
    print("\nTest completed successfully!")
    
except Exception as e:
    print(f"\nERROR: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)
