# Chrono-shift API 文档

## 基础信息

- **基础URL**: `https://localhost:4443`
- **请求格式**: `application/json`
- **响应格式**: `application/json`
- **传输加密**: HTTPS-only (TLS 1.3)，纯 HTTP 已移除

## 认证

所有需要登录的接口使用 Bearer Token 认证：

```
Authorization: Bearer <jwt_token>
```

JWT 令牌通过 [`POST /api/user/login`](#post-apiuserlogin) 获取，有效期为 24 小时。

## 安全响应头

所有 HTTP 响应默认包含以下安全头：

| 响应头 | 值 | 说明 |
|--------|-----|------|
| `Strict-Transport-Security` | `max-age=31536000; includeSubDomains` | 强制 HTTPS，有效期 1 年 |
| `Content-Security-Policy` | `default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'` | 限制资源加载来源 |
| `X-Content-Type-Options` | `nosniff` | 禁止 MIME 类型嗅探 |
| `X-Frame-Options` | `DENY` | 禁止页面嵌入 iframe |
| `Referrer-Policy` | `no-referrer` | 禁止 Referer 泄露 |

## 响应格式

### 成功响应
```json
{ "status": "ok", "data": { ... } }
```

### 错误响应
```json
{ "status": "error", "message": "错误描述信息" }
```

## API 接口

### 1. 用户系统

#### POST /api/user/register
注册新用户。用户名长度 3-32 字符（字母、数字、下划线），密码长度 ≥ 6 字符。

```json
// Request
{ "username": "test_user", "password": "123456", "nickname": "测试用户" }

// Response (201 Created)
{ "status": "ok", "message": "success", "data": { "user_id": 1 } }

// Error (用户名已存在)
{ "status": "error", "message": "用户名已存在" }
```

#### POST /api/user/login
用户登录，返回 JWT 令牌。

```json
// Request
{ "username": "test_user", "password": "123456" }

// Response
{ "status": "ok", "data": { "user_id": 1, "token": "jwt_token...", "nickname": "测试用户" } }

// Error
{ "status": "error", "message": "用户名或密码错误" }
```

#### GET /api/user/profile?user_id=X
获取用户信息（需要认证）。

```json
// Response
{ "status": "ok", "data": { "user_id": 1, "username": "test_user", "nickname": "测试用户", "avatar_url": "/api/file/avatars/1.jpg", "created_at": "2026-01-01T00:00:00Z" } }
```

#### PUT /api/user/update
更新用户资料（需要认证）。

```json
// Request
{ "nickname": "新昵称", "avatar_url": "/api/file/avatars/1.jpg" }

// Response
{ "status": "ok", "message": "更新成功" }
```

#### GET /api/user/search?keyword=X
搜索用户（需要认证）。

```json
// Response
{ "status": "ok", "data": { "users": [{ "user_id": 1, "username": "test_user", "nickname": "测试用户" }] } }
```

#### GET /api/user/friends
获取好友列表（需要认证）。

```json
// Response
{ "status": "ok", "data": { "friends": [...] } }
```

#### POST /api/user/friends/add
添加好友（需要认证）。

```json
// Request
{ "friend_id": 2, "message": "你好，加个好友" }

// Response
{ "status": "ok", "message": "好友请求已发送" }
```

### 2. 消息系统

#### POST /api/message/send
发送消息（需要认证）。

```json
// Request
{ "to_user_id": 2, "content": "消息内容" }

// Response (201 Created)
{ "status": "ok", "data": { "message_id": 100, "created_at": "2026-01-01T00:00:00Z" } }

// Error
{ "status": "error", "message": "接收方不存在" }
```

#### GET /api/message/list?user_id=X&offset=0&limit=50
获取与指定用户的消息历史（需要认证）。

```json
// Response
{ "status": "ok", "data": { "messages": [{ "message_id": 1, "from_user_id": 1, "to_user_id": 2, "content": "...", "created_at": "..." }], "has_more": false } }
```

### 3. 文件服务

#### POST /api/file/upload
上传文件（需要认证）。请求格式为 `multipart/form-data`。

| 字段 | 类型 | 说明 |
|------|------|------|
| `file` | File | 上传的文件 |

```json
// Response (201 Created)
{ "status": "ok", "data": { "file_id": "uuid", "url": "/api/file/uuid/filename.ext", "size": 1024 } }
```

#### GET /api/file/*
下载文件。公开可访问（头像、分享文件等）。

#### POST /api/avatar/upload
上传头像（需要认证）。请求格式为 `multipart/form-data`。

```json
// Response
{ "status": "ok", "data": { "avatar_url": "/api/file/avatars/1.jpg" } }
```

### 4. 社区模板

#### GET /api/templates?offset=0&limit=20
获取模板列表。

```json
// Response
{ "status": "ok", "data": { "templates": [...], "has_more": false } }
```

## HTTP 状态码说明

| 状态码 | 说明 |
|--------|------|
| `200 OK` | 请求成功 |
| `201 Created` | 资源创建成功 |
| `400 Bad Request` | 请求参数错误 |
| `401 Unauthorized` | 未认证或令牌无效 |
| `404 Not Found` | 资源不存在 |
| `500 Internal Server Error` | 服务器内部错误 |
