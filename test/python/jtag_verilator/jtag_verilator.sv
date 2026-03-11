module jtag_verilator ();
  wire    clk;

  chandle context_handle;
  event   context_initialized_event;

  chandle execution_gate_handle;
  event   execution_gate_initialized_event;

  chandle jtag_handle;
  chandle jtag_base_handle;
  event   jtag_initialized_event;

  wire    tck;
  wire    tms;
  wire    tdi;
  wire    tdo;

  // reg     [31:0] debug;
  // reg     [31:0] bs_chain;
  // reg     [31:0] mbist;

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

  test_tunnel test_tunnel_instance (
      .clk_o(clk),

      .context_handle_o(context_handle),
      .context_initialized_event_o(context_initialized_event),

      .execution_gate_handle_o(execution_gate_handle),
      .execution_gate_initialized_event_o(execution_gate_initialized_event),

      .driver_base_handle_i(jtag_base_handle),
      .driver_initialized_event_i(jtag_initialized_event)
  );

  edr_jtag #(
      .BITS_PER_BATCH(16)
  ) edr_jtag_instance (
      .clk_i(clk),
      .system_is_idle_i(1),

      .tck_o(tck),
      .tms_o(tms),
      .tdi_o(tdi),
      .tdo_i(tdo),

      .context_handle_i(context_handle),
      .context_initialized_event_i(context_initialized_event),

      .execution_gate_handle_i(execution_gate_handle),
      .execution_gate_initialized_event_i(execution_gate_initialized_event),

      .jtag_handle_o(jtag_handle),
      .driver_base_handle_o(jtag_base_handle),
      .jtag_initialized_event_o(jtag_initialized_event)
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
  end

endmodule
