## Questions on Environment Setup

**Question**: When running the program, an error occurs during the import statement "from fused_ssim import fused_ssim". The error message is related to libstdcxx.

**Analysis**: This issue occurs because the stdcxx-related dynamic libraries in the Conda environment are outdated. The fused_ssim module was compiled using a higher version of GCC from the local environment rather than the lower version of GCC in the Conda environment. 

**Solution**: You can specify the "conda-forge" channel when creating a new Conda environment to ensure downloading the latest GCC toolchain.

```bash
conda create --name gscodec_studio -c conda-forge python=3.10
```
---
