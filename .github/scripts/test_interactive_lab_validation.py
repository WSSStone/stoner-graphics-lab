"""Focused 030 provenance/coverage mutations; inherited image checks stay shared."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import interactive_lab_validation as lab

REV = 'a' * 40
ROOT = Path(__file__).resolve().parents[2]
CASE = json.loads((ROOT / lab.COVERAGE).read_text())['enduranceCases'][1]

def native():
    return dict(schemaVersion=1, evidenceClass='local-native-script', humanStatus='pending-human-review',
        softwareRevision=REV, backend='metal', platform='macos', adapter='Apple M4 Pro', softwareDevice=False, discreteDevice=False,
        nativeAvailable=True, workload=CASE['workloadRevision'], cookedGeneration='b'*64,
        rootIdentity='StaticModel:Sponza.gltf#idx.scene.0',sourceDigest='d'*64,
        outputProfileId='Sdr.sRGB.v1', capabilityDigest='c'*64, settingsGeneration=1, displayGeneration=1,
        outputGeneration=1, frameToken=1120, width=320, height=180, logicalWidth=320, logicalHeight=180,
        completedRenderSubmissions=1120,provenPresentationReleases=0,finalAcquisitionRecords=0,
        scriptSha256='', scriptComplete=True, scriptSteps=0, completedScriptSteps=0,
        firstFailure='', passed=True, shutdownAssurance='Proven', retirementMode='NativeCallback',
        retirementReason=1, optionalFenceAdvertised=False, optionalFenceEnabled=False,
        submittedFrames=1120, presentQueuedFrames=1120, uiFrames=1120,
        presentQueuedByProfile={"Sdr.sRGB.v1":1120},outputTransferCount=1,
        imageReadbackCopies=0, readbackMaps=0, readbackWaits=0, liveQueueIdles=0, liveDeviceIdles=0,
        finalPresentationOwners=0, activeAttachmentBytes=0, activeUIDrawBytes=0, activeSlots=0, busySlots=0,
        captureRequests=0, captureStagingBytes=0, peakAttachmentBytes=100, peakUIDrawBytes=100,
        peakPresentationBytes=100, preCleanupPresentationOwners=0, residualOwners=0, abandonedOwners=0,
        terminalIdleCalls=0, terminalIdleCompleted=False, terminalIdleResult=0, terminalIdleNanoseconds=0,
        finalNativeOwners=0, presentationImageCount=3, retiringImageCount=0,
        nativeMetadataObserved=False, nativeSystemToneMapping=False, uiWhiteMultiplier=1,
        referenceWhiteNits=100, exposureStops=0)

class ValidationTests(unittest.TestCase):
    def test_native_contract(self):
        lab.verify_native(native(), CASE, REV)

    def test_native_mutations(self):
        mutations = [('softwareRevision','d'*40), ('passed',False), ('firstFailure','device lost'),
            ('shutdownAssurance','Forced'), ('shutdownAssurance','DeviceLost'), ('nativeAvailable',False),
            ('presentQueuedFrames',120), ('presentQueuedFrames',True), ('uiFrames',0),
            ('backend','vulkan'), ('outputProfileId','Hdr.Linear.2000.v1'), ('workload','wrong'),
            ('readbackMaps',1), ('readbackWaits',1), ('liveDeviceIdles',1), ('finalNativeOwners',1),
            ('peakAttachmentBytes',1073741825), ('peakPresentationBytes',536870913),
            ('presentationImageCount',9), ('retiringImageCount',9), ('scriptComplete',False), ('outputTransferCount',2),
            ('presentQueuedByProfile',{'Sdr.sRGB.v1':120,'Hdr.Linear.2000.v1':1000})]
        for key,value in mutations:
            with self.subTest(key=key,value=value), self.assertRaises(ValueError):
                v=native(); v[key]=value; lab.verify_native(v,CASE,REV)

    def test_fallback_is_qualified(self):
        v=native(); c=copy.deepcopy(CASE); c.update(backend='vulkan',platform='windows',laneId='windows-vulkan-discrete')
        v.update(backend='vulkan',platform='windows',discreteDevice=True,retirementMode='AcquireHistory',shutdownAssurance='IdleAssumed',
                 terminalIdleCalls=1,terminalIdleCompleted=True)
        lab.verify_native(v,c,REV)
        for edit in [dict(shutdownAssurance='Proven'),dict(optionalFenceEnabled=True),dict(terminalIdleCompleted=False)]:
            with self.subTest(edit=edit),self.assertRaises(ValueError):lab.verify_native(dict(v,**edit),c,REV)
        v.update(retirementMode='PresentationFence',shutdownAssurance='Proven',optionalFenceAdvertised=True,optionalFenceEnabled=True,
                 terminalIdleCalls=0,terminalIdleCompleted=False)
        lab.verify_native(v,c,REV)
        v['optionalFenceEnabled']=False
        with self.assertRaises(ValueError):lab.verify_native(v,c,REV)

    def test_lifecycle_cycles(self):
        case={'backend':'vulkan','requiredAdditionalCycles':{'uiToggle':1,'fontScaleReplacement':1,'modeTransition':1}}
        steps=[{'action':a,'value':v} for a,v in [('ui',0),('ui',1),('scale',1),('scale',2),
            ('profile','Sdr.BT709.v1'),('profile','Sdr.ExplicitGamma22.v1'),('profile','Sdr.sRGB.v1')]]
        lab.verify_cycles(steps,case)
        for i in range(len(steps)):
            with self.subTest(missing=i),self.assertRaises(ValueError):lab.verify_cycles(steps[:i]+steps[i+1:],case)

    def test_private_json_boundary(self):
        import verify_output_transform_architecture as architecture
        with tempfile.TemporaryDirectory() as t:
            root=Path(t)
            for relative,allowed in [('Demo/StonerDemo/Private/FLabInputScript.cpp',True),
                                     ('Demo/StonerDemo/Public/FPublic.h',False),
                                     ('Source/Renderer/Private/FRenderer.cpp',False)]:
                path=root/relative;path.parent.mkdir(parents=True,exist_ok=True);path.write_text('#include "yyjson/yyjson.h"\n')
                findings=[];architecture._feature_030_checks(root,findings)
                self.assertEqual(any(relative in f and 'yyjson' in f for f in findings),not allowed)
                path.unlink()

    def test_command_bounds(self):
        a=['Build/StonerDemo','--interactive-lab','--mode','validate','--frames','1120','--lab-report','Build/native.json']
        lab.validate_command(a,CASE)
        for v in [[], 'shell command', a+['--visible-capture'], a+['--lab-report','Build/other.json'],
                  a+['--lab-input-script','script.json'], ['x']*129]:
            with self.subTest(argv=v),self.assertRaises(ValueError): lab.validate_command(v,CASE)

    def test_bounded_json(self):
        with tempfile.TemporaryDirectory() as t:
            p=Path(t)/'bad.json'
            for text in ['{"x":1,"x":2}', '{"x":NaN}', ' '*1048577]:
                p.write_text(text)
                with self.assertRaises(ValueError): lab.load_json(p)

    def test_artifact_and_case_mutations(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t); (root/lab.COVERAGE).parent.mkdir(parents=True)
            (root/lab.COVERAGE).write_bytes((ROOT/lab.COVERAGE).read_bytes());(root/lab.LIMITS).write_bytes((ROOT/lab.LIMITS).read_bytes())
            n=root/'native.json'; n.write_text(json.dumps(native()))
            report={'schema':'stoner.interactive-lab-report','schemaVersion':1,'gitRevision':REV,
                    'caseId':CASE['caseId'],'gateKind':'endurance','status':'passed','errors':[],
                    'artifacts':[lab.artifact(n,root)],'nativeReport':'native.json','exitCode':0,'stdoutSha256':'0'*64,'stdoutBytes':0,
                    'coverageSha256':lab.sha256((root/lab.COVERAGE).read_bytes()),'limitsSha256':lab.sha256((root/lab.LIMITS).read_bytes()),'forcedTermination':False,
                    'compatibilityLimitation':'','warmupPresented':120,'measuredPresented':1000,
                    'session':dict(name='unavailable',display='unavailable',scanoutAuthority=False)}
            p=root/'report.json'; p.write_text(json.dumps(report));lab.verify(p,root)
            for edit in [dict(caseId='wrong-case'),dict(gateKind='smoke'),dict(status='failed'),dict(exitCode=124),dict(gitRevision='d'*40)]:
                p.write_text(json.dumps(dict(report,**edit)))
                with self.subTest(edit=edit),self.assertRaises(ValueError):lab.verify(p,root)
            for field,value in [('sha256','e'*64),('sizeBytes',1),('path','../native.json')]:
                bad=copy.deepcopy(report);bad['artifacts'][0][field]=value;p.write_text(json.dumps(bad))
                with self.subTest(field=field),self.assertRaises(ValueError):lab.verify(p,root)

    def test_run_guards_and_forced_exit(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t); (root/lab.COVERAGE).parent.mkdir(parents=True)
            (root/lab.COVERAGE).write_bytes((ROOT/lab.COVERAGE).read_bytes());(root/lab.LIMITS).write_bytes((ROOT/lab.LIMITS).read_bytes());(root/'Build').mkdir()
            command=root/'command.json'; output=root/'report.json'; npath=root/'Build/native.json'
            command.write_text(json.dumps({'caseId':CASE['caseId'],'gateKind':'endurance','nativeCommand':
                ['Build/StonerDemo','--interactive-lab','--mode','validate','--frames','1120','--lab-report','Build/native.json']}))
            def execute(*args):npath.write_text(json.dumps(native()));return 0,'0'*64,0
            with mock.patch.object(lab,'require_frozen_revision') as guard,mock.patch.object(lab,'_execute',side_effect=execute):
                self.assertEqual(lab.run(command,REV,output,root)['status'],'passed');self.assertEqual(guard.call_count,2)
            lab.verify(output,root)
            with mock.patch.object(lab,'require_frozen_revision'),mock.patch.object(lab,'_execute') as execute:
                with self.assertRaises(ValueError):lab.run(command,REV,output,root)
                execute.assert_not_called()
                output.unlink()
                with self.assertRaises(ValueError):lab.run(command,REV,output,root)
                execute.assert_not_called()
            npath.unlink()
            with mock.patch.object(lab,'require_frozen_revision'),mock.patch.object(lab,'_execute',return_value=(124,'0'*64,0)):
                self.assertEqual(lab.run(command,REV,output,root)['status'],'failed')
            with self.assertRaises(ValueError):lab.verify(output,root)
            output.unlink()
            with mock.patch.object(lab,'require_frozen_revision',side_effect=[None,ValueError('software changed')]),mock.patch.object(lab,'_execute',side_effect=lambda *a:(0,'0'*64,0)):
                with self.assertRaises(ValueError):lab.run(command,REV,output,root)
            self.assertFalse(output.exists())

    def test_missing_human_never_closes(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t); (root/lab.COVERAGE).parent.mkdir(parents=True)
            (root/lab.COVERAGE).write_bytes((ROOT/lab.COVERAGE).read_bytes());(root/lab.LIMITS).write_bytes((ROOT/lab.LIMITS).read_bytes())
            p=root/'bundle.json';p.write_text(json.dumps({'schema':'stoner.interactive-lab-bundle','schemaVersion':1,
                'gitRevision':REV,'artifacts':[],'reports':[],'formalSdr':[],'humanDecisions':[],'hosted':[]}))
            result=lab.closeout(p,root,REV)
            self.assertFalse(result['complete']);self.assertTrue(any('human' in x for x in result['missing']))

    def test_human_request_link(self):
        req={'schema':'stoner.interactive-lab-human-request','schemaVersion':1,'gitRevision':REV,'gateId':'gate',
             'nativeReportSha256':'b'*64,'adapter':'Apple M4 Pro','outputProfileId':'Sdr.sRGB.v1','workloadRevision':'production-content-lantern-v3',
             'settingsGeneration':1,'frameToken':1120,'uiWhiteMultiplier':1,'referenceWhiteNits':100,'exposureStops':0}
        decision={'schema':'stoner.interactive-lab-human-decision','schemaVersion':1,'gitRevision':REV,'gateId':'gate',
                  'requestSha256':lab.sha256(lab.canonical(req)),'decision':'accepted','reviewer':'maintainer',
                  'observedAt':'2026-09-15T12:00:00Z','observations':'Current live scene and controls inspected.'}
        lab.verify_human(req,decision,lab.canonical(req),REV)
        for edit in [dict(decision='rejected'),dict(requestSha256='f'*64),dict(gitRevision='d'*40),dict(reviewer='')]:
            with self.subTest(edit=edit),self.assertRaises(ValueError):lab.verify_human(req,dict(decision,**edit),lab.canonical(req),REV)

if __name__=='__main__':unittest.main()
