import unittest

from site_scons.ContentStaging import StageContent


class FakeEnvironment:
    def __init__(self):
        self.commands = []

    def Command(self, target, source, action):
        self.commands.append((target, str(source), action))
        return [target]


class ContentStagingTests(unittest.TestCase):
    def test_declared_content_is_sorted_and_preserves_layout(self):
        env = FakeEnvironment()
        targets = StageContent(
            env,
            [
                "Content/Shaders/Triangle/Triangle.vert",
                "Content/Shaders/Triangle/Triangle.frag",
            ],
        )
        self.assertEqual(
            [
                "Content/Shaders/Triangle/Triangle.frag",
                "Content/Shaders/Triangle/Triangle.vert",
            ],
            targets,
        )

    def test_invalid_roots_and_traversal_are_rejected(self):
        for path in (
            "/tmp/file.spv",
            "../Content/file.spv",
            "Source/file.spv",
        ):
            with self.subTest(path=path):
                with self.assertRaises(ValueError):
                    StageContent(FakeEnvironment(), [path])

    def test_duplicate_destination_is_rejected(self):
        with self.assertRaises(ValueError):
            StageContent(
                FakeEnvironment(),
                [
                    "Content/Shaders/Triangle/Triangle.vert",
                    "Content/Shaders/Triangle/./Triangle.vert",
                ],
            )

    def test_missing_declared_source_is_rejected(self):
        with self.assertRaises(ValueError):
            StageContent(
                FakeEnvironment(),
                ["Content/Shaders/Triangle/DoesNotExist.spv"],
            )


if __name__ == "__main__":
    unittest.main()
