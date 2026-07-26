"""Optional desktop graphics dependency discovery for Feature 018."""

import os
import shutil
import glob
import logging

from SCons.Script import ARGUMENTS, Exit


logger = logging.getLogger('StonerBuild.GraphicsDependencyDetect')
_VALID_GRAPHICS_MODES = ('auto', 'disabled')


def _first_existing(paths, predicate=os.path.exists):
    for path in paths:
        if path and predicate(path):
            return os.path.abspath(path)
    return None


def _sdk_roots():
    roots = [os.environ.get('VULKAN_SDK'), os.environ.get('GLFW_ROOT')]
    home_sdk = os.path.expanduser('~/VulkanSDK')
    if os.path.isdir(home_sdk):
        versions = sorted(os.listdir(home_sdk), reverse=True)
        roots.extend(os.path.join(home_sdk, version, 'macOS') for version in versions)
    roots.extend(['/usr/local', '/opt/homebrew', '/usr'])
    return [root for root in roots if root]


def _graphics_mode():
    mode = ARGUMENTS.get('graphics', 'auto').lower()
    if mode not in _VALID_GRAPHICS_MODES:
        logger.error("Unknown graphics dependency mode '%s'.", mode)
        print(f"ERROR: graphics must be one of: {', '.join(_VALID_GRAPHICS_MODES)}.")
        Exit(1)
    return mode


def DetectGraphicsDependencies(platform):
    """Return paths and availability flags without making dependencies mandatory."""
    if _graphics_mode() == 'disabled':
        return {
            'vulkan_include_dir': None,
            'vulkan_library': None,
            'glfw_include_dir': None,
            'glfw_library': None,
            'moltenvk_library': None,
            'glslang_validator': shutil.which('glslangValidator'),
            'spirv_validator': shutil.which('spirv-val'),
            'vulkan_available': False,
            'glfw_available': False,
        }

    roots = _sdk_roots()
    include_dirs = [os.path.join(root, name) for root in roots for name in ('include', 'Include')]
    library_dirs = [os.path.join(root, name) for root in roots for name in ('lib', 'Lib')]
    library_dirs.extend(
        path
        for root in roots
        for path in sorted(glob.glob(os.path.join(root, 'lib-*')), reverse=True)
    )
    library_dirs.extend(glob.glob('/usr/lib/*-linux-gnu'))
    library_dirs.extend(glob.glob('/lib/*-linux-gnu'))

    vulkan_include = _first_existing(
        (os.path.join(path, 'vulkan', 'vulkan.h') for path in include_dirs),
        os.path.isfile,
    )
    glfw_include = _first_existing(
        (os.path.join(path, 'GLFW', 'glfw3.h') for path in include_dirs),
        os.path.isfile,
    )

    if platform == 'Win64':
        vulkan_names = ('vulkan-1.lib',)
        glfw_names = ('glfw3.lib', 'glfw.lib')
        molten_names = ()
    elif platform == 'Mac':
        vulkan_names = ('libvulkan.dylib', 'libMoltenVK.dylib', 'libMoltenVK.a')
        glfw_names = ('libglfw.3.dylib', 'libglfw.dylib', 'libglfw3.a')
        molten_names = ('libMoltenVK.dylib', 'libMoltenVK.a')
    else:
        vulkan_names = ('libvulkan.so', 'libvulkan.so.1')
        glfw_names = ('libglfw.so', 'libglfw.so.3', 'libglfw3.a')
        molten_names = ()

    def find_library(names):
        return _first_existing(
            (os.path.join(path, name) for path in library_dirs for name in names),
            os.path.isfile,
        )

    vulkan_library = find_library(vulkan_names)
    glfw_library = find_library(glfw_names)
    molten_library = find_library(molten_names)
    return {
        'vulkan_include_dir': os.path.dirname(os.path.dirname(vulkan_include)) if vulkan_include else None,
        'vulkan_library': vulkan_library,
        'glfw_include_dir': os.path.dirname(os.path.dirname(glfw_include)) if glfw_include else None,
        'glfw_library': glfw_library,
        'moltenvk_library': molten_library,
        'glslang_validator': shutil.which('glslangValidator'),
        'spirv_validator': shutil.which('spirv-val'),
        'vulkan_available': bool(vulkan_include and vulkan_library),
        'glfw_available': bool(glfw_include and glfw_library),
    }
