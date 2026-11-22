"""Segmentation models: a configurable U-Net written from scratch, and an ImageNet-pretrained ResNet34 U-Net."""
import torch, torch.nn as nn, torch.nn.functional as F


def norm_layer(kind, ch):
    if kind == "batch": return nn.BatchNorm2d(ch)
    if kind == "instance": return nn.InstanceNorm2d(ch, affine=True)
    if kind == "group": return nn.GroupNorm(8, ch)
    raise ValueError(kind)


class ConvBlock(nn.Module):
    """(3x3 conv -> norm -> ReLU) x 2"""
    def __init__(self, cin, cout, norm):
        super().__init__()
        self.body = nn.Sequential(
            nn.Conv2d(cin, cout, 3, padding=1, bias=False), norm_layer(norm, cout), nn.ReLU(inplace=True),
            nn.Conv2d(cout, cout, 3, padding=1, bias=False), norm_layer(norm, cout), nn.ReLU(inplace=True))

    def forward(self, x): return self.body(x)


class Up(nn.Module):
    """x2 upsampling: transposed convolution, or bilinear interpolation followed by a 1x1 convolution."""
    def __init__(self, cin, cout, mode):
        super().__init__()
        self.mode = mode
        self.op = nn.ConvTranspose2d(cin, cout, 2, stride=2) if mode == "transpose" else nn.Conv2d(cin, cout, 1)

    def forward(self, x):
        if self.mode == "transpose": return self.op(x)
        return self.op(F.interpolate(x, scale_factor=2, mode="bilinear", align_corners=False))


class UNet(nn.Module):
    """U-Net whose first level works at the input resolution (no stem downsampling).

    depth = number of resolution levels (depth 4 -> 1, 1/2, 1/4, 1/8), base = channels of the first level.
    The full-resolution skip connection carries 1-3 pixel wide structures to the decoder unchanged.
    """
    def __init__(self, in_ch=1, depth=4, base=32, norm="batch", up="transpose"):
        super().__init__()
        chs = [base * 2 ** i for i in range(depth)]
        self.enc = nn.ModuleList([ConvBlock(in_ch if i == 0 else chs[i - 1], chs[i], norm) for i in range(depth)])
        self.ups = nn.ModuleList([Up(chs[i], chs[i - 1], up) for i in range(depth - 1, 0, -1)])
        self.dec = nn.ModuleList([ConvBlock(chs[i - 1] * 2, chs[i - 1], norm) for i in range(depth - 1, 0, -1)])
        self.head = nn.Conv2d(base, 1, 1)

    def forward(self, x):
        skips = []
        for i, block in enumerate(self.enc):
            x = block(x if i == 0 else F.max_pool2d(x, 2))
            skips.append(x)
        for up, dec in zip(self.ups, self.dec):
            skips.pop()
            x = dec(torch.cat([up(x), skips[-1]], dim=1))
        return self.head(x)


def build_model(cfg):
    if cfg["model"] == "unet":
        return UNet(in_channels(cfg), cfg.get("depth", 4), cfg.get("base", 32), cfg.get("norm", "batch"), cfg.get("up", "transpose"))
    if cfg["model"] == "resnet34":
        import segmentation_models_pytorch as smp
        return smp.Unet("resnet34", encoder_weights="imagenet" if cfg.get("pretrained", True) else None, in_channels=3, classes=1)
    if cfg["model"] == "mitb2":          # SegFormer MiT-B2 encoder (transformer, ImageNet) + U-Net decoder
        import segmentation_models_pytorch as smp
        return smp.Unet("mit_b2", encoder_weights="imagenet" if cfg.get("pretrained", True) else None, in_channels=3, classes=1)
    raise ValueError(cfg["model"])


def in_channels(cfg): return 1 if cfg["model"] == "unet" and not cfg.get("context", 0) else 3


def n_params(model): return sum(p.numel() for p in model.parameters())
