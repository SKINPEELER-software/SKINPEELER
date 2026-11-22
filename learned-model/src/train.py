"""Train one model on pseudo-labels with a leave-one-specimen-out split.

  test specimen  : never seen (its expert slice is scored later by evaluate.py)
  val specimen   : one of the remaining four; model/epoch selection uses Dice against its pseudo-labels
  train specimens: the other three

Resumable: a checkpoint is written atomically at the end of every epoch; rerunning the same command continues.
usage: train.py <run_dir> key=value ...      (the config is stored in <run_dir>/config.json)
"""
import csv, hashlib, json, os, sys, time
import numpy as np, torch
from torch.utils.data import DataLoader, Dataset, WeightedRandomSampler
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from models import build_model, n_params, in_channels
from losses import build_loss

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPECIMENS = ["CB20-0003", "GC20-1006", "MM20-0006", "240927_soft", "KNU240704-4"]
DEFAULT = dict(model="unet", depth=4, base=32, norm="batch", up="transpose", pretrained=True, loss="bce_dice",
               lr=3e-4, weight_decay=1e-4, batch=16, epochs=30, seed=42, test="", val="", labels="labels.json",
               label_kind="skin", hu_min=-200, hu_max=800, workers=4, max_train_slices=0, max_val_slices=0, context=0)


def load_specimen(spec, labels, kind):
    d = f"{ROOT}/data/{spec}"
    hu = np.load(f"{d}/hu.npy", mmap_mode="r")
    name = f"{kind}_b{float(labels[spec]):.2f}.npy" if kind == "skin" else f"{kind}.npy"
    y = np.load(f"{d}/{name}", mmap_mode="r")
    keep = np.array(json.load(open(f"{d}/include.json")), dtype=np.int64)
    return hu, y, keep


class Slices(Dataset):
    def __init__(self, specs, cfg, labels, train, limit=0):
        self.cfg, self.train, self.items, self.arr = cfg, train, [], {}
        for s in specs:
            hu, y, keep = load_specimen(s, labels, cfg["label_kind"])
            if limit and len(keep) > limit: keep = keep[np.linspace(0, len(keep) - 1, limit).astype(int)]
            self.arr[s] = (hu, y); self.items += [(s, int(i)) for i in keep]
        counts = {s: sum(1 for t in self.items if t[0] == s) for s in specs}
        self.weights = [1.0 / counts[s] for s, _ in self.items]          # equal total weight per specimen
        self.channels = in_channels(cfg)

    def __len__(self): return len(self.items)

    def __getitem__(self, k):
        s, i = self.items[k]; hu, y = self.arr[s]
        if self.cfg.get("context", 0):                                    # 2.5D: slices i-1, i, i+1 as channels (clamped at the ends)
            n = len(hu); x = np.stack([np.asarray(hu[min(max(j, 0), n - 1)], np.float32) for j in (i - 1, i, i + 1)])
        else:
            x = np.asarray(hu[i], np.float32)[None]
        x = np.clip((x - self.cfg["hu_min"]) / (self.cfg["hu_max"] - self.cfg["hu_min"]), 0, 1); t = np.asarray(y[i], np.float32)
        if self.train:                                                    # label-preserving, no interpolation
            r = np.random.randint(4); x, t = np.rot90(x, r, axes=(1, 2)), np.rot90(t, r)
            if np.random.rand() < 0.5: x, t = x[:, :, ::-1], t[:, ::-1]
            x = np.clip(x * np.random.uniform(0.95, 1.05) + np.random.uniform(-0.02, 0.02), 0, 1)
        x = torch.from_numpy(np.ascontiguousarray(x))
        if x.shape[0] != self.channels: x = x.repeat(self.channels, 1, 1)
        return x, torch.from_numpy(np.ascontiguousarray(t))[None]


@torch.no_grad()
def validate(model, loader):
    model.eval(); inter = psum = tsum = 0.0; per = []
    for x, t in loader:
        x, t = x.cuda(non_blocking=True), t.cuda(non_blocking=True)
        with torch.autocast("cuda", dtype=torch.float16): p = (model(x) > 0).float()
        i, a, b = (p * t).sum((1, 2, 3)), p.sum((1, 2, 3)), t.sum((1, 2, 3))
        inter += i.sum().item(); psum += a.sum().item(); tsum += b.sum().item()
        per += ((2 * i + 1e-6) / (a + b + 1e-6)).tolist()
    return 2 * inter / max(psum + tsum, 1), float(np.mean(per))


def atomic_save(obj, path):
    tmp = path + ".tmp"; torch.save(obj, tmp)
    with open(tmp, "rb") as f: os.fsync(f.fileno())
    os.replace(tmp, path)


def main():
    run = sys.argv[1]; os.makedirs(run, exist_ok=True)
    cfg = dict(DEFAULT)
    for a in sys.argv[2:]:
        k, v = a.split("=", 1); cfg[k] = type(DEFAULT[k])(v) if not isinstance(DEFAULT[k], bool) else v.lower() in ("1", "true")
    cfg_hash = hashlib.sha256(json.dumps(cfg, sort_keys=True).encode()).hexdigest()[:12]
    json.dump(cfg, open(f"{run}/config.json", "w"), indent=1)
    labels = json.load(open(f"{ROOT}/{cfg['labels']}"))
    train_specs = [s for s in SPECIMENS if s not in (cfg["test"], cfg["val"])]
    torch.backends.cudnn.benchmark = True
    tr = Slices(train_specs, cfg, labels, True, cfg["max_train_slices"]); va = Slices([cfg["val"]], cfg, labels, False, cfg["max_val_slices"])
    val_loader = DataLoader(va, batch_size=cfg["batch"], shuffle=False, num_workers=2, pin_memory=True, persistent_workers=True)
    model = build_model(cfg).cuda(); loss_fn = build_loss(cfg["loss"])
    opt = torch.optim.AdamW(model.parameters(), lr=cfg["lr"], weight_decay=cfg["weight_decay"])
    steps = (len(tr) // cfg["batch"]) * cfg["epochs"]
    sched = torch.optim.lr_scheduler.OneCycleLR(opt, max_lr=cfg["lr"], total_steps=steps, pct_start=0.05)
    scaler = torch.amp.GradScaler("cuda"); start, best = 0, -1.0
    ck = f"{run}/last.pt"
    if os.path.exists(ck):
        s = torch.load(ck, map_location="cuda", weights_only=False)
        assert s["cfg_hash"] == cfg_hash, "config differs from the checkpoint: refusing to resume"
        model.load_state_dict(s["model"]); opt.load_state_dict(s["opt"]); sched.load_state_dict(s["sched"]); scaler.load_state_dict(s["scaler"])
        start, best = s["epoch"] + 1, s["best"]
    log_new = not os.path.exists(f"{run}/log.csv")
    log = open(f"{run}/log.csv", "a", newline=""); w = csv.writer(log)
    if log_new: w.writerow(["epoch", "train_loss", "val_dice_pooled", "val_dice_per_slice", "lr", "seconds", "gpu_mem_mb", "resumed_from"])
    print(f"run {run} | params {n_params(model)/1e6:.1f}M | train {len(tr)} ({','.join(train_specs)}) | val {len(va)} ({cfg['val']}) | test {cfg['test']} | start epoch {start}", flush=True)
    # One loader for the whole run (persistent workers): re-forking workers every epoch hung once at epoch 26.
    sampler = WeightedRandomSampler(tr.weights, num_samples=(len(tr) // cfg["batch"]) * cfg["batch"], replacement=True)
    loader = DataLoader(tr, batch_size=cfg["batch"], sampler=sampler, num_workers=cfg["workers"], pin_memory=True, drop_last=True,
                        persistent_workers=True, worker_init_fn=lambda i: np.random.seed(cfg["seed"] * 1000 + start * 10 + i))
    for ep in range(start, cfg["epochs"]):
        t0 = time.time(); torch.manual_seed(cfg["seed"] + ep)
        model.train(); tot = 0.0; torch.cuda.reset_peak_memory_stats()
        for it, (x, t) in enumerate(loader):
            x, t = x.cuda(non_blocking=True), t.cuda(non_blocking=True)
            with torch.autocast("cuda", dtype=torch.float16): loss = loss_fn(model(x).float(), t)
            opt.zero_grad(set_to_none=True); scaler.scale(loss).backward(); scaler.step(opt); scaler.update(); sched.step()
            tot += loss.item()
            if it % 10 == 0: json.dump(dict(pid=os.getpid(), epoch=ep, step=it, time=time.time()), open(f"{run}/heartbeat.json", "w"))
        json.dump(dict(pid=os.getpid(), epoch=ep, step="val", time=time.time()), open(f"{run}/heartbeat.json", "w"))
        vd, vs = validate(model, val_loader)
        if vd > best:
            best = vd; atomic_save(dict(model=model.state_dict(), epoch=ep, val_dice=vd, cfg=cfg), f"{run}/best.pt")
        atomic_save(dict(model=model.state_dict(), opt=opt.state_dict(), sched=sched.state_dict(), scaler=scaler.state_dict(),
                         epoch=ep, best=best, cfg_hash=cfg_hash), ck)
        w.writerow([ep, round(tot / len(loader), 5), round(vd, 5), round(vs, 5), sched.get_last_lr()[0], round(time.time() - t0, 1),
                    torch.cuda.max_memory_allocated() // 2**20, start if ep == start and start > 0 else ""]); log.flush()
        print(f"epoch {ep} loss {tot/len(loader):.4f} val_dice {vd:.4f} (per-slice {vs:.4f}) best {best:.4f} {time.time()-t0:.0f}s", flush=True)
    json.dump(dict(done=True, best_val_dice=best, epochs=cfg["epochs"]), open(f"{run}/done.json", "w"))


if __name__ == "__main__":
    main()
