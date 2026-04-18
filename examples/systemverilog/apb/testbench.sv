module testbench ();
  reg clk;
  reg presetn;

  // Helper entities, treat them as boilerplate for now.
  edr_Context ctx;
  edr_ExecutionGate execution_gate;

  // The APB driver's API for controlling the driver.
  // The driver instance will give it to us later.
  edr_APB apb;
  edr_APBTransaction apb_xact;

  // APB signals.

  wire [15:0] paddr;
  wire psel;
  wire penable;
  wire pwrite;
  wire [31:0] pwdata;

  wire pready;
  wire [31:0] prdata;
  wire pslverr;

  wire hw_ctl;

  // The driver instance. Most of the signals are APB signals.
  // The rest work like this:
  //
  // .context_i
  // .execution_gate_i - boilerplate, pass as is.
  //
  // .apb_o - The API for controlling this driver.
  //
  // .system_is_idle_i -
  //     Tells the driver whether the DUT is not doing anything meaningful.
  //     Depending on the mode of edr_ExecutionGate, the driver can pause the
  //     simulation in this case if there are no pending requests to avoid
  //     consuming compute resources and dumping empty waveforms.
  //     This is only useful when the test is performed by an external
  //     connection (e.g. Python). Since we are driving the test from SV,
  //     we can assume that the system is always doing something meaningful,
  //     otherwise the test scenario itself can get paused.
  edr_apb #(
      .ADDR_WIDTH(16),
      .DATA_WIDTH(32)
  ) edr_apb_instance (
      .system_is_idle_i(0),

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

  // The DUT goes below.

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

  // Clock.

  initial begin
    clk = 0;

    while (1) begin
      clk = 1;
      #10;
      clk = 0;
      #10;
    end
  end

  // Reset.

  initial begin
    presetn = 0;
    #50;
    presetn = 1;
  end

  // And finally, here is the test scenario.

  initial begin
    // APB addresses we are going to access.
    int unsigned addr1 = 0;
    int unsigned addr2 = 8;

    // Data to write.
    int unsigned data1 = 32'h11223344;
    int unsigned data2 = 32'haabbccdd;

    // Storage for reads.
    int unsigned out1;
    int unsigned out2;

    // $dumpfile("waves.vcd");
    // $dumpvars(0);

    // Initialize the prerequisites.
    //
    // edr_LogLevel_TRACE means that the driver will print very detailed
    // descriptions of every transaction including every action and every
    // piece of data inside.
    //
    // ctx.AddFile("edr_rtl.log") means that the log should be written into
    // the specified file. You can also redirect it to stdout by calling
    // ctx.AddStdStreams();
    //
    // edr_ExecutionGateMode_NoStall means that the simulation should never
    // be paused whether the DUT is idle or not.

    ctx = make_edr_Context(edr_LogLevel_TRACE);
    ctx.AddFile("edr_rtl.log");

    execution_gate = make_edr_ExecutionGate(ctx, "ExecutionGate");
    /* verilator lint_off IGNOREDRETURN */
    execution_gate.SetMode(edr_ExecutionGateMode_NoStall);
    /* verilator lint_on IGNOREDRETURN */

    // Initially, the API is not initialized, we need to wait until
    // the driver creates it.
    wait (apb != null);

    // Now we can start scheduling transactions.
    // Every transaction should have a name, it is written to the log and helps
    // to identify the intention of a failed operation.
    //
    // Then we can add as many actions into one transaction as needed.
    // Actions are executed sequentially. If one fails, the rest are rejected.
    //
    // Once all actions are added, call the "Do" task to schedule the
    // transaction and block the process until it is complete.
    // If you need multiple transactions to run in parallel (e.g. sent to
    // different drivers), use these methods instead:
    //
    // xact1.Schedule();
    // xact2.Schedule();
    // xact1.Join(clk);
    // xact2.Join(clk);

    apb_xact = apb.Initiate("Writing some regs");
    apb_xact.Write(addr1, data1);
    apb_xact.Write(addr2, data2);
    apb_xact.Do(clk);

    $display("Wrote %0h <- %0h", addr1, data1);
    $display("Wrote %0h <- %0h", addr2, data2);

    // Transaction instances can be reused to avoid allocating a new one
    // every time. Semantically, it is considered a completely new transaction,
    // so it needs a new name.

    apb_xact.Reuse("Reading regs");
    apb_xact.Read(addr1);
    apb_xact.Read(addr2);
    apb_xact.Do(clk);

    // Actions are added one by one, and the results are extracted in a similar
    // manner and in the same order.
    // Immediately after completion, the "extraction cursor" is pointing at
    // the very first action. Use Next() to move it to the next action.
    // Use NextN(10) to move it 10 actions forward.

    out1 = apb_xact.GetReadData();
    apb_xact.Next();
    out2 = apb_xact.GetReadData();

    $display("Read %0h -> %0h", addr1, out1);
    $display("Read %0h -> %0h", addr2, out2);

    // The data extracted by one of the previous transactions can be used in
    // subsequent transactions.

    apb_xact.Reuse("Writing &0 to &8");
    apb_xact.Write(addr2, out1);
    apb_xact.Do(clk);

    $display("Wrote %0h <- %0h", addr2, out1);

    apb_xact.Reuse("Reading regs");
    apb_xact.Read(addr1);
    apb_xact.Read(addr2);
    apb_xact.Do(clk);

    out1 = apb_xact.GetReadData();
    apb_xact.Next();
    out2 = apb_xact.GetReadData();

    $display("Read %0h -> %0h", addr1, out1);
    $display("Read %0h -> %0h", addr2, out2);

    $finish(0);
  end

endmodule
