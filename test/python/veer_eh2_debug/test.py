import edr
from helpers import EDRTestCase


class TestVeerEH2Debug(EDRTestCase):
    def test_read_idcode_and_dtmcs(self):
        tunnel = self.run_verilator("tb_top")
        jtag: edr.Jtag = tunnel.FindJtag("JTAG")
        self.assertTrue(jtag.IsValid())

        chain: edr.JtagChain = edr.JtagChain(self.context, "JtagChain", jtag)

        xact: edr.JtagChainTransaction = chain.Initiate("read_idcode")
        xact.SetIRLength(0, 5)
        xact.ShiftDR(0, 32)
        xact.WriteIR(0x10)
        xact.ShiftDR(0, 32)
        xact.Do()

        xact.Next()
        idcode = xact.GetShiftedData()
        xact.NextN(2)
        dtmcs = xact.GetShiftedData()

        self.assertEqual(idcode, 0x1000008B)
        self.assertEqual(dtmcs, 0x71)
