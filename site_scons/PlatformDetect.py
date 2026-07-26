"""
Platform detection and toolchain configuration for SCons build system.

Detects the host OS and configures the appropriate C++20 compiler toolchain.
"""

import sys
import logging
from SCons.Script import Exit

logger = logging.getLogger('StonerBuild.PlatformDetect')

# Platform name mapping from sys.platform
_PLATFORM_MAP = {
    'win32': 'Win64',
    'darwin': 'Mac',
    'linux': 'Linux',
}

# Expected compilers per platform (for error messages)
_EXPECTED_COMPILERS = {
    'Win64': 'MSVC (cl.exe)',
    'Mac': 'Apple Clang (clang++)',
    'Linux': 'GCC (g++) or Clang (clang++)',
}


def DetectPlatform():
    """Detect the host platform from sys.platform.

    Returns:
        str: One of 'Win64', 'Mac', or 'Linux'.

    Raises:
        SystemExit: If the platform is not recognized.
    """
    platform = _PLATFORM_MAP.get(sys.platform)
    if platform is None:
        logger.error("Unsupported platform '%s'.", sys.platform)
        print(f"ERROR: Unsupported platform '{sys.platform}'.")
        Exit(1)
    logger.info("Detected platform: %s", platform)
    return platform


def ConfigureToolchain(env, platform):
    """Configure the C++20 toolchain for the detected platform.

    Validates that a supported compiler is available, then sets C++20 flags.

    Args:
        env: SCons Environment object.
        platform: Platform name from DetectPlatform().

    Raises:
        SystemExit: If no supported C++ compiler is found.
    """
    # Verify compiler availability before configuring flags
    _VerifyCompiler(env, platform)

    if platform == 'Win64':
        env.Append(CXXFLAGS=[
            '/std:c++20',
            '/EHsc',
            '/utf-8',
            '/Zc:preprocessor',
        ])
    elif platform == 'Mac':
        env.Append(CXXFLAGS=['-std=c++20'])
    else:  # Linux
        env.Append(CXXFLAGS=['-std=c++20'])

    logger.info("Configured C++20 toolchain for %s", platform)


def _VerifyCompiler(env, platform):
    """Check that a supported C++ compiler is available on the system.

    Args:
        env: SCons Environment object.
        platform: Platform name from DetectPlatform().

    Raises:
        SystemExit: If no supported compiler is detected.
    """
    if platform == 'Win64':
        if not env.Detect('cl'):
            expected = _EXPECTED_COMPILERS['Win64']
            logger.error("No supported C++ compiler found for %s.", platform)
            print(f"ERROR: No supported C++ compiler found for {platform}. Expected: {expected}")
            Exit(1)
    else:
        # macOS / Linux — check for clang++ first, then g++
        compiler = env.Detect('clang++') or env.Detect('g++')
        if not compiler:
            expected = _EXPECTED_COMPILERS[platform]
            logger.error("No supported C++ compiler found for %s.", platform)
            print(f"ERROR: No supported C++ compiler found for {platform}. Expected: {expected}")
            Exit(1)
        logger.info("Detected compiler: %s", compiler)
