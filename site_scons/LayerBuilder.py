"""
Layer builder helper for SCons build system.

Provides BuildLayer() to compile a source layer into a static library
with strict include path enforcement for adjacent-only dependencies.
Provides DiscoverSubModules() for auto-discovering sub-modules that
contain their own SConscript files.
"""

import os
import logging
from SCons.Script import Dir, File, Glob, SConscript as _SConscript

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


def _FindCppFiles(source_dir_abs):
    """Find all .cpp files in a directory using the filesystem.

    Args:
        source_dir_abs: Absolute path to the source directory.

    Returns:
        Sorted list of .cpp filenames (basenames only).
    """
    if not os.path.isdir(source_dir_abs):
        return []
    return sorted(f for f in os.listdir(source_dir_abs) if f.endswith('.cpp'))


def _MakeRelativeToSConscriptDir(abs_path):
    """Convert an absolute path to a path relative to the current SConscript's
    source directory. This is needed for variant_dir to work correctly.

    In variant_dir context, Dir('.').srcnode() returns the original source
    directory that was mapped to the variant_dir. Paths relative to this
    directory will be correctly mapped by SCons.

    Args:
        abs_path: Absolute path to convert.

    Returns:
        Path relative to the current SConscript's source directory.
    """
    sconscript_src_dir = Dir('.').srcnode().abspath
    return os.path.relpath(abs_path, sconscript_src_dir)


def BuildLayer(
    env,
    layer_name,
    dependencies,
    source_dir=None,
    public_dir=None,
    private_c_sources=None,
    third_party_cflags=None,
    private_cpp_settings=None,
):
    """Build a source layer as a static library with dependency-controlled include paths.

    Clones the environment, sets CPPPATH to only the permitted dependencies'
    Public/ directories (enforcing adjacent-only layer isolation at compile time),
    finds all .cpp files from the Private/ directory, and returns a StaticLibrary.

    Works correctly with variant_dir: source files are referenced relative to
    the SConscript's source directory so SCons places .o and .a files in the
    variant_dir.

    Args:
        env: Parent SCons Environment (will be cloned).
        layer_name: Name for the output library (e.g., 'Core', 'VulkanRHI').
        dependencies: List of layer names this layer is allowed to depend on.
        source_dir: Override source directory (default: Source/{layer_name}/Private).
                    Must be relative to project root.
        public_dir: Override public include directory (default: Source/{layer_name}/Public).
                    Must be relative to project root.
        private_c_sources: Project-root-relative C sources compiled privately
                           into the layer.
        third_party_cflags: C-only flags applied to private C sources.
        private_cpp_settings: Optional mapping from a private C++ basename to
                              isolated include_paths and ccflags.

    Returns:
        SCons StaticLibrary node, or None if no source files found.
    """
    layer_env = env.Clone()

    if public_dir is None:
        public_dir = os.path.join(_SOURCE_ROOT, layer_name, 'Public')
    if source_dir is None:
        source_dir = os.path.join(_SOURCE_ROOT, layer_name, 'Private')

    # Build include path list: own public dir + permitted dependency public dirs
    # Use '#' prefix to get absolute paths for compiler -I flags
    include_paths = [Dir('#' + public_dir).abspath]
    for dep in dependencies:
        dep_public = _GetPublicIncludePath(dep)
        include_paths.append(Dir('#' + dep_public).abspath)

    layer_env.Append(CPPPATH=include_paths)

    # Find source files using filesystem
    source_dir_abs = Dir('#' + source_dir).abspath
    cpp_files = _FindCppFiles(source_dir_abs)

    private_c_sources = private_c_sources or []
    if not cpp_files and not private_c_sources:
        logger.warning("Layer '%s': no .cpp files found in %s", layer_name, source_dir)
        return None

    # Convert source_dir to a path relative to the current SConscript's source
    # directory. This ensures variant_dir mapping works correctly — SCons will
    # place .o files in variant_dir/<relative_path>/ and the .a file in variant_dir/.
    rel_source_dir = _MakeRelativeToSConscriptDir(source_dir_abs)
    sources = []
    private_cpp_settings = private_cpp_settings or {}
    for cpp_file in cpp_files:
        source = os.path.join(rel_source_dir, cpp_file)
        settings = private_cpp_settings.get(cpp_file)
        if not settings:
            sources.append(source)
            continue
        source_env = layer_env.Clone()
        source_env.Append(
            CPPPATH=[
                Dir('#' + path).abspath
                for path in settings.get('include_paths', [])
            ],
        )
        source_env.Append(CCFLAGS=settings.get('ccflags', []))
        sources.append(source_env.Object(source))
    for c_source in private_c_sources:
        c_source_node = File('#' + c_source)
        c_env = layer_env.Clone()
        if third_party_cflags:
            c_env.Append(CFLAGS=third_party_cflags)
        sources.append(c_env.Object(c_source_node))

    lib = layer_env.StaticLibrary(layer_name, sources)
    logger.info(
        "Layer '%s': building from %d C++ and %d private C source file(s)",
        layer_name,
        len(cpp_files),
        len(private_c_sources),
    )
    return lib


def DiscoverSubModules(layer_dir, exports):
    """Auto-discover sub-modules within a layer directory.

    Scans all immediate sub-directories of layer_dir for SConscript files.
    For each discovered sub-module, delegates to its SConscript via
    SCons.Script.SConscript(). This enables adding new modules without
    modifying the root SConstruct or parent SConscript.

    Sub-module SConscripts are called without their own variant_dir — they
    inherit the caller's variant_dir context. Combined with BuildLayer()'s
    relative-path source references, outputs land in the correct Build/ directory.

    Args:
        layer_dir: Relative path from project root to the layer directory
                   (e.g., 'Source/Renderer' or 'Source/Backend').
        exports: Dict of variables to export to sub-module SConscripts
                 (e.g., {'env': env, 'platform': platform, 'config': config}).

    Returns:
        List of SCons library nodes returned by discovered sub-module SConscripts.
        Entries that returned None are filtered out.
    """
    layer_abs = Dir('#' + layer_dir).abspath
    discovered_libs = []

    if not os.path.isdir(layer_abs):
        logger.warning("DiscoverSubModules: directory '%s' does not exist", layer_dir)
        return discovered_libs

    for entry in sorted(os.listdir(layer_abs)):
        subdir_abs = os.path.join(layer_abs, entry)
        if not os.path.isdir(subdir_abs):
            continue

        sconscript_abs = os.path.join(subdir_abs, 'SConscript')
        if not os.path.exists(sconscript_abs):
            continue

        logger.info("DiscoverSubModules: found sub-module '%s' in %s", entry, layer_dir)

        # Use '#' prefix path with per-sub-module variant_dir.
        # variant_dir is just the entry name, relative to the caller's
        # variant_dir. SCons resolves this relative to the current SConscript's
        # directory (which is the variant_dir when one is set).
        sconscript_path = '#' + os.path.join(layer_dir, entry, 'SConscript')

        result = _SConscript(
            sconscript_path,
            variant_dir=entry,
            duplicate=0,
            exports=exports,
        )
        if result is not None:
            discovered_libs.append(result)

    logger.info(
        "DiscoverSubModules: discovered %d sub-module(s) in '%s'",
        len(discovered_libs),
        layer_dir,
    )
    return discovered_libs
