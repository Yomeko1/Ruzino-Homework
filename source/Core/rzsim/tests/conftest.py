"""
Pytest configuration for BPM plugin tests.

This configuration ensures that:
1. The working directory is changed to Binaries/Release so DLLs can be loaded
2. The binary directory is added to Python path for compiled modules
"""

import sys
import os

# Set up the environment BEFORE pytest starts importing test modules
# This must be done at module level, not in a fixture

# Determine directories
# From Plugins/BPM/tests/ to binary directory
tests_dir = os.path.dirname(os.path.abspath(__file__))
binary_dir = os.path.abspath(os.path.join(tests_dir, "..", "..", "..", "..", "Binaries", "Release"))

# Save original directory
_original_cwd = os.getcwd()

# Change working directory to binary_dir
# This is required so that BPM DLLs can be loaded correctly
if os.path.exists(binary_dir):
    os.chdir(binary_dir)

# Add binary directory to Python path for compiled Python modules
if binary_dir not in sys.path:
    sys.path.insert(0, binary_dir)
