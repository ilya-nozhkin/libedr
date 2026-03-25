module apb_verilator ();
  wire clk;

  chandle context_handle;
  chandle execution_gate_handle;
  chandle apb_handle;

  wire presetn;

  wire [15:0] paddr;
  wire psel;
  wire penable;
  wire pwrite;
  wire [31:0] pwdata;

  wire pready;
  wire [31:0] prdata;
  wire pslverr;

  wire hw_ctl;

  test_tunnel test_tunnel_instance (
      .clk_o(clk),
      .resetn_o(presetn),

      .context_handle_o(context_handle),
      .execution_gate_handle_o(execution_gate_handle),
      .driver_handle_i(apb_handle)
  );

  edr_apb #(
      .ADDR_WIDTH(16),
      .DATA_WIDTH(32)
  ) edr_jtag_instance (
      .system_is_idle_i(1),

      .pclk(clk),
      .presetn(presetn),

      .paddr(paddr),
      .psel(psel),
      .penable(penable),
      .pwrite(pwrite),
      .pwdata(pwdata),

      .pready (pready),
      .prdata (prdata),
      .pslverr(pslverr),

      .context_handle_i(context_handle),
      .execution_gate_handle_i(execution_gate_handle),
      .apb_handle_o(apb_handle)
  );

  apb_slave #(
      .AW(16),
      .DW(32)
  ) apb_slave_instance (
      .pclk(clk),
      .presetn(presetn),

      .i_paddr  (paddr),
      .i_pwrite (pwrite),
      .i_psel   (psel),
      .i_penable(penable),
      .i_pwdata (pwdata),
      .i_pstrb  (4'b1111),
      .o_prdata (prdata),
      .o_pslverr(pslverr),
      .o_pready (pready),

      .o_hw_ctl(hw_ctl),
      .i_hw_sts(1)
  );

endmodule
