#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WebSocketsServer.h>
#include <vector>

// --- Configuration ---
const char* AP_SSID = "OffGrid-Chat";
const char* AP_PASS = "kiaGOD762"; // Open network
const byte DNS_PORT = 53;
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress NETMASK(255, 255, 255, 0);

// --- Global Servers ---
WebServer server(80);
DNSServer dnsServer;
WebSocketsServer webSocket(81);

// --- Message Buffer ---
struct ChatMessage {
  uint32_t id;
  String user;
  String text;
  String time;
  uint32_t replyToId;
  String replyUser;
  String replyText;
  bool isEdited;
  bool isDeleted;
};

const size_t MAX_HISTORY = 50;
std::vector<ChatMessage> history;
uint32_t messageCounter = 0;
int connectedClientsCount = 0;

// Escape JSON characters safely
String escapeJson(const String& input) {
  String out = "";
  out.reserve(input.length() + 16);
  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\b') out += "\\b";
    else if (c == '\f') out += "\\f";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else out += c;
  }
  return out;
}

// Extract JSON field value from a JSON string safely
String getJsonField(const String& json, const String& field) {
  String key = "\"" + field + "\":";
  int idx = json.indexOf(key);
  if (idx == -1) return "";
  idx += key.length();
  while (idx < json.length() && (json[idx] == ' ' || json[idx] == '\t')) idx++;
  if (idx >= json.length()) return "";

  if (json[idx] == '"') {
    int start = idx + 1;
    String val = "";
    bool esc = false;
    for (int i = start; i < json.length(); i++) {
      char c = json[i];
      if (esc) {
        if (c == 'n') val += '\n';
        else if (c == 'r') val += '\r';
        else if (c == 't') val += '\t';
        else if (c == '"') val += '"';
        else if (c == '\\') val += '\\';
        else val += c;
        esc = false;
      } else if (c == '\\') {
        esc = true;
      } else if (c == '"') {
        return val;
      } else {
        val += c;
      }
    }
    return val;
  } else {
    int end = idx;
    while (end < json.length() && json[end] != ',' && json[end] != '}' && json[end] != ' ') {
      end++;
    }
    return json.substring(idx, end);
  }
}

String formatMessageJson(const ChatMessage& m) {
  String json = "{";
  json += "\"type\":\"msg\",";
  json += "\"id\":" + String(m.id) + ",";
  json += "\"user\":\"" + escapeJson(m.user) + "\",";
  json += "\"text\":\"" + escapeJson(m.text) + "\",";
  json += "\"time\":\"" + escapeJson(m.time) + "\",";
  json += "\"replyTo\":" + String(m.replyToId) + ",";
  json += "\"replyUser\":\"" + escapeJson(m.replyUser) + "\",";
  json += "\"replyText\":\"" + escapeJson(m.replyText) + "\",";
  json += "\"isEdited\":" + String(m.isEdited ? "true" : "false") + ",";
  json += "\"isDeleted\":" + String(m.isDeleted ? "true" : "false");
  json += "}";
  return json;
}

String getSystemStatusJson() {
  uint32_t uptimeSec = millis() / 1000;
  String json = "{";
  json += "\"type\":\"status\",";
  json += "\"clients\":" + String(connectedClientsCount) + ",";
  json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"uptime\":" + String(uptimeSec);
  json += "}";
  return json;
}

// --- HTML / CSS / JS Single Page App ---
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fa" dir="rtl">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Off-Grid Mesh Chat</title>
  <style>
    :root {
      --bg-primary: #0f1117;
      --bg-secondary: #1a1d24;
      --bg-card: #222630;
      --accent: #22c55e;
      --accent-hover: #16a34a;
      --text-main: #f3f4f6;
      --text-muted: #9ca3af;
      --bubble-me: #1d4ed8;
      --bubble-other: #272d3b;
      --border: #333a48;
      --danger: #ef4444;
      --reply-bg: rgba(255, 255, 255, 0.08);
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Tahoma, sans-serif; }
    body { background-color: var(--bg-primary); color: var(--text-main); height: 100dvh; display: flex; flex-direction: column; overflow: hidden; }
    
    header {
      background: var(--bg-secondary);
      border-bottom: 1px solid var(--border);
      padding: 10px 16px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      flex-shrink: 0;
    }
    .header-title { display: flex; align-items: center; gap: 8px; }
    .status-dot { width: 10px; height: 10px; border-radius: 50%; background: #ef4444; transition: background 0.3s; }
    .status-dot.online { background: var(--accent); box-shadow: 0 0 8px var(--accent); }
    h1 { font-size: 1.05rem; font-weight: 600; }
    .header-info { display: flex; align-items: center; gap: 12px; font-size: 0.8rem; color: var(--text-muted); }
    .badge { background: var(--bg-card); padding: 4px 8px; border-radius: 12px; border: 1px solid var(--border); }
    
    .user-bar {
      background: var(--bg-card);
      padding: 6px 16px;
      font-size: 0.82rem;
      display: flex;
      justify-content: space-between;
      align-items: center;
      border-bottom: 1px solid var(--border);
    }
    .user-btn { background: none; border: none; color: #60a5fa; cursor: pointer; text-decoration: underline; font-size: 0.82rem; }

    #chat-box {
      flex: 1;
      overflow-y: auto;
      padding: 14px 14px 8px 14px;
      display: flex;
      flex-direction: column;
      gap: 12px;
      scroll-behavior: smooth;
    }
    .msg-wrap { display: flex; flex-direction: column; max-width: 85%; animation: fadeIn 0.2s ease-out; position: relative; }
    .msg-wrap.me { align-self: flex-start; }
    .msg-wrap.other { align-self: flex-end; }
    .msg-wrap.system { align-self: center; max-width: 90%; }
    
    .msg-meta { font-size: 0.72rem; color: var(--text-muted); margin-bottom: 3px; display: flex; align-items: center; gap: 6px; }
    .msg-wrap.me .msg-meta { justify-content: flex-start; }
    .msg-wrap.other .msg-meta { justify-content: flex-end; }
    .edited-tag { font-size: 0.68rem; color: #94a3b8; }
    
    .bubble {
      padding: 9px 13px;
      border-radius: 16px;
      font-size: 0.92rem;
      line-height: 1.45;
      word-break: break-word;
      white-space: pre-wrap;
      direction: auto;
      unicode-bidi: plaintext;
      position: relative;
    }
    .msg-wrap.me .bubble {
      background: var(--bubble-me);
      color: #fff;
      border-bottom-right-radius: 4px;
    }
    .msg-wrap.other .bubble {
      background: var(--bubble-other);
      color: var(--text-main);
      border-bottom-left-radius: 4px;
    }
    .msg-wrap.system .bubble {
      background: transparent;
      color: var(--text-muted);
      font-size: 0.75rem;
      border: 1px dashed var(--border);
      padding: 4px 10px;
      border-radius: 20px;
    }
    .bubble.deleted {
      font-style: italic;
      color: var(--text-muted) !important;
      background: rgba(255, 255, 255, 0.04) !important;
      border: 1px dashed var(--border);
    }

    /* Reply preview inside bubble */
    .reply-quote {
      background: var(--reply-bg);
      border-right: 3px solid #60a5fa;
      padding: 4px 8px;
      border-radius: 6px;
      margin-bottom: 6px;
      font-size: 0.78rem;
      cursor: pointer;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
    .reply-quote strong { color: #93c5fd; display: block; margin-bottom: 1px; }

    /* Action bar on message */
    .msg-actions {
      display: flex;
      gap: 6px;
      margin-top: 3px;
      opacity: 0.7;
      transition: opacity 0.2s;
    }
    .msg-wrap:hover .msg-actions { opacity: 1; }
    .msg-wrap.me .msg-actions { justify-content: flex-start; }
    .msg-wrap.other .msg-actions { justify-content: flex-end; }
    
    .act-btn {
      background: var(--bg-card);
      border: 1px solid var(--border);
      color: var(--text-muted);
      font-size: 0.72rem;
      padding: 2px 7px;
      border-radius: 10px;
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      gap: 3px;
    }
    .act-btn:hover { color: #fff; border-color: #60a5fa; }
    .act-btn.del:hover { border-color: var(--danger); color: var(--danger); }

    /* Action banner above input (Reply / Edit) */
    #action-banner {
      background: #1e293b;
      border-top: 1px solid var(--border);
      padding: 7px 14px;
      font-size: 0.82rem;
      display: none;
      align-items: center;
      justify-content: space-between;
      flex-shrink: 0;
    }
    #action-banner.show { display: flex; }
    .banner-text { display: flex; align-items: center; gap: 6px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
    .banner-cancel { background: none; border: none; color: #ef4444; font-size: 1.1rem; cursor: pointer; padding: 0 4px; }

    /* Input form */
    form#chat-form {
      background: var(--bg-secondary);
      border-top: 1px solid var(--border);
      padding: 10px 14px;
      display: flex;
      gap: 8px;
      flex-shrink: 0;
    }
    input#msg-input {
      flex: 1;
      background: var(--bg-card);
      border: 1px solid var(--border);
      color: var(--text-main);
      padding: 10px 14px;
      border-radius: 22px;
      font-size: 0.95rem;
      outline: none;
      direction: auto;
    }
    input#msg-input:focus { border-color: #60a5fa; }
    button#send-btn {
      background: var(--accent);
      color: #0f1117;
      border: none;
      padding: 0 18px;
      border-radius: 22px;
      font-weight: 600;
      font-size: 0.9rem;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      transition: background 0.2s;
    }
    button#send-btn:active { background: var(--accent-hover); }

    @keyframes fadeIn {
      from { opacity: 0; transform: translateY(6px); }
      to { opacity: 1; transform: translateY(0); }
    }
    .highlight { animation: flashHighlight 1.5s ease-out; }
    @keyframes flashHighlight {
      0% { transform: scale(1.03); outline: 2px solid #60a5fa; }
      100% { transform: scale(1); outline: none; }
    }
  </style>
</head>
<body>
  <header>
    <div class="header-title">
      <div id="status-dot" class="status-dot"></div>
      <h1>پیام‌رسان آفلاین (ESP32)</h1>
    </div>
    <div class="header-info">
      <div class="badge" id="client-badge">👥 ۱ آنلاین</div>
    </div>
  </header>

  <div class="user-bar">
    <div>نام شما: <strong id="username-display" style="color:#60a5fa">کاربر</strong></div>
    <button class="user-btn" onclick="promptUsername()">تغییر نام</button>
  </div>

  <div id="chat-box"></div>

  <!-- Banner for Reply or Edit -->
  <div id="action-banner">
    <div class="banner-text" id="banner-content"></div>
    <button class="banner-cancel" onclick="cancelAction()" title="لغو">✕</button>
  </div>

  <form id="chat-form" onsubmit="handleFormSubmit(event)">
    <input type="text" id="msg-input" placeholder="پیام خود را بنویسید..." autocomplete="off">
    <button type="submit" id="send-btn">ارسال</button>
  </form>

  <script>
    let myName = localStorage.getItem('offgrid_nick');
    if (!myName) {
      myName = 'کاربر_' + Math.floor(100 + Math.random() * 900);
      localStorage.setItem('offgrid_nick', myName);
    }
    document.getElementById('username-display').innerText = myName;

    function promptUsername() {
      const name = prompt('نام مستعار خود را وارد کنید:', myName);
      if (name && name.trim()) {
        myName = name.trim();
        localStorage.setItem('offgrid_nick', myName);
        document.getElementById('username-display').innerText = myName;
        renderAllMessages();
      }
    }

    const chatBox = document.getElementById('chat-box');
    const msgInput = document.getElementById('msg-input');
    const sendBtn = document.getElementById('send-btn');
    const statusDot = document.getElementById('status-dot');
    const clientBadge = document.getElementById('client-badge');
    const actionBanner = document.getElementById('action-banner');
    const bannerContent = document.getElementById('banner-content');

    let allMessages = [];
    let currentReply = null; // { id, user, text }
    let currentEdit = null;  // { id, text }
    let ws = null;

    function connectWs() {
      const host = window.location.hostname || '192.168.4.1';
      ws = new WebSocket('ws://' + host + ':81/');

      ws.onopen = () => {
        statusDot.classList.add('online');
      };

      ws.onclose = () => {
        statusDot.classList.remove('online');
        setTimeout(connectWs, 2000);
      };

      ws.onerror = () => ws.close();

      ws.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          if (data.type === 'history') {
            allMessages = data.messages || [];
            renderAllMessages();
          } else if (data.type === 'status') {
            if (data.clients !== undefined) clientBadge.innerText = '👥 ' + data.clients + ' آنلاین';
          } else if (data.type === 'msg') {
            allMessages.push(data);
            appendMessageToDOM(data, true);
          } else if (data.type === 'edit') {
            const m = allMessages.find(x => x.id === data.id);
            if (m) {
              m.text = data.newText;
              m.isEdited = true;
              updateMessageDOM(m);
            }
          } else if (data.type === 'delete') {
            const m = allMessages.find(x => x.id === data.id);
            if (m) {
              m.isDeleted = true;
              updateMessageDOM(m);
            }
          }
        } catch (e) {
          console.error(e);
        }
      };
    }

    function renderAllMessages() {
      chatBox.innerHTML = '';
      allMessages.forEach(m => appendMessageToDOM(m, false));
      chatBox.scrollTop = chatBox.scrollHeight;
    }

    function createMessageNode(data) {
      const wrap = document.createElement('div');
      wrap.id = 'msg-' + data.id;
      wrap.dataset.id = data.id;

      if (data.type === 'system') {
        wrap.className = 'msg-wrap system';
        wrap.innerHTML = `<div class="bubble">${data.text}</div>`;
        return wrap;
      }

      const isMe = (data.user === myName);
      wrap.className = 'msg-wrap ' + (isMe ? 'me' : 'other');

      // Meta
      const meta = document.createElement('div');
      meta.className = 'msg-meta';
      let metaHtml = `<span>${data.user}</span><span>•</span><span>${data.time || ''}</span>`;
      if (data.isEdited && !data.isDeleted) {
        metaHtml += `<span class="edited-tag">(ویرایش شده)</span>`;
      }
      meta.innerHTML = metaHtml;
      wrap.appendChild(meta);

      // Bubble
      const bubble = document.createElement('div');
      bubble.className = 'bubble' + (data.isDeleted ? ' deleted' : '');
      
      // Reply Quote Block
      if (data.replyTo && !data.isDeleted) {
        const quote = document.createElement('div');
        quote.className = 'reply-quote';
        quote.innerHTML = `<strong>↩️ ${data.replyUser}:</strong> ${data.replyText}`;
        quote.onclick = () => scrollToMessage(data.replyTo);
        bubble.appendChild(quote);
      }

      const textNode = document.createElement('span');
      textNode.className = 'msg-content-text';
      textNode.innerText = data.isDeleted ? '🚫 این پیام حذف شده است' : data.text;
      bubble.appendChild(textNode);
      wrap.appendChild(bubble);

      // Actions (Reply, Edit, Delete)
      if (!data.isDeleted) {
        const actions = document.createElement('div');
        actions.className = 'msg-actions';

        const replyBtn = document.createElement('button');
        replyBtn.className = 'act-btn';
        replyBtn.innerHTML = '↩️ پاسخ';
        replyBtn.onclick = () => startReply(data);
        actions.appendChild(replyBtn);

        if (isMe) {
          const editBtn = document.createElement('button');
          editBtn.className = 'act-btn';
          editBtn.innerHTML = '✏️ ویرایش';
          editBtn.onclick = () => startEdit(data);
          actions.appendChild(editBtn);

          const delBtn = document.createElement('button');
          delBtn.className = 'act-btn del';
          delBtn.innerHTML = '🗑️ حذف';
          delBtn.onclick = () => deleteMessage(data.id);
          actions.appendChild(delBtn);
        }

        wrap.appendChild(actions);
      }

      return wrap;
    }

    function appendMessageToDOM(data, autoScroll) {
      const node = createMessageNode(data);
      chatBox.appendChild(node);
      if (autoScroll) chatBox.scrollTop = chatBox.scrollHeight;
    }

    function updateMessageDOM(data) {
      const oldNode = document.getElementById('msg-' + data.id);
      if (oldNode) {
        const newNode = createMessageNode(data);
        chatBox.replaceChild(newNode, oldNode);
      }
    }

    function scrollToMessage(id) {
      const el = document.getElementById('msg-' + id);
      if (el) {
        el.scrollIntoView({ behavior: 'smooth', block: 'center' });
        el.classList.add('highlight');
        setTimeout(() => el.classList.remove('highlight'), 1600);
      }
    }

    function startReply(msg) {
      if (currentEdit) cancelAction();
      currentReply = { id: msg.id, user: msg.user, text: msg.text.slice(0, 35) };
      bannerContent.innerHTML = `<span>↩️ پاسخ به <strong>${currentReply.user}</strong>: "${currentReply.text}"</span>`;
      actionBanner.classList.add('show');
      msgInput.focus();
    }

    function startEdit(msg) {
      if (currentReply) cancelAction();
      currentEdit = { id: msg.id, text: msg.text };
      bannerContent.innerHTML = `<span>✏️ ویرایش پیام: "${msg.text.slice(0, 35)}"</span>`;
      actionBanner.classList.add('show');
      msgInput.value = msg.text;
      sendBtn.innerText = 'ذخیره';
      msgInput.focus();
    }

    function cancelAction() {
      currentReply = null;
      currentEdit = null;
      actionBanner.classList.remove('show');
      sendBtn.innerText = 'ارسال';
      msgInput.value = '';
    }

    function deleteMessage(id) {
      if (confirm('آیا از حذف این پیام مطمئن هستید؟')) {
        ws.send(JSON.stringify({
          type: 'delete',
          id: id,
          user: myName
        }));
      }
    }

    function handleFormSubmit(e) {
      e.preventDefault();
      const text = msgInput.value.trim();
      if (!text || !ws || ws.readyState !== WebSocket.OPEN) return;

      if (currentEdit) {
        ws.send(JSON.stringify({
          type: 'edit',
          id: currentEdit.id,
          user: myName,
          newText: text
        }));
        cancelAction();
      } else {
        const now = new Date();
        const timeStr = now.getHours().toString().padStart(2, '0') + ':' + now.getMinutes().toString().padStart(2, '0');

        const payload = {
          type: 'msg',
          user: myName,
          text: text,
          time: timeStr,
          replyTo: currentReply ? currentReply.id : 0
        };

        ws.send(JSON.stringify(payload));
        msgInput.value = '';
        if (currentReply) cancelAction();
      }
      msgInput.focus();
    }

    connectWs();
  </script>
</body>
</html>
)rawliteral";

// --- WebSocket Event Handler ---
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED: {
      if (connectedClientsCount > 0) connectedClientsCount--;
      String statusJson = getSystemStatusJson();
      webSocket.broadcastTXT(statusJson);
      Serial.printf("[WS] Client #%u disconnected. Active: %d\n", num, connectedClientsCount);
      break;
    }
    case WStype_CONNECTED: {
      connectedClientsCount++;
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[WS] Client #%u connected from %s. Active: %d\n", num, ip.toString().c_str(), connectedClientsCount);

      // Send message history to newly connected client
      String histJson = "{\"type\":\"history\",\"messages\":[";
      for (size_t i = 0; i < history.size(); i++) {
        histJson += formatMessageJson(history[i]);
        if (i + 1 < history.size()) histJson += ",";
      }
      histJson += "]}";
      webSocket.sendTXT(num, histJson);

      // Broadcast updated user count
      String statusJson = getSystemStatusJson();
      webSocket.broadcastTXT(statusJson);
      break;
    }
    case WStype_TEXT: {
      String msgStr = "";
      for (size_t i = 0; i < length; i++) msgStr += (char)payload[i];
      Serial.printf("[WS] Received: %s\n", msgStr.c_str());

      String eventType = getJsonField(msgStr, "type");

      // 1. New Message
      if (eventType == "msg") {
        String user = getJsonField(msgStr, "user");
        String text = getJsonField(msgStr, "text");
        String timeStr = getJsonField(msgStr, "time");
        uint32_t replyTo = getJsonField(msgStr, "replyTo").toInt();

        if (user.length() == 0) user = "ناشناس";
        if (text.length() > 0) {
          messageCounter++;
          ChatMessage newMsg;
          newMsg.id = messageCounter;
          newMsg.user = user;
          newMsg.text = text;
          newMsg.time = timeStr;
          newMsg.replyToId = replyTo;
          newMsg.isEdited = false;
          newMsg.isDeleted = false;

          // If it's a reply, find target message info
          if (replyTo > 0) {
            for (size_t i = 0; i < history.size(); i++) {
              if (history[i].id == replyTo) {
                newMsg.replyUser = history[i].user;
                newMsg.replyText = history[i].isDeleted ? "[حذف شده]" : history[i].text.substring(0, 30);
                break;
              }
            }
          }

          if (history.size() >= MAX_HISTORY) {
            history.erase(history.begin());
          }
          history.push_back(newMsg);

          // Broadcast to all clients
          String broadcastMsg = formatMessageJson(newMsg);
          webSocket.broadcastTXT(broadcastMsg);
        }
      }
      // 2. Edit Message
      else if (eventType == "edit") {
        uint32_t id = getJsonField(msgStr, "id").toInt();
        String newText = getJsonField(msgStr, "newText");
        String user = getJsonField(msgStr, "user");

        if (id > 0 && newText.length() > 0) {
          for (size_t i = 0; i < history.size(); i++) {
            if (history[i].id == id && !history[i].isDeleted) {
              history[i].text = newText;
              history[i].isEdited = true;

              String editNotice = "{\"type\":\"edit\",\"id\":" + String(id) + ",\"newText\":\"" + escapeJson(newText) + "\"}";
              webSocket.broadcastTXT(editNotice);
              break;
            }
          }
        }
      }
      // 3. Delete Message
      else if (eventType == "delete") {
        uint32_t id = getJsonField(msgStr, "id").toInt();

        if (id > 0) {
          for (size_t i = 0; i < history.size(); i++) {
            if (history[i].id == id) {
              history[i].isDeleted = true;
              history[i].text = "";

              String delNotice = "{\"type\":\"delete\",\"id\":" + String(id) + "}";
              webSocket.broadcastTXT(delNotice);
              break;
            }
          }
        }
      }
      break;
    }
    default:
      break;
  }
}

// --- HTTP Handlers for Captive Portal ---
void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleCaptiveRedirect() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n--- Initializing Off-Grid Mesh Chat (v2) ---");

  // Start Wi-Fi SoftAP
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, NETMASK);
  WiFi.softAP(AP_SSID, AP_PASS);

  Serial.print("[AP] SSID: ");
  Serial.println(AP_SSID);
  Serial.print("[AP] IP: ");
  Serial.println(WiFi.softAPIP());

  // Start DNS Server (Captive Portal)
  dnsServer.start(DNS_PORT, "*", AP_IP);
  Serial.println("[DNS] Captive Portal running on port 53");

  // Setup Web Server Routes
  server.on("/", handleRoot);
  server.on("/generate_204", handleCaptiveRedirect);
  server.on("/gen_204", handleCaptiveRedirect);
  server.on("/hotspot-detect.html", handleCaptiveRedirect);
  server.on("/ncsi.txt", handleCaptiveRedirect);
  server.onNotFound(handleCaptiveRedirect);

  server.begin();
  Serial.println("[HTTP] Web Server running on port 80");

  // Start WebSocket Server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("[WS] WebSocket Server running on port 81");
  Serial.println("--- System Ready! ---");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  webSocket.loop();
}
