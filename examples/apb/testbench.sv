module testbench ();
  reg clk;
  reg presetn;

  edr_Error error;
  edr_Context ctx;
  edr_ExecutionGate execution_gate;

  edr_ByteStream pipe;
  edr_ByteStreamTunnel tunnel;

  edr_APB apb;

  wire [15:0] paddr;
  wire psel;
  wire penable;
  wire pwrite;
  wire [31:0] pwdata;

  wire pready;
  wire [31:0] prdata;
  wire pslverr;

  wire hw_ctl;

  function static string get_pipe_name();
    string pipe_name;
    $value$plusargs("edr-pipe=%s", pipe_name);
    return pipe_name;
  endfunction

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
    // $dumpfile("waves.vcd");
    // $dumpvars(0);

    error = make_edr_Error();

    ctx   = make_edr_Context(edr_LogLevel_TRACE);
    ctx.AddFile("edr_rtl.log");

    execution_gate = make_edr_ExecutionGate(ctx, "ExecutionGate");

    pipe = make_edr_ByteStream_ConnectNamedPipe(ctx, get_pipe_name(), error);
    if (error.Fail()) begin
      $display("Failed to connect to named pipe '%s': %s", get_pipe_name(), error.Message());
      $finish(1);
    end

    tunnel = make_edr_ByteStreamTunnel(ctx, pipe);
    tunnel.RegisterDriver(execution_gate);

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

  initial begin
    if (hw_ctl);

    clk = 0;

    while (1) begin
      clk = 1;
      #10;
      clk = 0;
      #10;
    end
  end

  initial begin
    presetn = 0;
    #50;
    presetn = 1;
  end

endmodule
