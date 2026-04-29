export type WsIncomingMessage = {
  type?: string
  topic?: string
  payload?: string
  timestamp?: string
  level?: LogLevel
  message?: string
}

export type WsCommand =
  | {
      type: 'subscribe'
      topic: string
    }
  | {
      type: 'publish'
      topic: string
      payload: string
    }

export type LogLevel = 'info' | 'success' | 'error'

export type WsClientHandlers = {
  onOpen?: () => void
  onMessage?: (message: WsIncomingMessage) => void
  onError?: () => void
  onClose?: () => void
}

export type WsClient = {
  send: (data: WsCommand) => boolean
  close: () => void
  isOpen: () => boolean
}

export function createWsClient(url: string, handlers: WsClientHandlers = {}): WsClient {
  const ws = new WebSocket(url)

  ws.addEventListener('open', () => {
    handlers.onOpen?.()
  })

  ws.addEventListener('message', (event) => {
    try {
      handlers.onMessage?.(JSON.parse(event.data) as WsIncomingMessage)
    } catch {
      handlers.onMessage?.({
        type: 'raw',
        payload: String(event.data),
        timestamp: new Date().toISOString()
      })
    }
  })

  ws.addEventListener('error', () => {
    handlers.onError?.()
  })

  ws.addEventListener('close', () => {
    handlers.onClose?.()
  })

  return {
    send(data: WsCommand) {
      if (ws.readyState !== WebSocket.OPEN) {
        return false
      }

      ws.send(JSON.stringify(data))
      return true
    },
    close() {
      ws.close()
    },
    isOpen() {
      return ws.readyState === WebSocket.OPEN
    }
  }
}
