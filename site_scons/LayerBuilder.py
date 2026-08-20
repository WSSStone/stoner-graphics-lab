"""
Layer builder helper for SCons build system.

Provides BuildLayer() to compile a source layer into a static library
with strict include path enforcement for adjacent-only dependencies.
Provides DiscoverSubModules() for auto-discovering sub-modules that
contain their own SConscript files.
"""

import os
import logging
import subprocess
from SCons.Script import Dir, File, Glob, SConscript as _SConscript
from SCons.Script import Action

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


def _FindCppAndObjectiveCppFiles(source_dir_abs):
    """Find all C++ and Objective-C++ files in a directory.

    Args:
        source_dir_abs: Absolute path to the source directory.

    Returns:
        Sorted list of .cpp/.mm filenames (basenames only).
    """
    if not os.path.isdir(source_dir_abs):
        return []
    return sorted(
        filename for filename in os.listdir(source_dir_abs)
        if filename.endswith(('.cpp', '.mm'))
    )


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
    private_objects=None,
):
    """Build a source layer as a static library with dependency-controlled include paths.

    Clones the environment, sets CPPPATH to only the permitted dependencies'
    Public/ directories (enforcing adjacent-only layer isolation at compile time),
    finds all .cpp/.mm files from the Private/ directory, and returns a StaticLibrary.

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
                              isolated include_paths, cppdefines, and ccflags.
        private_objects: Optional prebuilt object nodes to append to the layer.

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
    cpp_files = _FindCppAndObjectiveCppFiles(source_dir_abs)

    private_c_sources = private_c_sources or []
    if not cpp_files and not private_c_sources:
        logger.warning("Layer '%s': no .cpp/.mm files found in %s", layer_name, source_dir)
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
        if settings is None and cpp_file.endswith('.mm'):
            settings = private_cpp_settings.get('*.mm')
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
        source_env.Append(CPPDEFINES=settings.get('cppdefines', []))
        source_env.Append(CCFLAGS=settings.get('ccflags', []))
        sources.append(source_env.Object(source))
    for c_source in private_c_sources:
        c_source_node = File('#' + c_source)
        c_env = layer_env.Clone()
        if third_party_cflags:
            c_env.Append(CFLAGS=third_party_cflags)
        sources.append(c_env.Object(c_source_node))
    sources.extend(private_objects or [])

    lib = layer_env.StaticLibrary(layer_name, sources)
    logger.info(
        "Layer '%s': building from %d C++ and %d private C source file(s)",
        layer_name,
        len(cpp_files),
        len(private_c_sources),
    )
    return lib


def BuildPrivateCMakeLibrary(
    env,
    target_name,
    source_dir,
    build_dir,
    library_relative_path,
    config,
):
    """Build a vendored static library without leaking its includes globally."""
    source_root = Dir('#' + source_dir).abspath
    build_root = Dir('#' + build_dir).abspath
    target = File('#' + os.path.join(build_dir, library_relative_path))

    sources = []
    for root, directories, files in os.walk(source_root):
        directories[:] = sorted(
            entry for entry in directories
            if entry not in ('.git', '__pycache__')
        )
        for filename in sorted(files):
            if filename != 'SHA256SUMS':
                sources.append(File(os.path.join(root, filename)))

    cmake_config = 'Debug' if config == 'debug' else 'Release'

    def _build_private_library(target, source, env):
        del target, source
        configure = [
            'cmake',
            '-S',
            source_root,
            '-B',
            build_root,
            '-DCMAKE_BUILD_TYPE=' + cmake_config,
            '-DBUILD_SHARED_LIBS=OFF',
        ]
        process_environment = os.environ.copy()
        process_environment.update(env.get('ENV', {}))
        deployment_target = process_environment.get('MACOSX_DEPLOYMENT_TARGET')
        if deployment_target:
            configure.append(
                '-DCMAKE_OSX_DEPLOYMENT_TARGET=' + deployment_target
            )
        result = subprocess.run(
            configure,
            check=False,
            env=process_environment,
        )
        if result.returncode != 0:
            return result.returncode
        build = [
            'cmake',
            '--build',
            build_root,
            '--config',
            cmake_config,
            '--target',
            target_name,
            '--parallel',
        ]
        return subprocess.run(
            build,
            check=False,
            env=process_environment,
        ).returncode

    return env.Command(
        target,
        sources,
        Action(
            _build_private_library,
            'Building private third-party library ' + target_name,
        ),
    )


def BuildPrivateCppLibrary(
    env,
    library_name,
    sources,
    include_paths,
    build_dir,
    ccflags=None,
    deployment_target=None,
):
    """Build selected vendored C++ sources without exporting include paths."""
    library_env = env.Clone()
    library_env.Append(
        CPPPATH=[Dir('#' + path).abspath for path in include_paths],
    )
    library_env.Append(CCFLAGS=ccflags or [])
    if deployment_target:
        library_env['ENV'] = dict(library_env.get('ENV', {}))
        library_env['ENV']['MACOSX_DEPLOYMENT_TARGET'] = deployment_target
        deployment_flag = '-mmacosx-version-min=' + deployment_target
        if deployment_flag not in library_env.get('CCFLAGS', []):
            library_env.Append(CCFLAGS=[deployment_flag])

    objects = []
    for source in sources:
        object_name = os.path.splitext(os.path.basename(source))[0]
        objects.extend(library_env.Object(
            '#' + os.path.join(build_dir, 'Objects', object_name),
            File('#' + source),
        ))
    library = library_env.StaticLibrary(
        '#' + os.path.join(build_dir, library_name),
        objects,
    )
    logger.info(
        "Private C++ library '%s': building from %d source file(s)",
        library_name,
        len(sources),
    )
    return library


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
