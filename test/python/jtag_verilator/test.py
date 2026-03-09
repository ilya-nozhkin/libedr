from helpers import EDRTestCase


class TestJTAGVerilator(EDRTestCase):
    def test_shift_idcode(self):
        tunnel = self.run_verilator("jtag_verilator")
