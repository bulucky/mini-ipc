<script setup lang="ts">
import { computed, ref, onMounted } from 'vue'
import {
  createWsClient,
  type LogLevel,
  type WsClient,
  type WsCommand,
  type WsIncomingMessage
} from './api/wsClient'

type ConnectionState = 'disconnected' | 'connecting' | 'connected'

type DashboardMessage = {
  type: string
  topic: string
  payload: string
  timestamp: string
}

type DashboardLog = {
  level: LogLevel
  message: string
  time: string
}

const theme = ref<'dark' | 'light'>('dark')
const wsUrl = ref('ws://127.0.0.1:9000')
const topic = ref('test_topic')
const payload = ref('hello from dashboard')
const connectionState = ref<ConnectionState>('disconnected')
const messages = ref<DashboardMessage[]>([])
const logs = ref<DashboardLog[]>([])

let client: WsClient | null = null

const isConnected = computed(() => connectionState.value === 'connected')

onMounted(() => {
  const savedTheme = localStorage.getItem('theme') as 'dark' | 'light' | null
  if (savedTheme) {
    theme.value = savedTheme
  } else {
    // 默认是 dark 模式
    theme.value = 'dark'
  }
  
  updateThemeClass()
})

function updateThemeClass() {
  if (theme.value === 'light') {
    document.documentElement.classList.add('theme-light')
  } else {
    document.documentElement.classList.remove('theme-light')
  }
}

function toggleTheme() {
  theme.value = theme.value === 'dark' ? 'light' : 'dark'
  localStorage.setItem('theme', theme.value)
  updateThemeClass()
}

function addLog(level: LogLevel, message: string) {
  logs.value.unshift({
    level,
    message,
    time: new Date().toLocaleTimeString()
  })
}

function addMessage(message: WsIncomingMessage) {
  messages.value.unshift({
    type: message.type ?? 'message',
    topic: message.topic ?? topic.value,
    payload: message.payload ?? '',
    timestamp: message.timestamp ?? new Date().toISOString()
  })
}

function connect() {
  if (client?.isOpen()) {
    return
  }

  connectionState.value = 'connecting'
  addLog('info', `Connecting to ${wsUrl.value}`)

  client = createWsClient(wsUrl.value, {
    onOpen() {
      connectionState.value = 'connected'
      addLog('success', 'WebSocket connected')
    },
    onMessage(message: WsIncomingMessage) {
      if (message.type === 'message' || message.type === 'raw') {
        addMessage(message)
      } else {
        addLog(message.level ?? 'info', message.message ?? JSON.stringify(message))
      }
    },
    onError() {
      addLog('error', 'WebSocket error')
    },
    onClose() {
      connectionState.value = 'disconnected'
      addLog('info', 'WebSocket disconnected')
    }
  })
}

function disconnect() {
  client?.close()
  client = null
}

function sendCommand(command: WsCommand) {
  if (!client?.send(command)) {
    addLog('error', 'WebSocket is not connected')
    return false
  }

  return true
}

function subscribe() {
  if (!topic.value.trim()) {
    addLog('error', 'Topic is required')
    return
  }

  if (sendCommand({ type: 'subscribe', topic: topic.value.trim() })) {
    addLog('info', `Subscribe command sent: ${topic.value.trim()}`)
  }
}

function publish() {
  if (!topic.value.trim()) {
    addLog('error', 'Topic is required')
    return
  }

  if (sendCommand({
    type: 'publish',
    topic: topic.value.trim(),
    payload: payload.value
  })) {
    addLog('info', `Publish command sent: ${topic.value.trim()}`)
    payload.value = ''
  }
}

function clearMessages() {
  messages.value = []
}

function clearLogs() {
  logs.value = []
}
</script>

<template>
  <!-- 液态流动气泡背景 -->
  <div class="g-background"></div>
  <div class="g-bubbles">
    <div class="g-bubble bubble-1"></div>
    <div class="g-bubble bubble-2"></div>
    <div class="g-bubble bubble-3"></div>
    <div class="g-bubble bubble-4"></div>
  </div>

  <main class="app-shell">
    <header class="topbar">
      <div class="brand">
        <div class="logo-icon">
          <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
            <polygon points="12 2 2 7 12 12 22 7 12 2"></polygon>
            <polyline points="2 17 12 22 22 17"></polyline>
            <polyline points="2 12 12 17 22 12"></polyline>
          </svg>
        </div>
        <div>
          <p class="eyebrow">MiniIPC</p>
          <h1>Dashboard</h1>
        </div>
      </div>

      <div class="conn-control">
        <div class="field-group">
          <div class="input-wrapper">
            <span class="ws-icon">
              <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                <path d="M10 13a5 5 0 0 0 7.54.54l3-3a5 5 0 0 0-7.07-7.07l-1.72 1.71"></path>
                <path d="M14 11a5 5 0 0 0-7.54-.54l-3 3a5 5 0 0 0 7.07 7.07l1.71-1.71"></path>
              </svg>
            </span>
            <input id="ws-url" v-model="wsUrl" :disabled="isConnected" placeholder="ws://127.0.0.1:9000" />
          </div>
          <button class="primary" :disabled="connectionState === 'connecting' || isConnected" @click="connect">
            Connect
          </button>
          <button :disabled="!isConnected" @click="disconnect">
            Disconnect
          </button>
        </div>
        
        <span class="status-pill" :class="connectionState">
          {{ connectionState }}
        </span>

        <!-- 主题切换按钮 -->
        <button class="theme-toggle ghost" @click="toggleTheme" title="Toggle theme" style="min-height: 40px; width: 40px; border-radius: 50%; padding: 0;">
          <!-- 月亮图标 (深色模式下显示，点击切浅色) -->
          <svg v-if="theme === 'dark'" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
            <path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"></path>
          </svg>
          <!-- 太阳图标 (浅色模式下显示，点击切深色) -->
          <svg v-else width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
            <circle cx="12" cy="12" r="5"></circle>
            <line x1="12" y1="1" x2="12" y2="3"></line>
            <line x1="12" y1="21" x2="12" y2="23"></line>
            <line x1="4.22" y1="4.22" x2="5.64" y2="5.64"></line>
            <line x1="18.36" y1="18.36" x2="19.78" y2="19.78"></line>
            <line x1="1" y1="12" x2="3" y2="12"></line>
            <line x1="21" y1="12" x2="23" y2="12"></line>
            <line x1="4.22" y1="19.78" x2="5.64" y2="18.36"></line>
            <line x1="18.36" y1="5.64" x2="19.78" y2="4.22"></line>
          </svg>
        </button>
      </div>
    </header>

    <section class="workspace">
      <div class="panel topic-panel">
        <div class="panel-head">
          <h2>Topic Control</h2>
        </div>

        <div class="topic-panel-content">
          <div class="field">
            <label for="topic">Topic Name</label>
            <input id="topic" v-model="topic" placeholder="e.g. test_topic" />
          </div>

          <button class="primary full" :disabled="!isConnected" @click="subscribe">
            <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
              <rect x="3" y="4" width="18" height="16" rx="2" ry="2"></rect>
              <line x1="16" y1="2" x2="16" y2="6"></line>
              <line x1="8" y1="2" x2="8" y2="6"></line>
              <line x1="3" y1="10" x2="21" y2="10"></line>
            </svg>
            Subscribe
          </button>

          <div class="field" style="flex-grow: 1; display: flex; flex-direction: column; margin-bottom: 0;">
            <label for="payload">Payload</label>
            <textarea id="payload" v-model="payload" placeholder="Enter message payload..." style="flex-grow: 1; min-height: 120px;" />
          </div>

          <button class="full" :disabled="!isConnected" @click="publish">
            <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
              <line x1="22" y1="2" x2="11" y2="13"></line>
              <polygon points="22 2 15 22 11 13 2 9 22 2"></polygon>
            </svg>
            Publish
          </button>
        </div>
      </div>

      <div class="panel message-panel">
        <div class="panel-head">
          <h2>Messages</h2>
          <button class="ghost" @click="clearMessages">
            <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
              <polyline points="3 6 5 6 21 6"></polyline>
              <path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path>
            </svg>
            Clear
          </button>
        </div>

        <div v-if="messages.length === 0" class="empty-state">
          <div class="empty-icon">
            <svg width="32" height="32" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
              <path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z"></path>
            </svg>
          </div>
          <span>Waiting for messages</span>
        </div>

        <ul v-else class="message-list">
          <li v-for="(message, index) in messages" :key="index" class="message-item">
            <div class="message-meta">
              <span>{{ message.topic }}</span>
              <time>{{ new Date(message.timestamp).toLocaleTimeString() }}</time>
            </div>
            <pre>{{ message.payload }}</pre>
          </li>
        </ul>
      </div>

      <div class="panel log-panel">
        <div class="panel-head">
          <h2>Logs</h2>
          <button class="ghost" @click="clearLogs">
            <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
              <polyline points="3 6 5 6 21 6"></polyline>
              <path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path>
            </svg>
            Clear
          </button>
        </div>

        <div v-if="logs.length === 0" class="empty-state">
          <div class="empty-icon">
            <svg width="32" height="32" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
              <rect x="3" y="3" width="18" height="18" rx="2" ry="2"></rect>
              <line x1="9" y1="9" x2="15" y2="9"></line>
              <line x1="9" y1="13" x2="15" y2="13"></line>
              <line x1="9" y1="17" x2="13" y2="17"></line>
            </svg>
          </div>
          <span>No logs yet</span>
        </div>

        <ul v-else class="log-list">
          <li v-for="(log, index) in logs" :key="index" :class="['log-item', log.level]">
            <div class="log-meta">
              <span class="log-badge">{{ log.level }}</span>
              <time>{{ log.time }}</time>
            </div>
            <p>{{ log.message }}</p>
          </li>
        </ul>
      </div>
    </section>
  </main>
</template>
