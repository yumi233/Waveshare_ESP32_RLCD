# RELEASE-01 · 源码打包与发布

## 当前发布范围

`v1.0.0-rc.3` 提供源码 ZIP、功能文档、第三方授权材料与 SHA-256 清单。没有把历史 Mac 应用、Windows 安装器或固件二进制混作本版产物。程序内部版本沿用 1.0.0，标签区分源码候选版。

## 按提交打包

先提交准备发布的代码和文档，再执行：

```bash
python3 release/package_source.py --ref HEAD --version 1.0.0-rc.3 --output release/artifacts/1.0.0-rc.3
```

产物为 `XiaNing-source-1.0.0-rc.3.zip` 和 `SHA256SUMS.json`。脚本用 `git archive` 打包指定提交，不使用未提交文件，也不打包被忽略的运行环境、设备备份或本机配置。发现常见私有路径时拒绝打包，已有文件不覆盖。该路径检查不能替代发布前对内容的检查。

校验清单记录完整源码提交、文件数、压缩包字节数和 SHA-256，可用 Python 核对：

```python
import hashlib, json
from pathlib import Path
root = Path('release/artifacts/1.0.0-rc.3')
manifest = json.loads((root / 'SHA256SUMS.json').read_text())
for entry in manifest['files']:
    data = (root / entry['name']).read_bytes()
    assert len(data) == entry['size_bytes']
    assert hashlib.sha256(data).hexdigest() == entry['sha256']
```

## 发布到自己的仓库

```bash
git push origin main
git tag -a v1.0.0-rc.3 -m '夏柠 1.0.0-rc.3：功能整合源码版'
git push origin v1.0.0-rc.3
gh release create v1.0.0-rc.3 \
  release/artifacts/1.0.0-rc.3/XiaNing-source-1.0.0-rc.3.zip \
  release/artifacts/1.0.0-rc.3/SHA256SUMS.json \
  --repo yumi233/Waveshare_ESP32_RLCD --verify-tag --prerelease \
  --title '夏柠 1.0.0-rc.3 · 功能整合源码版' \
  --notes-file docs/RELEASE_NOTES_1.0.0-rc.3.md
```

本仓库的发布目标为 `yumi233/Waveshare_ESP32_RLCD`，原作者仓库用于署名和上游参考。标签和版本号应随下一次发布更新。SHA-256 提供完整性校验，本次未声称使用原作者发布私钥签名。
