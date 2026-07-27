from pathlib import Path
from tempfile import TemporaryDirectory
import json
import sys
import unittest

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from reviewlib import atomic_write, read_json


class ReviewLibTests(unittest.TestCase):
    def test_atomic_json_round_trip(self):
        with TemporaryDirectory() as temporary:
            path = Path(temporary) / "state.json"
            atomic_write(path, json.dumps({"schema_version": 1}))
            self.assertEqual(read_json(path)["schema_version"], 1)


if __name__ == "__main__":
    unittest.main()
