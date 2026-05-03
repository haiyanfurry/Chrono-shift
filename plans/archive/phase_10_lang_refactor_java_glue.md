# Phase 10: 大规模语言重构计划

## 目标架构

```
┌──────────────────────────────────────────────────────────────────┐
│              Chrono-shift Windows Client (仅限于 Windows)         │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  C++ Core (主应用程序)                                     │   │
│  │  ├── app/       ─ 应用入口、窗口管理、IPC                  │   │
│  │  ├── network/   ─ TCP/HTTP/WebSocket 网络层                │   │
│  │  ├── storage/   ─ 本地存储、会话管理                       │   │
│  │  ├── security/  ─ Token管理、加密引擎调度                  │   │
│  │  └── util/      ─ 日志、工具函数                           │   │
│  │         │                                                   │   │
│  │         │ 嵌入 JVM (JNI Invocation API)                     │   │
│  │         ▼                                                   │   │
│  │  ┌──────────────────────────────────────────────────┐       │   │
│  │  │  Java Glue Layer (JNI 胶水层)                      │       │   │
│  │  │                                                    │       │   │
│  │  │  ┌─────────────────────────────────────────┐      │       │   │
│  │  │  │ TrafficFilter (流量过滤器) ★核心★         │      │       │   │
│  │  │  │ ├── SqlInjectionFilter  ─ SQL注入检测     │      │       │   │
│  │  │  │ ├── XssFilter           ─ XSS攻击检测     │      │       │   │
│  │  │  │ ├── CommandInjectionFilter ─ 命令注入检测  │      │       │   │
│  │  │  │ ├── PathTraversalFilter  ─ 路径遍历检测    │      │       │   │
│  │  │  │ └── RateLimitFilter     ─ 请求频率限制     │      │       │   │
│  │  │  └─────────────────────────────────────────┘      │       │   │
│  │  │                                                    │       │   │
│  │  │  所有出站流量 → Java 过滤 → 转发到 Server         │       │   │
│  │  │                                                    │       │   │
│  │  │  ┌─────────────────────────────────────────┐      │       │   │
│  │  │  │ Bridge Layer (桥接层)                    │      │       │   │
│  │  │  │ ├── CppBridge.java   ─ C++ ↔ Java JNI    │      │       │   │
│  │  │  │ └── RustBridge.java  ─ Java ↔ Rust JNI   │      │       │   │
│  │  │  └─────────────────────────────────────────┘      │       │   │
│  │  └──────────────────────────────────────────────────┘       │   │
│  │             │                                                  │   │
│  │             │ JNI                                               │   │
│  │             ▼                                                  │   │
│  │  ┌──────────────────────────────────────────────────────────┐ │   │
│  │  │  Rust Security Layer (cdylib .dll)                        │ │   │
│  │  │  ├── crypto.rs         ─ AES-GCM 加密解密                 │ │   │
│  │  │  ├── session.rs        ─ 会话 Token 管理                  │ │   │
│  │  │  ├── secure_storage.rs ─ 安全存储                         │ │   │
│  │  │  ├── safe_string.rs    ─ ★安全字符串桥接 (防高位截断)     │ │   │
│  │  │  ├── anti_debug.rs     ─ 反调试检测                       │ │   │
│  │  │  ├── integrity.rs      ─ 完整性校验                       │ │   │
│  │  │  └── memory_guard.rs   ─ 内存保护                         │ │   │
│  │  └──────────────────────────────────────────────────────────┘ │   │
│  │              │                                                  │   │
│  │  ┌──────────────────────────────────────────────────────────┐ │   │
│  │  │  网络出站 ──► Java 流量过滤 ──► 转发到 Server              │ │   │
│  │  │  (C++ network/)    (过滤攻击)    (工作室 9 人开发)          │ │   │
│  │  └──────────────────────────────────────────────────────────┘ │   │
│  │                                                                  │   │
│  │  ┌──────────────────────────────────────────────────────────┐   │   │
│  │  │  WebView UI (HTML/JS/CSS)                                 │   │   │
│  │  │  ├── index.html / oauth_callback.html                     │   │   │
│  │  │  ├── css/    ─ 主题、布局、动画                            │   │   │
│  │  │  └── js/     ─ API、Auth、Chat、OAuth                     │   │   │
│  │  └──────────────────────────────────────────────────────────┘   │   │
│  └──────────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 关键设计要点

### 1. Java 流量过滤 (核心功能)

所有从客户端发出的网络请求，经过 C++ 网络层后，先进入 Java 流量过滤器，再转发到 Server：

```
C++ network/ → JNI → Java TrafficFilter → (过滤后) → JNI → C++ → 发出请求到 Server
                    │                          │
                    └── 命中规则? ── 拦截 + 日志 ──┘
```

**流量过滤器规则示例 (Java)：**

```java
public class SqlInjectionFilter {
    // SQL 注入关键词模式
    private static final Pattern[] SQL_PATTERNS = {
        Pattern.compile("('|--|;|\\bOR\\b|\\bAND\\b|\\bUNION\\b|\\bDROP\\b|\\bDELETE\\b|\\bINSERT\\b)", 
            Pattern.CASE_INSENSITIVE),
        Pattern.compile("\\bSELECT\\b.*\\bFROM\\b", Pattern.CASE_INSENSITIVE),
        Pattern.compile("\\bEXEC\\b.*\\(", Pattern.CASE_INSENSITIVE),
        Pattern.compile("\\bWAITFOR\\b.*\\bDELAY\\b", Pattern.CASE_INSENSITIVE),
    };
    
    public static FilterResult check(String input) {
        for (Pattern p : SQL_PATTERNS) {
            Matcher m = p.matcher(input);
            if (m.find()) {
                return FilterResult.block("SQL injection detected: " + m.group());
            }
        }
        return FilterResult.PASS;
    }
}
```

### 2. Rust 防止 Java 高位截断 (关键安全设计)

**问题：** Java 的 `char` 是 UTF-16 (16位)，而 C/C++/Rust 的 `char` 是 UTF-8 (8位)。当 Java 通过 JNI 传递字符串到原生层时，UTF-16 → UTF-8 转换可能导致**高位截断绕过**。

**攻击示例：**
```
Java String: "\uFF00' OR 1=1--"  
  └── Java 内部: 0xFF00 0x0027 0x0020 0x004F 0x0052 ...
  └── 简单截断为 char: FF 00 27 20 4F 52 ...
  └── SQL 注入被绕过!
```

**解决方案 — Rust safe_string 模块：**

```rust
// safe_string.rs — 所有 Java 传入的字符串必须经过此模块校验
pub struct SafeString {
    inner: String,
}

impl SafeString {
    /// 从 JNI 字符串创建 SafeString，进行完整校验
    pub fn from_jni(env: &JNIEnv, input: &JStr) -> Result<SafeString, StringError> {
        let rust_str = input.to_str()?;  // JNI 自动处理 UTF-16 → UTF-8
        
        // 1. 检查是否有非 BMP 字符截断
        check_surrogate_pairs(&rust_str)?;
        
        // 2. 检查是否有空字节注入
        if rust_str.contains('\0') {
            return Err(StringError::NullByteInjection);
        }
        
        // 3. 检查长度一致性 (Java length vs Rust chars)
        //    Java 的 String.length() 返回 UTF-16 码元数量
        //    Rust 的 chars().count() 返回 Unicode 标量数量
        //    差异可能表明有截断攻击
        let java_len = env.get_string_length(input)?;
        let rust_chars = rust_str.chars().count();
        if (java_len - rust_chars).abs() > 2 {
            //  surrogate pair 可能导致少量差异，过大差异则可疑
            return Err(StringError::SuspiciousLengthMismatch);
        }
        
        Ok(SafeString { inner: rust_str })
    }
    
    /// 获取安全的字符串引用
    pub fn as_str(&self) -> &str {
        &self.inner
    }
}
```

**数据流：**
```
Java String (UTF-16)
  → JNI 自动转换
  → Rust safe_string::from_jni() 校验
      ├── 校验通过 → 返回 SafeString → 传给 C++ 或 Java
      └── 校验失败 → 返回错误 → Java 层拦截请求 + 记录日志
```

### 3. 三层语言各司其职

| 层 | 语言 | 职责 | 不可替代的理由 |
|---|------|------|---------------|
| **核心** | C++ | 窗口管理、WebView、IPC、网络 I/O | 原生 Windows 性能、直接 Win32 API |
| **胶水** | Java | 流量过滤、安全策略编排、规则引擎 | 成熟的规则引擎(Drools)、正则库、生态 |
| **安全** | Rust | 加密、防高位截断、反调试、完整性 | 内存安全、零成本抽象、防绕过 |

---

## 详细实施步骤

### Step 1: 删除 server/ 目录 + 清理 client 依赖

删除整个 `server/` 目录，清理 client 对 server 的所有引用。

**涉及文件：**
- 删除 `server/` 整个目录
- 删除 `client/include/` 中的旧 C 头文件
- 修改 `client/CMakeLists.txt`：
  - 移除 `../server/include` 引用
  - 移除 C 语言支持 (`LANGUAGES C`)
  - 移除 C 源文件 glob
- 更新根目录 `CMakeLists.txt` 移除 server 子目录
- 删除 `installer/server_installer.nsi`
- 删除 `docs/` 中 server 相关文档
- 删除 `tests/` 中 server 测试脚本

### Step 2: Rust staticlib → cdylib + safe_string 模块

**涉及文件：**
- `client/security/Cargo.toml`：`crate-type = ["cdylib"]`，新增 `jni` 依赖
- `client/security/src/safe_string.rs`：★ 新增 — 防高位截断字符串校验
- `client/security/src/lib.rs`：导出所有 JNI 接口
- `client/CMakeLists.txt`：不再直接链接 .a，改为构建后复制 .dll

**新 Rust 模块：**

```rust
// safe_string.rs  JNI 接口
#[no_mangle]
pub extern "C" fn Java_com_chronoshift_bridge_RustBridge_validateString(
    env: JNIEnv, _class: JClass, input: JString
) -> jboolean {
    match SafeString::from_jni(&env, &input) {
        Ok(_) => JNI_TRUE,
        Err(_) => JNI_FALSE,
    }
}

// 同时保留 C FFI 接口供 C++ 直接调用
#[no_mangle]
pub extern "C" fn rust_validate_utf8_safe(input: *const c_char, len: usize) -> i32 {
    // ... C 兼容接口
}
```

### Step 3: 创建 Java JNI 胶水层

```
client/java/
├── pom.xml
├── src/main/java/com/chronoshift/
│   ├── ChronoShiftClient.java       # JVM 入口
│   ├── bridge/
│   │   ├── CppBridge.java           # C++ ↔ Java JNI 桥接
│   │   └── RustBridge.java          # Java ↔ Rust JNI 桥接
│   ├── filter/
│   │   ├── TrafficFilter.java       # 流量过滤器主入口
│   │   ├── SqlInjectionFilter.java  # SQL 注入检测
│   │   ├── XssFilter.java           # XSS 攻击检测
│   │   ├── CommandInjectionFilter.java
│   │   ├── PathTraversalFilter.java
│   │   └── RateLimitFilter.java
│   ├── security/
│   │   └── SecurityOrchestrator.java
│   └── config/
│       └── FilterConfig.java
```

### Step 4: C++ 嵌入 JVM + 网络层集成过滤器

修改 `Main.cpp` 启动流程：

```
C++ 启动
  ├── 初始化 Win32 窗口
  ├── 创建 WebView
  ├── 启动 JVM (加载 chrono-glue.jar)
  ├── 初始化 Java TrafficFilter
  └── 启动 IPC 监听
```

网络请求流程改造：

```
用户操作 → C++ network 准备请求
  → 调用 Java TrafficFilter.check(request)
    ├── BLOCK → 返回错误给用户 + 日志
    └── PASS  → C++ 发出实际 HTTP 请求到 Server
```

### Step 5: Rust 防攻击层

| 文件 | 功能 | 关键 API |
|------|------|----------|
| `anti_debug.rs` | 反调试检测 | `checkAntiDebug(): int` — 检测调试器/沙箱 |
| `integrity.rs` | PE 文件完整性 | `verifyFileIntegrity(path): bool` — CRC 校验 |
| `memory_guard.rs` | 敏感数据保护 | `protectSensitiveData(data): byte[]` — 加密内存 |

### Step 6: 集成构建系统

```
1. cd client/security  && cargo build --release  → chrono_client_security.dll
2. cd client/java      && mvn package            → chrono-glue.jar
3. cd client           && cmake -B build && cmake --build build  → chrono-client.exe
``

### Step 7: 编译验证 + 集成测试

- 各层独立单元测试
- Java 过滤器模式匹配测试
- Rust safe_string 防绕过测试
- JNI 全链路集成测试
- 确认所有测试通过

### Step 8: Git 提交

---

## 风险与注意事项

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| JVM 启动开销 (~50ms) | 客户端启动变慢 | 懒加载 JVM，非必要流量不过滤 |
| Java UTF-16 高位截断 | 安全检查绕过 | Rust safe_string 层做严格校验 |
| Rust cdylib 分发 | 需额外分发 .dll | 静态链接 C Runtime，单文件部署 |
| 用户无 JRE | 无法启动 | jlink 定制最小 JRE (~15MB) 捆绑 |
| 流量过滤性能开销 | 网络请求变慢 | 过滤器可配置开关，按需启用 |
| server 接口变更 | 过滤规则失效 | 过滤规则独立配置，可热更新 |
