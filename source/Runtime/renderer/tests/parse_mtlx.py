"""
Script to parse MaterialX file and find material names
"""
import xml.etree.ElementTree as ET

mtlx_file = r"c:\Users\Pengfei\WorkSpace\Ruzino\Assets\matx_library\Aluminum_1k_8b_tAdaTTp\Aluminum.mtlx"

tree = ET.parse(mtlx_file)
root = tree.getroot()

print(f"Root tag: {root.tag}")
print(f"Root attribs: {root.attrib}")
print("\nChildren:")

for child in root:
    print(f"  {child.tag}: name='{child.get('name')}', type='{child.get('type')}'")
    if child.tag == 'surfacematerial':
        print(f"    -> This is the material!")
