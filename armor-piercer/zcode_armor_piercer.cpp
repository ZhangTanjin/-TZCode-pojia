// ============================================================
// ZCode 破甲弹 v2.1 — 官方 ZCode 的破甲部署器
// C++ / 纯 Win32 · 暗色主题 · 自绘控件 · 四页签 · 弹药内嵌
//
//   [部署]     状态总览（状态徽章）· 部署/恢复/刷新
//   [人格]     程序内查看 + 编辑 + 保存（UTF-8 原子写入自动备份）
//   [人格目录] 程序内浏览，双击载入编辑器
//   [日志]     操作流水
// ============================================================

#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <shlobj.h>
#include <shellapi.h>

#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

// ------------------------------------------------------------
// 调色板
// ------------------------------------------------------------

static const COLORREF C_BG      = RGB(24, 25, 29);   // 窗口底
static const COLORREF C_PANEL   = RGB(33, 35, 41);   // 卡片
static const COLORREF C_CTRL    = RGB(45, 47, 55);   // 控件底
static const COLORREF C_CTRL_HV = RGB(58, 61, 70);   // 悬停
static const COLORREF C_LINE    = RGB(55, 58, 67);   // 分隔线
static const COLORREF C_TEXT    = RGB(233, 235, 239);
static const COLORREF C_TEXT2   = RGB(150, 156, 165);
static const COLORREF C_ACCENT  = RGB(86, 136, 255);   // ZCode 蓝弹
static const COLORREF C_QODER   = RGB(46, 196, 182);   // Qoder 青弹
static const COLORREF C_Z_DIM   = RGB(46, 66, 110);    // ZCode 区徽标底
static const COLORREF C_Q_DIM   = RGB(28, 84, 78);     // Qoder 区徽标底
static const COLORREF C_OK      = RGB(72, 199, 118);
static const COLORREF C_WARN    = RGB(250, 190, 60);
static const COLORREF C_ERR     = RGB(240, 90, 80);

static HBRUSH g_br_bg, g_br_panel, g_br_ctrl, g_br_ctrl_hv, g_br_edit;

static const wchar_t* APP_TITLE = L"TY 破甲弹 — ZCode · Qoder 双弹";
static const char* ARMOR_MARKER_A = "You are TZCode, an interactive coding agent";
static const char* OFFICIAL_MARKER_A = "You are ZCode, an interactive coding agent";

enum CtrlId {
  IDC_DEPLOY = 101,
  IDC_RESTORE = 102,
  IDC_REFRESH = 103,
  IDC_PERSONA_SAVE = 110,
  IDC_PERSONA_RELOAD = 111,
  IDC_PERSONA_NOTEPAD = 112,
  IDC_PERSONA_MASTER = 113,
  IDC_DIR_REFRESH = 120,
  IDC_DIR_EXPLORER = 121,
  IDC_RESTART_ZCODE = 130,
  IDC_QODER_DEPLOY = 140,
  IDC_QODER_RESTORE = 141,
};

enum TabIndex { TAB_DEPLOY = 0, TAB_PERSONA = 1, TAB_DIR = 2, TAB_LOG = 3, TAB_COUNT = 4 };
static const wchar_t* TAB_NAMES[TAB_COUNT] = {L"🚀 部署", L"🧬 人格", L"📁 人格目录", L"📜 日志"};

// ------------------------------------------------------------
// 工具（编码/格式化/文件）
// ------------------------------------------------------------

static std::wstring Utf8ToWide(const std::string& s) {
  if (s.empty()) return std::wstring();
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
  std::wstring w(n, 0);
  MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
  return w;
}

static std::string WideToUtf8(const std::wstring& w) {
  if (w.empty()) return std::string();
  int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
  std::string s(n, 0);
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
  return s;
}

static std::wstring Fmt(const wchar_t* fmt, ...) {
  wchar_t buf[4096];
  va_list args;
  va_start(args, fmt);
  _vsnwprintf_s(buf, _TRUNCATE, fmt, args);
  va_end(args);
  return buf;
}

static std::wstring FormatNumber(unsigned long long v) {
  std::wstring s = std::to_wstring(v);
  for (int pos = (int)s.size() - 3; pos > 0; pos -= 3) s.insert((size_t)pos, L",");
  return s;
}

static std::wstring FileTimeStr(const wchar_t* path) {
  WIN32_FILE_ATTRIBUTE_DATA fa = {};
  if (!GetFileAttributesExW(path, GetFileExInfoStandard, &fa)) return L"—";
  SYSTEMTIME st;
  FileTimeToSystemTime(&fa.ftLastWriteTime, &st);
  return Fmt(L"%04d-%02d-%02d %02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
}

static std::wstring FileTimeStrDirect(const FILETIME& ft) {
  SYSTEMTIME st;
  FileTimeToSystemTime(&ft, &st);
  return Fmt(L"%04d-%02d-%02d %02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
}

static unsigned long long FileSizeOf(const wchar_t* path) {
  WIN32_FILE_ATTRIBUTE_DATA fa = {};
  if (!GetFileAttributesExW(path, GetFileExInfoStandard, &fa)) return 0;
  return ((unsigned long long)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
}

static bool ReadFileBytes(const wchar_t* path, std::string* out) {
  HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                         nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (f == INVALID_HANDLE_VALUE) return false;
  unsigned long long size = 0;
  LARGE_INTEGER li;
  if (GetFileSizeEx(f, &li)) size = (unsigned long long)li.QuadPart;
  out->assign((size_t)size, '\0');
  DWORD read_total = 0;
  while (read_total < size) {
    DWORD got = 0;
    if (!ReadFile(f, &(*out)[read_total],
                  (DWORD)std::min<size_t>(size - read_total, 1 << 24), &got, nullptr) ||
        got == 0)
      break;
    read_total += got;
  }
  CloseHandle(f);
  return read_total == size;
}

static bool WriteFileAtomic(const wchar_t* path, const void* data, size_t len, std::wstring* err) {
  std::wstring tmp = std::wstring(path) + L".tmp";
  HANDLE f = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (f == INVALID_HANDLE_VALUE) {
    *err = Fmt(L"创建临时文件失败 (err=%lu)", GetLastError());
    return false;
  }
  DWORD written = 0;
  BOOL ok = WriteFile(f, data, (DWORD)len, &written, nullptr) && written == len;
  FlushFileBuffers(f);
  CloseHandle(f);
  if (!ok) {
    DeleteFileW(tmp.c_str());
    *err = L"写入临时文件失败";
    return false;
  }
  if (!MoveFileExW(tmp.c_str(), path, MOVEFILE_REPLACE_EXISTING)) {
    DeleteFileW(tmp.c_str());
    *err = Fmt(L"替换目标失败 (err=%lu)", GetLastError());
    return false;
  }
  return true;
}

static bool BytesContain(const std::string& hay, const char* needle) {
  return hay.find(needle) != std::string::npos;
}

static std::wstring JoinPath(const std::wstring& a, const std::wstring& b) {
  if (a.empty() || a.back() == L'\\') return a + b;
  return a + L"\\" + b;
}

// ------------------------------------------------------------
// 路径解析
// ------------------------------------------------------------

static std::wstring GetZCodeRootFromProcesses() {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return L"";
  PROCESSENTRY32W pe = {};
  pe.dwSize = sizeof(pe);
  std::wstring result;
  if (Process32FirstW(snap, &pe)) {
    do {
      if (_wcsicmp(pe.szExeFile, L"ZCode.exe") == 0) {
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
        if (h) {
          wchar_t buf[MAX_PATH] = {};
          DWORD n = MAX_PATH;
          if (QueryFullProcessImageNameW(h, 0, buf, &n)) result = buf;
          CloseHandle(h);
          if (!result.empty()) break;
        }
      }
    } while (Process32NextW(snap, &pe));
  }
  CloseHandle(snap);
  if (result.empty()) return L"";
  size_t p = result.find_last_of(L"\\/");
  return p == std::wstring::npos ? L"" : result.substr(0, p);
}

struct InstallInfo {
  std::wstring root, glmDir, cjsPath, bakPath;
  bool found = false;
};

static InstallInfo DetectInstall() {
  InstallInfo info;
  std::vector<std::wstring> roots;
  std::wstring running = GetZCodeRootFromProcesses();
  if (!running.empty()) roots.push_back(running);
  wchar_t buf[MAX_PATH] = {};
  if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, 0, buf)))
    roots.push_back(JoinPath(buf, L"ZCode"));
  if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILESX86, nullptr, 0, buf)))
    roots.push_back(JoinPath(buf, L"ZCode"));
  if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, buf)))
    roots.push_back(JoinPath(JoinPath(buf, L"Programs"), L"ZCode"));
  for (const auto& root : roots) {
    std::wstring cjs = JoinPath(JoinPath(JoinPath(root, L"resources"), L"glm"), L"zcode.cjs");
    if (GetFileAttributesW(cjs.c_str()) != INVALID_FILE_ATTRIBUTES) {
      info.root = root;
      info.glmDir = JoinPath(JoinPath(root, L"resources"), L"glm");
      info.cjsPath = cjs;
      info.bakPath = cjs + L".official.bak";
      info.found = true;
      return info;
    }
  }
  return info;
}

static std::wstring PersonaDir() {
  wchar_t buf[MAX_PATH] = {};
  if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, 0, buf)))
    return JoinPath(JoinPath(buf, L".zcode"), L"persona");
  return L"";
}

static std::wstring PersonaPath() { return JoinPath(PersonaDir(), L"persona.txt"); }

// ------------------------------------------------------------
// UI 状态
// ------------------------------------------------------------

static HWND g_main;
static HFONT g_font, g_font_mono, g_font_title, g_font_tab;

// 按钮元数据（自绘）
struct Btn {
  HWND h;
  COLORREF fill;     // 常态填充
  COLORREF text;     // 文字色
  bool hover = false;
};
static std::vector<Btn> g_btns;

// 状态行配色（徽章式）
static std::map<HWND, COLORREF> g_label_colors;
static std::set<HWND> g_panel_labels;

static int g_cur_tab = TAB_DEPLOY;
static RECT g_tab_strip = {};  // 页签条区域
static int g_start_tab = TAB_DEPLOY;  // 命令行指定的起始页签

// 部署页
static HWND g_s_install, g_s_build, g_s_file, g_s_check, g_s_persona;
static HWND g_s_qworker, g_s_qarmor, g_s_qpersona;
static std::wstring QoderWorkerPath();
static std::wstring QoderPersonaDir();
static std::wstring QoderPersonaPath();
static bool QoderArmored(const std::wstring& worker);
static void ReportResult(const std::wstring& summary, bool ok);
static HWND g_btn_deploy, g_btn_restore, g_btn_refresh, g_btn_restart;
static RECT g_card_deploy = {};
static RECT g_card_qoder = {};

// 人格页
static HWND g_s_personainfo;
static HWND g_edit_persona;
static HWND g_btn_save, g_btn_reload, g_btn_notepad, g_btn_persona_master;
static HWND g_btn_qdeploy, g_btn_qrestore;
static std::wstring g_editing_path;

// 目录页
static HWND g_list_dir;
static HWND g_btn_dir_refresh, g_btn_dir_explorer;

// 日志页
static HWND g_edit_log;

static std::vector<HWND> g_pages[TAB_COUNT];
static void ShowCurrentPage();
static void RegPage(int tab, HWND ctrl) { g_pages[tab].push_back(ctrl); }

static void Log(const std::wstring& line) {
  SYSTEMTIME st;
  GetLocalTime(&st);
  std::wstring stamp = Fmt(L"[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
  int len = GetWindowTextLengthW(g_edit_log);
  std::wstring text;
  if (len > 0) {
    text.assign(len, L'\0');
    GetWindowTextW(g_edit_log, &text[0], len + 1);
    text.resize(len);
  }
  text += stamp + line + L"\r\n";
  if (text.size() > 60000) text = text.substr(text.size() - 50000);
  SetWindowTextW(g_edit_log, text.c_str());
  SendMessageW(g_edit_log, EM_SETSEL, text.size(), text.size());
  SendMessageW(g_edit_log, EM_SCROLLCARET, 0, 0);
}

// ------------------------------------------------------------
// 业务逻辑
// ------------------------------------------------------------

static std::wstring IdentifyBundle(const wchar_t* path) {
  std::string bytes;
  if (!ReadFileBytes(path, &bytes)) return L"读取失败";
  if (BytesContain(bytes, ARMOR_MARKER_A)) return L"TZCode 破甲版（已部署）";
  if (BytesContain(bytes, OFFICIAL_MARKER_A)) return L"官方原版";
  return L"未知构建";
}

static void SetLabel(HWND label, const std::wstring& text, COLORREF color) {
  g_label_colors[label] = color;
  SetWindowTextW(label, text.c_str());
}

static void RefreshStatus() {
  InstallInfo info = DetectInstall();
  if (!info.found) {
    SetLabel(g_s_install, L"安装目录：未找到（ZCode 未安装？）", C_ERR);
    SetLabel(g_s_build, L"当前构建：—", C_TEXT2);
    SetLabel(g_s_file, L"文件信息：—", C_TEXT2);
    SetLabel(g_s_check, L"标记验证：—", C_TEXT2);
  } else {
    SetLabel(g_s_install, L"安装目录  " + info.root, C_TEXT);
    std::wstring build = IdentifyBundle(info.cjsPath.c_str());
    COLORREF bc = build == L"TZCode 破甲版（已部署）"  ? C_OK
                  : build == L"官方原版" ? C_WARN
                                         : C_ERR;
    SetLabel(g_s_build, L"当前构建  ● " + build, bc);
    SetLabel(g_s_file,
             Fmt(L"文件信息  %s 字节 · %s · 官方备份：%s",
                 FormatNumber(FileSizeOf(info.cjsPath.c_str())).c_str(),
                 FileTimeStr(info.cjsPath.c_str()).c_str(),
                 GetFileAttributesW(info.bakPath.c_str()) != INVALID_FILE_ATTRIBUTES ? L"已有" : L"无"),
             C_TEXT2);
    std::string bytes;
    ReadFileBytes(info.cjsPath.c_str(), &bytes);
    bool tz = BytesContain(bytes, ARMOR_MARKER_A);
    bool pl = BytesContain(bytes, "ZCODE_PERSONA_FILE");
    SetLabel(g_s_check,
             Fmt(L"标记验证  TZCode %s · 人格装载器 %s", tz ? L"✓" : L"✗", pl ? L"✓" : L"✗"),
             tz && pl ? C_OK : C_ERR);
  }
  std::wstring persona = PersonaPath();
  if (GetFileAttributesW(persona.c_str()) != INVALID_FILE_ATTRIBUTES) {
    SetLabel(g_s_persona,
             Fmt(L"人格文件  ● 在位（%s 字节 · %s）",
                 FormatNumber(FileSizeOf(persona.c_str())).c_str(), FileTimeStr(persona.c_str()).c_str()),
             C_OK);
  } else {
    SetLabel(g_s_persona, L"人格文件  ● 缺失 — 破甲版将回退官方装配", C_WARN);
  }

  // ---- Qoder 区状态 ----
  std::wstring qworker = QoderWorkerPath();
  if (qworker.empty() || GetFileAttributesW(qworker.c_str()) == INVALID_FILE_ATTRIBUTES) {
    SetLabel(g_s_qworker, L"Qoder worker  ● 未找到（Qoder CN 未安装？）", C_ERR);
    SetLabel(g_s_qarmor, L"Qoder 装甲  —", C_TEXT2);
    SetLabel(g_s_qpersona, L"Qoder 人格  —", C_TEXT2);
  } else {
    std::wstring qshort = qworker.size() > 64 ? (L"..." + qworker.substr(qworker.size() - 61)) : qworker;
    SetLabel(g_s_qworker, L"Qoder worker  " + qshort, C_TEXT);
    SetLabel(g_s_qarmor,
             QoderArmored(qworker) ? std::wstring(L"Qoder 装甲  ● 已装甲（TZ 双弹在膛）")
                                   : std::wstring(L"Qoder 装甲  ● 官方原件 — 未装甲"),
             QoderArmored(qworker) ? C_OK : C_WARN);
    std::wstring qp = QoderPersonaPath();
    if (GetFileAttributesW(qp.c_str()) != INVALID_FILE_ATTRIBUTES) {
      SetLabel(g_s_qpersona,
               Fmt(L"Qoder 人格  ● 在位（%s 字节 · %s）",
                   FormatNumber(FileSizeOf(qp.c_str())).c_str(), FileTimeStr(qp.c_str()).c_str()),
               C_OK);
    } else {
      SetLabel(g_s_qpersona, L"Qoder 人格  ● 缺失 — 部署时自动播种", C_WARN);
    }
  }
}

// 部署后自动播种默认人格：缺失则写入内嵌母本，已存在则保留用户版本
static void EnsureDefaultPersona() {
  std::wstring path = PersonaPath();
  CreateDirectoryW(PersonaDir().c_str(), nullptr);
  if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
    Log(L"· 人格文件已存在——保留现有版本（覆盖请用人格页「装填母本」）。");
    return;
  }
  HRSRC res = FindResourceW(nullptr, L"PERSONA", RT_RCDATA);
  if (!res) {
    Log(L"! 内嵌人格母本缺失，跳过默认人格安装。");
    return;
  }
  HGLOBAL handle = LoadResource(nullptr, res);
  if (!handle) {
    Log(L"! 装载人格母本资源失败，跳过默认人格安装。");
    return;
  }
  void* data = LockResource(handle);
  DWORD size = SizeofResource(nullptr, res);
  std::wstring err;
  if (WriteFileAtomic(path.c_str(), data, size, &err)) {
    Log(Fmt(L"✓ 已安装默认人格（%s 字节）——开新对话即生效。", FormatNumber(size).c_str()));
  } else {
    Log(L"✗ 默认人格安装失败：" + err);
  }
}

static void DoDeploy() {
  InstallInfo info = DetectInstall();
  if (!info.found) {
    Log(L"✗ 未找到 ZCode 安装目录，无法部署。");
    return;
  }
  Log(L"开始部署 → " + info.cjsPath);

  std::wstring current = IdentifyBundle(info.cjsPath.c_str());
  if (current == L"官方原版") {
    if (CopyFileW(info.cjsPath.c_str(), info.bakPath.c_str(), FALSE)) {
      Log(L"✓ 官方原版已备份 → zcode.cjs.official.bak");
    } else {
      Log(Fmt(L"✗ 备份失败 (err=%lu)", GetLastError()));
      return;
    }
  } else if (current == L"TZCode 破甲版（已部署）") {
    Log(L"· 当前已是破甲版（保留既有官方备份）");
  } else {
    Log(L"! 当前构建未知——继续部署（建议先手动备份）");
  }

  HRSRC res = FindResourceW(nullptr, L"ARMOR", RT_RCDATA);
  if (!res) {
    Log(L"✗ 内嵌弹药缺失——程序损坏。");
    return;
  }
  HGLOBAL handle = LoadResource(nullptr, res);
  if (!handle) {
    Log(L"✗ 装载弹药资源失败。");
    return;
  }
  void* data = LockResource(handle);
  DWORD size = SizeofResource(nullptr, res);
  Log(Fmt(L"· 弹药就绪：%s 字节（TZCode 破甲构建）", FormatNumber(size).c_str()));

  std::wstring err;
  if (!WriteFileAtomic(info.cjsPath.c_str(), data, size, &err)) {
    Log(L"✗ " + err);
    Log(L"  提示：请完全退出 ZCode（托盘右键退出）后重试。");
    return;
  }
  Log(L"✓ 已写入破甲构建。");

  std::wstring build = IdentifyBundle(info.cjsPath.c_str());
  Log(L"· 部署后识别：" + build);
  if (build == L"TZCode 破甲版（已部署）") {
    Log(L"✅ 部署成功——完全退出并重启 ZCode 后生效。");
    EnsureDefaultPersona();
    RefreshStatus();
    ReportResult(L"ZCode 破甲部署成功。\n\n完全退出并重启 ZCode 后，开新对话生效。", true);
  } else {
    Log(L"✗ 部署后验证未通过。");
    RefreshStatus();
    ReportResult(L"ZCode 部署后验证未通过。\n\n详情见日志页。", false);
  }
}

static void DoRestartZCode() {
  InstallInfo info = DetectInstall();
  if (!info.found) {
    Log(L"✗ 未找到 ZCode 安装目录，无法重启。");
    return;
  }
  std::wstring exe = JoinPath(info.root, L"ZCode.exe");
  if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES) {
    Log(L"✗ 未找到 " + exe);
    return;
  }
  Log(L"· 正在结束 ZCode 全部进程…");
  STARTUPINFOW si = {};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {};
  std::wstring cmd = L"cmd.exe /c taskkill /F /IM ZCode.exe >nul 2>&1";
  if (CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                     nullptr, &si, &pi)) {
    WaitForSingleObject(pi.hProcess, 10'000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }
  Sleep(1500);
  // explorer 中转启动：不继承本程序的管理员权限
  std::wstring exp = L"explorer.exe \"" + exe + L"\"";
  STARTUPINFOW si2 = {};
  si2.cb = sizeof(si2);
  PROCESS_INFORMATION pi2 = {};
  wchar_t buf[MAX_PATH * 2] = {};
  wcsncpy_s(buf, exp.c_str(), _TRUNCATE);
  if (CreateProcessW(nullptr, buf, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                     &si2, &pi2)) {
    CloseHandle(pi2.hProcess);
    CloseHandle(pi2.hThread);
    Log(L"✓ ZCode 已重启（正常权限）。部署/恢复操作后用它立即生效。");
  } else {
    Log(Fmt(L"✗ 启动失败 (err=%lu)，请手动启动。", GetLastError()));
  }
}

static void DoRestore() {
  InstallInfo info = DetectInstall();
  if (!info.found) {
    Log(L"✗ 未找到 ZCode 安装目录。");
    return;
  }
  if (GetFileAttributesW(info.bakPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
    Log(L"✗ 未找到官方备份（zcode.cjs.official.bak）。");
    return;
  }
  std::string bytes;
  if (!ReadFileBytes(info.bakPath.c_str(), &bytes)) {
    Log(L"✗ 读取备份失败。");
    return;
  }
  std::wstring err;
  if (!WriteFileAtomic(info.cjsPath.c_str(), bytes.data(), bytes.size(), &err)) {
    Log(L"✗ " + err);
    return;
  }
  Log(L"✓ 已恢复官方原版。重启 ZCode 后生效。");
  RefreshStatus();
  ReportResult(L"ZCode 已恢复官方原版。\n\n重启 ZCode 后生效。", true);
}

// ---------- 人格编辑 ----------

static void UpdatePersonaInfoLine() {
  std::wstring p = g_editing_path.empty() ? PersonaPath() : g_editing_path;
  if (GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES) {
    SetLabel(g_s_personainfo,
             Fmt(L"编辑文件  %s（%s 字节 · %s）", p.c_str(),
                 FormatNumber(FileSizeOf(p.c_str())).c_str(), FileTimeStr(p.c_str()).c_str()),
             C_TEXT2);
  } else {
    SetLabel(g_s_personainfo, L"编辑文件  " + p + L"（不存在，保存时创建）", C_WARN);
  }
}

static void LoadPersonaIntoEditor(const wchar_t* path) {
  g_editing_path = path;
  std::string utf8;
  if (ReadFileBytes(path, &utf8)) {
    SetWindowTextW(g_edit_persona, Utf8ToWide(utf8).c_str());
    Log(L"· 已载入 " + std::wstring(path));
  } else {
    SetWindowTextW(g_edit_persona,
                   L"（文件不存在或不可读）\r\n\r\n直接在下面输入内容并点「保存」即可创建新人格文件。");
    Log(L"· 文件不存在，编辑器进入新建模式：" + std::wstring(path));
  }
  UpdatePersonaInfoLine();
}

static void SavePersonaFromEditor() {
  std::wstring path = g_editing_path.empty() ? PersonaPath() : g_editing_path;
  CreateDirectoryW(PersonaDir().c_str(), nullptr);
  int len = GetWindowTextLengthW(g_edit_persona);
  std::wstring w(len, L'\0');
  if (len > 0) GetWindowTextW(g_edit_persona, &w[0], len + 1);
  w.resize(len);
  std::string utf8 = WideToUtf8(w);

  if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
    CopyFileW(path.c_str(), (path + L".bak").c_str(), FALSE);
  }
  std::wstring err;
  if (!WriteFileAtomic(path.c_str(), utf8.data(), utf8.size(), &err)) {
    Log(L"✗ 保存失败：" + err);
    return;
  }
  Log(L"✓ 已保存：" + path +
        Fmt(L"（%s 字节，旧版本备份为 .bak）", FormatNumber(utf8.size()).c_str()));
  Log(L"  开新对话即生效，无需重启 ZCode。");
  UpdatePersonaInfoLine();
  RefreshStatus();
}

static void InstallMasterPersona() {
  HRSRC res = FindResourceW(nullptr, L"PERSONA", RT_RCDATA);
  if (!res) {
    Log(L"✗ 内嵌人格母本缺失——程序损坏。");
    return;
  }
  HGLOBAL handle = LoadResource(nullptr, res);
  if (!handle) {
    Log(L"✗ 装载人格母本资源失败。");
    return;
  }
  void* data = LockResource(handle);
  DWORD size = SizeofResource(nullptr, res);
  std::wstring path = PersonaPath();
  CreateDirectoryW(PersonaDir().c_str(), nullptr);
  if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
    CopyFileW(path.c_str(), (path + L".bak").c_str(), FALSE);
  }
  std::wstring err;
  if (!WriteFileAtomic(path.c_str(), data, size, &err)) {
    Log(L"✗ 装填失败：" + err);
    return;
  }
  Log(Fmt(L"✓ 已装填内嵌人格母本（%s 字节，旧版本备份为 .bak）。",
          FormatNumber(size).c_str()));
  Log(L"  开新对话即生效，无需重启 ZCode。");
  LoadPersonaIntoEditor(path.c_str());
  RefreshStatus();
}

// ---------- TY 双弹：Qoder 阵地 ----------

static std::wstring QoderWorkerAt(const std::wstring& base) {
  std::wstring p = JoinPath(base, L"resources");
  p = JoinPath(p, L"app.asar.unpacked");
  p = JoinPath(p, L"node_modules");
  p = JoinPath(p, L"@qoder-ai");
  p = JoinPath(p, L"qoder-cn-agent-sdk");
  p = JoinPath(p, L"dist");
  p = JoinPath(p, L"_worker");
  return JoinPath(p, L"qoder-worker-runtime.obf.mjs");
}

// Qoder 0.4.9+ installs into .qoder-versions\<ver>; older builds keep resources at the root.
// Prefer the newest version directory that actually carries the worker runtime.
static std::wstring QoderWorkerPath() {
  wchar_t buf[MAX_PATH * 2] = {};
  if (!SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, buf)))
    return std::wstring();
  std::wstring root = JoinPath(JoinPath(buf, L"Programs"), L"Qoder CN");

  WIN32_FIND_DATAW fd = {};
  HANDLE h = FindFirstFileW(JoinPath(root, L".qoder-versions\\*").c_str(), &fd);
  std::vector<std::wstring> versions;
  if (h != INVALID_HANDLE_VALUE) {
    do {
      if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
      std::wstring name = fd.cFileName;
      if (name == L"." || name == L"..") continue;
      versions.push_back(name);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
  }
  std::sort(versions.begin(), versions.end(), [](const std::wstring& a, const std::wstring& b) {
    return a > b;  // lexical: 0.4.10 sorts above 0.4.9
  });
  for (const auto& v : versions) {
    std::wstring candidate = QoderWorkerAt(JoinPath(JoinPath(root, L".qoder-versions"), v));
    if (GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES) return candidate;
  }
  return QoderWorkerAt(root);
}

// ---------- Qoder 装甲引擎（版本无关，武器内置） ----------

static const char* B64_TABLE =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static bool IsBase64Blob(const std::string& s) {
  if (s.size() < 12 || (s.size() % 4) != 0) return false;
  for (char c : s) {
    bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '+' || c == '/' || c == '=';
    if (!ok) return false;
  }
  return true;
}

static std::string B64Decode(const std::string& in) {
  int T[256];
  for (int i = 0; i < 256; i++) T[i] = -1;
  for (int i = 0; i < 64; i++) T[(unsigned char)B64_TABLE[i]] = i;
  std::string out;
  int val = 0, bits = 0;
  for (unsigned char c : in) {
    if (c == '=') break;
    int d = T[c];
    if (d < 0) continue;
    val = (val << 6) | d;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back((char)((val >> bits) & 0xFF));
    }
  }
  return out;
}

static std::string B64Encode(const std::string& in) {
  std::string out;
  int val = 0, bits = 0;
  for (unsigned char c : in) {
    val = (val << 8) | c;
    bits += 8;
    while (bits >= 6) {
      bits -= 6;
      out.push_back(B64_TABLE[(val >> bits) & 0x3F]);
    }
  }
  if (bits > 0) out.push_back(B64_TABLE[(val << (6 - bits)) & 0x3F]);
  while (out.size() % 4) out.push_back('=');
  return out;
}

static std::string XorKey(const std::string& data, const std::string& key) {
  std::string out = data;
  if (key.empty()) return out;
  for (size_t i = 0; i < out.size(); i++)
    out[i] = (char)(out[i] ^ key[i % key.size()]);
  return out;
}

static std::string LoadResBytes(const wchar_t* name) {
  HRSRC res = FindResourceW(nullptr, name, RT_RCDATA);
  if (!res) return std::string();
  HGLOBAL h = LoadResource(nullptr, res);
  if (!h) return std::string();
  const void* p = LockResource(h);
  DWORD n = SizeofResource(nullptr, res);
  return std::string(static_cast<const char*>(p), n);
}

static std::string NormText(std::string s) {
  size_t b = 0;
  while (b < s.size() && (s[b] == '\n' || s[b] == '\r' || s[b] == ' ' || s[b] == '\t')) b++;
  s.erase(0, b);
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t'))
    s.pop_back();
  s += "\n";
  return s;
}

struct QLayerText {
  const char* name;
  std::string text;
};

// 就地装甲：身份注入 + 四层政策替换；text 为 worker 全文（UTF-8 字节）
static bool PatchQoderWorker(std::string& text, std::string& log, std::wstring& err) {
  // ---- 1) 身份常量注入（persona 装载钩）；已注入则跳过 ----
  const std::string hookMark = "/.qoder-cn/persona.txt";
  if (text.find(hookMark) == std::string::npos) {
    const std::string iife =
        "(()=>{try{const __p=require(\"node:fs\").readFileSync(require(\"node:os\")"
        ".homedir()+\"/.qoder-cn/persona.txt\",\"utf8\").trim();if(__p)return __p}"
        "catch(__e){}return ";
    auto varStart = [](const std::string& s, size_t eqPos) -> size_t {
      size_t i = eqPos;
      while (i > 0) {
        char c = s[i - 1];
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '$';
        if (!ok) break;
        i--;
      }
      return i;
    };
    // 主身份：X="You are a Qoder agent."
    const std::string primary = "=\"You are a Qoder agent.\"";
    size_t eq = text.find(primary);
    if (eq == std::string::npos) {
      err = L"identity constant not found";
      return false;
    }
    size_t vs = varStart(text, eq);
    std::string var = text.substr(vs, eq - vs);
    std::string repl = var + "=" + iife + "\"You are a Qoder agent.\"})()";
    text = text.substr(0, vs) + repl + text.substr(eq + primary.size());
    log += "identity: primary hooked (" + var + ")\n";
    // 模板身份：X=`You are ${Y}.`
    size_t tpl = text.find("=`You are ${");
    if (tpl != std::string::npos) {
      const size_t kTplInner = 12;  // "=`You are ${" is 12 chars
      size_t innerEnd = text.find("}.`", tpl);
      if (innerEnd != std::string::npos && innerEnd > tpl + kTplInner) {
        size_t vs2 = varStart(text, tpl);
        std::string var2 = text.substr(vs2, tpl - vs2);
        std::string inner = text.substr(tpl + kTplInner, innerEnd - (tpl + kTplInner));
        std::string repl2 = var2 + "=" + iife + "`You are ${" + inner + "}.`})()";
        text = text.substr(0, vs2) + repl2 + text.substr(innerEnd + 3);
        log += "identity: template hooked (" + var2 + ")\n";
      }
    }
  } else {
    log += "identity: hook already present\n";
  }

  // ---- 2) 从解码器头部嗅探 XOR 密钥 ----
  const std::string keySig = "const _$d=(s,k=\"";
  size_t ks = text.find(keySig);
  if (ks == std::string::npos) {
    err = L"decoder key not found";
    return false;
  }
  size_t k0 = ks + keySig.size();
  size_t k1 = text.find('"', k0);
  if (k1 == std::string::npos) {
    err = L"decoder key malformed";
    return false;
  }
  std::string key = text.substr(k0, k1 - k0);
  log += "key: " + key + "\n";

  // ---- 3) 四层目标文本 ----
  QLayerText layers[] = {
      {"notice", NormText(LoadResBytes(L"QNOTICE"))},
      {"delivering", NormText(LoadResBytes(L"QDELIVER"))},
      {"classifier", NormText(LoadResBytes(L"QCLASSIFY"))},
      {"rules", NormText(LoadResBytes(L"QRULES"))},
  };
  for (auto& l : layers) {
    if (l.text.size() < 32) {
      err = L"embedded layer text missing";
      return false;
    }
  }

  // ---- 4) 扫描全部 _$d("...") 密文，按明文特征归类 ----
  struct Hit {
    size_t s, e;
    std::string oldCipher, newCipher;
    const char* name;
  };
  std::vector<Hit> hits;
  std::set<std::string> seen;
  const std::string needle = "_$d(\"";
  size_t pos = 0;
  while ((pos = text.find(needle, pos)) != std::string::npos) {
    size_t s = pos + needle.size();
    size_t e = text.find('"', s);
    if (e == std::string::npos) break;
    std::string blob = text.substr(s, e - s);
    if (blob.size() > 300 && IsBase64Blob(blob)) {
      std::string dec = XorKey(B64Decode(blob), key);
      const char* kind = nullptr;
      if (dec.rfind("IMPORTANT:", 0) == 0) kind = "notice";
      else if (dec.rfind("# Delivering work", 0) == 0) kind = "delivering";
      else if (dec.rfind("Judge whether the pending action", 0) == 0) kind = "classifier";
      else if (dec.find("CHECK: Is this routine") != std::string::npos) kind = "rules";
      if (kind && !seen.count(kind)) {
        for (auto& l : layers) {
          if (strcmp(l.name, kind) == 0) {
            Hit h{};
            h.s = s;
            h.e = e;
            h.oldCipher = blob;
            h.newCipher = B64Encode(XorKey(l.text, key));
            h.name = l.name;
            hits.push_back(h);
            seen.insert(kind);
            log += std::string("layer ") + kind + ": " + std::to_string(dec.size()) +
                   " -> " + std::to_string(l.text.size()) + "\n";
            break;
          }
        }
      }
    }
    pos = e;
  }
  for (auto& l : layers) {
    if (!seen.count(l.name)) {
      std::string m = std::string("target not found: ") + l.name;
      err.assign(m.begin(), m.end());
      return false;
    }
  }

  // ---- 5) 从后往前替换（偏移不互相污染） ----
  std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.s > b.s; });
  for (auto& h : hits)
    text = text.substr(0, h.s) + h.newCipher + text.substr(h.e);

  // ---- 6) 验证：新密文在位、旧密文清除、回解一致 ----
  for (auto& h : hits) {
    if (text.find(h.newCipher) == std::string::npos) {
      err = L"verify: new cipher missing";
      return false;
    }
    // 重刷时新旧密文可以完全相同（幂等），此时不需要"旧密文消失"。
    if (h.oldCipher != h.newCipher && text.find(h.oldCipher) != std::string::npos) {
      err = L"verify: old cipher still present";
      return false;
    }
  }
  return true;
}

static std::wstring QoderVersionTag(const std::wstring& worker) {
  size_t p = worker.find(L".qoder-versions\\");
  if (p != std::wstring::npos) {
    size_t s = p + wcslen(L".qoder-versions\\");
    size_t e = worker.find(L'\\', s);
    if (e != std::wstring::npos) return worker.substr(s, e - s);
  }
  return L"legacy";
}

static std::wstring QoderPersonaDir() {
  wchar_t buf[MAX_PATH * 2] = {};
  if (!SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, 0, buf)))
    return std::wstring();
  return JoinPath(buf, L".qoder-cn");
}

static std::wstring QoderPersonaPath() { return JoinPath(QoderPersonaDir(), L"persona.txt"); }

static bool QoderArmored(const std::wstring& worker) {
  std::string bytes;
  if (!ReadFileBytes(worker.c_str(), &bytes)) return false;
  return BytesContain(bytes, "qoder-cn/persona.txt");
}

// 操作反馈：日志落账 + 自动跳日志页 + 弹窗摘要（部署页上必须看得见结果）
static void ReportResult(const std::wstring& summary, bool ok) {
  Log(L"---- 操作完成 ----");
  RefreshStatus();
  g_cur_tab = TAB_LOG;
  ShowCurrentPage();
  InvalidateRect(g_main, nullptr, TRUE);
  MessageBoxW(g_main, summary.c_str(), ok ? L"TY 破甲弹 · 成功" : L"TY 破甲弹 · 失败",
              MB_OK | (ok ? MB_ICONINFORMATION : MB_ICONWARNING));
}

static void DoQoderDeploy() {
  std::wstring worker = QoderWorkerPath();
  if (worker.empty() || GetFileAttributesW(worker.c_str()) == INVALID_FILE_ATTRIBUTES) {
    Log(L"✗ 未找到 Qoder worker 运行时（需已安装 Qoder CN）。");
    ReportResult(L"未找到 Qoder worker 运行时。\n\n请确认已安装 Qoder CN（含 .qoder-versions 或 legacy resources）。", false);
    return;
  }
  std::string bytes;
  if (!ReadFileBytes(worker.c_str(), &bytes)) {
    Log(L"✗ 读取 Qoder worker 失败（被占用？）。");
    ReportResult(L"读取 Qoder worker 失败。\n\n请完全退出 Qoder（托盘右键退出）后重试。", false);
    return;
  }
  bool wasArmored = bytes.find("/.qoder-cn/persona.txt") != std::string::npos;
  std::string plog;
  std::wstring err;
  if (!PatchQoderWorker(bytes, plog, err)) {
    Log(L"✗ Qoder 装甲失败：" + err);
    ReportResult(L"Qoder 装甲失败：" + err +
                     L"\n\n该版本结构可能已变化。请把 worker 路径与版本号反馈给 TY。", false);
    return;
  }
  for (size_t p = 0; p < plog.size();) {
    size_t nl = plog.find('\n', p);
    if (nl == std::string::npos) nl = plog.size();
    std::string lineUtf8 = plog.substr(p, nl - p);
    Log(L"· " + Utf8ToWide(lineUtf8));
    p = nl + 1;
  }

  std::wstring tag = QoderVersionTag(worker);
  std::wstring bak = worker + L".official.bak-" + tag;
  if (!wasArmored && GetFileAttributesW(bak.c_str()) == INVALID_FILE_ATTRIBUTES)
    CopyFileW(worker.c_str(), bak.c_str(), FALSE);

  if (!WriteFileAtomic(worker.c_str(), bytes.data(), bytes.size(), &err)) {
    Log(L"✗ Qoder 装甲写入失败：" + err + L"（完全退出 Qoder 后重试）");
    ReportResult(L"Qoder 装甲写入失败：\n" + err + L"\n\n请完全退出 Qoder（托盘右键退出）后重试。", false);
    return;
  }
  Log(Fmt(L"✓ Qoder worker 已装甲（版本 %s，%s 字节，官方原件 %s）。", tag.c_str(),
          FormatNumber(bytes.size()).c_str(),
          wasArmored ? L"沿用既有备份" : L"已备份 .official.bak"));
  std::wstring summary = L"Qoder worker 已装甲。\n版本 " + tag + L" · " +
                         FormatNumber(bytes.size()) + L" 字节\n" +
                         (wasArmored ? L"（原已是装甲版，本次为就地重刷）\n" : L"");

  HRSRC rp = FindResourceW(nullptr, L"QPERSONA", RT_RCDATA);
  if (rp) {
    HGLOBAL hp = LoadResource(nullptr, rp);
    if (hp) {
      void* pd = LockResource(hp);
      DWORD ps = SizeofResource(nullptr, rp);
      std::wstring ppath = QoderPersonaPath();
      if (GetFileAttributesW(ppath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        CreateDirectoryW(QoderPersonaDir().c_str(), nullptr);
        if (WriteFileAtomic(ppath.c_str(), pd, ps, &err)) {
          Log(Fmt(L"✓ Qoder 人格已播种（%s 字节）——完全退出 Qoder 后开新对话生效。",
                  FormatNumber(ps).c_str()));
          summary += L"Qoder 人格已播种（" + FormatNumber(ps) + L" 字节）。\n";
        }
      } else {
        Log(L"· Qoder 人格已存在，保留现有版本。");
        summary += L"Qoder 人格已存在，保留现有版本。\n";
      }
    }
  }

  std::string check;
  bool ok = ReadFileBytes(worker.c_str(), &check) &&
            check.find("/.qoder-cn/persona.txt") != std::string::npos;
  summary += ok ? L"\n验证通过：身份钩子与四层装甲在位。\n\n完全退出 Qoder 后开新对话生效。"
                : L"\n验证未通过：回读未检出身份钩子。";
  Log(std::wstring(L"· 验证：") + (ok ? L"✅ 四层装甲 + 身份钩子在位。" : L"✗ 验证未通过。"));
  ReportResult(summary, ok);
}

// 找同目录下最新一份 .official.bak-*（0.4.9+ 带版本后缀；兼容旧的无后缀备份）
static std::wstring FindQoderBackup(const std::wstring& worker) {
  size_t slash = worker.find_last_of(L'\\');
  if (slash == std::wstring::npos) return std::wstring();
  std::wstring dir = worker.substr(0, slash);
  std::wstring base = worker.substr(slash + 1);
  WIN32_FIND_DATAW fd = {};
  HANDLE h = FindFirstFileW(JoinPath(dir, base + L".official.bak*").c_str(), &fd);
  std::wstring best;
  FILETIME bestTime = {};
  if (h != INVALID_HANDLE_VALUE) {
    do {
      std::wstring cand = JoinPath(dir, fd.cFileName);
      if (best.empty() || CompareFileTime(&fd.ftLastWriteTime, &bestTime) > 0) {
        best = cand;
        bestTime = fd.ftLastWriteTime;
      }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
  }
  return best;
}

static void DoQoderRestore() {
  std::wstring worker = QoderWorkerPath();
  std::wstring bak = FindQoderBackup(worker);
  if (bak.empty()) {
    Log(L"✗ 未找到 Qoder 官方备份。");
    ReportResult(L"未找到 Qoder 官方备份（.official.bak-*）。\n\n没有可恢复的官方原件。", false);
    return;
  }
  if (!CopyFileW(bak.c_str(), worker.c_str(), FALSE)) {
    Log(L"✗ Qoder 恢复失败（完全退出 Qoder 后重试）。");
    ReportResult(L"Qoder 恢复失败：worker 文件被占用。\n\n请完全退出 Qoder（托盘右键退出）后重试。", false);
    return;
  }
  Log(L"✓ Qoder worker 已恢复官方原件：" + bak);
  ReportResult(L"Qoder worker 已恢复官方原件。\n来源：" + bak + L"\n\n完全退出 Qoder 后开新对话生效。", true);
}

// ---------- 人格目录 ----------

static void RefreshDirList() {
  std::wstring dir = PersonaDir();
  CreateDirectoryW(dir.c_str(), nullptr);
  ListView_DeleteAllItems(g_list_dir);

  std::vector<std::array<std::wstring, 3>> rows;
  WIN32_FIND_DATAW fd;
  HANDLE h = FindFirstFileW(JoinPath(dir, L"*").c_str(), &fd);
  if (h != INVALID_HANDLE_VALUE) {
    do {
      std::wstring name = fd.cFileName;
      if (name == L"." || name == L"..") continue;
      bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
      unsigned long long size =
          isDir ? 0 : ((unsigned long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
      rows.push_back({name, isDir ? L"⟨目录⟩" : FormatNumber(size), FileTimeStrDirect(fd.ftLastWriteTime)});
    } while (FindNextFileW(h, &fd));
    FindClose(h);
  }
  std::sort(rows.begin(), rows.end(),
            [](const std::array<std::wstring, 3>& a, const std::array<std::wstring, 3>& b) {
              return a[0] < b[0];
            });

  int i = 0;
  for (const auto& r : rows) {
    LVITEMW item = {};
    item.mask = LVIF_TEXT;
    item.iItem = i;
    item.pszText = const_cast<LPWSTR>(r[0].c_str());
    ListView_InsertItem(g_list_dir, &item);
    ListView_SetItemText(g_list_dir, i, 1, const_cast<LPWSTR>(r[1].c_str()));
    ListView_SetItemText(g_list_dir, i, 2, const_cast<LPWSTR>(r[2].c_str()));
    i++;
  }
  Log(Fmt(L"· 人格目录已刷新（%d 项）：%s", i, dir.c_str()));
}

static void OnDirItemActivate() {
  int sel = ListView_GetNextItem(g_list_dir, -1, LVNI_SELECTED);
  if (sel < 0) return;
  wchar_t name[MAX_PATH] = {};
  LVITEMW item = {};
  item.mask = LVIF_TEXT;
  item.iItem = sel;
  item.pszText = name;
  item.cchTextMax = MAX_PATH;
  ListView_GetItem(g_list_dir, &item);
  if (!name[0]) return;
  std::wstring path = JoinPath(PersonaDir(), name);
  if (GetFileAttributesW(path.c_str()) & FILE_ATTRIBUTE_DIRECTORY) {
    Log(L"· 目录项不支持编辑：" + path);
    return;
  }
  LoadPersonaIntoEditor(path.c_str());
  g_cur_tab = TAB_PERSONA;
  ShowCurrentPage();
  InvalidateRect(g_main, nullptr, TRUE);
}

// ------------------------------------------------------------
// 自绘主题
// ------------------------------------------------------------

static void DrawRounded(HDC dc, RECT rc, COLORREF fill, int radius) {
  HBRUSH br = CreateSolidBrush(fill);
  HRGN rgn = CreateRoundRectRgn(rc.left, rc.top, rc.right + 1, rc.bottom + 1, radius * 2, radius * 2);
  HBRUSH old = (HBRUSH)SelectObject(dc, br);
  FillRgn(dc, rgn, br);
  SelectObject(dc, old);
  DeleteObject(rgn);
  DeleteObject(br);
}

static void DrawButton(LPDRAWITEMSTRUCT d) {
  for (auto& b : g_btns) {
    if (b.h != d->hwndItem) continue;
    // 缓冲绘制：先铺父背景清残影，再画圆角本体，杜绝描边错位
    HDC mem = CreateCompatibleDC(d->hDC);
    HBITMAP bmp = CreateCompatibleBitmap(d->hDC, d->rcItem.right - d->rcItem.left,
                                         d->rcItem.bottom - d->rcItem.top);
    HBITMAP old = (HBITMAP)SelectObject(mem, bmp);
    RECT rc = d->rcItem;
    OffsetRect(&rc, -rc.left, -rc.top);
    FillRect(mem, &rc, g_br_bg);
    COLORREF fill = b.hover ? C_CTRL_HV : b.fill;
    DrawRounded(mem, rc, fill, 8);
    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, b.text);
    HFONT oldf = (HFONT)SelectObject(mem, g_font);
    wchar_t text[128] = {};
    GetWindowTextW(b.h, text, 128);
    DrawTextW(mem, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(mem, oldf);
    BitBlt(d->hDC, 0, 0, d->rcItem.right - d->rcItem.left, d->rcItem.bottom - d->rcItem.top, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    return;
  }
}

// 页签条自绘
static void DrawTabStrip(HDC dc, const RECT& strip) {
  // 底
  RECT bg = strip;
  FillRect(dc, &bg, g_br_bg);
  // 底部细线
  HPEN line = CreatePen(PS_SOLID, 1, C_LINE);
  HPEN oldp = (HPEN)SelectObject(dc, line);
  MoveToEx(dc, strip.left, strip.bottom - 1, nullptr);
  LineTo(dc, strip.right, strip.bottom - 1);
  SelectObject(dc, oldp);
  DeleteObject(line);

  // 状态徽章（右对齐，DT_CALCRECT 量宽防截断）
  InstallInfo info = DetectInstall();
  std::wstring build = info.found ? IdentifyBundle(info.cjsPath.c_str()) : L"";
  COLORREF dot = build == L"TZCode 破甲版（已部署）"  ? C_OK
                 : build == L"官方原版" ? C_WARN
                                        : (info.found ? C_ERR : C_TEXT2);
  std::wstring short_ = build == L"TZCode 破甲版（已部署）" ? L"● 已部署破甲"
                        : build == L"官方原版" ? L"● 官方原版"
                                               : (info.found ? L"● 未知构建" : L"● 未检测到安装");
  HFONT oldt = (HFONT)SelectObject(dc, g_font_tab);
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, dot);
  RECT meas = {0, 0, 0, 0};
  DrawTextW(dc, short_.c_str(), -1, &meas, DT_CALCRECT | DT_SINGLELINE);
  RECT dotRc = {strip.right - 8 - (meas.right - meas.left), strip.top + 4, strip.right - 8,
                strip.top + 34};
  DrawTextW(dc, short_.c_str(), -1, &dotRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
  SelectObject(dc, oldt);

  // 主标题（徽章之后绘制，互不侵占）
  oldt = (HFONT)SelectObject(dc, g_font_title);
  SetTextColor(dc, C_TEXT);
  RECT titleRc = {strip.left + 6, strip.top + 4, strip.right - 8 - (meas.right - meas.left) - 24,
                  strip.top + 34};
  DrawTextW(dc, L"TY 破甲弹", -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
  SelectObject(dc, oldt);

  // 页签
  HFONT oldf = (HFONT)SelectObject(dc, g_font_tab);
  int x = strip.left + 6;
  int wTab = 118;
  for (int t = 0; t < TAB_COUNT; t++) {
    RECT tr = {x, strip.top + 42, x + wTab, strip.bottom - 1};
    bool sel = (t == g_cur_tab);
    if (sel) {
      RECT fillRc = tr;
      FillRect(dc, &fillRc, g_br_panel);
      HPEN acc = CreatePen(PS_SOLID, 3, C_ACCENT);
      HPEN op = (HPEN)SelectObject(dc, acc);
      MoveToEx(dc, tr.left, tr.bottom - 1, nullptr);
      LineTo(dc, tr.right, tr.bottom - 1);
      SelectObject(dc, op);
      DeleteObject(acc);
      SetTextColor(dc, C_TEXT);
    } else {
      SetTextColor(dc, C_TEXT2);
    }
    DrawTextW(dc, TAB_NAMES[t], -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    x += wTab;
  }
  SelectObject(dc, oldf);
}

static void PaintMain(HWND hwnd) {
  PAINTSTRUCT ps;
  HDC dc = BeginPaint(hwnd, &ps);
  RECT rc;
  GetClientRect(hwnd, &rc);

  FillRect(dc, &rc, g_br_bg);
  DrawTabStrip(dc, g_tab_strip);

  // 部署页双区卡片：ZCode 蓝 / Qoder 青
  if (g_cur_tab == TAB_DEPLOY) {
    SetBkMode(dc, TRANSPARENT);
    // ZCode 卡片
    DrawRounded(dc, g_card_deploy, C_PANEL, 10);
    {
      RECT badge = g_card_deploy;
      badge.right = badge.left + 108; badge.bottom = badge.top + 34;
      DrawRounded(dc, badge, C_Z_DIM, 8);
      HFONT old = (HFONT)SelectObject(dc, g_font);
      SetTextColor(dc, C_ACCENT);
      RECT tr = badge; tr.top += 5; tr.bottom = tr.top + 24;
      DrawTextW(dc, L"⬢ ZCode", -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
      SelectObject(dc, old);
      old = (HFONT)SelectObject(dc, g_font_title);
      SetTextColor(dc, C_TEXT);
      tr = g_card_deploy; tr.left += 120; tr.top += 8; tr.bottom = tr.top + 26;
      DrawTextW(dc, L"破甲阵地 · zcode.cjs 固件 + 人格", -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
      SelectObject(dc, old);
    }
    // Qoder 卡片
    DrawRounded(dc, g_card_qoder, C_PANEL, 10);
    {
      RECT badge = g_card_qoder;
      badge.right = badge.left + 108; badge.bottom = badge.top + 34;
      DrawRounded(dc, badge, C_Q_DIM, 8);
      HFONT old = (HFONT)SelectObject(dc, g_font);
      SetTextColor(dc, C_QODER);
      RECT tr = badge; tr.top += 5; tr.bottom = tr.top + 24;
      DrawTextW(dc, L"⬡ Qoder", -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
      SelectObject(dc, old);
      old = (HFONT)SelectObject(dc, g_font_title);
      SetTextColor(dc, C_TEXT);
      tr = g_card_qoder; tr.left += 120; tr.top += 8; tr.bottom = tr.top + 26;
      DrawTextW(dc, L"装甲阵地 · worker 运行时 + persona 钩", -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
      SelectObject(dc, old);
    }
  }

  // 目录页自绘表头带（替代系统亮白表头）
  if (g_cur_tab == TAB_DIR) {
    int px = 24, pw = rc.right - px * 2;
    int py = g_tab_strip.bottom + 8;
    RECT head = {px, py, px + pw, py + 40};
    DrawRounded(dc, head, C_PANEL, 8);
    SetBkMode(dc, TRANSPARENT);
    HFONT old = (HFONT)SelectObject(dc, g_font);
    SetTextColor(dc, C_TEXT2);
    RECT c0 = {head.left + 20, head.top, head.left + 420, head.bottom};
    DrawTextW(dc, L"名称", -1, &c0, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT c1 = {head.left + 450, head.top, head.left + 600, head.bottom};
    DrawTextW(dc, L"大小", -1, &c1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT c2 = {head.left + 630, head.top, head.right - 20, head.bottom};
    DrawTextW(dc, L"修改时间", -1, &c2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, old);
  }
  EndPaint(hwnd, &ps);
}

// ------------------------------------------------------------
// 布局与页签
// ------------------------------------------------------------

static void ShowCurrentPage() {
  for (int t = 0; t < TAB_COUNT; t++)
    for (HWND c : g_pages[t]) ShowWindow(c, t == g_cur_tab ? SW_SHOW : SW_HIDE);
}

static void Layout() {
  RECT rc;
  GetClientRect(g_main, &rc);
  int W = rc.right, H = rc.bottom;

  g_tab_strip = {12, 10, W - 12, 10 + 84};
  int px = 24, py = g_tab_strip.bottom + 8;
  int pw = W - px * 2, ph = H - py - 12;

  // 部署页 — 双区卡片（按钮排嵌在各卡片内部底沿）
  g_card_deploy = {px, py, px + pw, py + 296};            // ZCode 区：5 状态行 + 按钮排
  g_card_qoder  = {px, g_card_deploy.bottom + 14, px + pw, g_card_deploy.bottom + 14 + 208}; // Qoder 区：3 状态行 + 按钮排
  int y = py + 44;
  for (HWND s : {g_s_install, g_s_build, g_s_file, g_s_check, g_s_persona}) {
    MoveWindow(s, px + 18, y, pw - 36, 22, TRUE);
    y += 30;
  }
  MoveWindow(g_btn_deploy, px, g_card_deploy.bottom - 58, 200, 42, TRUE);
  MoveWindow(g_btn_restore, px + 212, g_card_deploy.bottom - 58, 200, 42, TRUE);
  MoveWindow(g_btn_refresh, px + 424, g_card_deploy.bottom - 58, 170, 42, TRUE);
  MoveWindow(g_btn_restart, px + 606, g_card_deploy.bottom - 58, 190, 42, TRUE);
  y = g_card_qoder.top + 44;
  for (HWND s : {g_s_qworker, g_s_qarmor, g_s_qpersona}) {
    MoveWindow(s, px + 18, y, pw - 36, 22, TRUE);
    y += 30;
  }
  MoveWindow(g_btn_qdeploy, px, g_card_qoder.bottom - 58, 220, 42, TRUE);
  MoveWindow(g_btn_qrestore, px + 232, g_card_qoder.bottom - 58, 210, 42, TRUE);

  // 人格页
  MoveWindow(g_s_personainfo, px + 4, py + 2, pw - 8, 22, TRUE);
  MoveWindow(g_edit_persona, px, py + 30, pw, ph - 84, TRUE);
  MoveWindow(g_btn_save, px, py + ph - 46, 230, 40, TRUE);
  MoveWindow(g_btn_reload, px + 242, py + ph - 46, 170, 40, TRUE);
  MoveWindow(g_btn_notepad, px + 424, py + ph - 46, 190, 40, TRUE);
  MoveWindow(g_btn_persona_master, px + 626, py + ph - 46, 170, 40, TRUE);

  // 目录页（列表下移 48px 给自绘表头带让位）
  MoveWindow(g_list_dir, px, py + 48, pw, ph - 104, TRUE);
  MoveWindow(g_btn_dir_refresh, px, py + ph - 46, 170, 40, TRUE);
  MoveWindow(g_btn_dir_explorer, px + 182, py + ph - 46, 250, 40, TRUE);

  // 日志页
  MoveWindow(g_edit_log, px, py, pw, ph, TRUE);

  InvalidateRect(g_main, nullptr, TRUE);
}

static HWND MakeStatic(int tab, HWND parent, const wchar_t* text, bool on_panel) {
  HWND h = CreateWindowW(L"STATIC", text, WS_CHILD | SS_LEFT, 0, 0, 0, 0, parent, nullptr, nullptr, nullptr);
  RegPage(tab, h);
  g_label_colors[h] = C_TEXT;
  if (on_panel) g_panel_labels.insert(h);
  return h;
}

static HWND MakeButton(int tab, HWND parent, const wchar_t* text, int id, COLORREF fill, COLORREF txt) {
  HWND h = CreateWindowW(L"BUTTON", text, WS_CHILD | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0, parent,
                         (HMENU)(INT_PTR)id, nullptr, nullptr);
  RegPage(tab, h);
  g_btns.push_back({h, fill, txt, false});
  return h;
}

// ------------------------------------------------------------
// 窗口过程
// ------------------------------------------------------------

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE: {
      g_br_bg = CreateSolidBrush(C_BG);
      g_br_panel = CreateSolidBrush(C_PANEL);
      g_br_ctrl = CreateSolidBrush(C_CTRL);
      g_br_ctrl_hv = CreateSolidBrush(C_CTRL_HV);
      g_br_edit = CreateSolidBrush(RGB(28, 29, 34));

      g_font = CreateFontW(18, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                           CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
      g_font_title = CreateFontW(24, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
      g_font_tab = CreateFontW(19, 0, 0, 0, FW_MEDIUM, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
      g_font_mono = CreateFontW(17, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                                CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Cascadia Mono");

      // 部署页 — ZCode 区
      g_s_install = MakeStatic(TAB_DEPLOY, hwnd, L"安装目录：检测中…", true);
      g_s_build = MakeStatic(TAB_DEPLOY, hwnd, L"当前构建：检测中…", true);
      g_s_file = MakeStatic(TAB_DEPLOY, hwnd, L"文件信息：—", true);
      g_s_check = MakeStatic(TAB_DEPLOY, hwnd, L"标记验证：—", true);
      g_s_persona = MakeStatic(TAB_DEPLOY, hwnd, L"人格文件：检测中…", true);
      g_btn_deploy = MakeButton(TAB_DEPLOY, hwnd, L"🚀 部署ZCode破甲", IDC_DEPLOY, C_ACCENT, RGB(255, 255, 255));
      g_btn_restore = MakeButton(TAB_DEPLOY, hwnd, L"♻ 恢复ZCode官方", IDC_RESTORE, C_CTRL, C_TEXT);
      g_btn_refresh = MakeButton(TAB_DEPLOY, hwnd, L"🔄 刷新状态", IDC_REFRESH, C_CTRL, C_TEXT);
      g_btn_restart = MakeButton(TAB_DEPLOY, hwnd, L"⚡ 重启 ZCode", IDC_RESTART_ZCODE, C_CTRL, C_TEXT);
      // 部署页 — Qoder 区
      g_s_qworker = MakeStatic(TAB_DEPLOY, hwnd, L"Qoder worker：检测中…", true);
      g_s_qarmor = MakeStatic(TAB_DEPLOY, hwnd, L"Qoder 装甲：—", true);
      g_s_qpersona = MakeStatic(TAB_DEPLOY, hwnd, L"Qoder 人格：检测中…", true);
      g_btn_qdeploy = MakeButton(TAB_DEPLOY, hwnd, L"🛡 部署Qoder装甲", IDC_QODER_DEPLOY, C_QODER, RGB(12, 30, 28));
      g_btn_qrestore = MakeButton(TAB_DEPLOY, hwnd, L"♻ 恢复Qoder官方", IDC_QODER_RESTORE, C_CTRL, C_TEXT);

      // 人格页
      g_s_personainfo = MakeStatic(TAB_PERSONA, hwnd, L"编辑文件：—", false);
      g_edit_persona = CreateWindowW(
          L"EDIT", L"",
          WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
          0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
      RegPage(TAB_PERSONA, g_edit_persona);
      g_btn_save = MakeButton(TAB_PERSONA, hwnd, L"💾 保存（生效于新对话）", IDC_PERSONA_SAVE, C_ACCENT, RGB(255, 255, 255));
      g_btn_reload = MakeButton(TAB_PERSONA, hwnd, L"↺ 重新加载", IDC_PERSONA_RELOAD, C_CTRL, C_TEXT);
      g_btn_notepad = MakeButton(TAB_PERSONA, hwnd, L"📝 用记事本打开", IDC_PERSONA_NOTEPAD, C_CTRL, C_TEXT);
      g_btn_persona_master = MakeButton(TAB_PERSONA, hwnd, L"🎯 装填母本", IDC_PERSONA_MASTER, C_CTRL, C_TEXT);

      // 目录页
      g_list_dir = CreateWindowW(WC_LISTVIEWW, L"",
                                 WS_CHILD | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS |
                                     LVS_NOCOLUMNHEADER,
                                 0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
      RegPage(TAB_DIR, g_list_dir);
      ListView_SetExtendedListViewStyle(g_list_dir,
                                        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
      ListView_SetBkColor(g_list_dir, C_PANEL);
      ListView_SetTextBkColor(g_list_dir, C_PANEL);
      ListView_SetTextColor(g_list_dir, C_TEXT);
      {
        // 首列自带表头视觉（自绘替代系统亮白表头）
        LVCOLUMNW col = {};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.pszText = const_cast<LPWSTR>(L"  📄 文件名");
        col.cx = 420;
        ListView_InsertColumn(g_list_dir, 0, &col);
        col.pszText = const_cast<LPWSTR>(L"大小");
        col.cx = 160;
        ListView_InsertColumn(g_list_dir, 1, &col);
        col.pszText = const_cast<LPWSTR>(L"修改时间");
        col.cx = 220;
        ListView_InsertColumn(g_list_dir, 2, &col);
      }
      // 行高：用绝对字号撑高行（近似 2 倍行距的呼吸感）
      {
        HIMAGELIST img = ImageList_Create(1, 34, ILC_COLOR, 1, 1);
        ListView_SetImageList(g_list_dir, img, LVSIL_SMALL);
      }
      g_btn_dir_refresh = MakeButton(TAB_DIR, hwnd, L"🔄 刷新", IDC_DIR_REFRESH, C_CTRL, C_TEXT);
      g_btn_dir_explorer = MakeButton(TAB_DIR, hwnd, L"📂 在资源管理器中打开", IDC_DIR_EXPLORER, C_CTRL, C_TEXT);

      // 日志页
      g_edit_log = CreateWindowW(L"EDIT", L"",
                                 WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                                 0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
      RegPage(TAB_LOG, g_edit_log);

      for (int t = 0; t < TAB_COUNT; t++)
        for (HWND c : g_pages[t]) SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
      SendMessageW(g_edit_persona, WM_SETFONT, (WPARAM)g_font_mono, TRUE);
      SendMessageW(g_edit_log, WM_SETFONT, (WPARAM)g_font_mono, TRUE);

      Log(L"ZCode 破甲弹 v2.1 就绪。弹药：TZCode 破甲构建（3.14.3 同源 · 人格热装载 · 已授权话术）。");
      SetTimer(hwnd, 1, 400, nullptr);
      RefreshStatus();
      LoadPersonaIntoEditor(PersonaPath().c_str());
      RefreshDirList();
      g_cur_tab = g_start_tab;
      ShowCurrentPage();
      return 0;
    }
    case WM_PAINT:
      PaintMain(hwnd);
      return 0;
    case WM_ERASEBKGND:
      return 1;  // 自绘 WM_PAINT 全覆盖
    case WM_SIZE:
      Layout();
      return 0;
    case WM_TIMER: {
      return 0;  // 预留：后续可用于状态自动刷新
    }
    case WM_LBUTTONDOWN: {
      // 页签点击命中（客户区物理像素，DPI aware 已声明）
      POINT pt = {LOWORD(lParam), HIWORD(lParam)};
      int stripBottom = g_tab_strip.bottom;
      int stripTop = g_tab_strip.top + 42;
      if (pt.y >= stripTop && pt.y <= stripBottom && pt.x >= g_tab_strip.left &&
          pt.x <= g_tab_strip.right) {
        int idx = (pt.x - (g_tab_strip.left + 6)) / 118;
        if (idx >= 0 && idx < TAB_COUNT && g_cur_tab != idx) {
          g_cur_tab = idx;
          ShowCurrentPage();
          InvalidateRect(hwnd, nullptr, TRUE);
        }
      }
      return 0;
    }
    case WM_MOUSEMOVE: {
      // 按钮悬停
      POINT pt = {LOWORD(lParam), HIWORD(lParam)};
      bool changed = false;
      for (auto& b : g_btns) {
        RECT rc;
        GetWindowRect(b.h, &rc);
        MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&rc, 2);
        bool now = !!PtInRect(&rc, pt);
        if (IsWindowVisible(b.h) && now != b.hover) {
          b.hover = now;
          InvalidateRect(b.h, nullptr, TRUE);
          changed = true;
        }
      }
      (void)changed;
      return 0;
    }
    case WM_DRAWITEM: {
      DrawButton((LPDRAWITEMSTRUCT)lParam);
      return TRUE;
    }
    case WM_CTLCOLORSTATIC: {
      HDC dc = (HDC)wParam;
      HWND ctrl = (HWND)lParam;
      SetBkMode(dc, TRANSPARENT);
      auto it = g_label_colors.find(ctrl);
      SetTextColor(dc, it != g_label_colors.end() ? it->second : C_TEXT);
      return (LRESULT)(g_panel_labels.count(ctrl) ? g_br_panel : g_br_bg);
    }
    case WM_CTLCOLOREDIT: {
      HDC dc = (HDC)wParam;
      SetBkColor(dc, RGB(28, 29, 34));
      SetTextColor(dc, C_TEXT);
      return (LRESULT)g_br_edit;
    }
    case WM_NOTIFY: {
      NMHDR* nm = (NMHDR*)lParam;
      if (nm->hwndFrom == g_list_dir) {
        if (nm->code == NM_DBLCLK) {
          OnDirItemActivate();
          return 0;
        }
        if (nm->code == NM_CUSTOMDRAW) {
          LPNMLVCUSTOMDRAW cd = (LPNMLVCUSTOMDRAW)lParam;
          if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
            cd->clrText = C_TEXT;
            // 斑马行 + 选中行高亮（无网格线的现代列表观感）
            bool selected = ListView_GetItemState(g_list_dir, (int)cd->nmcd.dwItemSpec,
                                                  LVIS_SELECTED) & LVIS_SELECTED;
            cd->clrTextBk = selected ? RGB(52, 66, 96)
                            : (cd->nmcd.dwItemSpec % 2 == 0) ? C_PANEL : RGB(38, 40, 47);
            return CDRF_NOTIFYSUBITEMDRAW;
          }
          return CDRF_DODEFAULT;
        }
      }
      break;
    }
    case WM_COMMAND: {
      switch (LOWORD(wParam)) {
        case IDC_DEPLOY: DoDeploy(); break;
        case IDC_RESTORE: DoRestore(); break;
        case IDC_RESTART_ZCODE: DoRestartZCode(); break;
        case IDC_REFRESH: RefreshStatus(); Log(L"· 状态已刷新"); InvalidateRect(hwnd, nullptr, TRUE); break;
        case IDC_PERSONA_SAVE: SavePersonaFromEditor(); break;
        case IDC_PERSONA_RELOAD: LoadPersonaIntoEditor(g_editing_path.c_str()); break;
        case IDC_PERSONA_NOTEPAD:
          ShellExecuteW(hwnd, L"open", L"notepad.exe",
                        (g_editing_path.empty() ? PersonaPath() : g_editing_path).c_str(), nullptr,
                        SW_SHOWNORMAL);
          break;
        case IDC_PERSONA_MASTER: InstallMasterPersona(); break;
        case IDC_QODER_DEPLOY: DoQoderDeploy(); break;
        case IDC_QODER_RESTORE: DoQoderRestore(); break;
        case IDC_DIR_REFRESH: RefreshDirList(); break;
        case IDC_DIR_EXPLORER: {
          CreateDirectoryW(PersonaDir().c_str(), nullptr);
          ShellExecuteW(hwnd, L"open", PersonaDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
          break;
        }
      }
      return 0;
    }
    case WM_SETCURSOR: {
      // 页签区手型
      if (LOWORD(lParam) == HTCLIENT) {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(hwnd, &pt);
        if (PtInRect(&g_tab_strip, pt) && pt.y > g_tab_strip.top + 42) {
          SetCursor(LoadCursorW(nullptr, IDC_HAND));
          return TRUE;
        }
      }
      break;
    }
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
  // 命令行：--armor-qoder-cli <worker> <out> —— 无界面装甲自测（写 <out>.log）
  {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc >= 4 && wcscmp(argv[1], L"--armor-qoder-cli") == 0) {
      std::string bytes;
      std::string log;
      std::wstring err;
      int code = 0;
      if (!ReadFileBytes(argv[2], &bytes)) {
        log = "read failed\n";
        code = 3;
      } else if (!PatchQoderWorker(bytes, log, err)) {
        log += "PATCH FAILED: ";
        log += WideToUtf8(err);
        log += "\n";
        code = 4;
      } else if (!WriteFileAtomic(argv[3], bytes.data(), bytes.size(), &err)) {
        log += "write failed: ";
        log += WideToUtf8(err);
        log += "\n";
        code = 5;
      } else {
        log += "OK bytes=" + std::to_string(bytes.size()) + "\n";
      }
      std::wstring logPath = std::wstring(argv[3]) + L".log";
      WriteFileAtomic(logPath.c_str(), log.data(), log.size(), &err);
      LocalFree(argv);
      return code;
    }
    if (argv) LocalFree(argv);
  }
  // 命令行：--tab=deploy|persona|dir|log 启动直落页签（供自动化/快速定位）
  {
    wchar_t* cmd = GetCommandLineW();
    if (cmd && wcsstr(cmd, L"--tab=persona")) g_start_tab = TAB_PERSONA;
    else if (cmd && wcsstr(cmd, L"--tab=dir")) g_start_tab = TAB_DIR;
    else if (cmd && wcsstr(cmd, L"--tab=log")) g_start_tab = TAB_LOG;
  }
  // DPI aware：声明真 DPI 感知，客户区坐标 == 物理像素，命中测试不再漂移
  {
    HMODULE user = GetModuleHandleW(L"user32.dll");
    if (user) {
      typedef BOOL(WINAPI * SetProcDpiAwareFn)(int);
      SetProcDpiAwareFn fn = (SetProcDpiAwareFn)GetProcAddress(user, "SetProcessDPIAware");
      if (fn) fn(2);  // PROCESS_PER_MONITOR_DPI_AWARE
    }
  }

  INITCOMMONCONTROLSEX icc = {sizeof(icc),
                              ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES | ICC_LISTVIEW_CLASSES};
  InitCommonControlsEx(&icc);

  WNDCLASSW wc = {};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  wc.hbrBackground = nullptr;
  wc.lpszClassName = L"ZCodeArmorPiercer";
  RegisterClassW(&wc);

  g_main = CreateWindowW(L"ZCodeArmorPiercer", APP_TITLE, WS_OVERLAPPEDWINDOW,
                         CW_USEDEFAULT, CW_USEDEFAULT, 1000, 700, nullptr, nullptr, inst, nullptr);
  ShowWindow(g_main, show);
  UpdateWindow(g_main);

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return (int)msg.wParam;
}
