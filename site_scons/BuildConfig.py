"""
Build configuration management for SCons build system.

Handles Debug/Release configuration selection and compiler flag application.
"""

import logging
from SCons.Script import ARGUMENTS, Exit

logger = logging.getLogger('StonerBuild.BuildConfig')

# Valid configuration names
_VALID_CONFIGS = ('debug', 'release')

# Configuration display names for output directory (PascalCase)
_CONFIG_DISPLAY = {
    'debug': 'Debug',
    'release': 'Release',
}

_TRUE_VALUES = ('1', 'true', 'yes', 'on')
_FALSE_VALUES = ('0', 'false', 'no', 'off')
_SANITIZER_FLAGS = {
    'none': [],
    'address': ['address'],
    'undefined': ['undefined'],
    'address,undefined': ['address', 'undefined'],
}


def _GetBooleanArgument(name, default='0'):
    value = ARGUMENTS.get(name, default).lower()
    if value in _TRUE_VALUES:
        return True
    if value in _FALSE_VALUES:
        return False
    logger.error("Invalid boolean value '%s' for %s.", value, name)
    print(f"ERROR: {name} must be one of: {', '.join(_TRUE_VALUES + _FALSE_VALUES)}.")
    Exit(1)


def _GetSanitizers(config, platform):
    value = ARGUMENTS.get('sanitizers', 'none').lower()
    if value not in _SANITIZER_FLAGS:
        logger.error("Unknown sanitizer selection '%s'.", value)
        print(f"ERROR: sanitizers must be one of: {', '.join(_SANITIZER_FLAGS)}.")
        Exit(1)
    if value != 'none' and config != 'debug':
        logger.error("Sanitizers require config=debug.")
        print("ERROR: sanitizers require config=debug.")
        Exit(1)
    if value != 'none' and platform == 'Win64':
        logger.error("The current MSVC toolchain does not provide the ASan/UBSan profile.")
        print("ERROR: sanitizers are currently supported with Clang/GCC on macOS and Linux.")
        Exit(1)
    return _SANITIZER_FLAGS[value]


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
    strict = _GetBooleanArgument('strict')
    sanitizers = _GetSanitizers(config, platform)

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

    if is_msvc:
        # The engine intentionally uses portable ISO C APIs where appropriate.
        env.Append(CPPDEFINES=['_CRT_SECURE_NO_WARNINGS'])
        if strict:
            env.Append(CXXFLAGS=['/WX'])
    else:
        if strict:
            env.Append(CXXFLAGS=['-Werror'])
        if sanitizers:
            sanitizer_value = ','.join(sanitizers)
            sanitizer_flags = [
                f'-fsanitize={sanitizer_value}',
                '-fno-omit-frame-pointer',
                '-fno-sanitize-recover=all',
            ]
            env.Append(CXXFLAGS=sanitizer_flags)
            env.Append(LINKFLAGS=sanitizer_flags)

    sanitizer_display = ','.join(sanitizers) if sanitizers else 'none'
    logger.info(
        "Applied %s flags for %s (strict=%s, sanitizers=%s)",
        config,
        platform,
        strict,
        sanitizer_display,
    )
