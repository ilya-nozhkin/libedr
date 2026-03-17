module testbench ();
  reg    clk;

  wire    tck;
  wire    tms;
  wire    tdi;
  wire    tdo;

  wire    tdo_padoe;

  wire    shift_dr;
  wire    pause_dr;
  wire    update_dr;
  wire    capture_dr;

  wire    extest_select;
  wire    sample_preload_select;
  wire    mbist_select;
  wire    debug_select;

  wire    internal_tdo;

  wire    debug_tdi = 0;
  wire    bs_chain_tdi = 0;
  wire    mbist_tdi = 0;

  tunneled_jtag tunneled_jtag_instance (
      .clk_i(clk),
      .system_is_idle_i(1),

      .tck_o(tck),
      .tms_o(tms),
      .tdi_o(tdi),
      .tdo_i(tdo)
  );

  tap_top tap_top_instance (
      .tms_pad_i  (tms),
      .tck_pad_i  (tck),
      .trst_pad_i (0),
      .tdi_pad_i  (tdi),
      .tdo_pad_o  (tdo),
      .tdo_padoe_o(tdo_padoe),

      .shift_dr_o  (shift_dr),
      .pause_dr_o  (pause_dr),
      .update_dr_o (update_dr),
      .capture_dr_o(capture_dr),

      .extest_select_o(extest_select),
      .sample_preload_select_o(sample_preload_select),
      .mbist_select_o(mbist_select),
      .debug_select_o(debug_select),

      .tdo_o(internal_tdo),

      .debug_tdi_i(debug_tdi),
      .bs_chain_tdi_i(bs_chain_tdi),
      .mbist_tdi_i(mbist_tdi)
  );

  initial begin
    if (&{1'b0, tdo_padoe, shift_dr,  pause_dr,  update_dr,  capture_dr,  extest_select,  sample_preload_select,  mbist_select,  debug_select,  internal_tdo}) begin
    end

    clk = 0;

    while (1) begin
      clk = 1;
      #10;
      clk = 0;
      #10;
    end
  end

endmodule
