import unittest
import threading
import edr
import time


class TestTCPSocketLoopback(unittest.TestCase):
    def test_send_receive(self):
        data1 = b"123456789"
        data2 = b"abcdef"

        error = edr.Error()
        context = edr.Context(edr.LogLevel_TRACE)
        context.AddStdStreams()

        server = edr.TCPServer(context, 0, 0, error)
        self.assertTrue(server.IsValid(), error.Message())

        port = server.GetPort()

        def client():
            client_error = edr.Error()
            client_stream: edr.ByteStream = edr.ByteStream.ConnectTCP(
                context, "localhost", port, client_error
            )

            self.assertTrue(client_stream.IsValid(), client_error.Message())

            # Make sure the reading request is issued first to test that
            # the 'Join' function releases the GIL. If it does not release it,
            # and the read request comes first, then the test will never
            # complete because the writing thread will be waiting for the
            # reading thread.
            time.sleep(0)

            xact: edr.ByteStreamTransaction = client_stream.Initiate("write")
            xact.WriteBytes(data1)
            xact.WriteBytes(data2)
            xact.Schedule()
            xact.Join()

            self.assertEqual(len(data1), xact.GetNumWrittenBytes())
            xact.Next()
            self.assertEqual(len(data2), xact.GetNumWrittenBytes())

        client_thread = threading.Thread(target=client)
        client_thread.start()

        server_stream: edr.ByteStream = server.Accept(error)
        self.assertTrue(server_stream.IsValid(), error.Message())

        xact: edr.ByteStreamTransaction = server_stream.Initiate("read")
        xact.ReadBytes(len(data1))
        xact.ReadBytes(len(data2))
        xact.Schedule()
        xact.Join()

        data_out1 = bytearray(len(data1))
        data_out2 = bytearray(len(data2))
        num_read1 = xact.GetReadBytes(data_out1)
        xact.Next()
        num_read2 = xact.GetReadBytes(data_out2)
        self.assertEqual(len(data1), num_read1)
        self.assertEqual(data1, data_out1)
        self.assertEqual(len(data2), num_read2)
        self.assertEqual(data2, data_out2)

        client_thread.join()
