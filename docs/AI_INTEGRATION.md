# AI 集成文档

> Chrono-shift 客户端 AI 功能集成指南 — 多提供商支持、配置说明、API 参考

---

## 目录

- [概述](#概述)
- [支持的 AI 提供商](#支持的-ai-提供商)
  - [提供商对比表](#提供商对比表)
- [架构设计](#架构设计)
  - [C++ 后端层](#c-后端层)
  - [前端层](#前端层)
  - [请求路由](#请求路由)
- [配置指南](#配置指南)
  - [OpenAI](#openai)
  - [DeepSeek (v4)](#deepseek-v4)
  - [xAI Grok](#xai-grok)
  - [Ollama (本地)](#ollama-本地)
  - [Google Gemini](#google-gemini)
  - [自定义 API](#自定义-api)
- [C++ API 参考](#c-api-参考)
  - [AIProviderType 枚举](#aiprovidertype-枚举)
  - [AIConfig 结构体](#aiconfig-结构体)
  - [AIProvider 工厂方法](#aiprovider-工厂方法)
  - [GeminiProvider](#geminiprovider)
- [JavaScript API 参考](#javascript-api-参考)
  - [AIChat.PROVIDERS](#aichatproviders)
  - [AIChat.callAPI()](#aichatcallapi)
  - [AIChat.testConnection()](#aichattestconnection)
  - [AIChat.getSmartReply()](#aichatgetsmartreply)
- [注意事项](#注意事项)
  - [DeepSeek v4 模型变更](#deepseek-v4-模型变更)
  - [Ollama CORS 问题](#ollama-cors-问题)
  - [Gemini API Key 安全](#gemini-api-key-安全)
  - [自定义 API 兼容性](#自定义-api-兼容性)

---

## 概述

Chrono-shift 的 AI 集成模块支持 **6 种 AI 提供商**，通过统一的接口提供 AI 对话、智能回复等功能。

- **OpenAI 兼容协议**：OpenAI、DeepSeek、xAI Grok、Ollama（均使用 `/v1/chat/completions` 和 `/v1/models` 端点）
- **Google Gemini 协议**：使用独特的 `generateContent` API 格式
- **自定义 API**：允许用户指定任意兼容 OpenAI 协议的端点

---

## 支持的 AI 提供商

### 提供商对比表

| 提供商 | 协议类型 | 默认端点 | 默认模型 | 需 API Key | 代码标识 |
|--------|----------|----------|----------|-----------|----------|
| **OpenAI** | OpenAI 兼容 | `https://api.openai.com` | `gpt-4o` | 是 | `kOpenAI` |
| **DeepSeek** | OpenAI 兼容 | `https://api.deepseek.com` | `deepseek-v4-flash` | 是 | `kDeepSeek` |
| **xAI Grok** | OpenAI 兼容 | `https://api.x.ai` | `grok-3` | 是 | `kXAI` |
| **Ollama** | OpenAI 兼容 | `http://localhost:11434` | `llama3` | **否** | `kOllama` |
| **Google Gemini** | Gemini 原生 | `https://generativelanguage.googleapis.com` | `gemini-2.0-flash` | 是 | `kGemini` |
| **自定义** | 用户指定 | 用户填写 | 用户填写 | 可选 | `kCustom` |

---

## 架构设计

```
┌────────────────────────────────────────────────────────────┐
│                   前端 UI (JavaScript)                       │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  AIChat.PROVIDERS 常量表                              │  │
│  │  ├─ callAPI() ─── 路由分发                              │  │
│  │  │   ├─ callOpenAICompatibleAPI()  ── OpenAI/DS/xAI/Ollama │  │
│  │  │   └─ callGeminiAPI()           ── Gemini                  │  │
│  │  ├─ testConnection() ── 连接测试                          │  │
│  │  └─ getSmartReply()  ── 智能回复                          │  │
│  └──────────────────────────────────────────────────────┘  │
├────────────────────────────────────────────────────────────┤
│               IPC Bridge / HTTP 请求                        │
├────────────────────────────────────────────────────────────┤
│                    C++ 后端层                                │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  AIConfig 配置管理层                                   │  │
│  │  ├─ AIProviderType 枚举: kOpenAI/kDeepSeek/kXAI/...   │  │
│  │  ├─ ProviderPreset 预设结构体                         │  │
│  │  └─ JSON 序列化 (from_json / to_json)                  │  │
│  ├──────────────────────────────────────────────────────┤  │
│  │  AIProvider 工厂层                                     │  │
│  │  ├─ kOpenAI/kDeepSeek/kXAI/kOllama → OpenAIProvider   │  │
│  │  ├─ kGemini             → GeminiProvider               │  │
│  │  └─ kCustom             → CustomProvider               │  │
│  ├──────────────────────────────────────────────────────┤  │
│  │  协议实现层                                            │  │
│  │  ├─ OpenAIProvider (WinHTTP)                          │  │
│  │  │   ├─ /v1/chat/completions (POST)                    │  │
│  │  │   └─ /v1/models (GET)                              │  │
│  │  ├─ GeminiProvider (WinHTTP)                          │  │
│  │  │   ├─ /v1beta/models/{model}:generateContent (POST)  │  │
│  │  │   └─ /v1beta/models (GET)                          │  │
│  │  └─ CustomProvider (WinHTTP)                          │  │
│  └──────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────┘
```

### C++ 后端层

位于 [`client/src/ai/`](client/src/ai/) 目录下：

| 文件 | 说明 |
|------|------|
| [`AIConfig.h`](client/src/ai/AIConfig.h) | AI 配置结构体、`AIProviderType` 枚举、`ProviderPreset` 预设、序列化 |
| [`AIConfig.cpp`](client/src/ai/AIConfig.cpp) | JSON 序列化实现 |
| [`AIProvider.h`](client/src/ai/AIProvider.h) | 抽象基类、`ProviderInfo` 结构体 |
| [`AIProvider.cpp`](client/src/ai/AIProvider.cpp) | 工厂方法分发 |
| [`OpenAIProvider.h`](client/src/ai/OpenAIProvider.h) | OpenAI 兼容协议实现（OpenAI/DeepSeek/xAI/Ollama 共用） |
| [`OpenAIProvider.cpp`](client/src/ai/OpenAIProvider.cpp) | WinHTTP 实现、Ollama 无 key 认证 |
| [`GeminiProvider.h`](client/src/ai/GeminiProvider.h) | Google Gemini 专用实现 |
| [`GeminiProvider.cpp`](client/src/ai/GeminiProvider.cpp) | WinHTTP 实现、Gemini 请求/响应格式转换 |
| [`CustomProvider.h`](client/src/ai/CustomProvider.h) | 自定义 API 实现 |
| [`CustomProvider.cpp`](client/src/ai/CustomProvider.cpp) | WinHTTP 实现 |
| [`AIChatSession.h`](client/src/ai/AIChatSession.h) | AI 对话会话管理 |
| [`AIChatSession.cpp`](client/src/ai/AIChatSession.cpp) | 会话管理实现 |

### 前端层

| 文件 | 说明 |
|------|------|
| [`ai_chat.js`](client/ui/js/ai_chat.js) | AI 聊天模块核心（PROVIDERS 常量、API 调用、连接测试、智能回复） |
| [`app.js`](client/ui/js/app.js) | AI 配置管理 UI（`saveAIConfig`、`testAIConnection`、`onAIProviderChange`） |
| [`index.html`](client/ui/index.html) | 设置页面 AI 配置表单（6 选项提供商下拉框） |
| [`ai.css`](client/ui/css/ai.css) | AI 面板样式 |

### 请求路由

```
用户消息 → AIChat.send()
    ↓
AIChat.callAPI(content)
    ↓
┌─ provider === 'gemini' ───→ callGeminiAPI()
│                               ↓ Gemini 原生格式
│                               POST /v1beta/models/{model}:generateContent?key={API_KEY}
│                               ↓ 解析 candidates[0].content.parts[0].text
│                               → 返回回复
│
└─ provider 为 OpenAI 兼容  → callOpenAICompatibleAPI()
  (openai/deepseek/xai/ollama)  ↓ OpenAI 标准格式
                                POST /v1/chat/completions
                                ↓ 解析 choices[0].message.content
                                → 返回回复
```

---

## 配置指南

### OpenAI

| 字段 | 值 |
|------|-----|
| 提供商选择 | **OpenAI** |
| API 端点 | `https://api.openai.com`（自动填充） |
| API Key | `sk-...`（必填，从 [OpenAI Platform](https://platform.openai.com/api-keys) 获取） |
| 模型 | `gpt-4o`（自动填充，推荐） |
| 温度 | 0.0 ~ 2.0（默认 0.7） |
| 最大 Token | 1 ~ 8192（默认 2048） |

### DeepSeek (v4)

| 字段 | 值 |
|------|-----|
| 提供商选择 | **DeepSeek** |
| API 端点 | `https://api.deepseek.com`（自动填充） |
| API Key | `sk-...`（必填，从 [DeepSeek Platform](https://platform.deepseek.com/api_keys) 获取） |
| 模型 | `deepseek-v4-flash`（自动填充） |
| 温度 | 0.0 ~ 2.0（默认 0.7） |
| 最大 Token | 1 ~ 8192（默认 2048） |

> **⚠️ DeepSeek v4 模型变更 (2025-2026)**
>
> DeepSeek 已升级至 v4 系列。旧模型 `deepseek-chat` 和 `deepseek-reasoner` 将于 **2026 年 7 月 24 日** 停止服务。
>
> | 旧模型 (即将废弃) | 新模型 (推荐) |
> |-------------------|---------------|
> | `deepseek-chat` | `deepseek-v4-flash` (快速、经济) |
> | `deepseek-reasoner` | `deepseek-v4-pro` (更强推理) |
>
> 本项目的默认值已更新为 `deepseek-v4-flash`。请确保您的 API Key 有 v4 模型访问权限。

### xAI Grok

| 字段 | 值 |
|------|-----|
| 提供商选择 | **xAI Grok** |
| API 端点 | `https://api.x.ai`（自动填充） |
| API Key | `sk-...`（必填，从 [xAI Console](https://console.x.ai) 获取） |
| 模型 | `grok-3`（自动填充） |
| 温度 | 0.0 ~ 2.0（默认 0.7） |
| 最大 Token | 1 ~ 8192（默认 2048） |

### Ollama (本地)

| 字段 | 值 |
|------|-----|
| 提供商选择 | **Ollama（本地）** |
| API 端点 | `http://localhost:11434`（自动填充） |
| API Key | **不需要** (Key 输入框自动禁用) |
| 模型 | `llama3`（自动填充，可按需修改） |
| 温度 | 0.0 ~ 2.0（默认 0.7） |
| 最大 Token | 1 ~ 8192（默认 2048） |

> **Ollama 设置步骤：**
>
> 1. 安装 Ollama: [https://ollama.com/download](https://ollama.com/download)
> 2. 启动 Ollama 服务: `ollama serve`
> 3. 下载模型: `ollama pull llama3` (或 `mistral`, `qwen2.5` 等)
> 4. 在 Chrono-shift 设置中选择 **Ollama（本地）**
> 5. API Key 字段会自动禁用，无需填写
> 6. 点击 **保存配置**，然后 **测试连接**

> **⚠️ CORS 问题**
>
> 如果前端浏览器无法连接 Ollama，请设置 Ollama 允许跨域：
> ```bash
> # Linux/macOS
> export OLLAMA_ORIGINS=*
> ollama serve
>
> # Windows PowerShell
> $env:OLLAMA_ORIGINS="*"
> ollama serve
> ```
> 或在启动 Ollama 服务前设置环境变量。详见 [Ollama FAQ](https://github.com/ollama/ollama/blob/main/docs/faq.md)。

### Google Gemini

| 字段 | 值 |
|------|-----|
| 提供商选择 | **Google Gemini** |
| API 端点 | `https://generativelanguage.googleapis.com`（自动填充，**只读**） |
| API Key | `AIza...`（必填，从 [Google AI Studio](https://aistudio.google.com/apikey) 获取） |
| 模型 | `gemini-2.0-flash`（自动填充，推荐） |
| 温度 | 0.0 ~ 2.0（默认 0.7） |
| 最大 Token | 1 ~ 8192（默认 2048） |

> **注意:** Gemini 的 API 端点地址是固定的，因此在设置页面中被禁用编辑。

### 自定义 API

| 字段 | 值 |
|------|-----|
| 提供商选择 | **自定义 API** |
| API 端点 | 用户填写（任何兼容 OpenAI 协议的端点） |
| API Key | 可选（由端点决定是否需要） |
| 模型 | 用户填写 |

适用于：
- 反向代理中转（如 `http://localhost:8080/v1`）
- 自建 API 服务
- 其他 OpenAI 兼容提供商

---

## C++ API 参考

### AIProviderType 枚举

定义于 [`AIConfig.h`](client/src/ai/AIConfig.h)。

```cpp
enum class AIProviderType : std::uint8_t {
    kNone      = 0,   // 未配置
    kOpenAI    = 1,   // OpenAI
    kDeepSeek  = 2,   // DeepSeek
    kXAI       = 3,   // xAI Grok
    kOllama    = 4,   // Ollama (本地)
    kGemini    = 5,   // Google Gemini
    kCustom    = 6    // 自定义 API
};
```

**工具方法：**

```cpp
// 判断是否为 OpenAI 兼容协议（OpenAI/DeepSeek/xAI/Ollama）
static bool is_openai_compatible(AIProviderType type);

// 获取提供商可读名称
static const char* provider_name(AIProviderType type);

// 获取提供商预设（默认端点、模型、Key 需求）
static ProviderPreset get_preset(AIProviderType type);
```

### AIConfig 结构体

```cpp
struct AIConfig {
    AIProviderType provider_type;       // 提供商类型
    std::string api_endpoint;           // API 端点
    std::string api_key;                // API 密钥
    std::string model;                  // 模型名称
    std::string system_prompt;          // 系统提示词
    int max_tokens;                     // 最大 Token
    float temperature;                  // 温度

    bool is_valid() const;              // 配置有效性检查
    std::string to_json() const;        // 序列化为 JSON
    static AIConfig from_json(...);     // 从 JSON 反序列化
};
```

**`is_valid()` 逻辑：**

| 提供商 | 需要 Endpoint | 需要 API Key |
|--------|--------------|-------------|
| OpenAI/DeepSeek/xAI | ✅ | ✅ |
| Ollama | ✅ | ❌ |
| Gemini | ✅ (固定) | ✅ |
| Custom | ✅ | 可选 |

### AIProvider 工厂方法

```cpp
// 创建对应类型的 AIProvider 实例
std::unique_ptr<AIProvider> AIProvider::create(
    AIProviderType type, const AIConfig& config);

// 获取提供商信息
ProviderInfo AIProvider::get_provider_info(AIProviderType type);

struct ProviderInfo {
    const char* display_name;       // 显示名称
    const char* default_endpoint;   // 默认端点
    const char* default_model;      // 默认模型
    bool        requires_api_key;   // 是否需要 API Key
};
```

**工厂分发逻辑：**

| `AIProviderType` | 创建实例 |
|-----------------|---------|
| `kOpenAI` | `OpenAIProvider` |
| `kDeepSeek` | `OpenAIProvider` |
| `kXAI` | `OpenAIProvider` |
| `kOllama` | `OpenAIProvider` |
| `kGemini` | `GeminiProvider` |
| `kCustom` | `CustomProvider` |

### GeminiProvider

`GeminiProvider` 是 Google Gemini API 的专用实现，解决了 Gemini API 与 OpenAI 兼容协议之间的差异。

**关键差异处理：**

| 特性 | OpenAI 兼容 | Gemini |
|------|------------|--------|
| 端点 | `POST /v1/chat/completions` | `POST /v1beta/models/{model}:generateContent` |
| 认证 | `Authorization: Bearer {key}` | URL 参数 `?key={API_KEY}` |
| 请求体 | `{ messages: [{role, content}] }` | `{ contents: [{role, parts: [{text}]}] }` |
| System prompt | 作为 system 角色的 message | `systemInstruction.parts[].text` |
| 角色映射 | `assistant` | `model` |
| 响应格式 | `choices[0].message.content` | `candidates[0].content.parts[0].text` |
| 模型列表 | `GET /v1/models` | `GET /v1beta/models` |

**GeminiProvider 方法：**

```cpp
class GeminiProvider : public AIProvider {
    std::string chat(const std::string& message) override;
    std::string generate(const std::string& prompt) override;
    bool test_connection() override;
    bool is_available() const override;
    AIProviderType type() const override { return AIProviderType::kGemini; }
    const char* name() const override { return "Google Gemini"; }
};
```

---

## JavaScript API 参考

### AIChat.PROVIDERS

提供商预设常量表，定义于 [`ai_chat.js`](client/ui/js/ai_chat.js)。

```javascript
AIChat.PROVIDERS = {
    openai:   { name: 'OpenAI',        endpoint: 'https://api.openai.com',                   model: 'gpt-4o',           keyRequired: true  },
    deepseek: { name: 'DeepSeek',      endpoint: 'https://api.deepseek.com',                 model: 'deepseek-v4-flash', keyRequired: true  },
    xai:      { name: 'xAI Grok',      endpoint: 'https://api.x.ai',                         model: 'grok-3',            keyRequired: true  },
    ollama:   { name: 'Ollama（本地）', endpoint: 'http://localhost:11434',                    model: 'llama3',            keyRequired: false },
    gemini:   { name: 'Google Gemini', endpoint: 'https://generativelanguage.googleapis.com',  model: 'gemini-2.0-flash',  keyRequired: true  },
    custom:   { name: '自定义 API',     endpoint: '',                                          model: '',                  keyRequired: false }
};
```

### AIChat.callAPI()

```javascript
/**
 * 根据当前配置的提供商路由到对应的 API 调用方法
 * @param {string} content - 用户消息
 * @returns {Promise<string>} AI 回复
 * @throws {Error} 调用失败时抛出
 */
AIChat.callAPI = async function(content)
```

**路由逻辑：**
- `provider === 'gemini'` → `callGeminiAPI(content)`
- `provider` 为 OpenAI 兼容 → `callOpenAICompatibleAPI(content)`

### AIChat.testConnection()

```javascript
/**
 * 测试当前配置的 AI 提供商连接
 * @returns {Promise<{success: boolean, message: string}>}
 */
AIChat.testConnection = async function()
```

**测试方法：**
- OpenAI 兼容: `GET /v1/models`（Ollama 跳过 Authorization header）
- Gemini: `GET /v1beta/models?key={API_KEY}`

### AIChat.getSmartReply()

```javascript
/**
 * 获取智能回复建议
 * @param {string} messageContent - 收到的消息内容
 * @returns {Promise<string[]>} 最多 3 条回复建议
 */
AIChat.getSmartReply = async function(messageContent)
```

---

## 注意事项

### DeepSeek v4 模型变更

| 旧模型 | 状态 | 新模型 |
|--------|------|--------|
| `deepseek-chat` | **2026/07/24 停止服务** | `deepseek-v4-flash` |
| `deepseek-reasoner` | **2026/07/24 停止服务** | `deepseek-v4-pro` |

本项目默认模型已更新为 `deepseek-v4-flash`。建议用户在 DeepSeek 控制台检查 API Key 的 v4 模型访问权限。

### Ollama CORS 问题

如果通过浏览器（而非 C++ 后端）直接调用 Ollama API，可能遇到 CORS 限制：

```bash
# 设置 Ollama 允许所有来源
export OLLAMA_ORIGINS=*
ollama serve

# 或允许特定来源
export OLLAMA_ORIGINS=http://localhost:9010
ollama serve
```

### Gemini API Key 安全

Gemini API 使用 URL 参数传递 API Key (`?key=...`)，这可能导致 API Key 出现在服务器访问日志中。注意事项：

- 仅在受信任的网络环境中使用
- 定期轮换 API Key
- 在 Google Cloud Console 中设置 API Key 限制（仅允许 Generative Language API）

### 自定义 API 兼容性

自定义 API 必须兼容 OpenAI 的 `/v1/chat/completions` 端点格式：

- 请求: `POST /v1/chat/completions`
- Header: `Authorization: Bearer {key}`
- 请求体: `{ model, messages: [{role, content}], max_tokens, temperature }`
- 响应: `{ choices: [{ message: { content } }] }`

不兼容的 API 需要通过反向代理适配。

---

> 最后更新: 2026-05-02 · Chrono-shift v0.3.0
