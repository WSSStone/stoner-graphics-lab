# Contract: Material and Shader Source Definitions

## File Classes

| Suffix | Root schema | Top-level Asset type |
|---|---|---|
| `.shader.json` | `stoner.shader-program` | `ShaderProgram` |
| `.material.json` | `stoner.material` | `Material` |
| `.material-instance.json` | `stoner.material-instance` | `MaterialInstance` |

Suffix is an importer hint only. The root `schema` plus bounded content probe is
authoritative. Definitions are strict UTF-8 JSON, version 1.

## Common Envelope

Canonical order begins:

```json
{
  "schema": "stoner.shader-program",
  "version": 1,
  "id": {
    "type": "ShaderProgram",
    "path": "Engine/Shaders/Triangle"
  },
  "requiredExtensions": [],
  "...kind fields...": {},
  "extensions": {}
}
```

`id.subresource` is omitted when absent. Identity creation uses Feature 020 and
must reproduce the exact canonical object values.

The definition does not contain its own digest. After canonical writing,
`FAssetVersion` is derived from the complete canonical bytes; explicit digest
fields apply only to external source/payload dependencies.

### Extension Contract

- Extension names match `[A-Za-z][A-Za-z0-9_.-]{0,126}` and contain at least
  one namespace separator `.`.
- `requiredExtensions` input order is not semantic; names must be
  duplicate-free and canonical output sorts them.
- `extensions` input object order is not semantic; decoded keys must be
  duplicate-free and canonical output sorts them.
- every required name has a corresponding body.
- known required/optional bodies are schema-validated by their registered
  extension strategy.
- unknown optional bodies are skipped and omitted by canonical output.
- unknown required names fail before kind-specific publication.
- unknown ordinary fields anywhere outside extension bodies fail.

## Asset Reference Object

```json
{
  "type": "ShaderPayload",
  "path": "Engine/Shaders/Triangle",
  "subresource": "payload.vulkan.desktop.vertex.default"
}
```

`subresource` is optional. Expected type is fixed by the containing field and
must match `type`.

## Dependency Source Object

```json
{
  "asset": {
    "type": "ShaderSource",
    "path": "Engine/Shaders/Triangle",
    "subresource": "source.vertex"
  },
  "locator": "Triangle.vert",
  "digest": "sha256:<64-lowercase-hex>"
}
```

Locator is a resolver-private relative locator, not identity. It rejects
absolute/drive/network roots, parent traversal, invalid UTF-8, NUL/control
characters, and values above `MaxLocatorBytes` (default 1,024). Canonical
separators are `/`.

## Shader Program Definition

```json
{
  "schema": "stoner.shader-program",
  "version": 1,
  "id": {
    "type": "ShaderProgram",
    "path": "Engine/Shaders/Triangle"
  },
  "requiredExtensions": [],
  "programKind": "graphics",
  "stages": [
    {
      "stage": "vertex",
      "entryPoint": "main",
      "language": "glsl",
      "source": {
        "asset": {
          "type": "ShaderSource",
          "path": "Engine/Shaders/Triangle",
          "subresource": "source.vertex"
        },
        "locator": "Triangle.vert",
        "digest": "sha256:<source-sha256>"
      }
    },
    {
      "stage": "fragment",
      "entryPoint": "main",
      "language": "glsl",
      "source": {
        "asset": {
          "type": "ShaderSource",
          "path": "Engine/Shaders/Triangle",
          "subresource": "source.fragment"
        },
        "locator": "Triangle.frag",
        "digest": "sha256:<source-sha256>"
      }
    }
  ],
  "allowedPermutationFlags": [],
  "requiredParameters": [],
  "interface": {
    "bindings": [],
    "constantRanges": []
  },
  "variants": [
    {
      "name": "default",
      "flags": [],
      "payloads": [
        {
          "backend": "vulkan",
          "profile": "desktop-baseline",
          "format": "spirv",
          "stage": "vertex",
          "entryPoint": "main",
          "asset": {
            "type": "ShaderPayload",
            "path": "Engine/Shaders/Triangle",
            "subresource": "payload.vulkan.desktop.vertex.default"
          },
          "locator": "Triangle.vert.spv",
          "digest": "sha256:<payload-sha256>",
          "producer": "Stoner.CheckedInSpirv",
          "producerVersion": "023-v1"
        },
        {
          "backend": "vulkan",
          "profile": "desktop-baseline",
          "format": "spirv",
          "stage": "fragment",
          "entryPoint": "main",
          "asset": {
            "type": "ShaderPayload",
            "path": "Engine/Shaders/Triangle",
            "subresource": "payload.vulkan.desktop.fragment.default"
          },
          "locator": "Triangle.frag.spv",
          "digest": "sha256:<payload-sha256>",
          "producer": "Stoner.CheckedInSpirv",
          "producerVersion": "023-v1"
        }
      ]
    }
  ],
  "extensions": {}
}
```

### Shader Ordering

- stages: Vertex, Fragment, Compute enum order after validation;
- allowed flags: lexicographic;
- required parameters: name;
- bindings: `(set, binding, kind, name)`;
- constant ranges: `(offset, size, visibility)`;
- variants: canonical permutation key, then name;
- payloads: `(backend, profile, format, stage, entryPoint, asset identity)`.

Input order is not semantic for these collections. Duplicate canonical keys fail.

### Program Validation

- graphics v1: exactly Vertex + Fragment;
- compute v1: exactly Compute;
- current source language: GLSL;
- every variant payload profile contains all program stages or is incomplete and
  unavailable; records do not combine across profiles;
- every payload stage/entry matches one stage record;
- every permutation flag is declared;
- interface bindings/ranges are unique, in bounds, and visible to program stages;
- source/payload IDs use exact expected types;
- loaded bytes match declared digest/stage/entry.

## Material Definition

```json
{
  "schema": "stoner.material",
  "version": 1,
  "id": {
    "type": "Material",
    "path": "Engine/Materials/DefaultSurface"
  },
  "requiredExtensions": [],
  "domain": "surface",
  "blendMode": "opaque",
  "renderState": {
    "depthTest": true,
    "depthWrite": true,
    "twoSided": false
  },
  "shader": {
    "type": "ShaderProgram",
    "path": "Engine/Shaders/Deferred/Surface"
  },
  "permutationFlags": [],
  "parameters": [
    {
      "name": "BaseColor",
      "type": "color",
      "value": [1, 1, 1, 1]
    },
    {
      "name": "AlbedoTexture",
      "type": "texture",
      "value": {
        "type": "Texture",
        "path": "Game/Textures/DefaultWhite"
      }
    },
    {
      "name": "Roughness",
      "type": "scalar",
      "value": 0.5
    }
  ],
  "extensions": {}
}
```

Canonical parameter types:

| Type | JSON value |
|---|---|
| `scalar` | finite number |
| `vector` | array of four finite numbers |
| `color` | array of four finite numbers |
| `texture` | Asset reference with type `Texture` |

Parameters sort by name. Duplicate names, non-finite/out-of-range float values,
wrong array lengths/types, invalid texture references, unknown flags, and
Feature 014-invalid domain/blend/render combinations fail.

## Material Instance Definition

```json
{
  "schema": "stoner.material-instance",
  "version": 1,
  "id": {
    "type": "MaterialInstance",
    "path": "Game/Materials/RedSurface"
  },
  "requiredExtensions": [],
  "parent": {
    "type": "Material",
    "path": "Engine/Materials/DefaultSurface"
  },
  "overrides": [
    {
      "name": "BaseColor",
      "type": "color",
      "value": [1, 0, 0, 1]
    }
  ],
  "extensions": {}
}
```

Parent type is exactly Material or MaterialInstance. Overrides use the same
value grammar as parameters and sort by name. The definition alone can validate
syntax/type shape; root parameter existence/type and cycles require immutable
parent lookup.

## Canonical Writer

The writer receives validated typed models only. It never canonicalizes an
arbitrary JSON DOM. It:

1. emits common fields in contract order;
2. emits kind fields in contract order;
3. sorts all unordered model collections;
4. normalizes model float values and Asset IDs;
5. omits default-absent fields and unknown optional extensions;
6. emits two-space pretty JSON with LF and one final newline.

Repeated canonical writes are byte-identical. Parsing canonical output yields a
canonically equivalent model.

## Error Location Contract

Parse diagnostics may carry byte offset, line, and column computed from the
bounded input. Normalized diagnostics use:

- Stage: Parse, Normalize, Validate, Dependency.
- Subject: expected/top-level Asset ID when available, otherwise source
  descriptor.
- Field: JSON pointer-like stable schema path such as
  `/variants/2/payloads/1/digest`.
- Result and reason token.
- Actual count/size and configured limit when applicable.

They never copy arbitrary input lines, absolute paths, third-party parser text,
or payload bytes into normalized output.
