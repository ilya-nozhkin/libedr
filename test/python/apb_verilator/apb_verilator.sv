module apb_verilator ();
  wire clk;

  edr_Error error;
  edr_Context ctx;
  edr_ExecutionGate execution_gate;
  edr_ByteStreamTunnel tunnel;

  edr_APB apb;

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

      .error_o(error),
      .context_o(ctx),
      .execution_gate_o(execution_gate),
      .tunnel_o(tunnel)
  );

  edr_apb #(
      .ADDR_WIDTH(16),
      .DATA_WIDTH(32)
  ) edr_apb_instance (
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

      .context_i(ctx),
      .execution_gate_i(execution_gate),
      .apb_o(apb)
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

  initial begin
    wait (apb != null);
    tunnel.RegisterDriver(apb);

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
