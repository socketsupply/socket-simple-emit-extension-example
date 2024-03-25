#include <socket/extension.h>
#include <thread>
#include <atomic>

// state
static std::atomic<bool> running = false;
static std::thread* thread = nullptr;
static std::atomic<int> elapsed = 0; // in milliseconds

static void onstart (
  sapi_context_t* context,
  sapi_ipc_message_t* message,
  const sapi_ipc_router_t* router
) {
  // every IPC request needs a reply result
  const auto result = sapi_ipc_result_create(context, message);

  // return early with an error if we are already running
	if (running) {
    sapi_ipc_reply_with_error(result, "{\"message\": \"Already running\"}");
		return;
	}

  // arguments
  std::string event = "";
  int timeout = 0;

  sapi_log(context, "Starting emit loop");

  // validate 'event' argument
  try {
    event = std::string(sapi_ipc_message_get(message, "event"));
  } catch (std::exception e) {
    sapi_ipc_reply_with_error(result, "{\"message\": \"Invalid 'event' value given\"}");
    return;
  }

  // validate 'timeout' argument
  try {
    timeout = std::atoi(sapi_ipc_message_get(message, "timeout"));
  } catch (std::exception e) {
    sapi_ipc_reply_with_error(result, "{\"message\": \"Invalid 'timeout' value given\"}");
    return;
  }

  // create a thread context that is retained for lifetime of thread as contexts in a request only live
  // as long as the request itself, which is destroyed in the reply below
  const auto threadContext = sapi_context_create(context, true);

  // start the thread with the 'event' and  'timeout' values copied in the 'threadContext`
  // that we created above. the 'ipc://emit.stop' request will destroy the thread by setting
  // 'running = false' causing the loop to break in which we'll release the 'threadContext'
	thread = new std::thread([threadContext, event, timeout]() {
		running = true;
		while (running) {
			std::this_thread::yield();
			std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
      elapsed += timeout;
      // create a JSON object context that we can use to allocate JSON objects,
      // we'll release it at the end of the loop
      auto jsonObjectContext = sapi_context_create(threadContext, true);
      auto json = sapi_json_object_create(jsonObjectContext);
      sapi_json_object_set(json, "elapsed", sapi_json_number_create(jsonObjectContext, elapsed));
      sapi_ipc_emit(threadContext, event.c_str(), sapi_json_stringify(json));
      sapi_context_release(jsonObjectContext);
		}

    // release 'threadContext' after loop ends
    sapi_context_release(threadContext);
	});

  // reply to caller
  sapi_ipc_reply(result);
}

void onstop (
  sapi_context_t* context,
  sapi_ipc_message_t* message,
  const sapi_ipc_router_t* router
) {
  // every IPC request needs a reply result
  auto result = sapi_ipc_result_create(context, message);

  // return early with an error if we are already running
  if (!running) {
    sapi_ipc_reply_with_error(result, "{\"message\": \"Not running\"}");
		return;
  }

  sapi_log(context, "Stopping emit loop");

  // signal thread to stop running which should release the
  // 'threadContext' created before
  running = false;

  // destroy thread
  if (thread) {
    if (thread->joinable()) {
      thread->join();
    }

    delete thread;
    thread = nullptr;
  }

  // reply to caller
  sapi_ipc_reply(result);
}

static bool initialize (sapi_context_t* context, const void *data) {
	sapi_ipc_router_map(context, "emit.start", onstart, data);
	sapi_ipc_router_map(context, "emit.stop", onstop, data);
  return true;
}

static bool deinitialize (sapi_context_t* context, const void *data) {
  // if 'ipc://emit.stop' was not called, we should still clean up state here
  running = false;
  if (thread) {
    if (thread->joinable()) {
      thread->join();
    }

    delete thread;
    thread = nullptr;
  }

	sapi_ipc_router_unmap(context, "emit.start");
	sapi_ipc_router_unmap(context, "emit.stop");
  return true;
}

SOCKET_RUNTIME_REGISTER_EXTENSION("emit", initialize, deinitialize);
