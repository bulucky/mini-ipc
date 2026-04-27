<script setup>
import { computed, ref } from 'vue'
import { createWsClient } from './api/wsClient'

const wsUrl = ref('ws://127.0.0.1:9000')
const topic = ref('test_topic')
const payload = ref('hello from dashboard')
const connectionState = ref('disconnected')
const messages = ref([])
const logs = ref([])

let client = null

const isConnected = computed(() => connectionState.value === 'connected')

function addLog(level, message) {
  logs.value.unshift({
    level,
    message,
    time: new Date().toLocaleTimeString()
  })
}

function addMessage(message) {
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
    onMessage(message) {
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

function sendCommand(command) {
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
  <main class="app-shell">
    <header class="topbar">
      <div>
        <p class="eyebrow">MiniIPC</p>
        <h1>Dashboard</h1>
      </div>
      <span class="status-pill" :class="connectionState">
        {{ connectionState }}
      </span>
    </header>

    <section class="control-band">
      <div class="field wide">
        <label for="ws-url">WebSocket</label>
        <input id="ws-url" v-model="wsUrl" :disabled="isConnected" />
      </div>
      <button class="primary" :disabled="connectionState === 'connecting' || isConnected" @click="connect">
        Connect
      </button>
      <button :disabled="!isConnected" @click="disconnect">
        Disconnect
      </button>
    </section>

    <section class="workspace">
      <div class="panel topic-panel">
        <div class="panel-head">
          <h2>Topic</h2>
        </div>

        <div class="field">
          <label for="topic">Name</label>
          <input id="topic" v-model="topic" />
        </div>

        <button class="primary full" :disabled="!isConnected" @click="subscribe">
          Subscribe
        </button>

        <div class="field">
          <label for="payload">Payload</label>
          <textarea id="payload" v-model="payload" rows="7" />
        </div>

        <button class="full" :disabled="!isConnected" @click="publish">
          Publish
        </button>
      </div>

      <div class="panel message-panel">
        <div class="panel-head">
          <h2>Messages</h2>
          <button class="ghost" @click="clearMessages">Clear</button>
        </div>

        <div v-if="messages.length === 0" class="empty-state">
          Waiting for messages
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
          <button class="ghost" @click="clearLogs">Clear</button>
        </div>

        <div v-if="logs.length === 0" class="empty-state">
          No logs yet
        </div>

        <ul v-else class="log-list">
          <li v-for="(log, index) in logs" :key="index" :class="['log-item', log.level]">
            <span>{{ log.time }}</span>
            <p>{{ log.message }}</p>
          </li>
        </ul>
      </div>
    </section>
  </main>
</template>
