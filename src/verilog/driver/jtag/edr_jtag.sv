module edr_jtag #(
    parameter int BITS_PER_BATCH = 128,
    parameter string NAME = "JTAG"
) (
    input clk_i,
    input system_is_idle_i,

    output tck_o,
    output tms_o,
    output tdi_o,
    input  tdo_i,

    input chandle context_handle_i,
    input event   context_initialized_event_i,

    input chandle execution_gate_handle_i,
    input event   execution_gate_initialized_event_i,

    output chandle jtag_handle_o,
    output chandle driver_base_handle_o,
    output event   jtag_initialized_event_o
);
  import "DPI-C" function chandle edr_PullJtag_new(
    input chandle ctx,
    input string  name,
    input chandle exe_gate
  );

  import "DPI-C" function void edr_PullJtag_delete(input chandle jtag);

  import "DPI-C" function int unsigned edr_PullJtag_PullTMSTDI(
    input chandle jtag,
    output byte tms_dest[BYTES_PER_BATCH],
    input int unsigned max_num_tms,
    output byte tdi_dest[BYTES_PER_BATCH],
    input int unsigned max_num_tdi
  );

  import "DPI-C" function int unsigned edr_PullJtag_PushTDO(
    input chandle jtag,
    input byte src_bits[BYTES_PER_BATCH],
    input int unsigned num_bits
  );

  import "DPI-C" function chandle edr_PullJtag_CastToBase(input chandle jtag);

  import "DPI-C" function void edr_ExecutionGate_StallIfNeeded(
    input chandle exe_gate,
    input byte target_is_idle
  );

  parameter int LAST_BIT = BITS_PER_BATCH - 1;
  parameter int BYTES_PER_BATCH = BITS_PER_BATCH / 8;

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

    num_pushed_bits = edr_PullJtag_PushTDO(jtag_handle_o, tdo_buf, num_bits_to_capture);
    if (num_pushed_bits != num_bits_to_capture) begin
      $display("The number of pushed TDO bits does not equal to to the number of requested bits");
      $finish(0);
    end

    bit_to_capture <= 0;

    edr_ExecutionGate_StallIfNeeded(execution_gate_handle_i,
                                    system_is_idle_i ? byte'(1) : byte'(0));

    new_num_bits =
        edr_PullJtag_PullTMSTDI(jtag_handle_o, tms_buf, BITS_PER_BATCH, tdi_buf, BITS_PER_BATCH);

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
    jtag_handle_o = 0;

    wait (context_initialized_event_i.triggered);
    wait (execution_gate_initialized_event_i.triggered);

    jtag_handle_o = edr_PullJtag_new(context_handle_i, NAME, execution_gate_handle_i);
    driver_base_handle_o = edr_PullJtag_CastToBase(jtag_handle_o);

    ->jtag_initialized_event_o;
  end

  final begin
    if (jtag_handle_o != 0) begin
      edr_PullJtag_delete(jtag_handle_o);
    end
  end

  always @(negedge clk_i) begin
    if (jtag_handle_o != 0) begin
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
