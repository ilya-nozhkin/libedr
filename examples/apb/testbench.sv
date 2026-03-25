module testbench ();
  reg clk;
  reg presetn;

  wire [15:0] paddr;
  wire psel;
  wire penable;
  wire pwrite;
  wire [31:0] pwdata;

  wire pready;
  wire [31:0] prdata;
  wire pslverr;

  wire hw_ctl;

  tunneled_apb #(
      .ADDR_WIDTH(16),
      .DATA_WIDTH(32)
  ) tunneled_apb_instance (
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
      .pslverr(pslverr)
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
