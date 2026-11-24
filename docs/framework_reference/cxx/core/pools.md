# Pools

The pool classes solve a recurring problem throughout the Constellation framework: many components need to maintain
connections to a dynamically changing set of remote hosts, receive messages from all of them on a single thread, and react
to hosts joining or leaving the group.

## The `BasePool` Design

Constellation's `BasePool` class is designed to solve this problem. The main constraint comes from the fact that ZeroMQ
sockets are not thread-safe and consequently have to be handled from only one thread. Consequently, the pool centralizes
socket access in a dedicated background thread that owns all sockets for the lifetime of the pool. All sockets are held by
a common poller which allows checking for any message arrived on any of the sockets.

On each iteration of this pool thread it calls `poller_.wait()` with a short timeout, which returns the set of sockets that
have at least one message ready to receive. For each ready socket the thread then calls `recv` and directly forwards the
decoded message to user callbacks registered upon pool creation.

The number of events returned by the most recent `poller_.wait()` call is exposed via `pollerEvents()`, which allows derived
classes to observe how many sockets had pending messages in the last poll cycle.

### Socket Lifecycle and the Mutex

Sockets do not have a fixed lifetime since new remote hosts can appear at any time via CHIRP discovery or depart. When a
discovery or departure callback is delivered from CHIRP, the `BasePool` uses a mutex to add or remove the socket from the
poller socket map.

The same mutex is used by the pool thread to rebuild the poller and while waiting for events. Instead of using the
`zmq::active_poller_t`, a dedicated implementation is used to avoid locking while waiting for messages and while calling
message delivery callbacks. After acquiring the lock, the poller waits for new events for a maximum timeout of 1ms and then
returns. If no events have been received, the mutex is unlocked and the thread sleeps for 50ms before resuming. If events
were received, they are read from the sockets under lock, the lock is released, and the message callbacks are executed.

### Connection Policy and `should_connect`

By default `BasePool` connects to every service it discovers that matches the template `SERVICE` CHIRP identifier. Derived
classes can override `should_connect` to filter the discovered services to e.g. accept only services from a specific host,
or only services that pass some other predicate, without having to re-implement the socket lifecycle or the poll loop.

### Template Parameters

`BasePool` is a class template with three parameters:

- **`MESSAGE`** — The Constellation message type the pool decodes. `recv` is called on each
  ready socket and the result is decoded as `MESSAGE` before being forwarded to the callback.
- **`SERVICE`** — The CHIRP `ServiceIdentifier` this pool listens for. Only services advertised
  with this identifier trigger a connection.
- **`SOCKET_TYPE`** — The ZMQ socket type (`zmq::socket_type::sub`, `zmq::socket_type::pull`,
  etc.). This determines the ZMQ pattern used for each connection.

`SubscriberPool` fixes `SOCKET_TYPE` to `zmq::socket_type::sub` and adds the
`subscribe`/`unsubscribe` API. Subscription changes are applied to every connected socket in the
map while holding `sockets_mutex_`, so they are safe to call from any thread.

### Starting and Stopping the Pool

The pool has to be started and stopped explicitly at construction with `startPool()` and before destruction via `stopPool()`.
The active starting procedure allows to only send CHIRP requests for the pooled service once all derived classes are set up
and ready to receive messages. Similarly, the destructor is not responsible for stopping the pool thread, because derived
class destructors run before the base class destructor. This means that by the time `~BasePool()` would call `stopPool()`,
members of the derived class that might be referenced by the callback or the virtual hooks, have already been destroyed.
Therefore, callers are responsible for calling `stopPool()` before the pool object goes out of scope, typically in the
derived class destructor.

## The `SubscriberPool`

The `SubscriberPool` is an extension of the `BasePool` class that provides facilities for managing ZMQ topic subscriptions.
Different `subscribe` and `unsubscribe` methods are provided that operate either on all sockets held by the poller, and that
alter the subscription to a topic across all of them, or that work on individual sockets identified either by their CHIRP
host hash, or by their unique host identifier.

Subscriptions are altered while holding the sockets mutex.

## `constellation::pools` Namespace

```{doxygenclass} constellation::pools::BasePool
:members:
:protected-members:
```

```{doxygenclass} constellation::pools::SubscriberPool
:members:
```
