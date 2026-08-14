from __future__ import annotations

import importlib.util
import json
import struct
import unittest
from pathlib import Path


_PATH = Path(__file__).parent / "Fixtures/StaticModel/Performance/generate_performance_fixture.py"
_SPEC = importlib.util.spec_from_file_location("generate_performance_fixture", _PATH)
if _SPEC is None or _SPEC.loader is None:
    raise RuntimeError("unable to load performance fixture generator")
_MODULE = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(_MODULE)


class StaticModelPerformanceFixtureTests(unittest.TestCase):
    def test_generator_is_deterministic_and_has_required_shape(self) -> None:
        first = _MODULE.generate()
        self.assertEqual(first, _MODULE.generate())
        magic, version, total = struct.unpack_from("<4sII", first)
        self.assertEqual((b"glTF", 2, len(first)), (magic, version, total))
        json_size, chunk_type = struct.unpack_from("<I4s", first, 12)
        self.assertEqual(b"JSON", chunk_type)
        document = json.loads(first[20:20 + json_size].decode("utf-8"))
        primitives = document["meshes"][0]["primitives"]
        self.assertEqual(16, len(primitives))
        self.assertEqual(16, len(document["materials"]))
        accessors = document["accessors"]
        vertex_count = sum(accessors[item["attributes"]["POSITION"]]["count"]
                           for item in primitives)
        index_count = sum(accessors[item["indices"]]["count"] for item in primitives)
        self.assertEqual(100_000, vertex_count)
        self.assertEqual(300_000, index_count)


if __name__ == "__main__":
    unittest.main()
