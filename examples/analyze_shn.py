import numpy as np
import pandas as pd
import os
import json
import torch
import matplotlib.pyplot as plt
import random

def visualize_random_samples(
    flat: np.ndarray,       # shape (M, 45)
    shn_masked: np.ndarray, # shape (M, 15, 3)
    labels: np.ndarray,     # shape (M,)
    centroids: np.ndarray,  # shape (K, 45)
    num_clusters: int = 5,  # 시각화할 클러스터 수
    num_samples: int = 3,    # 클러스터당 시각화할 샘플 수
    output_path: str = "random_cluster_samples.png"  # 출력 파일 경로
):
    """각 클러스터에서 임의의 샘플을 선택하여 시각화"""
    K = centroids.shape[0]  # 클러스터 개수
    print(f"Number of clusters (K): {K}")
    # 시각화할 클러스터 ID 선택
    cluster_ids = random.sample(range(K), min(num_clusters, K))

    # 전체 subplot 개수 계산
    total_subplots = len(cluster_ids) * num_samples

    # subplot 생성
    fig, axes = plt.subplots(len(cluster_ids), num_samples, figsize=(5 * num_samples, 5 * len(cluster_ids)))
    fig.suptitle("Random Samples from Clusters")

    # 각 클러스터에 대해 반복
    for i, cluster_id in enumerate(cluster_ids):
        # 해당 클러스터에 속하는 샘플의 인덱스
        idxs = np.where(labels == cluster_id)[0]

        # 샘플 수가 충분한지 확인
        if len(idxs) < num_samples:
            print(f"Cluster {cluster_id} has fewer than {num_samples} samples.")
            continue

        # 임의의 샘플 인덱스 선택
        sample_idxs = random.sample(list(idxs), num_samples)

        # 각 샘플에 대해 반복
        for j, sample_idx in enumerate(sample_idxs):
            # SH 계수 가져오기
            sh_coeffs = shn_masked[sample_idx]

            # subplot에 시각화
            if len(cluster_ids) == 1 and num_samples == 1:
                ax = axes  # axes가 2D 배열이 아닌 경우
            elif len(cluster_ids) == 1:
                ax = axes[j]  # axes가 1D 배열인 경우
            else:
                ax = axes[i, j]  # axes가 2D 배열인 경우
            plot_sh_coeffs(ax, sh_coeffs, f"Cluster {cluster_id}, Sample {j}")

    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    plt.savefig(output_path)
    plt.show()

    print(f"Saved visualization: {output_path}")

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

    return flat, shn_masked, labels, centroids


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
    compress_dir = "./results/Bartender_1/hevc/rp0_vq12/compression"
    # 2) 그 폴더에 함께 저장된 raw shN 파일
    raw_path = os.path.join(compress_dir, "shN_raw.npy")

    # 3) 데이터 로드
    #    flat: (M,45), shn_masked: (M,15,3), labels: (M,), centroids: (K,45)
    flat, shn_masked, labels, centroids = load_data(compress_dir, raw_path)

    # 4) 시각화
    visualize_random_samples(
        flat=flat,
        shn_masked=shn_masked,
        labels=labels,
        centroids=centroids,
        output_path=os.path.join(compress_dir, "random_cluster_samples.png"),
    )
