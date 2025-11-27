import pytest

@pytest.fixture
def binary_dir():
    """Provide the binary directory path"""
    import os
    test_dir = os.path.dirname(os.path.abspath(__file__))
    binary_dir = os.path.join(test_dir, '..', '..', '..', '..', 'Binaries', 'Release')
    return os.path.abspath(binary_dir)
