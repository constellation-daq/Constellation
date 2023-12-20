import time
from enum import Enum
import logging

import zmq
import msgpack

from logging.handlers import QueueHandler, QueueListener


class MsgHeader:
    def __init__(self, sender, timestamp: msgpack.Timestamp, tags: dict):

        self.sender = sender
        self.time = timestamp
        self.tags = tags

    def time_ns(self): return self.time.to_unix_nano()

    def time_s(self): return self.time.to_unix()

    def encode(_obj):
        return [_obj.sender, _obj.time, _obj.tags]

    def decode(_data):
        return MsgHeader(_data[0], _data[1], _data[2])


class DataType(Enum):
    INT = 1
    FLOAT = 2
    ARRAY2D = 3


class LogLevels(Enum):
    DEBUG = logging.DEBUG
    INFO = logging.INFO
    WARNING = logging.WARNING
    ERROR = logging.ERROR


class Statistics:
    def __init__(self, name: str, socket: zmq.Socket) -> None:
        self.name = name
        self.socket = socket

    def _dispatch(self, topic: str, header, value):
        self.socket.send_string(topic, zmq.SNDMORE)
        self.socket.send(msgpack.packb(header, default=MsgHeader.encode), zmq.SNDMORE)
        self.socket.send(value)

    def sendStats(self, topic: str, type: DataType, value):
        statTopic = "STATS/" + topic
        header = MsgHeader(
            self.name,
            msgpack.Timestamp.from_unix_nano(time.time_ns()),
            {'type': type.value}
        )
        self._dispatch(statTopic, header, bytes(str(value), 'utf-8'))


def getLoggerAndStats(name: str, context: zmq.Context, port: int):
    """Set up and return a logger and statistics object."""
    # Create socket and bind wildcard
    socket = context.socket(zmq.PUB)
    socket.bind(f'tcp://*:{port}')
    logger = logging.getLogger(name)
    zmqhandler = ZeroMQSocketHandler(socket)
    # set lowest level to ensure that all messages are published
    zmqhandler.setLevel(0)
    logger.addHandler(zmqhandler)
    stats = Statistics(name, socket)
    return logger, stats


class ZeroMQSocketHandler(QueueHandler):
    """This handler sends records to a ZMQ socket."""

    def __init__(self, socket: zmq.Socket):
        super().__init__(socket)

    def enqueue(self, record):
        topic = f'LOG/{record.levelname}/{record.filename}'
        header = MsgHeader(record.name, msgpack.Timestamp.from_unix_nano(time.time_ns()), {})
        self.queue.send_string(topic, zmq.SNDMORE)
        self.queue.send(msgpack.packb(header, default=MsgHeader.encode), zmq.SNDMORE)
        # Instead of just adding the formatted message, this adds all attributes
        # of the logRecord, allowing to reconstruct the entire message on the
        # other end.
        self.queue.send(msgpack.packb(
            {'name': record.name,
             'msg': record.msg,
             'args': record.args,
             'levelname': record.levelname,
             'levelno': record.levelno,
             'pathname': record.pathname,
             'filename': record.filename,
             'module': record.module,
             'exc_info': record.exc_info,
             'exc_text': record.exc_text,
             'stack_info': record.stack_info,
             'lineno': record.lineno,
             'funcName': record.funcName,
             'created': record.created,
             'msecs': record.msecs,
             'relativeCreated': record.relativeCreated,
             'thread': record.thread,
             'threadName': record.threadName,
             'processName': record.processName,
             'process': record.process,
             'message': record.message,
             }
        ))

    def close(self):
        if not self.queue.closed:
            self.queue.close()


class ZeroMQSocketListener(QueueListener):
    def __init__(self, uri, /, *handlers, **kwargs):
        self.ctx = kwargs.get('ctx') or zmq.Context()
        socket = zmq.Socket(self.ctx, zmq.SUB)
        socket.setsockopt_string(zmq.SUBSCRIBE, 'LOG/')  # subscribe to LOGs
        socket.connect(uri)
        kwargs.pop('ctx', None)
        super().__init__(socket, *handlers, **kwargs)

    def dequeue(self, block):
        _topic = self.queue.recv()
        _header = MsgHeader.decode(msgpack.unpackb(self.queue.recv()))
        record = msgpack.unpackb(self.queue.recv())
        return logging.makeLogRecord(record)


def main(args=None):
    """Start a simple log listener service."""
    import argparse

    parser = argparse.ArgumentParser(description=main.__doc__)
    parser.add_argument("--log-level", default="info")
    parser.add_argument("host", type=str)
    parser.add_argument("port", type=int)
    args = parser.parse_args(args)
    logger = logging.getLogger(__name__)
    formatter = logging.Formatter('%(asctime)s | %(name)s |  %(levelname)s: %(message)s')
    stream_handler = logging.StreamHandler()
    stream_handler.setLevel(args.log_level.upper())
    stream_handler.setFormatter(formatter)
    logger.addHandler(stream_handler)

    ctx = zmq.Context()
    zmqlistener = ZeroMQSocketListener(f'tcp://{args.host}:{args.port}', stream_handler, ctx=ctx)
    zmqlistener.start()
    while True:
        #msg = socket.recv_json()
        #print(msg)
        time.sleep(0.01)


if __name__ == "__main__":
    main()
