# Contract: Material Texture Binding Schema v2

## New Value Shape

```cpp
struct FMaterialSamplerIntent
{
    EAssetSamplerFilter MinFilter;
    EAssetSamplerFilter MagFilter;
    EAssetSamplerMipFilter MipFilter;
    EAssetSamplerAddressMode AddressU;
    EAssetSamplerAddressMode AddressV;
};

struct FMaterialTextureBinding
{
    TSoftAssetRef<FTextureAsset> Texture;
    Core::uint32 TexCoordSet = 0;
    FMaterialSamplerIntent Sampler;
};
```

`EMaterialAssetParameterType` gains `TextureBinding`. Material and
MaterialInstance parameter variants gain this struct. Asset owns all enums and
does not include RHI.

## JSON v2 Shape

```json
{
  "name": "BaseColorTexture",
  "type": "textureBinding",
  "value": {
    "texture": "Texture:/content/model#key.base-color",
    "texCoord": 0,
    "sampler": {
      "min": "automatic",
      "mag": "linear",
      "mip": "automatic",
      "addressU": "repeat",
      "addressV": "repeat"
    }
  }
}
```

The exact canonical key order follows the existing JSON codec discipline.
Unknown members and duplicate members follow the established strict parsing
rules.

## Compatibility

- Readers accept v1 and v2.
- A v1 texture reference upgrades in memory to UV0, repeat U/V, automatic min/
  mip, and linear mag defaults.
- Serializing an unchanged v1 definition as v1 preserves its previous
  canonical bytes and digest.
- glTF-generated definitions use v2.
- MaterialInstance overrides support the complete binding.
- A v2 binding with non-default UV/sampler state cannot serialize to v1.

## Validation

- Texture reference is typed and valid.
- `TexCoordSet` is 0 or 1.
- Enum values are recognized.
- Compare sampling and border color are absent from Feature 024.
- Dependency extraction records the bound texture exactly once.
- Normal scale, occlusion strength, alpha cutoff, and color/factor values remain
  separate typed parameters.

## Renderer Mapping

Renderer maps Asset sampler intent to `FRHISamplerDesc`. Unsupported device
sampling behavior returns a normal Renderer/RHI compatibility failure; it does
not mutate the Asset definition or silently change address/filter semantics.
