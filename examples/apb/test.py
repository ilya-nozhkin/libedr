import subprocess
import os
import sys
from pathlib import Path

EDR_INSTALL_DIR = os.environ.get("EDR_INSTALL_DIR")
if EDR_INSTALL_DIR is None:
    print(
        "EDR_INSTALL_DIR environment variable is not set. "
        "Point it to an EDR installation directory."
    )
    sys.exit(1)

# Adding the Python API to sys.path to be able to import it.
EDR_PACKAGE_DIR = Path(EDR_INSTALL_DIR) / "python"
sys.path.append(os.fspath(EDR_PACKAGE_DIR))

import edr

# libcedr.so should be visible from LD_LIBRARY_PATH so that the model
# built by Verilator can link dynamically to the C API.
EDR_SHLIB_DIR = Path(EDR_INSTALL_DIR) / "lib"
os.environ["LD_LIBRARY_PATH"] = os.pathsep.join(
    [os.fspath(EDR_SHLIB_DIR), os.environ.get("LD_LIBRARY_PATH", "")]
)


class Example:
    def __init__(self):
        # Context serves two purposes: it owns all allocated objects, and it
        # also controls logging (AddFile means that the log should be tee-d to
        # the specified file).
        self.context = edr.Context(edr.LogLevel_TRACE)
        self.context.AddFile("edr_python.log")

        # Some functions need an error object to be able to return the status
        # and the explanation through it.
        self.error = edr.Error()

        exe_name = "Vtestbench"
        pipe_name = f"{exe_name}-{os.getpid()}"

        # Creating a Unix domain socket the model will be connecting to.
        # We will pass the name of the server as a plusarg.
        pipe_server = edr.NamedPipeServer(self.context, pipe_name, 1, self.error)
        if self.error.Fail():
            raise Exception(self.error.Message())

        command = f"./obj_dir/{exe_name}"

        # Launching the model.
        self.model_process = subprocess.Popen(
            [command, f"+edr-pipe={pipe_name}"],
            text=True,
            stdout=sys.stdout,
            stderr=sys.stderr,
        )

        # Here we wait until the model connects to the server, this gives us
        # a bi-directional byte stream that we can use for tunnelling.
        pipe = pipe_server.Accept(self.error)
        if self.error.Fail():
            raise Exception(self.error.Message())

        # Creating a tunnel over the byte stream. "Handshake" is a synchronous
        # method that guarantees that we receive all definitions of remote
        # drivers provided by the opposite side of the tunnel.
        self.tunnel = edr.ByteStreamTunnel(self.context, pipe)

        self.tunnel.Hanshake(self.error)
        if self.error.Fail():
            raise Exception(self.error.Message())

        # And now we can start requesting handles of remote drivers.
        # Each driver is given a name (see the NAME parameter specified
        # during the instantiation of edr_apb module in tunneled_apb.sv).
        # It is possible to get a handle of a tunneled driver that would
        # have the exact same API as if it was a local instance.
        self.apb: edr.APB = self.tunnel.FindAPB("APB")

    def terminate(self):
        # The model keeps running while the tunnel is open, so we need to
        # close the tunnel to let the model know we are done with the tests.
        self.tunnel.Terminate()
        self.model_process.wait()

    def do_test(self):
        # This function shows an example of a multi-action transaction.
        # A transaction is initiated by a driver object. Each transaction
        # must be given a name which is used for logging and error reporting.
        xact: edr.APBTransaction = self.apb.Initiate("Write regs")

        # Each of the following calls adds one action to the transaction, but
        # nothing is getting sent to the model yet.

        # Write at addresses 0x0 and 0x8
        xact.Write(0x0, 0x11223344)
        xact.Write(0x8, 0x55667788)

        # Wait 6 ticks of pclk without initiating any transfers
        xact.SkipCycles(6)

        # Read from 0x0 and 0x8
        xact.Read(0x0)
        xact.Read(0x8)

        # The "Schedule" method finally sends the entire transaction to the
        # target. But it does not block the current thread. It is possible
        # to schedule multiple transactions simultaneously without waiting
        # for them to complete.
        xact.Schedule()

        # The "Join" method blocks the current thread until the transaction
        # is completed. There is also a shortcut for Schedule + Join:
        # xact.Do()
        xact.Join()

        # At this point, the transaction is considered complete. It means
        # that either all actions have been executed successfully or one
        # of the actions failed and the rest of them were dropped.
        assert xact.Done()

        # Now we can iterate through actions and check their statuses or
        # extract the results. Initially, the iterator points to the very
        # first action. "Next" advances it to the next action.
        # We are interested in the TDO which was the third action, so
        # let's advance the iterator twice and check the status.
        # There is a shortcut for multiple "Next"s:
        # xact.NextN(3)
        xact.Next()
        xact.Next()
        xact.Next()
        if xact.ActionFail():
            raise Exception(xact.ErrorMessage())

        # Now we can extract data captured by the "Read" actions.
        data0 = xact.GetReadData()

        # In this example, each action status is checked explicitly, but it is
        # also possible to check whether the entire transaction was completed
        # successfully or if at least one action failed using:
        # xact.Success()
        # xact.Fail()
        xact.Next()
        if xact.ActionFail():
            raise Exception(xact.ErrorMessage())

        data8 = xact.GetReadData()

        print(f"0x0: {hex(data0)}")
        print(f"0x8: {hex(data8)}")


if __name__ == "__main__":
    try:
        example = Example()
        example.do_test()
        example.terminate()
    except Exception as e:
        print(f"{e}", file=sys.stderr)
        sys.exit(1)
