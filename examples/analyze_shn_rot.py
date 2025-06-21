import numpy as np
import pandas as pd
import os
import json
import torch
import matplotlib.pyplot as plt
import random

def visualize_random_samples_with_quats(
    flat: np.ndarray,       # shape (M, D) = (M, 45 + 4)
    shn_masked: np.ndarray, # shape (M, 15, 3)
    quats_masked: np.ndarray, # shape (M, 4)
    labels: np.ndarray,     # shape (M,)
    centroids: np.ndarray,  # shape (K, D)
    num_clusters: int = 5,
    num_samples: int = 3,
    output_path: str = "random_cluster_samples_w_quats.png"
):
    """각 클러스터에서 임의의 샘플을 선택하여 시각화 (shN + quats 포함)"""
    K = centroids.shape[0]
    print(f"Number of clusters (K): {K}")
    cluster_ids = random.sample(range(K), min(num_clusters, K))

    fig, axes = plt.subplots(len(cluster_ids) * 2, num_samples, figsize=(5 * num_samples, 3 * 2 * len(cluster_ids)))
    fig.suptitle("Random Cluster Samples (SH + Quats)", fontsize=16)

    for i, cluster_id in enumerate(cluster_ids):
        idxs = np.where(labels == cluster_id)[0]
        if len(idxs) < num_samples:
            print(f"Cluster {cluster_id} has fewer than {num_samples} samples.")
            continue
        sample_idxs = random.sample(list(idxs), num_samples)

        for j, sample_idx in enumerate(sample_idxs):
            sh_coeffs = shn_masked[sample_idx]
            quat = quats_masked[sample_idx]

            ax_sh = axes[i * 2, j] if len(cluster_ids) > 1 else axes[0, j]
            ax_q = axes[i * 2 + 1, j] if len(cluster_ids) > 1 else axes[1, j]

            plot_sh_coeffs(ax_sh, sh_coeffs, f"Cluster {cluster_id}, Sample {j}")
            plot_quats(ax_q, quat)

    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    plt.savefig(output_path)
    plt.show()
    print(f"Saved visualization: {output_path}")

def plot_quats(ax, quat):
    """회전 쿼터니언을 막대그래프로 시각화"""
    x = np.arange(4)
    ax.bar(x, quat, color="purple", alpha=0.7)
    ax.set_xticks(x)
    ax.set_xticklabels(["w", "x", "y", "z"])
    ax.set_ylabel("Quaternion")
    ax.set_ylim(-1.1, 1.1)
    ax.grid(True)
    ax.set_title("Rotation (quaternion)")

def load_data(compress_dir, shn_original_path):
    """데이터 로드 함수 (이전 코드와 동일)"""
    meta_all = json.load(open(os.path.join(compress_dir, "meta.json"), "r"))
    meta = meta_all["shN"]
    # mask
    bits = np.fromfile(os.path.join(compress_dir, "mask.bin"), dtype=np.uint8)
    mask = np.unpackbits(bits)[: meta["mask_bits"]].astype(bool)
    # npz
    data = np.load(os.path.join(compress_dir, "shN.npz"))
    labels = data["labels"]               # (M,)
    centroids_q = data["centroids"]       # (K, D)
    # dequantize centroids
    mins = torch.tensor(meta["mins"], dtype=torch.float32)
    maxs = torch.tensor(meta["maxs"], dtype=torch.float32)
    quant = meta["quantization"]
    centroids = (torch.tensor(centroids_q, dtype=torch.float32) /
                 (2 ** quant - 1) *
                 (maxs - mins) + mins).numpy()    # (K, D)

    # --- raw shN 로드 및 mask 적용 ---
    shn_all = np.load(shn_original_path)
    # 5D → 3D flatten
    if shn_all.ndim == 5:
        T, H, W, K, C = shn_all.shape
        shn_all = shn_all.reshape(T * H * W, K, C)
    elif shn_all.ndim != 3:
        raise ValueError(f"Unexpected raw SHN ndim={shn_all.ndim}")
    # mask 적용
    shn_masked = shn_all[mask]           # (M, 15, 3)
    flat = shn_masked.reshape(shn_masked.shape[0], -1)  # (M, 45)

    return flat, shn_masked, labels, centroids, mask


def plot_sh_coeffs(ax, sh_coeffs, title):
    """SH 계수를 막대 그래프로 시각화 (이전 코드와 동일)"""
    x = np.arange(15)
    width = 0.2

    ax.bar(x - width, sh_coeffs[:, 0], width, label="R", color="red")
    ax.bar(x, sh_coeffs[:, 1], width, label="G", color="green")
    ax.bar(x + width, sh_coeffs[:, 2], width, label="B", color="blue")

    ax.set_xlabel("SH Basis Function")
    ax.set_ylabel("Coefficient Value")
    ax.set_title(title)
    ax.set_xticks(x)
    ax.legend()
    ax.grid(True)


if __name__ == "__main__":
    # 1) 압축 결과가 저장된 폴더
    compress_dir = "./results/Bartender_1/hevc/rp0_rot12/compression"
    # 2) 그 폴더에 함께 저장된 raw shN 파일
    raw_path = os.path.join(compress_dir, "shN_raw.npy")
    

    # 3) 데이터 로드
    #    flat: (M,45), shn_masked: (M,15,3), labels: (M,), centroids: (K,45)
    flat, shn_masked, labels, centroids, mask = load_data(compress_dir, raw_path)

    quats_all = np.load(os.path.join(compress_dir, "quats_raw.npy"))  # [T*H*W, 4]
    quats_masked = quats_all[mask]

    # 4) 시각화
    visualize_random_samples_with_quats(
        flat=flat,
        shn_masked=shn_masked,
        quats_masked=quats_masked,
        labels=labels,
        centroids=centroids,
        output_path=os.path.join(compress_dir, "random_cluster_samples_w_quats.png"),
    )