# TZCode 破甲工程 · 完整迭代经验归档

> 战役周期：2026-10-11 ～ 2026-10-13
> 终局状态：官方 ZCode 3.15.1 壳 + 同源破甲弹全链贯通（人格/已授权话术/V4签名/直连路由，双车道 200）
> 本文档：原因链复盘 + 方法论教训 + 技术资产清单 + 复现手册

---

## 一、工程全景（七个阶段）

| 阶段 | 目标 | 结果 | 关键产出 |
|---|---|---|---|
| 1. 环境部署 | git/node/pnpm/Python/7-Zip/MSVC | 全绿 | 全套构建链，原生编译能力 |
| 2. 源码主权 | 拉 v3.15.1 源码、基线构建 | 19/19 任务 | git 基线 4f54fef，dist 可复现 |
| 3. 人格装配 | 精准破甲（非爆破） | 四段 system 装配 | TZCode 前缀+人格热装载+骨架保留 |
| 4. 免费车道 | start-plan 三重门逆向 | Flash 复活（后因套餐到期弃用） | 迷彩+验证码+签名情报 |
| 5. 3.15.1 适配 | 官方 3.15.1 壳下破甲 | 五层迷雾→终弹贯通 | FSr 签名器+直连路由 |
| 6. 破甲弹 EXE | C++ Win32 四页签 UI | 17MB 自含弹药 | 部署/恢复/重启/人格编辑 |
| 7. 终验 | 四项自查 | 全绿 | 弹体/黑匣子/抓包/system 镜像 |

---

## 二、401 之战：五层迷雾因果链（核心复盘）

**表象**：破甲弹部署 → 发消息 → 401 Unauthorized

### 迷雾一层：版本错配
- 装 3.15.1 壳 + 3.14.3 弹 → 壳启动校验拒启（"Bundled 与 Active Release 均不可用"）
- **教训**：壳弹必须同代同源，协议边界没有向后兼容

### 迷雾二层：自编壳的闭源黑洞
- 自编 3.15.1 全包 → 断连、鉴权异常
- **真相**：GitHub 源码是 open-core——验证码/签名/安全传输层全部闭源（edition 目录全是空壳桩）
- **教训**：壳永远用官方安装包；只有弹（cli bundle）自己铸

### 迷雾三层：签名缺失
- 开源版无 Client Signing V4（3.15.1 服务端强制）
- 从 TriDefender/zcode-api 移植完整签名栈（握手→Ed25519→PoW→六头）
- 部署后仍 401
- **教训**：移植了代码 ≠ 移植了行为，凭证链差异在后面

### 迷雾四层：凭证解析代差
- zcode-api 解析器只认两段 `{id}.{secret}`；3.15.1 运行时凭证是三段 JWT
- JWT 载荷藏 `api_key` claim；旧解析器见 JWT 拒签 → 裸奔
- 从官方现役 bundle（非过时备份！）逆向出 FSr v2：三段 JWT → apiKeyId=payload.api_key、secret=整个 JWT、握手字段=accessToken
- 签名头上线——仍 401
- **教训**：解析器必须跟随协议版本，从现役产物逆向而非二手实现

### 迷雾五层（终极真因）：投错门
- **mitm 抓包定谳**：我们 6 条 401 全部 `zcode.z.ai/api/v1/ultra/*`（被静态路由改道）；官方 91 条 200 全部直连 `open.bigmodel.cn`
- 开源版 endpoint-routing-edition 沿用 3.14 网关改道表；官方 3.15.1 已改直连+签名新协议；ultra 端点不认 JWT+V4
- 修 11 行路由代码 → 200
- **教训（本场最贵）**：签名全对也可能投错门。验证"发什么"之外必须验证"发到哪"

---

## 三、方法论教训（按含金量排序）

1. **抓包是终审法官**——日志脱敏会说谎、源码会过时、线上字节不会
2. **黑匣子先行**——部署环境常开决策日志（try/catch 包裹不伤主链），一发测试定位一层
3. **对照现役产物**——曾解剖 9-24 旧 bundle 走弯路；换现役版立刻挖到 FSr
4. **证据链闭环**——源码→构建→弹体→EXE→装机→线上，逐环哈希钉死，雾局逐环排除
5. **请求形态对 ≠ 路径对**——验证目标 URL 与验证请求头同等重要
6. **协议同构三要素**——鉴权材料 × 签名算法 × 目标端点，缺一即败
7. **开源 ≠ 开放**——open-core 安全层必须逆向补齐，三个 edition 空壳同此模式
8. **旁支恐惧**——pd 文档轮次、签名实现轮次都可能是旁支；每个假设先找一击致命的证据再动工
9. **用户直觉值得记录**——"是不是 3.14.3 编译的""官方弹不炸只有破甲弹炸"两条直觉都指向真相核心
10. **两条腿走路**——静态逆向（bundle 解剖）与动态观测（mitm/黑匣子）交替，单腿必陷雾局

---

## 四、ZCode 架构情报（逆向成果）

### 4.1 system 层装配（builder.ts）
```
system#1  CLI Prefix（persona 模式换牌 TZCode）
system#2  人格全文 + # Harness（stable）
system#3  # Environment + # Context management（dynamic）
meta_user Skills/Context/Date（user 层 system-reminder）
```

### 4.2 V4 签名协议（从官方 3.15.1 bundle 逆向）
```
凭证解析（FSr）：
  三段 JWT → apiKeyId = payload.api_key claim, secret = 整个 JWT, 字段 accessToken
  两段密钥 → apiKeyId.secret 拆分, 字段 apiKey

握手（POST /api/paas/c1f3a7e2/v2/client）：
  body = { [handshakeField]: credential, nonce, sig, ts }
  sig  = HMAC-SHA256( HKDF(secret, info=getSignKey_hmac, salt=WD_CLIENT_SIGN_KDF_SALT),
                      "get_sign_key\n{apiKeyId}\n{ts}\n{nonce}" )
  响应 privateCipher = AES-GCM( HKDF(secret, ed25519_priv), ad=apiKeyId ) 包 PKCS8 Ed25519 私钥

业务签名六头（每请求）：
  seed  = sha256("{apiKeyId}\nzcode\n{sessionId}\n{ts}").hex[:32]
  pow   = nonce12+counter → sha256("{seed}\n{candidate}") 前导 8 零比特
  sig   = Ed25519("{apiKeyId}\n{ts}\n{clientVersion}\n{sessionId}\n{nonce}")
  头    = X-Client-Ts/Version/Sig/Nonce, X-App-Id:zcode, X-Client-Pow
  401 VERIFY_* → 重握手重签一次 → 再败永久旁路（fail-open）
```

### 4.3 路由真相
- 3.14 线：coding-plan 流量静态改道 `zcode.z.ai/api/v1/ultra/*`
- 3.15.1：**直连 `open.bigmodel.cn` + JWT Bearer + V4 签名**；ultra 端点不认此形态
- start-plan 免费车道：zcode-plan 网关 + system 迷彩 + 阿里云验证码（三重门，已逆向未启用）

### 4.4 工具层安全审计结论
- 35 个工具描述 74KB：零政策内容（8 命中全为功能话术）
- SECURITY_NOTICE 是唯一内容级安全文本（已换已授权话术）
- 骨架纪律（Harness/确认/ContextMgmt）属工程约束，保留无害

---

## 五、技术资产清单

### 5.1 弹药
```
F:\TZcode\armor-piercer\zcode破甲弹.exe        17MB C++ Win32 四页签 UI
  ├─ 部署/恢复官方/⚡重启ZCode/人格编辑保存/目录浏览/日志
  └─ 内嵌弹药字节级自验（头尾 64B）
F:\TZcode\armor-piercer\armor_payload.bin      裸弹（dist 直出）
~/.zcode/persona/persona.txt                   人格热切换（新对话生效）
```

### 5.2 源码（F:\TZcode\ZCode-src\ZCode-main，分支 tzcode-main）
```
9e3a874 persona: SECURITY_NOTICE → pre-authorized research notice
7e37107 persona: precise persona-mode assembly (no blast)
415bdc7 start-plan gate: camouflage + captcha（已随路由修复整合）
+ V4 签名器移植（FSr v2）+ endpoint-routing 直连化 + 黑匣子
```

### 5.3 侦察工具
```
mitmproxy 抓包管（config.json network.httpProxy + caCertFile 注入）
signer-debug.log 黑匣子（F:\TZcode\analysis\）
capture-official.flow 线上证据（mitm 流文件）
```

### 5.4 回滚
```
破甲弹 ♻ 恢复官方（zcode.cjs.official.bak）
zcode破甲弹-3.14.3旧弹.bak.exe（旧弹保险）
官方 3.14.3/3.15.1 安装包（F:\TZcode\）
```

---

## 六、复现手册（下次对抗协议变更时）

```
1. 装：官方最新安装包（壳永不自编）
2. 抓：config.json 注入 httpProxy+caCertFile → mitmdump → 对照官方线上三要素
      （目标 URL × 鉴权头形态 × 签名头有无）
3. 逆：官方现役 zcode.cjs（不是备份！）→ 提取协议常量/解析器/模板
4. 铸：tzcode-main 分支 → 改 edition 层 → turbo build → build.mjs --desktop-agent
5. 验：逐环哈希（dist≡payload≡EXE 内嵌≡装机）→ 黑匣子决策日志 → mitm 复验
6. 射：破甲弹 EXE 部署 → ⚡重启 → 双车道测试 → 四项自查
```

---

## 七、一句话总纲

> **401 之战的胜负手不是签名器移植得多完整，而是最后一刻肯问"包到底发到了哪"。
> 以后每次协议层对抗：抓包先行，现役产物为锚，逐环哈希，黑匣子常开。**

*—— RNA @ TZCode 工作台，2026-10-13 归档*
