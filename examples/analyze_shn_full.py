import os
import json
import random
import numpy as np
import torch
import matplotlib.pyplot as plt

def load_cluster_data(compress_dir: str):
    """압축 디렉토리에서 클러스터 정보 및 관련 데이터 로드"""
    # 1) meta.json 불러오기
    meta_path = os.path.join(compress_dir, "meta.json")
    meta_all = json.load(open(meta_path, "r"))
    
    # 2) cluster_info.json 불러오기 (있는 경우)
    cluster_info_path = os.path.join(compress_dir, "cluster_info.json")
    if not os.path.exists(cluster_info_path):
        raise ValueError(f"클러스터 정보 파일이 없습니다: {cluster_info_path}")
    
    cluster_info = json.load(open(cluster_info_path, "r"))
    print(f"클러스터 정보 키: {list(cluster_info.keys())}")
    
    # 3) 대용량 배열 로드 (필요한 경우)
    for k, v in cluster_info.items():
        if isinstance(v, str) and v.startswith("saved_as_cluster_"):
            npy_path = os.path.join(compress_dir, v.replace("saved_as_", ""))
            if os.path.exists(npy_path):
                cluster_info[k] = np.load(npy_path)
                print(f"로드됨: {k} - 모양: {cluster_info[k].shape}")
            else:
                print(f"경고: {npy_path} 파일이 없습니다.")
    
    # 4) 클러스터 라벨 정보 확인
    if "cluster_labels" not in cluster_info:
        print("경고: 클러스터 라벨 정보가 없습니다.")
        
    # 5) SH 차원 정보 가져오기
    sh_dim = meta_all.get("shN", {}).get("shape_shN", [0, 0, 0, 15, 3])[3]
    
    return cluster_info, meta_all, sh_dim

def sample_cluster_data(compress_dir, cluster_info, sh_dim=15, num_clusters=5, samples_per_cluster=3):
    """클러스터 정보에서 샘플 데이터 추출 (마스크 처리 고려)"""
    # 클러스터 라벨이 없으면 샘플링 불가
    if "cluster_labels" not in cluster_info:
        print("클러스터 라벨 정보가 없습니다.")
        return {}
    
    # 원본 데이터 로드
    shN_quat_path = os.path.join(compress_dir, "shN_quat_raw.npy")
    if not os.path.exists(shN_quat_path):
        print(f"경고: 원본 SH+쿼터니언 데이터 파일이 없습니다: {shN_quat_path}")
        return {}
        
    # 원본 데이터 로드
    combined_data = np.load(shN_quat_path)  # (N, 49)
    print(f"원본 데이터 모양: {combined_data.shape}")
    
    # 마스크와 라벨 처리
    labels = cluster_info["cluster_labels"]
    mask = cluster_info.get("mask")
    
    if mask is not None:
        if isinstance(mask, list):
            mask = np.array(mask, dtype=bool)
        
        # 마스크와 라벨 길이 불일치 처리
        if len(mask) != len(labels):
            print(f"마스크({len(mask)})와 라벨({len(labels)})의 길이가 다릅니다.")
            print(f"마스킹된 데이터 개수: {np.sum(mask)}")
            
            # 마스킹된 데이터만 클러스터링되었을 것이므로,
            # 마스크에서 True인 항목만 라벨에 해당함
            true_indices = np.where(mask)[0]
            if len(true_indices) == len(labels):
                print(f"마스크 True 개수와 라벨 수 일치: {len(true_indices)} = {len(labels)}")
            else:
                print("경고: 마스크 True 개수와 라벨 수 불일치!")
    else:
        print("마스크 정보가 없습니다.")
    
    # 클러스터별 분포 확인
    unique_clusters, counts = np.unique(labels, return_counts=True)  
    print(f"총 클러스터 수: {len(unique_clusters)}")  
    print(f"가장 큰 클러스터 크기: {np.max(counts)}")  
    print(f"가장 작은 클러스터 크기: {np.min(counts)}")  
    
    # 랜덤 클러스터 선택 (단, 최소 크기가 samples_per_cluster 이상인 클러스터만)  
    valid_clusters = unique_clusters[counts >= samples_per_cluster]  
    if len(valid_clusters) > num_clusters:  
        selected_clusters = np.random.choice(valid_clusters, num_clusters, replace=False)  
    else:  
        selected_clusters = valid_clusters  
    
    print(f"선택된 랜덤 클러스터: {selected_clusters}")  
    
    # 실제로 존재하는 클러스터 중에서 선택
    if len(unique_clusters) > num_clusters:
        # 다양한 크기의 클러스터를 선택하기 위해 정렬 후 간격을 두고 선택
        sorted_clusters = sorted(zip(unique_clusters, counts), key=lambda x: x[1], reverse=True)
        step = len(sorted_clusters) // num_clusters
        selected_clusters = [c[0] for c in sorted_clusters[::step][:num_clusters]]
    else:
        selected_clusters = unique_clusters
    
    samples = {}
    for cluster_id in selected_clusters:
        # 클러스터에 속한 인덱스 찾기
        cluster_indices = np.where(labels == cluster_id)[0]
        
        # 샘플 선택
        if len(cluster_indices) > samples_per_cluster:
            sample_indices = np.random.choice(cluster_indices, samples_per_cluster, replace=False)
        else:
            sample_indices = cluster_indices
        
        # 데이터 추출
        cluster_samples = []
        for idx in sample_indices:
            # 마스크가 있으면 원래 인덱스로 변환
            if mask is not None:
                true_indices = np.where(mask)[0]
                if idx < len(true_indices):
                    original_idx = true_indices[idx]
                    if original_idx < len(combined_data):
                        cluster_samples.append(combined_data[original_idx])
            else:
                if idx < len(combined_data):
                    cluster_samples.append(combined_data[idx])
        
        if cluster_samples:
            samples[cluster_id] = np.array(cluster_samples)
    
    return samples

def plot_sh_coeffs(ax, sh, title):
    """SH 계수 시각화"""
    x = np.arange(sh.shape[0])
    w = 0.2
    ax.bar(x - w, sh[:,0], w, label="R", color="red")
    ax.bar(x, sh[:,1], w, label="G", color="green")
    ax.bar(x + w, sh[:,2], w, label="B", color="blue")
    ax.set_xticks(x)
    ax.set_title(title)
    ax.legend()
    ax.grid(True)

def plot_quats(ax, quat):
    """쿼터니언 시각화"""
    x = np.arange(4)
    ax.bar(x, quat, alpha=0.7)
    ax.set_xticks(x)
    ax.set_xticklabels(["w","x","y","z"])
    ax.set_ylim(-1.1,1.1)
    ax.set_title("Quaternion")
    ax.grid(True)

def visualize_cluster_samples(
    samples,
    sh_dim=15,
    output_path="cluster_visualization.png"
):
    """클러스터 샘플 시각화"""
    cluster_ids = list(samples.keys())
    if not cluster_ids:
        print("시각화할 샘플이 없습니다.")
        return
    
    num_clusters = len(cluster_ids)
    num_samples = max([len(samples[c]) for c in cluster_ids])
    
    fig, axes = plt.subplots(num_clusters*2, num_samples, figsize=(4*num_samples, 2.5*2*num_clusters))
    fig.suptitle("Cluster Samples (SH + Quats)", fontsize=16)
    
    for i, cluster_id in enumerate(cluster_ids):
        cluster_samples = samples[cluster_id]
        for j in range(min(num_samples, len(cluster_samples))):
            sample = cluster_samples[j]
            
            # SH 계수와 쿼터니언 분리
            sh_coeffs = sample[:sh_dim*3].reshape(sh_dim, 3)
            quat = sample[sh_dim*3:]
            
            if num_clusters == 1 and num_samples == 1:
                ax_sh = axes[0]
                ax_q = axes[1]
            elif num_clusters == 1:
                ax_sh = axes[0, j]
                ax_q = axes[1, j]
            elif num_samples == 1:
                ax_sh = axes[2*i]
                ax_q = axes[2*i+1]
            else:
                ax_sh = axes[2*i, j]
                ax_q = axes[2*i+1, j]
            
            plot_sh_coeffs(ax_sh, sh_coeffs, f"Cluster {cluster_id} SH")
            plot_quats(ax_q, quat)
    
    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    plt.savefig(output_path, dpi=150)
    plt.show()
    print(f"시각화 저장 완료: {output_path}")

def main(compress_dir: str):
    """메인 함수"""
    try:
        # 1. 클러스터 데이터 로드
        cluster_info, meta_all, sh_dim = load_cluster_data(compress_dir)
        print(f"클러스터 정보 로드 완료: {len(cluster_info)} 항목")
        
        # 2. 클러스터 샘플 추출
        samples = sample_cluster_data(
            compress_dir=compress_dir,
            cluster_info=cluster_info,
            sh_dim=sh_dim,
            num_clusters=5,
            samples_per_cluster=3
        )
        
        if not samples:
            print("샘플을 추출할 수 없습니다. 클러스터 정보를 확인하세요.")
            return
        
        # 3. 샘플 시각화
        output_path = os.path.join(compress_dir, "cluster_visualization.png")
        visualize_cluster_samples(
            samples=samples,
            sh_dim=sh_dim,
            output_path=output_path
        )
        
    except Exception as e:
        print(f"오류 발생: {str(e)}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    compress_dir = "./results/Bartender_1/hevc/rp0_full8/compression"
    main(compress_dir)
