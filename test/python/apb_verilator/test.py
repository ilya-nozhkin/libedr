import edr
from helpers import EDRTestCase


class TestAPBVerilator(EDRTestCase):
    def test_write_read_simple(self):
        tunnel = self.run_verilator("apb_verilator")
        apb: edr.APB = tunnel.FindAPB("APB")

        xact: edr.APBTransaction = apb.Initiate("write-read")
        xact.Write(0, 0x11223344)
        xact.Write(8, 0x55667788)
        xact.Read(8)
        xact.Read(0)

        xact.Do()

        xact.NextN(2)
        self.assertTrue(xact.ActionSuccess(), xact.ErrorMessage())
        self.assertEqual(xact.GetReadData(), 0x55667788)

        xact.Next()
        self.assertTrue(xact.ActionSuccess(), xact.ErrorMessage())
        self.assertEqual(xact.GetReadData(), 0x11223344)

    def test_error_prevents_further_actions(self):
        tunnel = self.run_verilator("apb_verilator")
        apb: edr.APB = tunnel.FindAPB("APB")

        xact: edr.APBTransaction = apb.Initiate("write-check")
        xact.Write(0, 0x11223344)
        xact.Write(8, 0xAABBCCDD)
        xact.Read(8)
        xact.Write(12, 0xFFFFFFFF)  # Reg #3 is read-only
        xact.Write(0, 0xFFFFFFFF)
        xact.Write(8, 0xFFFFFFFF)

        xact.Do()

        xact.NextN(2)
        self.assertTrue(xact.ActionSuccess(), xact.ErrorMessage())
        self.assertEqual(xact.GetReadData(), 0xAABBCCDD)

        xact.Next()
        self.assertTrue(xact.ActionFail())

        xact.Reuse("read")
        xact.Read(0)
        xact.Read(8)

        xact.Do()

        self.assertTrue(xact.ActionSuccess(), xact.ErrorMessage())
        self.assertEqual(xact.GetReadData(), 0x11223344)

        xact.Next()

        self.assertTrue(xact.ActionSuccess(), xact.ErrorMessage())
        self.assertEqual(xact.GetReadData(), 0xAABBCCDD)
