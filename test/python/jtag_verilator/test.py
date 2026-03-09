import unittest
import edr
import os
import sys


class TestJTAGVerilator(unittest.TestCase):
    def test_shift_idcode(self):
        context = edr.Context(edr.LogLevel_TRACE)
        context.AddStdStreams()

        error = edr.Error()

        pipe_name = f"edr-pipe-{os.getpid()}"

        pipe_server = edr.NamedPipeServer(context, pipe_name, 1, error)
        self.assertTrue(error.Success(), error.Message())

        # pipe = pipe_server.Accept(error)
        # self.assertTrue(error.Success(), error.Message())
