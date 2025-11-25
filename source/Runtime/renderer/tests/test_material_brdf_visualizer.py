#!/usr/bin/env python3
"""
Material BRDF Visualizer Test

Analyzes material BRDF properties:
1. BRDF Eval distribution
2. PDF distribution  
3. Sample frequency distribution

Outputs three visualization images for analysis.
"""

import sys
import os
from pathlib import Path
import numpy as np

# Setup paths
script_dir = Path(__file__).parent.resolve()
workspace_root = script_dir.parent.parent.parent.parent
binary_dir = workspace_root / "Binaries" / "Debug"
assets_dir = workspace_root / "Assets"

# Set environment variables
os.environ['PXR_USD_WINDOWS_DLL_PATH'] = str(binary_dir)
mtlx_stdlib = binary_dir / "libraries"
if mtlx_stdlib.exists():
    os.environ['PXR_MTLX_STDLIB_SEARCH_PATHS'] = str(mtlx_stdlib)
os.environ['PATH'] = str(binary_dir) + os.pathsep + os.environ.get('PATH', '')

if hasattr(os, 'add_dll_directory'):
    os.add_dll_directory(str(binary_dir))

os.chdir(str(binary_dir))
sys.path.insert(0, str(binary_dir))

# Import USD first
from pxr import Usd, UsdGeom, UsdShade, Sdf, UsdMtlx

# Import modules
import hd_USTC_CG_py as renderer
import nodes_core_py as core
import nodes_system_py as system

def set_node_inputs(executor, inputs_dict):
    """Helper to set multiple node inputs."""
    for (node, socket_name), value in inputs_dict.items():
        socket = node.get_input_socket(socket_name)
        if socket is None:
            raise ValueError(f"Socket '{socket_name}' not found on node '{node.ui_name}'")
        meta_value = core.to_meta_any(value)
        executor.sync_node_from_external_storage(socket, meta_value)

def bind_material_to_shader_ball(shader_ball_path, material_path, output_path):
    """Bind a MaterialX material to shader_ball meshes"""
    stage = Usd.Stage.Open(str(shader_ball_path))
    if not stage:
        return False
    
    # Find meshes
    preview_mesh = stage.GetPrimAtPath("/root/Preview_Mesh/Preview_Mesh")
    calibration_mesh = stage.GetPrimAtPath("/root/Calibration_Mesh/Calibration_Mesh")
    
    if not preview_mesh or not calibration_mesh:
        print(f"✗ Could not find meshes in shader_ball")
        return False
    
    # Create materials scope
    materials_scope_path = "/root/_materials"
    materials_scope = stage.GetPrimAtPath(materials_scope_path)
    if not materials_scope:
        materials_scope = stage.DefinePrim(materials_scope_path, "Scope")
    
    # Get material name
    material_name = material_path.stem
    material_path_in_stage = f"{materials_scope_path}/{material_name}"
    
    # Remove old material if exists
    if stage.GetPrimAtPath(material_path_in_stage):
        stage.RemovePrim(material_path_in_stage)
    
    # Open MaterialX file to find material
    mtlx_stage = Usd.Stage.Open(str(material_path))
    if not mtlx_stage:
        print(f"✗ Could not open MaterialX file: {material_path}")
        return False
    
    mtlx_material_path = None
    for prim in mtlx_stage.Traverse():
        if prim.IsA(UsdShade.Material):
            mtlx_material_path = prim.GetPath()
            break
    
    if not mtlx_material_path:
        print(f"✗ No material found in {material_path}")
        return False
    
    # Create material prim and reference the .mtlx file
    material_prim = stage.DefinePrim(material_path_in_stage, "Material")
    material_prim.GetReferences().AddReference(str(material_path), mtlx_material_path)
    material = UsdShade.Material(material_prim)
    
    # Connect surface output
    shader_prim = None
    for child in material_prim.GetChildren():
        if child.IsA(UsdShade.Shader):
            shader_prim = child
            break
    
    if shader_prim:
        shader = UsdShade.Shader(shader_prim)
        shader_surface_output = shader.GetOutput("surface")
        if shader_surface_output:
            material_surface_output = material.CreateSurfaceOutput()
            material_surface_output.ConnectToSource(shader_surface_output)
    
    # Bind material to meshes
    UsdShade.MaterialBindingAPI(preview_mesh).Bind(material)
    UsdShade.MaterialBindingAPI(calibration_mesh).Bind(material)
    
    # Save
    stage.GetRootLayer().Export(str(output_path))
    print(f"✓ Material bound and saved to: {output_path}")
    return True

# Test configuration
RESOLUTION = 512
NUM_SAMPLES = 1000000  # 1M samples for better statistics

print("="*70)
print("Material BRDF Analyzer")
print("="*70)

# Bind material to shader_ball
shader_ball_path = assets_dir / "shader_ball.usdc"
material_path = assets_dir / "matx_library" / "Aluminum_1k_8b_tAdaTTp" / "Aluminum.mtlx"
output_usd = binary_dir / "shader_ball_with_material.usdc"

print(f"Shader ball: {shader_ball_path}")
print(f"Material: {material_path.name}")
print(f"Material exists: {material_path.exists()}")

if not bind_material_to_shader_ball(shader_ball_path, material_path, output_usd):
    print("✗ Failed to bind material")
    exit(1)

print()

# Create HydraRenderer with material-bound USD scene
print(f"Loading USD stage: {output_usd}")
try:
    # Use same resolution as BRDF analyzer
    hydra = renderer.HydraRenderer(str(output_usd), width=RESOLUTION, height=RESOLUTION)
    print("✓ HydraRenderer created successfully")
except Exception as e:
    print(f"✗ Failed to create HydraRenderer: {e}")
    import traceback
    traceback.print_exc()
    exit(1)

# Get node system (already initialized by HydraRenderer)
node_system = hydra.get_node_system()

# Load render nodes configuration if not already loaded
config_path = binary_dir / "render_nodes.json"
if config_path.exists():
    print(f"Loading render nodes configuration from: {config_path}")
    try:
        node_system.load_configuration(str(config_path))
        print("✓ Configuration loaded successfully")
    except Exception as e:
        print(f"✗ Failed to load configuration: {e}")
        exit(1)
else:
    print(f"✗ Configuration file not found: {config_path}")
    exit(1)

tree = node_system.get_node_tree()
executor = node_system.get_node_tree_executor()

print(f"Available node types: {tree.get_all_node_types() if hasattr(tree, 'get_all_node_types') else 'N/A'}")
print(f"Current node count: {len(tree.nodes) if hasattr(tree, 'nodes') else 'N/A'}")

# Render a frame first to initialize materials
print("Rendering initialization frame...")
try:
    hydra.render()
    print("✓ Frame rendered, materials should be loaded")
except Exception as e:
    print(f"✗ Failed to render: {e}")

print()

# Create BRDF analyzer node
try:
    analyzer = tree.add_node("material_brdf_analyzer")
    print(f"✓ Successfully created analyzer node")
except Exception as e:
    print(f"✗ Failed to create analyzer node: {e}")
    exit(1)
analyzer.ui_name = "BRDFAnalyzer"

# Create present nodes for all four outputs
present_eval = tree.add_node("present_color")
present_eval.ui_name = "PresentEval"
present_pdf = tree.add_node("present_color")
present_pdf.ui_name = "PresentPDF"
present_sample = tree.add_node("present_color")
present_sample.ui_name = "PresentSample"
present_importance = tree.add_node("present_color")
present_importance.ui_name = "PresentImportance"

# Connect analyzer outputs to present nodes
tree.add_link(analyzer.get_output_socket("BRDF Eval"), present_eval.get_input_socket("Color"))
tree.add_link(analyzer.get_output_socket("PDF"), present_pdf.get_input_socket("Color"))
tree.add_link(analyzer.get_output_socket("Sample Distribution"), present_sample.get_input_socket("Color"))
tree.add_link(analyzer.get_output_socket("Importance Test"), present_importance.get_input_socket("Color"))

# Set analysis parameters (common for all four analyses)
# Use 45-degree angle to see broader distribution  
import math
angle = math.radians(45)
incident_dir_x, incident_dir_y, incident_dir_z = math.sin(angle), 0.0, math.cos(angle)  # 45 degrees from normal

# Normalize incident direction
incident_dir_length = np.sqrt(incident_dir_x**2 + incident_dir_y**2 + incident_dir_z**2)
if incident_dir_length > 0:
    incident_dir_x /= incident_dir_length
    incident_dir_y /= incident_dir_length
    incident_dir_z /= incident_dir_length
uv_x, uv_y = 0.5, 0.5

inputs = {
    (analyzer, "Incident Direction X"): incident_dir_x,
    (analyzer, "Incident Direction Y"): incident_dir_y,
    (analyzer, "Incident Direction Z"): incident_dir_z,
    (analyzer, "UV X"): uv_x,
    (analyzer, "UV Y"): uv_y,
    (analyzer, "Material ID"): 0,
    (analyzer, "Resolution"): RESOLUTION,
    (analyzer, "Num Samples"): NUM_SAMPLES,
}

print(f"Analysis parameters:")
print(f"  Incident direction: ({incident_dir_x}, {incident_dir_y}, {incident_dir_z})")
print(f"  Resolution: {RESOLUTION}x{RESOLUTION}")
print(f"  Number of samples: {NUM_SAMPLES}")
print()

# Execute analysis (all four at once)
print("Computing BRDF analysis (Eval + PDF + Sample Distribution + Importance Test)...")
try:
    executor.reset_allocator()
    # Use present_eval as root, but it will execute the whole tree
    executor.prepare_tree(tree, present_eval)
    set_node_inputs(executor, inputs)
    hydra.render()
    print("✓ Rendering complete")
except Exception as e:
    print(f"✗ Failed during rendering: {e}")
    import traceback
    traceback.print_exc()
    exit(1)

# Retrieve all four outputs
print("\n Retrieving outputs...")
try:
    brdf_eval_data = hydra.get_output_texture("PresentEval")
    eval_array = np.array(brdf_eval_data, dtype=np.float32).reshape(RESOLUTION, RESOLUTION, 4)
    print(f"✓ BRDF Eval: mean={eval_array[:,:,:3].mean():.6f}, max={eval_array[:,:,:3].max():.6f}")
    
    pdf_data = hydra.get_output_texture("PresentPDF")
    pdf_array = np.array(pdf_data, dtype=np.float32).reshape(RESOLUTION, RESOLUTION, 4)
    print(f"✓ PDF: mean={pdf_array[:,:,:3].mean():.6f}, max={pdf_array[:,:,:3].max():.6f}")
    
    sample_data = hydra.get_output_texture("PresentSample")
    sample_array_raw = np.array(sample_data, dtype=np.float32).reshape(RESOLUTION, RESOLUTION, 4)
    # Normalize sample distribution in Python: divide by num_samples
    # The GPU output is raw sample counts, we normalize here for probability density
    sample_array = sample_array_raw.copy()
    sample_array[:,:,:3] = sample_array_raw[:,:,:3] / NUM_SAMPLES
    print(f"✓ Sample Distribution: mean={sample_array[:,:,:3].mean():.6f}, max={sample_array[:,:,:3].max():.6f}")
    
    importance_data = hydra.get_output_texture("PresentImportance")
    importance_array = np.array(importance_data, dtype=np.float32).reshape(RESOLUTION, RESOLUTION, 4)
    print(f"✓ Importance Test: mean={importance_array[:,:,:3].mean():.6f}, max={importance_array[:,:,:3].max():.6f}")
except Exception as e:
    print(f"✗ Failed to retrieve textures: {e}")
    import traceback
    traceback.print_exc()
    exit(1)
print()

# Save images and perform statistical analysis
try:
    from PIL import Image
    
    def save_texture(array, filename, log_scale=False):
        rgb = array[:, :, :3]
        
        # Save EXR with original HDR values
        exr_filename = filename.replace('.png', '.exr')
        rgb_exr = np.flipud(rgb.astype(np.float32))
        try:
            import imageio
            imageio.imwrite(exr_filename, rgb_exr, format='EXR')
            print(f"✓ Saved EXR: {exr_filename}")
        except Exception as e:
            print(f"⚠ Could not save EXR (install imageio-ffmpeg): {e}")
        
        # Save PNG with tone mapping for visualization
        if log_scale or True:  # Always use log scale for better visualization
            # Use log scale for BRDF/PDF values
            rgb_vis = np.log1p(np.clip(rgb, 0, None))  # log(1+x) to handle 0s
            rgb_vis = rgb_vis / (rgb_vis.max() + 1e-8)  # Normalize to [0,1]
        else:
            rgb_vis = np.clip(rgb, 0, 1)
        
        img_uint8 = (rgb_vis * 255).astype(np.uint8)
        img_uint8 = np.flipud(img_uint8)
        Image.fromarray(img_uint8).save(filename)
        print(f"✓ Saved PNG: {filename}")
    
    save_texture(eval_array, "./brdf_eval.png", log_scale=True)
    save_texture(pdf_array, "./brdf_pdf.png", log_scale=True)
    save_texture(sample_array, "./brdf_sample_distribution.png", log_scale=True)  # Now use log scale
    save_texture(importance_array, "./brdf_importance_test.png", log_scale=True)  # Now use log scale
    
    # ===== Statistical Analysis =====
    print("\n" + "="*70)
    print("Statistical Analysis")
    print("="*70)
    
    # Extract single channel values (they're stored as RGB with same values)
    eval_values = eval_array[:, :, 0]
    pdf_values = pdf_array[:, :, 0]
    sample_values = sample_array[:, :, 0]
    importance_values = importance_array[:, :, 0]
    
    # Only consider valid regions (inside hemisphere projection)
    # Create mask for valid pixels
    height, width = eval_values.shape
    y_coords, x_coords = np.meshgrid(np.arange(height), np.arange(width), indexing='ij')
    u = (x_coords + 0.5) / width
    v = (y_coords + 0.5) / height
    p = np.stack([u * 2 - 1, v * 2 - 1], axis=-1)
    r = np.sqrt(p[:,:,0]**2 + p[:,:,1]**2)
    valid_mask = r <= 1.0
    
    eval_valid = eval_values[valid_mask]
    pdf_valid = pdf_values[valid_mask]
    sample_valid = sample_values[valid_mask]
    importance_valid = importance_values[valid_mask]
    
    print(f"\nValid pixels: {valid_mask.sum()} / {height*width}")
    
    # Print distribution statistics for debugging
    print(f"\nDistribution Analysis:")
    print(f"  BRDF Eval: non-zero pixels = {np.count_nonzero(eval_valid)}/{len(eval_valid)}")
    print(f"             percentiles [0, 25, 50, 75, 95, 100] = {np.percentile(eval_valid, [0, 25, 50, 75, 95, 100])}")
    print(f"  PDF: non-zero pixels = {np.count_nonzero(pdf_valid)}/{len(pdf_valid)}")
    print(f"       percentiles [0, 25, 50, 75, 95, 100] = {np.percentile(pdf_valid, [0, 25, 50, 75, 95, 100])}")
    print(f"  Sample: non-zero pixels = {np.count_nonzero(sample_valid)}/{len(sample_valid)}")
    print(f"          percentiles [0, 25, 50, 75, 95, 100] = {np.percentile(sample_valid, [0, 25, 50, 75, 95, 100])}")
    sample_sum = np.sum(sample_valid)
    print(f"          sum = {sample_sum:.6f} (should be ~1.0 for normalized frequency)")
    print()
    
    # 1. Compare sample frequency with PDF
    print("\n1. Sample Frequency vs PDF Comparison:")
    print(f"   PDF mean: {pdf_valid.mean():.6f}, std: {pdf_valid.std():.6f}")
    print(f"   Sample mean: {sample_valid.mean():.6f}, std: {sample_valid.std():.6f}")
    
    # Compute correlation
    if len(pdf_valid) > 0 and pdf_valid.std() > 0 and sample_valid.std() > 0:
        correlation = np.corrcoef(pdf_valid, sample_valid)[0, 1]
        print(f"   Correlation: {correlation:.6f}")
        
        # Compute normalized error
        pdf_norm = pdf_valid / (pdf_valid.mean() + 1e-8)
        sample_norm = sample_valid / (sample_valid.mean() + 1e-8)
        rel_error = np.abs(pdf_norm - sample_norm)
        print(f"   Mean relative error: {rel_error.mean():.6f}")
        print(f"   Max relative error: {rel_error.max():.6f}")
    
    # 2. Variance analysis of sample distribution
    print("\n2. Sample Distribution Variance Analysis:")
    print(f"   Variance: {sample_valid.var():.6f}")
    print(f"   Coefficient of variation: {sample_valid.std() / (sample_valid.mean() + 1e-8):.6f}")
    
    # Check uniformity - for ideal importance sampling, variance should be low
    # Expected value for uniform distribution over N bins
    expected_mean = sample_valid.mean()
    chi_square = np.sum((sample_valid - expected_mean)**2 / (expected_mean + 1e-8))
    print(f"   Chi-square statistic: {chi_square:.2f}")
    
    # 3. Compare BRDF eval with PDF
    print("\n3. BRDF Eval vs PDF Comparison:")
    print(f"   BRDF mean: {eval_valid.mean():.6f}, std: {eval_valid.std():.6f}")
    
    if len(eval_valid) > 0 and eval_valid.std() > 0 and pdf_valid.std() > 0:
        correlation_eval_pdf = np.corrcoef(eval_valid, pdf_valid)[0, 1]
        print(f"   Correlation BRDF-PDF: {correlation_eval_pdf:.6f}")
    
    # 4. Integration check - PDF should integrate to ~1
    print("\n4. Integration Tests:")
    # For equal-area projection, each pixel represents equal solid angle
    # Solid angle element: dω = 2π * pixel_area (for equal-area projection)
    pixel_area = 1.0 / (height * width)  # in texture space
    solid_angle_per_pixel = 2.0 * np.pi * pixel_area  # Jacobian = 2π
    
    pdf_integral = pdf_valid.sum() * pixel_area
    sample_integral = sample_valid.sum() * pixel_area
    
    print(f"   PDF integral (texture space): {pdf_integral:.6f}")
    print(f"   Sample integral (normalized frequency): {sample_integral:.6f} (should be ~1.0)")
    print(f"   Ratio (Sample/PDF): {sample_integral / (pdf_integral + 1e-8):.6f}")
    
    # 5. Importance sampling test - should be approximately uniform
    print("\n5. Importance Sampling Test (throughput/pdf):")
    importance_nonzero = importance_valid[importance_valid > 0]
    if len(importance_nonzero) > 0:
        importance_percentiles = np.percentile(importance_nonzero, [0, 25, 50, 75, 95, 100])
        importance_mean = importance_nonzero.mean()
        importance_std = importance_nonzero.std()
        importance_cv = importance_std / (importance_mean + 1e-8)
        
        print(f"   Non-zero samples: {len(importance_nonzero)}/{len(importance_valid)}")
        print(f"   Mean: {importance_mean:.6f}")
        print(f"   Std: {importance_std:.6f}")
        print(f"   Coefficient of variation: {importance_cv:.6f} (lower is better, <0.5 is good)")
        print(f"   Percentiles [0, 25, 50, 75, 95, 100]: {importance_percentiles}")
        print(f"   Range: [{importance_nonzero.min():.6f}, {importance_nonzero.max():.6f}]")
        print(f"   → For good importance sampling, CV should be low and distribution tight")
    else:
        print(f"   No valid samples")
    
    # Create comparison plots if matplotlib available
    try:
        import matplotlib.pyplot as plt
        
        fig, axes = plt.subplots(2, 3, figsize=(18, 10))
        
        # Plot 1: Sample vs PDF scatter
        axes[0, 0].scatter(pdf_valid, sample_valid, alpha=0.3, s=1)
        axes[0, 0].plot([pdf_valid.min(), pdf_valid.max()], 
                       [pdf_valid.min(), pdf_valid.max()], 'r--', label='y=x')
        axes[0, 0].set_xlabel('PDF')
        axes[0, 0].set_ylabel('Sample Frequency')
        axes[0, 0].set_title('Sample Frequency vs PDF')
        axes[0, 0].legend()
        axes[0, 0].grid(True, alpha=0.3)
        
        # Plot 2: BRDF vs PDF scatter
        axes[0, 1].scatter(pdf_valid, eval_valid, alpha=0.3, s=1)
        axes[0, 1].set_xlabel('PDF')
        axes[0, 1].set_ylabel('BRDF Eval')
        axes[0, 1].set_title('BRDF Eval vs PDF')
        axes[0, 1].grid(True, alpha=0.3)
        
        # Plot 3: Histograms
        axes[1, 0].hist(pdf_valid, bins=50, alpha=0.5, label='PDF', density=True)
        axes[1, 0].hist(sample_valid, bins=50, alpha=0.5, label='Sample', density=True)
        axes[1, 0].set_xlabel('Value')
        axes[1, 0].set_ylabel('Density')
        axes[1, 0].set_title('Distribution Histograms')
        axes[1, 0].legend()
        axes[1, 0].grid(True, alpha=0.3)
        
        # Plot 4: Error distribution
        if len(pdf_valid) > 0:
            error = sample_valid - pdf_valid
            axes[1, 1].hist(error, bins=50, alpha=0.7)
            axes[1, 1].axvline(0, color='r', linestyle='--', label='Zero error')
            axes[1, 1].set_xlabel('Error (Sample - PDF)')
            axes[1, 1].set_ylabel('Frequency')
            axes[1, 1].set_title(f'Error Distribution (mean={error.mean():.6f})')
            axes[1, 1].legend()
            axes[1, 1].grid(True, alpha=0.3)
        
        # Plot 5: Importance sampling histogram (should be uniform)
        axes[0, 2].hist(importance_valid, bins=50, alpha=0.7, color='green')
        axes[0, 2].axvline(importance_valid.mean(), color='r', linestyle='--', label=f'Mean={importance_valid.mean():.4f}')
        axes[0, 2].set_xlabel('Throughput / PDF')
        axes[0, 2].set_ylabel('Frequency')
        axes[0, 2].set_title('Importance Sampling Test (should be uniform)')
        axes[0, 2].legend()
        axes[0, 2].grid(True, alpha=0.3)
        
        # Plot 6: Importance sampling spatial distribution
        im = axes[1, 2].imshow(importance_values, cmap='viridis', origin='lower')
        axes[1, 2].set_title(f'Importance Test (CV={importance_valid.std()/importance_valid.mean():.4f})')
        axes[1, 2].set_xlabel('X')
        axes[1, 2].set_ylabel('Y')
        plt.colorbar(im, ax=axes[1, 2])
        
        plt.tight_layout()
        plt.savefig('./brdf_analysis_plots.png', dpi=150)
        print(f"\n✓ Saved: ./brdf_analysis_plots.png")
        plt.close()
        
    except ImportError:
        print("\n✗ matplotlib not available, skipping plots")
    
except ImportError:
    print("✗ PIL not available, skipping image save")

print()
print("="*70)
print("✓ BRDF Analysis Complete!")
print("="*70)

