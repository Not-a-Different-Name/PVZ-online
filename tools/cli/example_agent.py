"""Small public-API example. Runs a new local game in CLI's isolated data root."""
import json
from pathlib import Path
import subprocess
import sys

home=Path(__file__).resolve().parent
entry=[str(home/'pvz-cli.exe')] if (home/'pvz-cli.exe').exists() else [sys.executable,str(home/'pvz-cli.py')]
if len(sys.argv)>1:
    entry+=['--data-root',sys.argv[1]]
with subprocess.Popen(entry,stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True,encoding='utf-8') as game:
    ready=json.loads(game.stdout.readline())
    if not ready.get('ok'):
        raise RuntimeError(ready)
    def call(cmd,**fields):
        game.stdin.write(json.dumps(dict(cmd=cmd,**fields))+'\n');game.stdin.flush()
        response=json.loads(game.stdout.readline())
        if not response.get('ok'):raise RuntimeError(response.get('error'))
        return response['state']
    # This is a demonstration of control, not a winning strategy.
    state=call('new_run',mode=0)
    for _ in range(60):
        if state['choice']:
            state=call('choice',index=0)
        elif state['run'] and (state['run']['plant_picks'] or state['run']['buff_picks']):
            state=call('step',frames=5)
        else:break
    state=call('step',frames=1500)
    if state['board'] and state['scene']==3:
        sunflower=next((p for p in state['board']['packets'] if p['seed']==1),None)
        if sunflower and sunflower['active'] and sunflower['cost']<=state['board']['sun']:
            state=call('plant',slot=sunflower['slot'],row=0,col=0)
        for _ in range(20):
            state=call('step',frames=100)
            if state['board']:
                for coin in state['board']['coins']:
                    state=call('collect',coin_id=coin['id'])
    call('save')
    print(json.dumps({'sun':state['board']['sun'] if state['board'] else None,'tick':state['tick']},ensure_ascii=False))
    call('quit')
