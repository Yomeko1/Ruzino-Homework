from pxr import UsdGeom, Gf, Sdf, UsdShade
import math

# Get the USD stage (global stage object is already available)
usd_stage = stage.get_pxr_stage()

# Clean up existing spheres from previous runs
spheres_path = Sdf.Path("/Spheres")
existing_prim = usd_stage.GetPrimAtPath(spheres_path)
if existing_prim:
    usd_stage.RemovePrim(spheres_path)
    print("Cleaned up existing spheres")

# Create materials scope
materials_path = Sdf.Path("/Materials")
materials_prim = usd_stage.GetPrimAtPath(materials_path)
if not materials_prim:
    UsdGeom.Scope.Define(usd_stage, materials_path)

# Create 10x10 grid of spheres with animation
radius = 0.5
base_z_height = 2.0  # Average height at z=2
z_amplitude = 1.0    # Amplitude of oscillation (±1)
spacing = 1.5        # Distance between sphere centers

# Animation parameters
animation_duration = 120  # frames
period = 60           # frames per complete oscillation

# Calculate grid offset to center it
grid_size = 10
grid_offset = (grid_size - 1) * spacing / 2.0

for i in range(grid_size):
    for j in range(grid_size):
        # Calculate base position
        x = i * spacing - grid_offset
        y = j * spacing - grid_offset
        
        # Create sphere
        sphere_path = f"/Spheres/Sphere_{i}_{j}"
        sphere = UsdGeom.Sphere.Define(usd_stage, sphere_path)
        sphere.CreateRadiusAttr(radius)
        
        # Get sphere prim for transforms
        sphere_prim = usd_stage.GetPrimAtPath(sphere_path)
        xformable = UsdGeom.Xformable(sphere_prim)
        
        # Create unique material for this sphere
        material_path = f"/Materials/Material_{i}_{j}"
        material = UsdShade.Material.Define(usd_stage, material_path)
        
        # Create UsdPreviewSurface shader
        shader = UsdShade.Shader.Define(usd_stage, f"{material_path}/PreviewSurface")
        shader.CreateIdAttr("UsdPreviewSurface")
        
        # Create diffuse color input
        diffuse_input = shader.CreateInput("diffuseColor", Sdf.ValueTypeNames.Color3f)
        
        shader.CreateInput("roughness", Sdf.ValueTypeNames.Float).Set(1.0)
        
        # Create output
        shader.CreateOutput("surface", Sdf.ValueTypeNames.Token)
        
        # Connect material output
        material.CreateSurfaceOutput().ConnectToSource(shader.GetOutput("surface"))
        
        # Bind material to sphere
        UsdShade.MaterialBindingAPI(sphere_prim).Bind(material)
        
        # Add animation keyframes
        translate_op = xformable.AddTranslateOp()
        
        for frame in range(animation_duration + 1):
            # Normalize time to 0-1 range
            t = frame / animation_duration
            
            # Wave phase offset based on position (creates wave effect)
            phase_offset = 2.0 * math.pi * (i + j) / (2.0 * grid_size)
            
            # Calculate oscillating z height with wave effect
            z = base_z_height + z_amplitude * math.sin(2.0 * math.pi * t + phase_offset)
            
            # Set position with time sampling
            translate_op.Set(Gf.Vec3d(x, y, z), Sdf.TimeCode(frame))
            
            # Calculate color that changes over time
            hue_shift = t * 2.0 * math.pi  # Full color cycle over animation
            r = 0.5 + 0.5 * math.sin(hue_shift + phase_offset)
            g = 0.5 + 0.5 * math.sin(hue_shift + phase_offset + 2.0 * math.pi / 3.0)
            b = 0.5 + 0.5 * math.sin(hue_shift + phase_offset + 4.0 * math.pi / 3.0)
            
            # Set color with time sampling
            diffuse_input.Set(Gf.Vec3f(r, g, b), Sdf.TimeCode(frame))

print(f"Created 100 animated colored spheres (10x10 grid) with wave motion and color animation")
