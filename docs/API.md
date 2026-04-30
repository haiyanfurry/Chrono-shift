# Chrono-shift API 文档

## 基础信息

- **基础URL**: `http://localhost:8080`
- **请求格式**: `application/json`
- **响应格式**: `application/json`

## 认证

所有需要登录的接口使用 Bearer Token 认证：

```
Authorization: Bearer <jwt_token>
```

## API 接口

### 1. 用户系统

#### POST /api/user/register
注册新用户

```json
// Request
{ "username": "test_user", "password": "123456", "nickname": "测试用户" }

// Response
{ "status": "ok", "message": "success", "data": { "user_id": 1 } }
```

#### POST /api/user/login
用户登录

```json
// Request
{ "username": "test_user", "password": "123456" }

// Response
{ "status": "ok", "data": { "user_id": 1, "token": "jwt_token...", "nickname": "测试用户" } }
```

### 2. 消息系统

#### POST /api/message/send
发送消息（需要认证）

```json
// Request
{ "to_user_id": 2, "content": "加密后的消息内容" }

// Response
{ "status": "ok", "data": { "message_id": 100 } }
```

### 3. 社区模板

#### GET /api/templates?offset=0&limit=20
获取模板列表

```json
// Response
{ "status": "ok", "data": { "templates": [...] } }
```

*更多 API 细节在后续 Phase 中完善*
