"""
Script to parse MaterialX file and find material names
"""
import os
import xml.etree.ElementTree as ET

# Calculate path relative to script location
script_dir = os.path.dirname(os.path.abspath(__file__))
assets_dir = os.path.abspath(os.path.join(script_dir, "..", "..", "..", "..", "Assets"))
mtlx_file = os.path.join(assets_dir, "matx_library", "Aluminum_1k_8b_tAdaTTp", "Aluminum.mtlx")

tree = ET.parse(mtlx_file)
root = tree.getroot()

print(f"Root tag: {root.tag}")
print(f"Root attribs: {root.attrib}")
print("\nChildren:")

for child in root:
    print(f"  {child.tag}: name='{child.get('name')}', type='{child.get('type')}'")
    if child.tag == 'surfacematerial':
        print(f"    -> This is the material!")
