"""Mutation tests for the read-only Roadmap 3.0 structural/semantic gate."""
from __future__ import annotations

import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest

SCRIPT = Path(__file__).with_name('verify_roadmap_consistency.py')
SPEC = importlib.util.spec_from_file_location('roadmap_scan', SCRIPT)
assert SPEC is not None and SPEC.loader is not None
SCAN = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SCAN)
ROOT = SCRIPT.parents[2]


class RoadmapConsistencyTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        paths = [ROOT / 'doc/roadmap.md', ROOT / 'AGENTS.md']
        paths += list((ROOT / 'specs/002-engine-development-roadmap').rglob('*.md'))
        paths += [ROOT / 'specs/002-engine-development-roadmap/phase-index.json']
        paths += [ROOT / 'specs/029-hdr-output-transform' / n for n in ('spec.md', 'plan.md', 'tasks.md')]
        for source in paths:
            target = self.root / source.relative_to(ROOT)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)

    def replace(self, path, before, after):
        target = self.root / path
        text = target.read_text()
        self.assertIn(before, text)
        target.write_text(text.replace(before, after, 1))

    def rejected(self, fragment):
        result = SCAN.scan(self.root)
        self.assertEqual(result['status'], 'failed')
        self.assertTrue(any(fragment in f for f in result['findings']), result)

    def test_current_roadmap_passes(self):
        result = SCAN.scan(self.root)
        self.assertEqual(result['findings'], [])
        self.assertEqual(result['checks']['runtimePhaseCount'], 51)
        self.assertEqual(result['checks']['futurePhaseCount'], 24)

    def test_bad_anchor(self):
        self.replace('doc/roadmap.md', '(#phase-039--renderer-virtual-shadow-maps)', '(#missing-shadow)')
        self.rejected('anchor')

    def test_missing_future_phase(self):
        self.replace('doc/roadmap.md', '### Phase 040 —', '### Phase 059 —')
        self.rejected('runtime set')

    def test_completed_details_are_immutable(self):
        self.replace('doc/roadmap.md', 'Provide fixed-width types', 'Provide modified types')
        self.rejected('completed phase detail digest')

    def test_dependency_drift(self):
        self.replace('doc/roadmap.md', '| 043 | Meshlet Derived Data | Asset | 024, 025, 026, 028 |', '| 043 | Meshlet Derived Data | Asset | 024, 025, 026, 029 |')
        self.rejected('dependencies')

    def test_unknown_graph_node_fails_without_crashing(self):
        self.replace('doc/roadmap.md', '    P031 --> P033', '    P999 --> P033')
        self.rejected('unknown graph')

    def test_missing_milestones(self):
        self.replace('doc/roadmap.md', '#### Delivery Milestones', '#### Not Milestones')
        self.rejected('Delivery Milestones')

    def test_missing_prompt(self):
        self.replace('doc/roadmap.md', 'Implement Renderer Volumetric Clouds on Features', 'See above. Volumetric Clouds on Features')
        self.rejected('self-contained')

    def test_stale_active_meshlet_reference(self):
        p = self.root / 'AGENTS.md'
        p.write_text(p.read_text() + '\nFeature 031 Meshlet Derived Data is next.\n')
        self.rejected('stale active')

    def test_historical_reference_is_not_active(self):
        p = self.root / 'specs/029-hdr-output-transform/spec.md'
        p.write_text(p.read_text() + '\nHistorical delivery reference: Feature 031 Meshlet Derived Data.\n')
        self.assertEqual(SCAN.scan(self.root)['findings'], [])

    def test_unknown_task_reference(self):
        p = self.root / 'specs/002-engine-development-roadmap/quickstart.md'
        p.write_text(p.read_text() + '\nComplete T999 before starting.\n')
        self.rejected('unknown task reference T999')

    def test_effect_order(self):
        self.replace('doc/roadmap.md', '038 DOF -> 031 TAA', '031 TAA -> 038 DOF')
        self.rejected('DOF/TAA')

    def test_virtual_shadow_fallback(self):
        self.replace('doc/roadmap.md', 'Retain Feature 032 shadow maps/CSM when unavailable or over budget.', 'Remove all conventional fallback.')
        self.rejected('virtual-shadow fallback')

    def test_shadow_acronym_separation(self):
        self.replace('doc/roadmap.md', 'Use VarianceShadowMaps for that technique; reserve VirtualShadowMaps for page-based virtualization.', 'Use VSM for everything.')
        self.rejected('shadow terminology')

    def test_index_owner_drift(self):
        p = self.root / 'specs/002-engine-development-roadmap/phase-index.json'
        import json
        data = json.loads(p.read_text())
        data['futurePhases'][0]['layer'] = 'Asset'
        p.write_text(json.dumps(data))
        self.rejected('owner/title')

    def test_no_early_full_profiling_dependency(self):
        self.replace('doc/roadmap.md',
            '| 032 | Raster Shadow Maps & Cascades | Renderer | 013, 015, 017, 019, 027, 028, 029, 030 |',
            '| 032 | Raster Shadow Maps & Cascades | Renderer | 013, 015, 017, 019, 027, 028, 029, 030, 041 |')
        self.rejected('full profiling must follow')

    def test_acceptance_requires_profiling(self):
        self.replace('doc/roadmap.md',
            '| 042 | Complete Rendering Pipeline Integration & Quality Baseline | Renderer | 028, 031, 036, 038, 039, 040, 041 |',
            '| 042 | Complete Rendering Pipeline Integration & Quality Baseline | Renderer | 028, 031, 036, 038, 039, 040 |')
        self.rejected('explicitly require full profiling')

    def test_stale_active_profiling_reference(self):
        p = self.root / 'AGENTS.md'
        p.write_text(p.read_text() + '\\nFeature 031 Frame Profiling & Render Diagnostics is next.\\n')
        self.rejected('stale active profiling')

    def test_profiling_covers_delivered_effects(self):
        self.replace('doc/roadmap.md',
            '| 041 | Frame Profiling & Render Diagnostics | Renderer | 008, 013, 018, 019, 027, 029, 036, 038, 039, 040 |',
            '| 041 | Frame Profiling & Render Diagnostics | Renderer | 008, 013, 018, 019, 027, 029 |')
        self.rejected('profiling must cover completed effect')

    def test_missing_layout(self):
        self.replace('doc/roadmap.md', '## Complete Rendering Pipeline Layout', '## Unspecified Pipeline')
        self.rejected('pipeline layout')

    def test_prompt_dependency_drift(self):
        self.replace('doc/roadmap.md',
            'Implement Renderer Raster Shadow Maps & Cascades on Features 013, 015, 017, 019, 027, 028, 029, 030.',
            'Implement Renderer Raster Shadow Maps & Cascades on Features 013, 015, 017, 019, 027, 028, 029, 030, 041.')
        self.rejected('prompt dependencies differ')

    def test_stale_virtual_shadow_identity(self):
        p = self.root / 'AGENTS.md'
        p.write_text(p.read_text() + '\nFeature 038 Virtual Shadow Maps is next.\n')
        self.rejected('stale active 3.0.1 phase reference')

    def test_interactive_ui_input_arbitration(self):
        self.replace('doc/roadmap.md', 'explicit UI capture arbitration', 'unrestricted input delivery')
        self.rejected('UI input arbitration')

    def test_ui_hdr_composition(self):
        self.replace('doc/roadmap.md', 'Display-linear UI composition after scene post-processing', 'UI composition inside scene lighting')
        self.rejected('HDR UI composition')

    def test_ui_scene_evidence_isolation(self):
        self.replace('doc/roadmap.md', 'Formal scene captures default to UI disabled', 'Formal scene captures include arbitrary widgets')
        self.rejected('UI-free scene evidence')

    def test_ui_readback_not_a_per_frame_requirement(self):
        self.replace('doc/roadmap.md', 'Interactive native presentation must not require synchronous CPU readback every frame', 'Interactive native presentation always requires readback')
        self.rejected('interactive native presentation')

    def test_stale_temporal_owner(self):
        p = self.root / 'AGENTS.md'
        p.write_text(p.read_text() + '\nFeature 030 Anti-Aliasing & Temporal Reconstruction is next.\n')
        self.rejected('stale active 3.0.1 phase reference')

    def test_ui_requires_hands_on_review(self):
        self.replace('doc/roadmap.md', 'maintainer hands-on navigation/control review', 'automatically accepted interaction')
        self.rejected('hands-on acceptance')

    def test_taa_requires_interactive_lab(self):
        self.replace('doc/roadmap.md',
            '| 031 | Anti-Aliasing & Temporal Reconstruction | Renderer | 004, 013, 015, 017, 019, 028, 029, 030 |',
            '| 031 | Anti-Aliasing & Temporal Reconstruction | Renderer | 004, 013, 015, 017, 019, 028, 029 |')
        self.rejected('missing interactive lab dependency')

    def test_ui_layout_before_output_transfer(self):
        self.replace('doc/roadmap.md', '| UI composition |', '| Undefined widgets |')
        self.rejected('UI must precede sole output transfer')


if __name__ == '__main__':
    unittest.main()
