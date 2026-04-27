export function createWsClient(url, handlers = {}) {
  const ws = new WebSocket(url)

  ws.addEventListener('open', () => {
    handlers.onOpen?.()
  })

  ws.addEventListener('message', (event) => {
    try {
      handlers.onMessage?.(JSON.parse(event.data))
    } catch {
      handlers.onMessage?.({
        type: 'raw',
        payload: event.data,
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
    send(data) {
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
