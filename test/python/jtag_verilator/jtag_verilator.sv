module jtag_verilator ();
  wire clk;
  wire reset;

  edr_Error error;
  edr_Context ctx;
  edr_ExecutionGate execution_gate;
  edr_ByteStreamTunnel tunnel;

  edr_Jtag jtag;

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
      .resetn_o(reset),

      .error_o(error),
      .context_o(ctx),
      .execution_gate_o(execution_gate),
      .tunnel_o(tunnel)
  );

  edr_jtag edr_jtag_instance (
      .clk_i(clk),
      .system_is_idle_i(1),

      .tck_o(tck),
      .tms_o(tms),
      .tdi_o(tdi),
      .tdo_i(tdo),

      .context_i(ctx),
      .execution_gate_i(execution_gate),
      .jtag_o(jtag)
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
    wait (jtag != null);
    tunnel.RegisterDriver(jtag);

    tunnel.StartServer(error);
    if (error.Fail()) begin
      $display("Failed to start serving the byte stream tunnel: ", error.Message());
      $finish(1);
    end

    while (tunnel.IsAlive()) begin
      @(posedge clk);
    end

    $finish(0);
  end

endmodule
