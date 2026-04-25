import edr
from helpers import EDRTestCase


class TestJTAGVerilator(EDRTestCase):
    def test_shift_idcode(self):
        tunnel = self.run_verilator("jtag_verilator")
        jtag: edr.Jtag = tunnel.FindJtag("JTAG")

        xact: edr.JtagTransaction = jtag.Initiate("reset")
        xact.PutTMS(0b11111.to_bytes(2, "little"), 5)
        xact.Do()

        xact.Reuse("read-IDCODE")
        xact.PutTMS(0b0010.to_bytes(2, "little"), 4)
        xact.PutTDIGetTDO(b"\0" * 4, 32, 1)
        xact.Do()

        xact.Next()
        self.assertFalse(xact.ActionFail(), xact.ErrorMessage())

        tdo = bytearray(4)
        xact.GetTDO(tdo, 32)

        idcode = int.from_bytes(tdo, "little")
        self.assertEqual(idcode, 0x149511C3)

    def test_shift_idcode_jtag_chain(self):
        tunnel = self.run_verilator("jtag_verilator")
        chain: edr.JtagChain = tunnel.FindJtagChain("JtagChain")

        xact: edr.JtagChainTransaction = chain.Initiate("read_idcode")
        xact.ShiftDR(0, 32)
        xact.Do()

        idcode = xact.GetShiftedData()

        self.assertEqual(idcode, 0x149511C3)
