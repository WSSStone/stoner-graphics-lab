"""
Build configuration management for SCons build system.

Handles Debug/Release configuration selection and compiler flag application.
"""

import logging

logger = logging.getLogger('StonerBuild.BuildConfig')

# Valid configuration names
_VALID_CONFIGS = ('debug', 'release')

# Configuration display names for output directory (PascalCase)
_CONFIG_DISPLAY = {
    'debug': 'Debug',
    'release': 'Release',
}


def GetBuildConfig():
    """Read and validate the build configuration from command-line arguments.

    Reads 'config' from SCons ARGUMENTS, defaults to 'debug'.

    Returns:
        str: Validated config name ('debug' or 'release').

    Raises:
        SystemExit: If config value is not recognized.
    """
    config = ARGUMENTS.get('config', 'debug').lower()
    if config not in _VALID_CONFIGS:
        logger.error("Unknown config '%s'.", config)
        print(f"ERROR: Unknown config '{config}'. Use 'debug' or 'release'.")
        Exit(1)
    logger.info("Build configuration: %s", config)
    return config


def GetConfigDisplayName(config):
    """Get the PascalCase display name for a config (used in output paths).

    Args:
        config: Config name ('debug' or 'release').

    Returns:
        str: 'Debug' or 'Release'.
    """
    return _CONFIG_DISPLAY[config]


def ApplyConfig(env, config, platform):
    """Apply configuration-specific compiler flags to the environment.

    Args:
        env: SCons Environment object.
        config: Config name ('debug' or 'release').
        platform: Platform name from DetectPlatform().
    """
    is_msvc = (platform == 'Win64')

    if config == 'debug':
        if is_msvc:
            env.Append(CXXFLAGS=['/Od', '/Zi', '/MDd', '/W4'])
            env.Append(CPPDEFINES=['_DEBUG'])
        else:
            env.Append(CXXFLAGS=['-O0', '-g', '-Wall', '-Wextra'])
            env.Append(CPPDEFINES=['_DEBUG'])
    else:  # release
        if is_msvc:
            env.Append(CXXFLAGS=['/O2', '/MD', '/W4'])
            env.Append(CPPDEFINES=['NDEBUG'])
        else:
            env.Append(CXXFLAGS=['-O2', '-Wall', '-Wextra'])
            env.Append(CPPDEFINES=['NDEBUG'])

    logger.info("Applied %s flags for %s", config, platform)
