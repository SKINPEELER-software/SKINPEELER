"""Draw the U-Net block diagram used in the README (assets/unet_architecture.png).

usage: python tools/draw_architecture.py [out.png]
The numbers come from src/models.py: UNet(in_ch=1, depth=5, base=32, norm="batch", up="bilinear").
"""
import sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch

DEPTH, BASE, SIZE = 5, 32, 512
ENC, DEC, INK, GRAY = "#dbe9f6", "#dff0df", "#1f2328", "#5b6470"
C_DOWN, C_UP, C_SKIP = "#c0392b", "#2e7d32", "#7f8c8d"
W, DX, DY = 1.25, 1.7, 1.25                      # block width, column step, row step


def height(level):                                # block height shrinks with resolution
    return 1.0 - 0.08 * level


def block(ax, x, level, channels, color):
    h = height(level); y = -level * DY
    ax.add_patch(FancyBboxPatch((x - W / 2, y - h / 2), W, h, boxstyle="round,pad=0.02,rounding_size=0.06",
                                fc=color, ec=INK, lw=1.1))
    ax.text(x, y + 0.08, f"{channels}", ha="center", va="center", fontsize=12, fontweight="bold", color=INK)
    ax.text(x, y - 0.20, "channels", ha="center", va="center", fontsize=7.5, color=GRAY)
    return y, h


def arrow(ax, p, q, color, style="-", rad=0.0):
    ax.add_patch(FancyArrowPatch(p, q, arrowstyle="-|>", mutation_scale=13, lw=1.6, color=color, linestyle=style,
                                 connectionstyle=f"arc3,rad={rad}", shrinkA=2, shrinkB=2))


def main(out):
    chs = [BASE * 2 ** i for i in range(DEPTH)]
    fig, ax = plt.subplots(figsize=(13, 7.2))
    xe = [1.6 + i * DX for i in range(DEPTH)]                         # encoder columns (last one = bottleneck)
    xd = [xe[-1] + (DEPTH - 1 - i) * DX for i in range(DEPTH - 1)]     # decoder column of level i
    # resolution labels on the left
    for i in range(DEPTH):
        r = SIZE // 2 ** i
        ax.text(-0.05, -i * DY, f"{r} × {r}", ha="center", va="center", fontsize=10, color=GRAY)
    ax.text(-0.05, -(DEPTH - 1) * DY - 0.55, "resolution", ha="center", va="center", fontsize=9, color=GRAY, style="italic")
    # encoder
    for i in range(DEPTH):
        block(ax, xe[i], i, chs[i], ENC)
        if i:
            arrow(ax, (xe[i - 1], -(i - 1) * DY - height(i - 1) / 2), (xe[i] - W / 2, -i * DY), C_DOWN, rad=0.25)
    # decoder
    for i in range(DEPTH - 2, -1, -1):
        block(ax, xd[i], i, chs[i], DEC)
        src_x = xe[-1] if i == DEPTH - 2 else xd[i + 1]
        arrow(ax, (src_x + W / 2, -(i + 1) * DY), (xd[i], -i * DY - height(i) / 2), C_UP, rad=0.25)
        arrow(ax, (xe[i] + W / 2, -i * DY), (xd[i] - W / 2, -i * DY), C_SKIP, style=(0, (5, 3)))
    # input and output
    ax.text(xe[0], 1.02, "input\n1 channel, HU clipped to [−200, 800] → [0, 1]", ha="center", va="bottom", fontsize=9.5, color=INK)
    arrow(ax, (xe[0], 1.0), (xe[0], height(0) / 2), INK)
    xo = xd[0] + 2.4
    ax.add_patch(FancyBboxPatch((xo - 0.5, -0.5), 1.0, 1.0, boxstyle="round,pad=0.02,rounding_size=0.06", fc="#fff3cd", ec=INK, lw=1.1))
    ax.text(xo, 0.08, "1", ha="center", va="center", fontsize=12, fontweight="bold", color=INK)
    ax.text(xo, -0.20, "channel", ha="center", va="center", fontsize=7.5, color=GRAY)
    arrow(ax, (xd[0] + W / 2, 0), (xo - 0.5, 0), INK)
    ax.text((xd[0] + W / 2 + xo - 0.5) / 2, 0.14, "1×1 conv", ha="center", va="bottom", fontsize=8.5, color=INK)
    ax.text(xo, 1.02, "output\nlogit → sigmoid → threshold 0.5\n= boundary-layer mask", ha="center", va="bottom", fontsize=9.5, color=INK)
    arrow(ax, (xo, 0.5), (xo, 1.0), INK)
    ax.text(xe[-1], -(DEPTH - 1) * DY - 0.72, "bottleneck", ha="center", va="top", fontsize=9, color=GRAY, style="italic")
    ax.text((xe[0] + xe[-2]) / 2 - 0.7, -(DEPTH - 1) * DY + 0.2, "encoder", ha="center", fontsize=12, color="#2c5d8a", fontweight="bold")
    ax.text((xd[0] + xd[-1]) / 2 + 0.7, -(DEPTH - 1) * DY + 0.2, "decoder", ha="center", fontsize=12, color="#2e7d32", fontweight="bold")
    # legend, placed under the diagram
    ly = -(DEPTH - 1) * DY - 1.45
    items = [("box", ENC, "encoder block: 2 × (3×3 conv → BatchNorm → ReLU)"),
             ("box", DEC, "decoder block: concatenate skip, then 2 × (3×3 conv → BatchNorm → ReLU)"),
             ("line", C_DOWN, "2×2 max pooling"), ("line", C_UP, "bilinear upsampling ×2 + 1×1 conv"),
             ("dash", C_SKIP, "skip connection (same resolution)")]
    pos = [(0.6, ly), (0.6, ly - 0.45), (12.2, ly), (12.2, ly - 0.45), (12.2, ly - 0.9)]
    for (kind, color, text), (x, y) in zip(items, pos):
        if kind == "box":
            ax.add_patch(FancyBboxPatch((x, y - 0.14), 0.5, 0.28, boxstyle="round,pad=0.01,rounding_size=0.04", fc=color, ec=INK, lw=0.9))
        else:
            ax.plot([x, x + 0.5], [y, y], color=color, lw=2.0, linestyle=(0, (5, 3)) if kind == "dash" else "-")
        ax.text(x + 0.68, y, text, ha="left", va="center", fontsize=9.5, color=INK)
    ax.text(0.6, ly - 0.9, "7.2 million parameters · first stage at input resolution, no stem downsampling", ha="left", va="center", fontsize=9.5, color=GRAY)
    ax.set_xlim(-0.4, xo + 1.9); ax.set_ylim(ly - 1.25, 2.05); ax.set_aspect("equal"); ax.axis("off")
    fig.savefig(out, dpi=200, bbox_inches="tight", pad_inches=0.15, facecolor="white")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "assets/unet_architecture.png")
