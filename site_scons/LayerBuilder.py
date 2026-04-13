"""
Layer builder helper for SCons build system.

Provides BuildLayer() to compile a source layer into a static library
with strict include path enforcement for adjacent-only dependencies.
"""

import os
import logging

logger = logging.getLogger('StonerBuild.LayerBuilder')

_SOURCE_ROOT = 'Source'


def _GetPublicIncludePath(layer_name):
    """Get the public include path for a standard layer.

    Args:
        layer_name: Layer name (e.g., 'Core', 'RHI').

    Returns:
        str: Relative path to the layer's Public/ directory.
    """
    return os.path.join(_SOURCE_ROOT, layer_name, 'Public')


def BuildLayer(env, layer_name, dependencies, source_dir=None, public_dir=None):
    """Build a source layer as a static library with dependency-controlled include paths.

    Clones the environment, sets CPPPATH to only the permitted dependencies'
    Public/ directories (enforcing adjacent-only layer isolation at compile time),
    globs all .cpp files from the Private/ directory, and returns a StaticLibrary.

    Args:
        env: Parent SCons Environment (will be cloned).
        layer_name: Name for the output library (e.g., 'Core', 'VulkanRHI').
        dependencies: List of layer names this layer is allowed to depend on.
        source_dir: Override source directory (default: Source/{layer_name}/Private).
        public_dir: Override public include directory (default: Source/{layer_name}/Public).

    Returns:
        SCons StaticLibrary node, or None if no source files found.
    """
    layer_env = env.Clone()

    if public_dir is None:
        public_dir = os.path.join(_SOURCE_ROOT, layer_name, 'Public')
    if source_dir is None:
        source_dir = os.path.join(_SOURCE_ROOT, layer_name, 'Private')

    # Build include path list: own public dir + permitted dependency public dirs
    include_paths = [Dir(public_dir).srcnode().abspath]
    for dep in dependencies:
        dep_public = _GetPublicIncludePath(dep)
        include_paths.append(Dir(dep_public).srcnode().abspath)

    layer_env.Append(CPPPATH=include_paths)

    # Glob source files from Private/ directory
    sources = Glob(os.path.join(source_dir, '*.cpp'))

    if not sources:
        logger.warning("Layer '%s': no .cpp files found in %s", layer_name, source_dir)
        return None

    lib = layer_env.StaticLibrary(layer_name, sources)
    logger.info("Layer '%s': building from %d source file(s)", layer_name, len(sources))
    return lib
