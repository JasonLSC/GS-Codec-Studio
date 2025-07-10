from typing import Dict

import torch
from torch import Tensor


def sort_splats(
        splats: Dict[str, Tensor], 
        verbose: bool = True, 
        return_indices: bool = False,
        sort_with_shN: bool = False
        ) -> Dict[str, Tensor]:
    """Sort splats with Parallel Linear Assignment Sorting from the paper `Compact 3D Scene Representation via
    Self-Organizing Gaussian Grids <https://arxiv.org/pdf/2312.13299>`_.

    .. warning::
        PLAS must installed to use sorting.

    Args:
        splats (Dict[str, Tensor]): splats
        verbose (bool, optional): Whether to print verbose information. Default to True.
        return_indices (bool, optional): Whether to return sorted indices. Default to False.
        sort_with_shN (bool, optional): Whether to consider shN when sorting. Default to False.

    Returns:
        Dict[str, Tensor]: sorted splats
    """
    try:
        from plas import sort_with_plas
    except:
        raise ImportError(
            "Please install PLAS with 'pip install git+https://github.com/fraunhoferhhi/PLAS.git' to use sorting"
        )

    n_gs = len(splats["means"])
    n_sidelen = int(n_gs**0.5)
    assert n_sidelen**2 == n_gs, "Must be a perfect square"

    if not sort_with_shN:
        sort_keys = [k for k in splats if k != "shN"]
    else:
        sort_keys = [k for k in splats]
    params_to_sort = torch.cat([splats[k].reshape(n_gs, -1) for k in sort_keys], dim=-1)
    shuffled_indices = torch.randperm(
        params_to_sort.shape[0], device=params_to_sort.device
    )
    print("shuffled_indices:", shuffled_indices)
    params_to_sort = params_to_sort[shuffled_indices]
    grid = params_to_sort.reshape((n_sidelen, n_sidelen, -1))
    _, sorted_indices = sort_with_plas(
        grid.permute(2, 0, 1), improvement_break=1e-4, verbose=verbose
    )
    sorted_indices = sorted_indices.squeeze().flatten()
    sorted_indices = shuffled_indices[sorted_indices]
    for k, v in splats.items():
        splats[k] = v[sorted_indices]
    
    if return_indices:
        return splats, sorted_indices
    else:
        return splats
    
    
def sort_splats_morton(splats: Dict[str, Tensor], verbose: bool = True, return_indices: bool = False) -> Dict[str, Tensor]:
    def mortonEncode(pos: torch.Tensor) -> torch.Tensor:
        def splitBy3(a):
            x = a & 0x1FFFFF  # we only look at the first 21 bits
            x = (x | x << 32) & 0x1F00000000FFFF
            x = (x | x << 16) & 0x1F0000FF0000FF
            x = (x | x << 8) & 0x100F00F00F00F00F
            x = (x | x << 4) & 0x10C30C30C30C30C3
            x = (x | x << 2) & 0x1249249249249249
            return x
        x, y, z = pos.unbind(-1)
        answer = torch.zeros(len(pos), dtype=torch.long, device=pos.device)
        answer |= splitBy3(x) | splitBy3(y) << 1 | splitBy3(z) << 2
        return answer

    with torch.no_grad():
        xyz_q = (
            (2**21 - 1) * (splats["means"] - splats["means"].min(0).values)
            / (splats["means"].max(0).values - splats["means"].min(0).values)
        ).long()
        order = mortonEncode(xyz_q).sort().indices
        for k,v in splats.items():
            splats[k] = v[order]

        if return_indices:
            return splats, order
        else:
            return splats
