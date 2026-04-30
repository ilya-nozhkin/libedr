import edr
from helpers import EDRTestCase


class TestVeerEH2Debug(EDRTestCase):
    def setup_chain(self, jtag: edr.Jtag) -> edr.JtagChain:
        chain: edr.JtagChain = edr.JtagChain(self.context, "JtagChain", jtag)

        xact: edr.JtagChainTransaction = chain.Initiate("Set IR length")
        xact.SetIRLength(0, 5)
        xact.Do()

        return chain

    def test_read_idcode_and_dtmcs(self):
        tunnel = self.run_verilator("tb_top")
        jtag: edr.Jtag = tunnel.FindJtag("JTAG")
        self.assertTrue(jtag.IsValid())

        chain = self.setup_chain(jtag)

        xact: edr.JtagChainTransaction = chain.Initiate("Read IDCODE")
        xact.ShiftDR(0, 32)
        xact.WriteIR(0x10)
        xact.ShiftDR(0, 32)
        xact.Do()

        idcode = xact.GetShiftedData()
        xact.NextN(2)
        dtmcs = xact.GetShiftedData()

        self.assertEqual(idcode, 0x1000008B)
        self.assertEqual(dtmcs, 0x71)

    def test_reset_dm_halt_core(self):
        tunnel = self.run_verilator("tb_top")
        jtag: edr.Jtag = tunnel.FindJtag("JTAG")
        self.assertTrue(jtag.IsValid())

        chain = self.setup_chain(jtag)

        dtm_config = edr.RVJtagDTMConfiguration()
        dtm_config.SetMinMaxLatency(5, 100)
        dtm: edr.RVDTM = edr.RVDTM.RVJtagDTM(self.context, "RVDTM", chain, dtm_config)

        xact: edr.RVDTMTransaction = dtm.Initiate("Reset DM, read dmstatus")
        xact.Write(0x10, 0)
        xact.Write(0x10, 1)
        xact.Read(0x11)
        xact.Do()

        xact.NextN(2)
        self.assertEqual(xact.GetReadData(), 0xc0c82)

        xact: edr.RVDTMTransaction = dtm.Initiate("Halt the core")
        xact.Write(0x10, 1 | (1 << 31))
        xact.Write(0x10, 1)
        xact.Read(0x11)
        xact.Do()

        xact.NextN(2)
        self.assertEqual(xact.GetReadData(), 0xc0382)
