#!/usr/bin/env python3
"""Bounded 030 run/verify/closeout. Machine checks never author human decisions."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import threading
import time
from typing import Any
from output_transform_provenance import require_frozen_revision, artifact, canonical, validate_sdr_bundle
from verify_output_transform_evidence import load_bounded_json, validate_artifacts, validate_output_report, validate_sdr_baseline, _keys

COVERAGE = 'Config/Validation/InteractiveLab/Coverage-v1.json'
LIMITS = 'Config/Validation/InteractiveLab/Limits-v1.json'
HOSTED = {'windows-debug','windows-release','linux-debug','linux-release','macos-debug','macos-release',
          'linux-asan-ubsan','linux-tsan','linux-native','macos-native','medium-integration','shader-producer','shader-consumer'}
FALLBACK_LIMITATION = 'Terminal idle permits compatibility cleanup only; presentation release and physical scanout are not proven.'
SHA = re.compile(r'^[0-9a-f]{64}$')
REV = re.compile(r'^[0-9a-f]{40}$')

def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def require(condition: bool, message: str) -> None:
    if not condition: raise ValueError(message)

def load_json(path: Path) -> dict:
    value, errors, _ = load_bounded_json(path)
    require(not errors and isinstance(value,dict), '; '.join(errors) or 'JSON must be an object')
    return value

def schema_keys(value: dict, name: str) -> None:
    definition=load_json(Path(__file__).resolve().parents[2]/'Config/Validation/InteractiveLab/Report-v1.schema.json')['$defs'][name]
    errors=_keys(value,definition['required'],definition['properties'],name)
    require(not errors,'; '.join(errors))

def count(v: dict, key: str, low: int=0, high: int=2**63-1) -> int:
    n=v.get(key)
    require(type(n) is int and low<=n<=high, f'{key}: invalid bounded count')
    return n

def coverage(root: Path) -> dict:
    return load_json(root/COVERAGE)

def cases(root: Path) -> dict:
    c=coverage(root)
    return {x['caseId']:x for group in ('enduranceCases','smokeCases','lifecycleCases','hostedCases') for x in c[group]}

def verify_native(v: dict, case: dict, revision: str) -> None:
    schema_keys(v,"nativeReport")
    require(v.get("evidenceClass")=="local-native-script" and v.get("humanStatus")=="pending-human-review","native report cannot claim human authority")
    require(type(v.get('schemaVersion')) is int and v.get('schemaVersion')==1 and v.get('passed') is True and v.get('firstFailure')=='','native run failed')
    require(v.get('nativeAvailable') is True,'native execution unavailable')
    if not case.get('hosted',False):require(v.get('softwareDevice') is False,'physical case requires non-software device')
    if case.get('laneId')=='windows-vulkan-discrete':require(v.get('discreteDevice') is True,'Windows physical lane requires observed discrete adapter')
    if case.get('laneId')=='macos-metal-m4':require(str(v.get('adapter','')).startswith('Apple M4'),'physical M4 lane requires observed M4 adapter')
    require(v.get('backend')==case['backend'] and v.get('platform')==case['platform'],'wrong platform/backend case')
    require(v.get('workload')==case['workloadRevision'],'wrong workload case')
    require(v.get('outputProfileId')==case.get('outputProfileId',case.get('initialOutputProfileId')),'wrong effective output')
    require(isinstance(v.get('adapter'),str) and 0<len(v['adapter'])<=256,'missing adapter identity')
    require(v.get('rootIdentity')=={'lantern':'StaticModel:Lantern.glb#idx.scene.0','sponza':'StaticModel:Sponza.gltf#idx.scene.0'}.get(case['workloadId']),'wrong strict-cooked root')
    for key in ('cookedGeneration','capabilityDigest','sourceDigest'):
        require(isinstance(v.get(key),str) and bool(SHA.fullmatch(v[key])),f'invalid {key}')
    for key in ('settingsGeneration','displayGeneration','outputGeneration','frameToken'):count(v,key,1)
    count(v,'logicalWidth',1,4096);count(v,'logicalHeight',1,4096)
    count(v,'completedRenderSubmissions');count(v,'provenPresentationReleases')
    require(count(v,'finalAcquisitionRecords')==0,'retained acquisition records')
    w=count(v,'width',1,4096);h=count(v,'height',1,4096);require(w*h<=7864320,'drawable budget exceeded')
    count(v,'presentQueuedFrames',case.get('frames',{}).get('totalPresented',1))
    require(count(v,'submittedFrames')>=v['presentQueuedFrames'],'presentation exceeds submissions')
    count(v,'uiFrames',1,v['submittedFrames']);count(v,'outputTransferCount',1,1)
    profiles=v.get('presentQueuedByProfile')
    require(isinstance(profiles,dict) and 1<=len(profiles)<=7 and all(type(n) is int and n>=0 for n in profiles.values()),'invalid per-profile presentation counts')
    require(sum(profiles.values())==v['presentQueuedFrames'],'profile count total mismatch')
    if case['gateKind']!='lifecycle':require(profiles.get(v['outputProfileId'],0)>=case['frames']['totalPresented'],'profile smoke cannot masquerade as endurance')
    for key in ('imageReadbackCopies','readbackMaps','readbackWaits','liveQueueIdles','liveDeviceIdles',
                'finalPresentationOwners','activeAttachmentBytes','activeUIDrawBytes','activeSlots','busySlots',
                'captureRequests','captureStagingBytes','residualOwners','abandonedOwners','finalNativeOwners'):
        require(count(v,key)==0,f'{key}: nonzero forbidden/quiescent counter')
    count(v,'peakAttachmentBytes',0,1073741824);count(v,'peakUIDrawBytes',0,18874368)
    count(v,'peakPresentationBytes',0,536870912)
    count(v,'presentationImageCount',0,8);count(v,'retiringImageCount',0,8)
    require(v.get('scriptComplete') is True and count(v,'scriptSteps',0,256)==count(v,'completedScriptSteps',0,256),'script incomplete')
    if case['gateKind']!='lifecycle':require(v['scriptSteps']==0,'smoke/endurance cannot substitute a settings script')
    mode=v.get('retirementMode');assurance=v.get('shutdownAssurance')
    advertised=v.get('optionalFenceAdvertised');enabled=v.get('optionalFenceEnabled')
    require(type(advertised) is bool and type(enabled) is bool,'optional capability inventory missing')
    count(v,'retirementReason',1,11);count(v,'preCleanupPresentationOwners',0,16)
    count(v,'terminalIdleNanoseconds');count(v,'terminalIdleCalls',0,1)
    if case['backend']=='metal':require(mode=='NativeCallback' and assurance=='Proven','Metal release proof missing')
    elif mode=='PresentationFence':require(advertised and enabled and assurance=='Proven','presentation fence not enabled/proven')
    elif mode=='AcquireHistory':
        require(not enabled and assurance=='IdleAssumed','fallback cannot claim presentation proof')
        require(v['terminalIdleCalls']==1 and v.get('terminalIdleCompleted') is True and v.get('terminalIdleResult')==0,'fallback terminal idle failed')
    else:raise ValueError('unknown presentation retirement mode')
    required_mode=case.get('requiredPresentationRetirementMode')
    if case.get('presentationRetirementSelection')=='acquire-history':required_mode='AcquireHistory'
    if required_mode:require(mode==required_mode,'required forced fallback not exercised')
    if v['outputProfileId'].startswith('Hdr.'):
        require(v.get('nativeMetadataObserved') is True and v.get('nativeSystemToneMapping') is False,'HDR native metadata policy unobserved/incorrect')
    for key in ('uiWhiteMultiplier','referenceWhiteNits','exposureStops'):
        require(type(v.get(key)) in (int,float) and abs(v[key])<100000,f'invalid {key}')

    require(bool(REV.fullmatch(revision)) and v.get('softwareRevision')==revision,'stale or non-frozen native software')

def validate_command(argv: Any, case: dict) -> dict:
    require(isinstance(argv,list) and 1<=len(argv)<=128 and all(isinstance(a,str) and 0<len(a)<=4096 and '\0' not in a for a in argv),'invalid bounded argv')
    require(sum(map(len,argv))<=65536 and Path(argv[0]).name.lower() in {'stonerdemo','stonerdemo.exe'},'expected Demo executable')
    options={};i=1
    flags={'--interactive-lab','--enable-validation'}
    forbidden={'--visible-capture','--production-camera-preview','--production-capture-root','--output-native-probe','--output-native-profile','--camera-preset-output'}
    while i<len(argv):
        key=argv[i];require(key.startswith('--') and key not in options and key not in forbidden,'duplicate/forbidden argument')
        if key in flags:options[key]=True;i+=1
        else:
            require(i+1<len(argv) and not argv[i+1].startswith('--'),'missing argument value')
            options[key]=argv[i+1];i+=2
    require(options.get('--interactive-lab') is True and options.get('--mode')=='validate','native command is not bounded lab validation')
    require(str(options.get('--frames','')).isdigit() and int(options['--frames'])>=case.get('frames',{}).get('totalPresented',1),'insufficient frame budget')
    require('--lab-report' in options,'native report path required')
    if case['gateKind']!='lifecycle':require('--lab-input-script' not in options,'smoke/endurance script override forbidden')
    return options

def verify(path: Path, root: Path) -> dict:
    v=load_json(path);schema_keys(v,'report')
    require(v.get('schema')=='stoner.interactive-lab-report' and v.get('schemaVersion')==1,'wrong report schema')
    require(v.get('status')=='passed' and v.get('errors')==[] and v.get('exitCode')==0,'failed or incomplete report')
    count(v,'exitCode',0,0); count(v,'stdoutBytes',0,1064960)
    require(isinstance(v.get('stdoutSha256'),str) and SHA.fullmatch(v['stdoutSha256']),'stdout digest missing')
    require(v.get('coverageSha256')==sha256((root/COVERAGE).read_bytes()),'coverage identity changed')
    require(v.get('limitsSha256')==sha256((root/LIMITS).read_bytes()),'limits identity changed')
    c=cases(root).get(v.get('caseId'));require(c is not None and v.get('gateKind')==c['gateKind'],'wrong case/gate')
    errors=validate_artifacts(v.get('artifacts'),root);require(not errors,'; '.join(errors))
    require(v.get('nativeReport') in {a['path'] for a in v['artifacts']},'native report not linked')
    n=load_json(root/v['nativeReport']);verify_native(n,c,v.get('gitRevision',''))
    require(v.get('forcedTermination') is False,'forced termination cannot pass')
    require(v.get('compatibilityLimitation')==(FALLBACK_LIMITATION if n['retirementMode']=='AcquireHistory' else ''),'missing/incorrect compatibility qualification')
    warmup=c.get('frames',{}).get('warmupPresented',0)
    require(count(v,'warmupPresented')==warmup and count(v,'measuredPresented')==n['presentQueuedFrames']-warmup,'false warmup/measured accounting')
    session=v.get('session');require(isinstance(session,dict) and set(session)=={'name','display','scanoutAuthority'} and session['scanoutAuthority'] is False,'session provenance missing')
    require(all(isinstance(session[k],str) and len(session[k])<=256 for k in ('name','display')),'unbounded session')
    if c.get('laneId')=='windows-vulkan-discrete':require(session['name']=='Console' or session['name'].startswith('RDP-'),'physical Windows session must identify Console/RDP')
    if c['gateKind']=='lifecycle':
        require(v.get('script') in {a['path'] for a in v['artifacts']},'lifecycle script not linked')
        s=load_json(root/v['script']);require(sha256((root/v['script']).read_bytes())==n['scriptSha256'],'script digest mismatch')
        require(len(s.get('steps',[]))==n['scriptSteps'],'script step count mismatch')
        verify_cycles(s['steps'],c)
    return v

def verify_cycles(steps: list, case: dict) -> None:
    need=case.get('cycles',case.get('requiredAdditionalCycles',{}))
    def values(action):return [s.get('value') for s in steps if s.get('action')==action]
    def pairs(sequence, down, up):
        armed=False;total=0
        for value in sequence:
            if value==down:armed=True
            elif value==up and armed:total+=1;armed=False
        return total
    scale=values('scale')
    require(not scale or all(type(x) in (int,float) and 0.5<=x<=4 for x in scale),'invalid scale cycle')
    counts={'resize':len(values('resize')),'uiToggle':pairs(values('ui'),0,1),
            'focusRecovery':pairs(values('focus'),0,1),
            'minimizeRestore':pairs([s['action'] for s in steps if s['action'] in {'minimize','restore'}],'minimize','restore'),
            'fontScaleReplacement':sum(1 for i in range(1,len(scale),2) if scale[i]!=scale[i-1])}
    sequence=case.get('profileSequence',[])
    if not sequence and need.get('modeTransition') and case['backend']=='vulkan':
        sequence=['Sdr.sRGB.v1','Sdr.BT709.v1','Sdr.ExplicitGamma22.v1','Sdr.sRGB.v1']
    if sequence:
        expected=sequence[1:];observed=values('profile')
        cycles=need.get('modeTransition',1)
        require(observed==expected*cycles,'mode transition sequence/cycles mismatch')
        counts['modeTransition']=cycles
    for key,n in need.items():require(counts.get(key,0)>=n,f'missing lifecycle cycles: {key}')
    rejection=case.get('unavailableProfileRejection')
    if rejection:require(rejection['profileId'] in values('rejectProfile'),'unavailable profile rejection missing')

def verify_human(request: dict, decision: dict, request_bytes: bytes, revision: str) -> None:
    schema_keys(request,'humanRequest');schema_keys(decision,'humanDecision')
    require(request.get('schema')=='stoner.interactive-lab-human-request' and decision.get('schema')=='stoner.interactive-lab-human-decision','wrong human schema')
    require(request.get('schemaVersion')==1 and decision.get('schemaVersion')==1,'wrong human schema version')
    require(request.get('gitRevision')==decision.get('gitRevision')==revision,'stale human software')
    require(request.get('gateId')==decision.get('gateId') and decision.get('requestSha256')==sha256(request_bytes),'human request link mismatch')
    require(decision.get('decision')=='accepted','human decision missing or rejected')
    for key in ('settingsGeneration','frameToken'):count(request,key,1)
    for key in ('uiWhiteMultiplier','referenceWhiteNits','exposureStops'):
        require(type(request.get(key)) in (int,float) and abs(request[key])<100000,f'missing human context {key}')
    for key,bound in [('reviewer',256),('observations',4096),('observedAt',64)]:
        require(isinstance(decision.get(key),str) and 0<len(decision[key].strip())<=bound,f'missing/bounded human {key}')

def expected_hosted_checks() -> dict:
    return {'linux-native':{'build','native-ui','native-lab'},'macos-native':{'build','native-ui','native-lab'},
            'medium-integration':{'build','cook','native-lab'},'shader-producer':{'build','derive','cook','shader-verify'},
            'shader-consumer':{'strict-load','shader-verify'}}

def closeout(path: Path, root: Path, revision: str) -> dict:
    b=load_json(path);schema_keys(b,'bundle');require(b.get('schema')=='stoner.interactive-lab-bundle' and b.get('schemaVersion')==1 and b.get('gitRevision')==revision and bool(REV.fullmatch(revision)),'wrong bundle identity')
    errors=validate_artifacts(b.get('artifacts'),root);require(not errors,'; '.join(errors))
    for key,limit in [('reports',19),('formalSdr',4),('humanDecisions',12),('hosted',13)]:
        require(isinstance(b.get(key),list) and len(b[key])<=limit,f'bounded bundle {key} required')
    linked={a['path']:a for a in b['artifacts']};missing=[];c=coverage(root)
    def document(p):require(p in linked,'unlinked bundle document');return load_json(root/p)
    found={};native={}
    for p in b.get('reports',[]):
        document(p);r=verify(root/p,root);require(r['gitRevision']==revision and r['caseId'] not in found,'duplicate/stale machine case')
        found[r['caseId']]=r;native[linked[p]['sha256']]=load_json(root/r['nativeReport'])
    missing.extend('machine: '+k for k,case in cases(root).items() if not case.get('hosted') and k not in found)
    formal={}
    for item in b.get('formalSdr',[]):
        r=document(item['report']);record=document(item['baseline'])
        require(item['gateId'] not in formal,'duplicate formal gate')
        errors=validate_output_report(r,root)+validate_sdr_baseline(record)+validate_sdr_bundle(r,record,root,validate_sdr_baseline)
        require(not errors and r['gitRevision']==revision and record.get('state')=='accepted','invalid current formal SDR authority: '+'; '.join(errors))
        formal[item['gateId']]=r
    for gate in c['formalSdrGates']:
        r=formal.get(gate['gateId'])
        if not r:missing.append('formal: '+gate['gateId'])
        else:require(r['backend']==gate['backend'] and r['workloadRevision']==gate['workloadRevision'],'wrong formal case')
    human={}
    for item in b.get('humanDecisions',[]):
        req=document(item['request']);dec=document(item['decision']);verify_human(req,dec,(root/item['request']).read_bytes(),revision)
        n=native.get(req.get('nativeReportSha256'));require(n is not None,'human decision has no current native report')
        require(all(req.get(k)==n[k] for k in ('settingsGeneration','frameToken','uiWhiteMultiplier','referenceWhiteNits','exposureStops')),'human settings/frame context mismatch')
        require(req.get('adapter')==n['adapter'] and req.get('workloadRevision')==n['workload'] and req.get('outputProfileId')==n['outputProfileId'],'human context differs from native report')
        require(req['gateId'] not in human,'duplicate human gate');human[req['gateId']]=req
    for gate in c['currentHumanNavigationGates']+c['currentHumanHdrGates']:
        r=human.get(gate['gateId'])
        if not r:missing.append('human: '+gate['gateId'])
        else:require(r['workloadRevision']==gate['workloadRevision'] and r['outputProfileId']==gate['outputProfileId'],'wrong human case')
    hosted=set()
    for p in b.get('hosted',[]):
        r=document(p);schema_keys(r,'hosted');require(r.get('schema')=='stoner.interactive-lab-hosted' and r.get('gitRevision')==revision and r.get('passed') is True,'failed/stale hosted result')
        require(r.get('jobId') in HOSTED and r['jobId'] not in hosted,'unknown/duplicate hosted job')
        require(isinstance(r.get('runUrl'),str) and r['runUrl'].startswith('https://github.com/') and '/actions/runs/' in r['runUrl'],'hosted run link missing')
        require(isinstance(r.get('checks'),list) and r['checks'] and len(r['checks'])<=32 and all(isinstance(x,dict) and set(x)=={'name','exitCode','logSha256'} and type(x.get('exitCode')) is int and x['exitCode']==0 and isinstance(x.get('name'),str) and isinstance(x.get('logSha256'),str) and SHA.fullmatch(x['logSha256']) for x in r['checks']),'hosted checks missing/failed')
        expected={'linux-native':{'hosted-linux-lantern-auto','hosted-linux-lantern-fallback'},
                  'macos-native':{'hosted-macos-lantern'},'medium-integration':{'hosted-linux-sponza'}}.get(r['jobId'],set())
        artifacts=r.get('artifacts',[]);errors=validate_artifacts(artifacts,root);require(not errors,'; '.join(errors))
        names={a['path'] for a in artifacts};observed=set()
        for path in r.get('nativeReports',[]):
            require(path in names,'hosted native report is not digest-linked')
            result=verify(root/path,root);require(result['gitRevision']==revision,'stale hosted native software');observed.add(result['caseId'])
        require(observed==expected,'hosted native cases missing or mismatched')
        labels={x['name'] for x in r['checks']}
        required={'build','focused','architecture','validator'} if r['jobId'].endswith(('debug','release')) else {'build','focused'}
        if r['jobId'] in expected_hosted_checks():required=expected_hosted_checks()[r['jobId']]
        require(required<=labels,'hosted checks incomplete')
        hosted.add(r['jobId'])
    missing.extend('hosted: '+x for x in sorted(HOSTED-hosted))
    return {'schema':'stoner.interactive-lab-aggregate','schemaVersion':1,'gitRevision':revision,'complete':not missing,'missing':missing}

def _execute(argv: list[str], root: Path, timeout: int, max_output: int=1048576, echo: bool=False) -> tuple[int,str,int]:
    process=subprocess.Popen(argv,cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    digest=hashlib.sha256();size=0;overflow=False
    def read():
        nonlocal size,overflow
        assert process.stdout is not None
        for chunk in iter(lambda:process.stdout.read(16384),b''):
            size+=len(chunk)
            if size>max_output:overflow=True;process.kill();break
            digest.update(chunk)
            if echo:sys.stdout.write(chunk.decode('utf-8',errors='replace'));sys.stdout.flush()
    reader=threading.Thread(target=read,daemon=True);reader.start()
    try:code=process.wait(timeout=timeout)
    except subprocess.TimeoutExpired:process.kill();process.wait();code=124
    reader.join(timeout=2)
    if reader.is_alive():return 125,digest.hexdigest(),size
    process.stdout.close()
    return (125 if overflow else code),digest.hexdigest(),size

def write_new(path: Path, value: dict) -> None:
    data=canonical(value);require(len(data)<=1048576,'report exceeds 1 MiB')
    with path.open('xb') as f:f.write(data)

def run(command_file: Path, revision: str, output: Path, root: Path) -> dict:
    require_frozen_revision(root,revision)
    require(not output.exists(),'stale output path')
    command=load_json(command_file);c=cases(root).get(command.get('caseId'));require(c is not None,'unknown case')
    require(command.get('gateKind')==c['gateKind'],'wrong declared gate kind')
    argv=command.get('nativeCommand');options=validate_command(argv,c)
    npath=(root/options['--lab-report']).resolve();npath.relative_to(root.resolve())
    require(not npath.exists() and npath!=output.resolve(),'stale/native output collision')
    if command.get('humanRequest'):
        human_path=(root/command['humanRequest']).resolve();human_path.relative_to(root.resolve())
        require(not human_path.exists() and human_path not in {npath,output.resolve()},'stale/human output collision')
    timeout=command.get('timeoutSeconds',600);require(type(timeout) is int and 10<=timeout<=600,'invalid command timeout')
    code,digest,size=_execute(argv,root,timeout)
    require_frozen_revision(root,revision)
    report={'schema':'stoner.interactive-lab-report','schemaVersion':1,'gitRevision':revision,'caseId':c['caseId'],
            'gateKind':c['gateKind'],'status':'failed','errors':[],'artifacts':[],
            'nativeReport':npath.relative_to(root.resolve()).as_posix(),'coverageSha256':sha256((root/COVERAGE).read_bytes()),'limitsSha256':sha256((root/LIMITS).read_bytes()),
            'exitCode':code,'stdoutSha256':digest,'stdoutBytes':size,
            'forcedTermination':code!=0,'compatibilityLimitation':'', 'warmupPresented':0,'measuredPresented':0,
            'session':{'name':os.environ.get('SESSIONNAME','unavailable')[:256],
                       'display':os.environ.get('DISPLAY','unavailable')[:256],'scanoutAuthority':False}}
    try:
        require(code==0,'native process failed/forced/timed out: '+str(code))
        n=load_json(npath);verify_native(n,c,revision);report['artifacts']=[artifact(npath,root)]
        if '--lab-input-script' in options:
            script=(root/options['--lab-input-script']).resolve();report['script']=script.relative_to(root.resolve()).as_posix();report['artifacts'].append(artifact(script,root))
            verify_cycles(load_json(script)['steps'],c)
            require(sha256(script.read_bytes())==n['scriptSha256'],'script changed during execution')
        report['compatibilityLimitation']=FALLBACK_LIMITATION if n['retirementMode']=='AcquireHistory' else ''
        report['warmupPresented']=c.get('frames',{}).get('warmupPresented',0)
        report['measuredPresented']=n['presentQueuedFrames']-report['warmupPresented']
        report['status']='passed'
    except (ValueError,OSError,KeyError) as error:report['errors']=[str(error)]
    write_new(output,report)
    if report['status']=='passed' and command.get('humanRequest'):
        require(not c.get('hosted'),'hosted case cannot prepare physical human authority')
        gate=next((x for x in coverage(root)['currentHumanNavigationGates']+coverage(root)['currentHumanHdrGates'] if x['gateId']==command.get('humanGateId')),None)
        require(gate is not None and gate['backend']==n['backend'] and gate['workloadRevision']==n['workload'] and gate['outputProfileId']==n['outputProfileId'],'human request gate mismatch')
        request={'schema':'stoner.interactive-lab-human-request','schemaVersion':1,'gitRevision':revision,
                 'gateId':gate['gateId'],'nativeReportSha256':sha256(output.read_bytes()),'adapter':n['adapter'],
                 'outputProfileId':n['outputProfileId'],'workloadRevision':n['workload']}
        for key in ('settingsGeneration','frameToken','uiWhiteMultiplier','referenceWhiteNits','exposureStops'):request[key]=n[key]
        write_new(root/command['humanRequest'],request)
    return report

def main() -> int:
    p=argparse.ArgumentParser();sub=p.add_subparsers(dest='command',required=True)
    for name in ('run','verify','closeout'):
        q=sub.add_parser(name);q.add_argument('--root',type=Path,default=Path.cwd())
        if name=='run':q.add_argument('--command-file',type=Path,required=True);q.add_argument('--output',type=Path,required=True)
        elif name=='verify':q.add_argument('--report',type=Path,required=True)
        else:q.add_argument('--manifest',type=Path,required=True);q.add_argument('--output',type=Path)
        if name!='verify':q.add_argument('--git-revision',required=True)
    a=p.parse_args()
    try:
        if a.command=='run':r=run(a.command_file,a.git_revision,a.output,a.root);ok=r['status']=='passed'
        elif a.command=='verify':r=verify(a.report,a.root);ok=True
        else:
            r=closeout(a.manifest,a.root,a.git_revision);ok=r['complete']
            if a.output:write_new(a.output,r)
        print(json.dumps(r,sort_keys=True));return 0 if ok else 1
    except (ValueError,OSError,KeyError,TypeError,subprocess.SubprocessError) as e:p.exit(1,str(e)+'\n')
if __name__=='__main__':raise SystemExit(main())
