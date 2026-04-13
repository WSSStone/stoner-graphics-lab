"""
SConstruct - Root build entry point for Stoner Graphics Lab.

Validates SCons version, detects platform, applies build configuration,
and delegates to each layer's SConscript via hierarchical build.
"""

import SCons
import os
import logging

# ---------------------------------------------------------------------------
# Logging Setup
# ---------------------------------------------------------------------------
logging.basicConfig(
    level=logging.INFO,
    format='[%(name)s] %(levelname)s: %(message)s',
)
logger = logging.getLogger('StonerBuild')

# ---------------------------------------------------------------------------
# SCons Version Validation (FR-007, R-006)
# ---------------------------------------------------------------------------
MINIMUM_SCONS_VERSION = '4.10.1'

scons_ver = SCons.__version__
# Handle version strings with extra components (e.g., '4.10.1.dev0')
scons_ver_tuple = tuple(int(x) for x in scons_ver.split('.')[:3])
min_ver_tuple = tuple(int(x) for x in MINIMUM_SCONS_VERSION.split('.'))

if scons_ver_tuple < min_ver_tuple:
    logger.error("SCons %s+ required. Found: %s", MINIMUM_SCONS_VERSION, scons_ver)
    print(f"ERROR: SCons {MINIMUM_SCONS_VERSION}+ required. Found: {scons_ver}")
    Exit(1)

# ---------------------------------------------------------------------------
# Import build system modules from site_scons/ (R-005)
# ---------------------------------------------------------------------------
from PlatformDetect import DetectPlatform, ConfigureToolchain
from BuildConfig import GetBuildConfig, GetConfigDisplayName, ApplyConfig

# ---------------------------------------------------------------------------
# Platform Detection (R-002)
# ---------------------------------------------------------------------------
platform = DetectPlatform()

# ---------------------------------------------------------------------------
# Build Configuration (R-003)
# ---------------------------------------------------------------------------
config = GetBuildConfig()
config_display = GetConfigDisplayName(config)

# ---------------------------------------------------------------------------
# Base Environment
# ---------------------------------------------------------------------------
env = Environment()
ConfigureToolchain(env, platform)
ApplyConfig(env, config, platform)

logger.info("Stoner Graphics Lab")
logger.info("  Platform : %s", platform)
logger.info("  Config   : %s", config_display)
logger.info("  SCons    : %s", scons_ver)

# ---------------------------------------------------------------------------
# Build output base path: Build/<Platform>/<Config>/
# ---------------------------------------------------------------------------
build_base = os.path.join('Build', platform, config_display)

# ---------------------------------------------------------------------------
# Layer Delegation (R-001)
# Each layer gets its own variant_dir under Build/<Platform>/<Config>/<Layer>
# duplicate=0 keeps source tree clean
# ---------------------------------------------------------------------------

# Export variables for SConscript files
Export('env', 'platform', 'config')

# Core layer (no dependencies)
SConscript(
    'Source/Core/SConscript',
    variant_dir=os.path.join(build_base, 'Core'),
    duplicate=0,
)

# RHI layer (depends on Core)
SConscript(
    'Source/RHI/SConscript',
    variant_dir=os.path.join(build_base, 'RHI'),
    duplicate=0,
)

# Renderer layer (depends on RHI, Core)
SConscript(
    'Source/Renderer/SConscript',
    variant_dir=os.path.join(build_base, 'Renderer'),
    duplicate=0,
)

# Application layer (depends on Renderer, Core)
SConscript(
    'Source/Application/SConscript',
    variant_dir=os.path.join(build_base, 'Application'),
    duplicate=0,
)

# Backend layers — each backend gets its own variant_dir
# Only Vulkan has source files; others are stubs
_BACKENDS = ['Vulkan', 'DX12', 'DX11', 'Metal', 'OpenGL', 'GLES', 'WebGL']
for backend in _BACKENDS:
    backend_sconscript = os.path.join('Source', 'Backend', backend, 'SConscript')
    if os.path.exists(backend_sconscript):
        SConscript(
            backend_sconscript,
            variant_dir=os.path.join(build_base, 'Backend', backend),
            duplicate=0,
        )

# Tests (links against all layers)
SConscript(
    'Tests/SConscript',
    variant_dir=os.path.join(build_base, 'Tests'),
    duplicate=0,
)
