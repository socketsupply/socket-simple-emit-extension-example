import extension from 'socket:extension'

try {
  const eventName = 'my-ipc-event'
  const emit = await extension.load('emit')
  // emit event every 250 milliseconds
  const result = await emit.binding.start({ event: eventName, timeout: 250 })

  if (result.err) {
    throw result.err
  }

  globalThis.addEventListener(eventName, (event) => {
    console.log(event.detail) // { data: 'hello world' }
    document.body.innerHTML = `
      <div>Elapsed: ${event.detail.elapsed}
    `
  })

  // stop emit after 5000 milliseconds
  setTimeout(async () => {
    try {
      await emit.binding.stop()
    } catch (err) {
      globalThis.reportError(err)
    }
  }, 5000)
} catch (err) {
  globalThis.reportError(err)
}
