import torch, torch.nn.functional as F


def dice_loss(logits, target, eps=1.0):
    p = torch.sigmoid(logits); dims = (1, 2, 3)
    inter = (p * target).sum(dims); den = p.sum(dims) + target.sum(dims)
    return 1 - ((2 * inter + eps) / (den + eps)).mean()


def focal_loss(logits, target, gamma=2.0, alpha=0.75):
    bce = F.binary_cross_entropy_with_logits(logits, target, reduction="none")
    pt = torch.exp(-bce); w = alpha * target + (1 - alpha) * (1 - target)
    return (w * (1 - pt) ** gamma * bce).mean()


def build_loss(name):
    if name == "bce_dice": return lambda l, t: F.binary_cross_entropy_with_logits(l, t) + dice_loss(l, t)
    if name == "focal_dice": return lambda l, t: focal_loss(l, t) + dice_loss(l, t)
    if name == "dice": return dice_loss
    raise ValueError(name)
