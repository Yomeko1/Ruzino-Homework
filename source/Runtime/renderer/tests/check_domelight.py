"""
Check dome light configuration in shader_ball
"""
import sys
import os

# Setup paths
tests_dir = os.path.dirname(os.path.abspath(__file__))
binary_dir = os.path.abspath(os.path.join(tests_dir, "..", "..", "..", "..", "Binaries", "Debug"))

os.environ['PXR_USD_WINDOWS_DLL_PATH'] = binary_dir
sys.path.insert(0, binary_dir)
os.chdir(binary_dir)

from pxr import Usd, UsdLux

# Calculate path relative to script location
assets_dir = os.path.abspath(os.path.join(tests_dir, "..", "..", "..", "..", "Assets"))
shader_ball_path = os.path.join(assets_dir, "shader_ball.usdc")
stage = Usd.Stage.Open(shader_ball_path)

print("\n=== Dome Lights ===")
for prim in stage.Traverse():
    if prim.GetTypeName() == "DomeLight":
        print(f"\nDome Light: {prim.GetPath()}")
        light = UsdLux.DomeLight(prim)
        
        # Get texture file
        texture_file_attr = light.GetTextureFileAttr()
        if texture_file_attr:
            texture_path = texture_file_attr.Get()
            print(f"  Texture File: {texture_path}")
            print(f"  Type: {type(texture_path)}")
            
            # Try to resolve it
            if hasattr(texture_path, 'path'):
                print(f"  Asset Path: {texture_path.path}")
                resolved = stage.ResolveIdentifierToEditTarget(texture_path.path)
                print(f"  Resolved: {resolved}")
        
        # Get all attributes
        print(f"  All attributes:")
        for attr in prim.GetAttributes():
            if "texture" in attr.GetName().lower() or "file" in attr.GetName().lower():
                print(f"    {attr.GetName()}: {attr.Get()}")
