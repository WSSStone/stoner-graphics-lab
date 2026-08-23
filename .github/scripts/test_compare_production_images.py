#!/usr/bin/env python3

import importlib.util
from pathlib import Path
import unittest


SCRIPT = Path(__file__).with_name("compare_production_images.py")


class ProductionImageComparatorContractTests(unittest.TestCase):
    def test_comparator_exposes_canonical_image_entry_point(self):
        self.assertTrue(SCRIPT.is_file(), "production image comparator is not implemented")
        spec = importlib.util.spec_from_file_location("compare_production_images", SCRIPT)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        self.assertTrue(callable(getattr(module, "compare_images", None)))


if __name__ == "__main__":
    unittest.main()
