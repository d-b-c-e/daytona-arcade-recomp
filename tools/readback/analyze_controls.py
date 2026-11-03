"""Correlate actual guest MMIO returns and guest-written caches, not injector intent alone."""
import json, pathlib

def analyze(path, session, budget, program):
    rows=[json.loads(s) for s in pathlib.Path(path).read_text(encoding='utf-8').splitlines()]
    if len(rows)!=budget or [r['frame'] for r in rows]!=list(range(1,budget+1)):
        raise RuntimeError('incomplete/reordered ordinary-input trace')
    offered={r['frame']:r['offered'] for r in rows}
    previous=[0]*5; delivered=[0]*5
    for row in rows:
        if row['session']!=session or row['revoked'] or row['source']!='Controls::sample(private-keys)':
            raise RuntimeError('input epoch revoked, wrong nonce or wrong consumer')
        for channel in range(5):
            count=row['readCounts'][channel]; source=row['readFrames'][channel]
            if count<previous[channel] or source>row['frame']:
                raise RuntimeError('non-monotonic guest consumption identity')
            if count>previous[channel] and source:
                if row['guestValues'][channel]!=offered[source][channel]:
                    raise RuntimeError('guest returned value differs from its attributed offered frame')
                delivered[channel]+=1
            previous[channel]=count
    if not all(delivered): raise RuntimeError('some requested ADC/button channels were never polled')
    def any_match(first,last,condition):
        return any(condition(r) for r in rows if first<=r['frame']<=last)
    checks=[]
    def assertion(label,first,last,predicate):
        ok=any_match(first,last,predicate);checks.append(dict(label=label,first=first,last=last,pass_=ok))
        if not ok: raise RuntimeError('ordinary-input assertion missing: '+label)
    commands=[]
    for line in pathlib.Path(program).read_text(encoding='utf-8').splitlines():
        words=line.split()
        if not words or words[0].startswith('#') or words[0]=='frames':continue
        first,last=map(int,words[:2]);commands.append((first,last,words[2]))
    expected_gears={}; gear=1
    for first,last,action in sorted(c for c in commands if c[2].startswith('gear')):
        if action=='gear_up':gear=min(4,gear+1)
        elif action=='gear_down':gear=max(1,gear-1)
        else:gear=int(action[-1])
        expected_gears[first]=gear
    for first,last,action in commands:
        if action in ('steer_left','steer_right','accelerate','brake'):
            channel={'steer_left':0,'steer_right':0,'accelerate':1,'brake':2}[action]
            value=32 if action=='steer_left' else 224
            # Endpoint contract: twenty frames covers a full opposite-lock key ramp.
            begin=first+20 if channel==0 else first+8
            assertion(f'{action}@{first}: guest-read + raw/filtered guest cache',begin,last,
                lambda r,c=channel,v=value,b=begin: r['readFrames'][c]>=b and r['guestValues'][c]==v
                    and r['cacheAdc'][c]==v and r['cacheAverage'][c]==v*256)
            if channel==0 and last+40<=budget and not any(a.startswith('steer_') and f<=last+40 and t>=last+1 for f,t,a in commands):
                assertion(f'steering release@{last}',last+20,last+40,
                    lambda r:r['guestValues'][0]==128 and r['cacheAdc'][0]==128 and r['cacheAverage'][0]==32768)
        elif action in ('coin','start','view1'):
            bit={'coin':1,'start':16,'view1':32}[action]
            assertion(f'{action}@{first}: guest button/cache pressed',first,min(budget,last+4),
                lambda r,b=bit,f=first,t=last:f<=r['readFrames'][3]<=t and not r['guestValues'][3]&b and r['cacheButtons']&b)
        elif action.startswith('gear'):
            # Selected gear is mapper state; correlate its encoding with the guest's word.
            # This does not claim the vehicle's transmission mode or engaged gear.
            expected=expected_gears[first]; nibble={1:2,2:1,3:6,4:5}[expected]
            assertion(f'{action}@{first}: guest gearbox-input cache',first,min(budget,last+8),
                lambda r,f=first,n=nibble,g=expected:r['readFrames'][4]>=f and r['controlsGear']==g
                    and ((r['guestValues'][4]>>4)&7)==n and ((r['cacheButtons']>>20)&7)==((~n)&7))
        else: raise RuntimeError('analyzer has no assertion for this scripted action')
    assertion('final release: centred steer, pedals/buttons released',budget-100,budget,
        lambda r:r['guestValues'][:3]==[128,32,32] and r['cacheAdc']==[128,32,32]
            and r['cacheAverage']==[32768,8192,8192] and r['guestValues'][3]==255)
    transitions=[]
    old=None
    for row in rows:
        if row['gameMode']!=old:
            transitions.append(dict(frame=row['frame'],value=row['gameMode']));old=row['gameMode']
    return dict(result='pass',consumer='Controls::sample(private-keys) -> GameLoop -> masked MB8421 guest reads',
        frames=budget,deliveredObservations=delivered,checks=checks,modeTransitions=transitions,
        vehicleSpeedPose='unverified fields; no physics claim from input caches',
        engagedTransmission='unverified; gearbox input cache is not an engaged-gear witness',
        physicalInput='none',physicalAcceptance='untested')
