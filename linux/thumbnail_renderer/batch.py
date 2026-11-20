"""Prepare inventory thumbnails in up to three independent renderer processes."""
from collections import deque
from pathlib import Path
import json, os, queue, re, subprocess, sys, threading, time, traceback

ROOT=Path(__file__).resolve().parent
sys.path[:0]=[str(ROOT),str(ROOT/'vendor')]
PROGRESS=ROOT/'progress.txt'
LOG=ROOT/'thumbnail-errors.log'
MAX_WORKERS=3

def available_cpus():
    try: count=len(os.sched_getaffinity(0))
    except (AttributeError,OSError): count=os.cpu_count() or 1
    # Honour container CPU quotas when they are smaller than the affinity mask.
    try:
        quota,period=Path('/sys/fs/cgroup/cpu.max').read_text().split()[:2]
        if quota!='max':count=min(count,max(1,int(quota)//int(period)))
    except (OSError,ValueError,ZeroDivisionError):pass
    return max(1,count)

def worker_count(cpus,override=None):
    if override:
        try:return min(MAX_WORKERS,max(1,int(override)))
        except ValueError:pass
    # Keep capacity available for the desktop and Steam, especially in VMs.
    return min(MAX_WORKERS,max(1,cpus//2))

def worker_environment():
    env=os.environ.copy()
    # Software OpenGL and numerical libraries can otherwise spawn many threads
    # per process, multiplying the load inside a VM.
    for name in ('OPENBLAS_NUM_THREADS','OMP_NUM_THREADS','MKL_NUM_THREADS',
                 'NUMEXPR_NUM_THREADS','VECLIB_MAXIMUM_THREADS','LP_NUM_THREADS'):
        env[name]='1'
    return env

def progress(message):
    PROGRESS.write_text(message,encoding='utf-8')

def locate_tf():
    candidates=[]
    override=os.environ.get('TF2_TF_DIR')
    if override:candidates.append(Path(override))
    home=Path.home()
    steam_roots=[Path(value) for key in ('STEAM_DIR','STEAM_ROOT')
                 if (value:=os.environ.get(key))]
    steam_roots.extend([home/'.steam/steam',home/'.local/share/Steam',
                        home/'.var/app/com.valvesoftware.Steam/.local/share/Steam'])
    for steam in steam_roots:
        libraries=[steam];file=steam/'steamapps/libraryfolders.vdf'
        if file.is_file():
            for encoded in re.findall(r'"path"\s*"([^\"]+)"',file.read_text(encoding='utf-8',errors='replace'),re.I):
                libraries.append(Path(encoded.replace('\\\\','\\')))
        candidates.extend(library/'steamapps/common/Team Fortress 2/tf' for library in libraries)
    for path in candidates:
        if (path/'tf2_misc_dir.vpk').is_file() and (path/'scripts/protodefs/proto_defs.vpd').is_file():return path
    raise RuntimeError('Team Fortress 2 installation was not found; existing item images will be used.')

def watch_parent(pid):
    """Exit if Item Manager terminates the coordinator during a refresh or exit."""
    def wait():
        while os.getppid()==pid:
            time.sleep(0.5)
        os._exit(2)
    threading.Thread(target=wait,daemon=True).start()

def worker(index,parent_pid,tf):
    watch_parent(parent_pid)
    from tf2_assets.renderer import Renderer
    weapons={int(item['id']):item for item in json.loads((ROOT/'catalog.json').read_text(encoding='utf-8'))['weapons']}
    renderer=Renderer(tf,ROOT/'model-cache'/('worker-'+str(index)))
    try:
        for line in sys.stdin:
            task=json.loads(line);key=task['key']
            try:
                if not re.fullmatch(r'[0-9a-f-]+',key):raise ValueError('Invalid cache key')
                item=weapons.get(int(task['definition']))
                if not item:raise ValueError('Weapon model is not in the renderer catalog')
                image,_,_=renderer.render(item['model'],wear=float(task['wear']),seed=int(task['seed']),
                    team='red',paint_id=int(task['paint']),width=256,height=160,texture_size=512,
                    thumbnail=True,stat_clock=bool(task.get('stat_clock')),
                    stat_clock_scale=float(task.get('stat_clock_scale',1)),stat_clock_count=int(task.get('stat_clock_count',0)))
                cache=ROOT/'cache';cache.mkdir(exist_ok=True)
                dest=cache/(key+'-display.png');tmp=cache/(key+'.worker-'+str(index)+'.tmp')
                image.save(tmp,format='PNG');os.replace(tmp,dest)
                result={'key':key,'ok':True}
            except Exception as exc:
                result={'key':key,'ok':False,'error':str(exc)}
            finally:
                renderer.images.clear()
                for texture_key in list(renderer.gpu_textures):
                    if texture_key[0]=='paint':renderer.gpu_textures.pop(texture_key).release()
            print(json.dumps(result),flush=True)
    finally:renderer.close()

def main():
    entries=json.loads((ROOT/'manifest.json').read_text(encoding='utf-8-sig'))['items']
    cache=ROOT/'cache';cache.mkdir(exist_ok=True)
    unique={};errors=[]
    for entry in entries:
        key=entry.get('key','')
        if not isinstance(key,str) or not re.fullmatch(r'[0-9a-f-]+',key):
            errors.append('Item has an invalid cache key');continue
        unique.setdefault(key,entry)
    total=len(unique)+len(errors);complete=0;failures=len(errors)
    pending=deque(entry for key,entry in unique.items() if not (cache/(key+'-display.png')).is_file())
    complete=len(errors)+len(unique)-len(pending)
    def update():progress(f'Preparing War Paint thumbnails... {complete} / {total}' + (f' ({failures} unavailable)' if failures else ''))
    update()
    with LOG.open('w',encoding='utf-8') as log:
        for error in errors:log.write(error+'\n')
        if not pending:
            progress(f'Thumbnails ready: {complete} / {total}' + (f' ({failures} unavailable)' if failures else ''))
            return
        tf=str(locate_tf());events=queue.Queue();processes={};active={};errlines={};readers=[]
        def read_output(index,proc):
            try:
                for line in proc.stdout:
                    try:events.put(('result',index,json.loads(line)))
                    except json.JSONDecodeError:events.put(('diagnostic',index,line.rstrip()))
            finally:
                code=proc.wait();events.put(('exit',index,code))
        def read_errors(index,proc):
            for line in proc.stderr:events.put(('diagnostic',index,line.rstrip()))
        def send(index):
            if not pending or index not in processes:return
            task=pending.popleft();active[index]=task
            try:
                processes[index].stdin.write(json.dumps(task)+'\n');processes[index].stdin.flush()
            except (BrokenPipeError,OSError):
                # Its exit event requeues this job for another worker.
                pass
        try:
            workers=worker_count(available_cpus(),os.environ.get('JIM_PREVIEW_WORKERS'))
            for index in range(min(workers,len(pending))):
                proc=subprocess.Popen([sys.executable,str(Path(__file__).resolve()),'--worker',str(index),str(os.getpid()),tf],
                    stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,encoding='utf-8',errors='replace',
                    bufsize=1,env=worker_environment(),creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
                processes[index]=proc
                for target in (read_output,read_errors):
                    thread=threading.Thread(target=target,args=(index,proc),daemon=True);thread.start();readers.append(thread)
                send(index)
            while processes:
                kind,index,value=events.get()
                if kind=='diagnostic':
                    if value:log.write(f'Worker {index}: {value}\n');log.flush()
                elif kind=='result':
                    task=active.pop(index,None)
                    if task is None or value.get('key')!=task['key']:
                        log.write(f'Worker {index}: unexpected response\n');continue
                    complete+=1
                    if not value.get('ok'):
                        failures+=1;log.write(f"Item definition {task.get('definition')} / paint {task.get('paint')}: {value.get('error','Unknown error')}\n");log.flush()
                    update();send(index)
                    if not pending and not active:
                        for proc in processes.values():
                            if not proc.stdin.closed:proc.stdin.close()
                elif kind=='exit':
                    proc=processes.pop(index,None);task=active.pop(index,None)
                    if task is not None:pending.appendleft(task)
                    if value:log.write(f'Worker {index} exited with code {value}.\n');log.flush()
                    if proc:
                        if not proc.stdin.closed:proc.stdin.close()
                        proc.stdout.close();proc.stderr.close()
                    # Idle workers receive tasks requeued after another worker exits.
                    for other in list(processes):
                        if other not in active and not processes[other].stdin.closed:send(other)
            if pending:
                failures+=len(pending);complete+=len(pending)
                for item in pending:log.write(f"No worker available for item {item.get('definition')} / paint {item.get('paint')}.\n")
        finally:
            for proc in processes.values():
                if proc.poll() is None:proc.terminate()
            for proc in processes.values():proc.wait()
    progress(f'Thumbnails ready: {total-failures} / {total}' + (f' ({failures} unavailable)' if failures else ''))

if __name__=='__main__':
    try:
        if len(sys.argv)>1 and sys.argv[1]=='--worker':worker(int(sys.argv[2]),int(sys.argv[3]),sys.argv[4])
        else:main()
    except Exception:
        if len(sys.argv)>1 and sys.argv[1]=='--worker':traceback.print_exc()
        else:
            LOG.write_text(traceback.format_exc(),encoding='utf-8')
            progress('Thumbnail renderer unavailable; using existing item images.')
        raise SystemExit(1)
