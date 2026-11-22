"""Run training jobs one at a time. A job is skipped when <run>/done.json exists, resumed when a checkpoint exists.
usage: run_queue.py <queue.json>    queue = [[run_name, "key=value ...", (optional) training script in src/], ...]"""
import json, os, signal, subprocess, sys, time, shutil
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STALE = 240
q = json.load(open(sys.argv[1])); logp = sys.argv[1].replace(".json", ".log")
def log(m):
    line = time.strftime("%m-%d %H:%M:%S ") + m; print(line, flush=True); open(logp, "a").write(line + "\n")
def avail_mb(): return next(int(l.split()[1]) // 1024 for l in open("/proc/meminfo") if l.startswith("MemAvailable"))
for name, args, *rest in q:
    script = rest[0] if rest else "train.py"
    run = f"{ROOT}/runs/{name}"
    if os.path.exists(run + "/done.json"): log(f"skip {name}"); continue
    if avail_mb() < 4000 or shutil.disk_usage(ROOT).free < 20 * 2**30: log("guard: low memory or disk, stop"); sys.exit(2)
    for attempt in (1, 2, 3):
        t0 = time.time(); os.makedirs(run, exist_ok=True)
        pr = subprocess.Popen([f"{ROOT}/.venv/bin/python", f"{ROOT}/src/{script}", run] + args.split(), stdout=open(run + "/stdout.log", "a"), stderr=subprocess.STDOUT, start_new_session=True)
        hb = run + "/heartbeat.json"; started = time.time()
        while pr.poll() is None:                       # watchdog: no heartbeat for STALE seconds = hung, kill the process group and resume
            time.sleep(15)
            last = os.path.getmtime(hb) if os.path.exists(hb) and os.path.getmtime(hb) > started else started
            if time.time() - last > STALE:
                log(f"{name} heartbeat stale {time.time()-last:.0f}s: killing process group {pr.pid}")
                os.killpg(pr.pid, signal.SIGKILL); pr.wait(); break
        rc = pr.returncode
        log(f"{name} attempt {attempt} rc={rc} {time.time()-t0:.0f}s" + (" best=" + str(json.load(open(run + '/done.json'))['best_val_dice']) if rc == 0 else ""))
        if rc == 0: break
        time.sleep(20)
log("queue finished")
