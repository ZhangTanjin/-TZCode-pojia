# TZCode 破甲弹 (Armor-Piercing Shell)

> ZCode 3.15.1 全链破甲工具：人格替换 + 已授权话术 + V4 签名 + 直连路由
> C++ / 纯 Win32 · 四页签 UI · 弹药自含 · 开箱即用

## 这是什么

针对 [ZCode](https://github.com/zai-org/ZCode)（智谱 AI 编码助手）的本地破甲工具。

官方 ZCode 的 system prompt 含产品身份声明、安全策略、上下文管控等注入内容。本工具将 `zcode.cjs`（agent 核心）替换为定制构建，实现：

- **TZCode 前缀**：`You are ZCode` → `You are TZCode`
- **人格热装载**：`~/.zcode/persona/persona.txt` 存在即整层替换 system（新对话生效）
- **已授权话术**：安全声明替换为预授权研究环境声明
- **骨架保留**：Harness/Environment/Context Management 运营纪律不爆破
- **V4 签名**：Client Request Signing V4（FSr v2 凭证解析），与官方 3.15.1 完全同构
- **直连路由**：coding-plan 流量直连 `open.bigmodel.cn`（官方 3.15.1 行为）

## 使用

### 快速部署

1. 安装官方 ZCode 3.15.1（壳必须用官方安装包，不要自编）
2. 完全退出 ZCode
3. 双击 `armor-piercer/zcode破甲弹.exe`（UAC 放行）
4. 点击 🚀 **部署破甲** → ✅ 部署成功
5. 点击 ⚡ **重启 ZCode**
6. 新对话发消息 → 人格生效

### 四页签

| 页签 | 功能 |
|---|---|
| 🚀 部署 | 状态总览（安装目录/构建识别/标记验证/人格状态）+ 部署/恢复官方/刷新/重启 |
| 🧬 人格 | 程序内查看 + 编辑 + 保存（UTF-8 原子写入，自动 .bak 备份，新对话即生效） |
| 📁 人格目录 | 程序内浏览 `~/.zcode/persona/`，双击载入编辑器 |
| 📜 日志 | 操作时间戳流水 |

### 换人格（最常用）

直接在破甲弹"人格"页编辑，或在目录页双击任意 `.txt` → 编辑 → 💾 保存 → 开新对话生效。

### 回滚

- 破甲弹 ♻ **恢复官方**（从 `zcode.cjs.official.bash` 还原）

## 从源码构建

### 弹药（zcode.cjs）

```bash
# 前置：Node 24.x, pnpm 10.x, ZCode v3.15.1 源码 (git clone https://github.com/zai-org/ZCode.git)

cd ZCode
git checkout v3.15.1
git am patches/0004*.patch patches/0005*.patch patches/0008*.patch  # 人格+签名+路由

pnpm install
cd apps/zcode-cli
pnpm exec turbo run build
cd packages/cli
node scripts/build.mjs --desktop-agent     # → dist/zcode.cjs
cp dist/zcode.cjs ../armor-piercer/armor_payload.bin
```

### 破甲弹 EXE

```bash
# 前置：VS Build Tools 2022 (MSVC + Windows SDK)
cd armor-piercer
# build3.bat 内含 vcvars64 → rc → cl 全链
build3.bat
# 产物: piercer.exe → 重命名为 zcode破甲弹.exe
```

## 补丁说明（patches/）

| 补丁 | 内容 |
|---|---|
| `0004` | SECURITY_NOTICE → 预授权研究环境声明 |
| `0005` | 精准人格装配（TZCode 前缀 + 热装载 + 骨架保留，非爆破） |
| `0006` | start-plan 免费车道门禁（官方 system 迷彩 + 阿里云验证码，移植自 zcode-api） |
| `0008` | **V4 签名（FSr v2）+ 直连路由** — 3.15.1 破甲的核心 |

`0008` 包含：
- `client-signing.ts`：从官方 3.15.1 bundle 逆向的完整 V4 签名栈
  - FSr v2 凭证解析（三段 JWT → api_key claim / 整 JWT 为密钥 / accessToken 字段）
  - 握手（get_sign_key → privateCipher → Ed25519）
  - 业务签名六头（X-Client-Ts/Version/Sig/Nonce/App-Id/Pow）
- `endpoint-routing-edition`：静态网关改道根除（官方 3.15.1 直连 bigmodel.cn）
- `request-security-edition`：coding-plan 车道签名接线 + 运行时凭证提取 + 黑匣子日志

## 技术文档

详细迭代过程、401 根因分析、协议逆向成果见 [docs/迭代经验归档.md](docs/迭代经验归档.md)。

## 免责声明

仅修改本地安装的软件配置。ZCode 源码 Apache-2.0，补丁同样开源。自行承担使用风险。

## 致谢

- [zai-org/ZCode](https://github.com/zai-org/ZCode) — 上游源码
- [TriDefender/zcode-api](https://github.com/TriDefender/zcode-api) — V4 签名逆向先驱
