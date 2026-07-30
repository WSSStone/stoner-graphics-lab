"""Deterministic byte-preserving staging for explicit Content files."""

import os

from SCons.Script import Copy, File


def StageContent(env, relative_paths, destination_root='Content'):
    """Copy declared repository-relative Content files into a build root."""
    canonical = []
    seen = set()
    for path in relative_paths:
        normalized = os.path.normpath(path).replace('\\', '/')
        if (
            os.path.isabs(path)
            or normalized == '..'
            or normalized.startswith('../')
            or not normalized.startswith('Content/')
        ):
            raise ValueError('content source must be repository-relative: ' + path)
        destination = normalized[len('Content/'):]
        if destination in seen:
            raise ValueError('duplicate content destination: ' + destination)
        source_node = File('#' + normalized).srcnode()
        if not source_node.exists():
            raise ValueError('content source does not exist: ' + normalized)
        seen.add(destination)
        canonical.append((source_node, destination))

    targets = []
    for source, destination in sorted(
        canonical,
        key=lambda entry: entry[1],
    ):
        target = os.path.join(destination_root, destination)
        targets.extend(
            env.Command(target, source, Copy('$TARGET', '$SOURCE'))
        )
    return targets
