from typing import List

import edr
import unittest
import os
import subprocess


class EDRTestCase(unittest.TestCase):
    def setUp(self):
        super().setUp()

        self.context = edr.Context(edr.LogLevel_TRACE)
        self.context.AddStdStreams()

        self.error = edr.Error()

        self.subprocesses: List[subprocess.Popen] = []
        self.tunnels: List[edr.ByteStreamTunnel] = []

    def tearDown(self):
        for tunnel in self.tunnels:
            tunnel.Terminate()

        for subp in self.subprocesses:
            subp.wait()
            self.assertEqual(subp.returncode, 0)

        super().tearDown()

    def run_verilator(self, exe_name: str) -> edr.ByteStreamTunnel:
        pipe_name = f"{exe_name}-{os.getpid()}"

        pipe_server = edr.NamedPipeServer(self.context, pipe_name, 1, self.error)
        self.assertTrue(self.error.Success(), self.error.Message())

        command = f"./obj_dir/V{exe_name}"

        subp = subprocess.Popen([command, f"+pipe={pipe_name}"], text=True)
        self.subprocesses.append(subp)

        self.assertIsNone(subp.returncode)

        pipe = pipe_server.Accept(self.error)
        self.assertTrue(self.error.Success(), self.error.Message())

        tunnel = edr.ByteStreamTunnel(self.context, pipe)

        tunnel.Hanshake(self.error)
        self.assertTrue(self.error.Success(), self.error.Message())

        self.tunnels.append(tunnel)
        return tunnel
