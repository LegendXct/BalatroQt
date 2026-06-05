# 仓库整理方案 (Repo Cleanup) — 2026-06-05

## 背景

`origin/develop` 上误入库了大量 IDE 生成物与机器本地文件，仓库显得臃肿。
本次目标：清掉这些噪音，补全 `.gitignore` 防止复发。**不重写 git 历史**（仓库是与
伙伴 PiggyQwQ 共享的 `develop`，重写需 force-push + 对方重新 clone，风险大）。

## 清理清单

| 目标 | 文件数 | 性质 | 处理 |
|---|---|---|---|
| `.qtc_clangd/` | 266 | Qt Creator clangd 索引缓存 | 停止追踪，磁盘保留 |
| `BalatroQt.{cflags,config,creator,cxxflags,files,includes}` | 6 | Qt Creator 生成的工程元数据 | 停止追踪，磁盘保留 |
| `.qtcreator/BalatroQt.creator.user` | 1 | 个人 IDE 设置 | 停止追踪，磁盘保留 |
| `.claude/settings.local.json` | 1 | 机器本地 Claude 设置 | 停止追踪，磁盘保留 |
| `balatro_original_code/` | 50 | 仅供参考的原版 Lua 源码 | 停止追踪，磁盘保留（仍可参考） |
| `git` | 1 | 0 字节误生成垃圾文件 | 从库 + 磁盘删除 |
| `AGENTS.md` | 1 | 与 `CLAUDE.md` 字节级重复 | 改为一行指针，消除内容漂移 |

`build/`（407 MB）已被 `.gitignore` 正确忽略，不动。

## 执行步骤

1. **补 `.gitignore`**：追加 `.qtc_clangd/`、`.qtcreator/`、`*.creator.user*`、
   六个 `BalatroQt.*` 工程文件、`.claude/settings.local.json`、`balatro_original_code/`。
2. **停止追踪**：`git rm -r --cached` 上述五类（`--cached` → 磁盘文件保留）。
3. **删垃圾**：`git rm git` 并删盘。
4. **合并 agent 指令**：保留 `CLAUDE.md` 为正文，`AGENTS.md` 改为
   `See CLAUDE.md for project guidance.`。
5. **提交**：在 `develop` 上，仅显式暂存上述清理路径；
   `src/game/gamestate.cpp` 的未提交改动留在工作区不动。**不自动推送。**

## 已知取舍

- 不重写历史 → `.git`（~50 MB）体积不变，旧 clangd blob 仍在历史里。
  本次之后新提交不再 churn 这些二进制。
- 日后若要彻底瘦身，可单独跑 `git filter-repo`（需 force-push + 伙伴重新 clone），
  作为可选后续，不在本次范围。
