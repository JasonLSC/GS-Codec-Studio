import numpy as np
import pandas as pd
import os
import json
import torch
import matplotlib.pyplot as plt
import random

def visualize_random_samples(
    flat: np.ndarray,       # shape (M, 45)
    shn_masked: np.ndarray, # shape (C*K, 15)
    labels: np.ndarray,     # shape (M,)
    centroids: np.ndarray,  # shape (K*3, 15)
    num_clusters: int = 5,  # 시각화할 클러스터 수
    num_samples: int = 3,    # 클러스터당 시각화할 샘플 수
    output_path: str = "random_cluster_samples.png"  # 출력 파일 경로
):
    """각 클러스터에서 임의의 샘플을 선택하여 시각화"""
    K = centroids.shape[0] // 3 # 클러스터 개수
    # K = centroids.shape[0] # K 값을 centroids.shape[0]로 보정  # 이 줄은 삭제
    print(f"centroids.shape[0]: {centroids.shape[0]}")
    print(f"K: {K}")

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
        try:
            sample_idxs = random.sample(list(idxs), num_samples)
        except ValueError:  # len(idxs) < num_samples 일 때 발생
            print(f"Cluster {cluster_id} does not have enough samples ({len(idxs)} < {num_samples})")
            continue

        # 각 샘플에 대해 반복
        for j, sample_idx in enumerate(sample_idxs):
            # 각 클러스터에 해당하는 R, G, B 채널의 인덱스 계산
            cluster_start_index_r = cluster_id
            cluster_start_index_g = cluster_id + K
            cluster_start_index_b = cluster_id + 2 * K

            # SH 계수 가져오기 (각 채널에서 가져오기)
            sh_coeffs_r = centroids[cluster_start_index_r]
            sh_coeffs_g = centroids[cluster_start_index_g]
            sh_coeffs_b = centroids[cluster_start_index_b]

            # SH 계수를 하나로 합치기
            sh_coeffs = np.concatenate([sh_coeffs_r, sh_coeffs_g, sh_coeffs_b])

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

    print("Saved visualization: random_cluster_samples.png")



def load_data(compress_dir, raw_path):
    data = np.load(os.path.join(compress_dir, "shN.npz"))
    raw = np.load(raw_path)

    # RGB 채널별 centroid와 label 로드
    centroids_r = data["centroids_r"]
    labels_r = data["labels_r"]
    centroids_g = data["centroids_g"]
    labels_g = data["labels_g"]
    centroids_b = data["centroids_b"]
    labels_b = data["labels_b"]

    meta_all = json.load(open(os.path.join(compress_dir, "meta.json"), "r"))
    meta = meta_all["shN"]
    # dequantize centroids
    mins = torch.tensor(meta["mins"], dtype=torch.float32)
    maxs = torch.tensor(meta["maxs"], dtype=torch.float32)
    quant = meta["quantization"]
    centroids_r = (torch.tensor(centroids_r, dtype=torch.float32) /
                   (2 ** quant - 1) * (maxs[0] - mins[0]) + mins[0]).numpy()  # (K, D)
    centroids_g = (torch.tensor(centroids_g, dtype=torch.float32) /
                   (2 ** quant - 1) * (maxs[1] - mins[1]) + mins[1]).numpy()  # (K, D)
    centroids_b = (torch.tensor(centroids_b, dtype=torch.float32) /
                   (2 ** quant - 1) * (maxs[2] - mins[2]) + mins[2]).numpy()  # (K, D)

    shn_masked = np.concatenate([centroids_r, centroids_g, centroids_b], axis=0) # (C, K*C)

    flat = raw.reshape(-1, 45)             # (N, K*C)

    # 필요에 따라 labels를 합치는 코드 추가 (예: 시각화를 위해)
    labels = np.concatenate([labels_r, labels_g, labels_b])

    return flat, shn_masked, labels, np.concatenate([centroids_r, centroids_g, centroids_b], axis=0)



def plot_sh_coeffs(ax, sh_coeffs, title):
    """SH 계수를 막대 그래프로 표시합니다."""
    K = 15
    x = np.arange(K)
    width = 0.2

    # sh_coeffs가 1차원 배열인 경우, 각 채널별로 분리하여 처리
    ax.bar(x - width, sh_coeffs[:K], width, label="R", color="red")
    ax.bar(x, sh_coeffs[K:2*K], width, label="G", color="green")
    ax.bar(x + width, sh_coeffs[2*K:3*K], width, label="B", color="blue")

    ax.set_xticks(x)
    ax.set_xticklabels([f"SH{i}" for i in range(K)])
    ax.set_ylabel("Coefficient Value")
    ax.set_title(title)
    ax.legend()
    ax.grid(True)



if __name__ == "__main__":
    # 1) 압축 결과가 저장된 폴더
    compress_dir = "./results/Bartender_1/hevc/rp0_vq_rgb8/compression"
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
