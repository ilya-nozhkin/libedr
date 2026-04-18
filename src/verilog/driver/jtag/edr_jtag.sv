module edr_jtag #(
    parameter string NAME = "JTAG"
) (
    input clk_i,
    input system_is_idle_i,

    output tck_o,
    output tms_o,
    output tdi_o,
    input  tdo_i,

    input edr_Context context_i,
    input edr_ExecutionGate execution_gate_i,

    output edr_Jtag jtag_o
);
  parameter int BYTES_PER_BATCH = EDR_ARRAY_SIZE;
  parameter int BITS_PER_BATCH = BYTES_PER_BATCH * 8;
  parameter int LAST_BIT = BITS_PER_BATCH - 1;

  edr_PullJtag pull_jtag;

  reg [LAST_BIT:0] tms;
  reg [LAST_BIT:0] tdi;
  reg [LAST_BIT:0] tdo;

  int unsigned num_pending_bits;
  int unsigned num_bits_to_capture;

  int unsigned bit_to_capture;

  wire tck_gate;
  assign tck_gate = num_pending_bits != 0;

  assign tck_o = clk_i & tck_gate;
  assign tms_o = tms[0];
  assign tdi_o = tdi[0];

  function automatic void ExchangeData();
    byte tms_buf[BYTES_PER_BATCH];
    byte tdi_buf[BYTES_PER_BATCH];
    byte tdo_buf[BYTES_PER_BATCH];
    int unsigned num_pushed_bits;
    int unsigned new_num_bits;

    for (
        int unsigned bits = 0
        ,
        int unsigned bytes = 0;
        bits < num_bits_to_capture;
        bits += 8, bytes++
    ) begin
      tdo_buf[bytes] = tdo[bits+:8];
    end

    num_pushed_bits = pull_jtag.PushTDO(tdo_buf, num_bits_to_capture);
    if (num_pushed_bits != num_bits_to_capture) begin
      $display("The number of pushed TDO bits does not equal to to the number of requested bits");
      $finish(0);
    end

    bit_to_capture <= 0;

    execution_gate_i.StallIfNeeded(system_is_idle_i ? byte'(1) : byte'(0));

    new_num_bits = pull_jtag.PullTMSTDI(tms_buf, BITS_PER_BATCH, tdi_buf, BITS_PER_BATCH);

    num_pending_bits <= new_num_bits;
    num_bits_to_capture <= new_num_bits;

    for (
        int unsigned bits = 0, int unsigned bytes = 0; bits < new_num_bits; bits += 8, bytes++
    ) begin
      tms[bits+:8] <= tms_buf[bytes];
      tdi[bits+:8] <= tdi_buf[bytes];
    end
  endfunction

  initial begin
    tms = 0;
    tdi = 0;
    tdo = 0;
    num_pending_bits = 0;
    num_bits_to_capture = 0;
    bit_to_capture = 0;

    wait (context_i != null);
    wait (execution_gate_i != null);

    pull_jtag = make_edr_PullJtag(context_i, NAME, execution_gate_i);
    jtag_o = pull_jtag;
  end

  final begin
    if (pull_jtag != null) begin
      pull_jtag.delete();
    end
  end

  always @(negedge clk_i) begin
    if (pull_jtag != null) begin
      if (num_pending_bits == 0 || num_pending_bits == 1) ExchangeData();
      else begin
        tms <= {1'b0, tms[LAST_BIT:1]};
        tdi <= {1'b0, tdi[LAST_BIT:1]};
        num_pending_bits <= num_pending_bits - 1;
        bit_to_capture <= bit_to_capture + 1;
      end
    end
  end

  always @(posedge clk_i) begin
    if (num_bits_to_capture != 0) begin
      tdo[bit_to_capture] <= tdo_i;
    end
  end

endmodule
