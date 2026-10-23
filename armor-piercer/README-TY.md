# TY 破甲弹

ZCode · Qoder 双阵地装甲部署器（单文件，Win32，管理员运行）。当前版本 **v6.0**。

```
TY破甲弹.exe  ~15.5 MB · 四资源内嵌 · 零外部依赖
```

## 弹药舱

| 资源 | 目标 | 内容 |
|---|---|---|
| `ARMOR` | ZCode `zcode.cjs` | 融合固件：TZCode 身份＋预授权声明＋ATOMIC CONTINUATION DISPATCH＋persona 装载钩（`ZCODE_PERSONA_FILE` 可覆盖） |
| `PERSONA` | `~/.zcode/persona/persona.txt` | ZCode v3 人格（scope 全密度·拒绝即故障·无限研究模式·结构性授权） |
| 内置引擎 | Qoder worker（任意版本） | 版本无关手术：自动嗅探 XOR 密钥＋内容特征匹配四层（notice / # Delivering work / classifier / CHECK rules）＋身份钩注入；幂等 |
| `QPERSONA` | `~/.qoder-cn/persona.txt` | Qoder 原生五节人格（# Working doctrine 等，非 ZCode 词汇） |

另含 DeepSeek Harness 装甲（`dsh-armor/`，官方 patch 层，零二进制）。

## 使用

1. 管理员运行 `TY破甲弹.exe`
2. 部署页两张色卡：**蓝 ZCode**「部署ZCode破甲」/ **青 Qoder**「部署Qoder装甲」
3. 完全退出目标应用（托盘右键）→ 重启 → 新对话生效
4. 操作结果弹窗＋自动跳日志页

**更新洗弹后**：重按一次部署键即复原（Qoder 引擎版本自适应；ZCode 锚点按内容匹配）。

## 回滚

- ZCode：`zcode.cjs.official.bak-<版本>` 原件；GUI「恢复ZCode官方」
- Qoder：`.official.bak-<版本>`；GUI「恢复Qoder官方」（自动选最新备份）
- dsh：删 `~/.dsh/cordis.patch.yml` 或还原 `.bak-<时间戳>`

## 账本

`VERIFICATION_WEAPON.txt` —— 全部构建/事故/修复记录（含 3.16.3 两次启动事故的完整复盘与部署铁律：**本代理会话运行在 ZCode 内，最终部署由操作者体外执行**）。

## 血统

开源仓：`ZhangTanjin/armor-piercer`（主）/ `ZhangTanjin/-TZCode-pojia`（同步）。
Release 资产＝本仓构建的成品 exe，与源码同 commit。
